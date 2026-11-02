# Kage Implementation Reference

This document describes the internal architecture of the Kage interpreter. It is intended for contributors and anyone implementing Kage from scratch.

---

## Pipeline

```
Source Code (UTF-8)
        |
        v
      LEXER
        |  flat token list
        v
    PARSER Pass 1: Pre-Scan
        |  instruction count, label count, patch count
        v
    PARSER Pass 2: Code Generation
        |  IR array + label map + patch list + register address normalization
        v
    PARSER Pass 3: Backpatching
        |  IR with resolved jump targets. Error on undefined labels.
        v
        VM
        |
        v
      stdout / stdin
```

---

## Lexer

The lexer reads the source character by character and produces a flat list of tokens. Whitespace and comments are discarded. The output contains no structural information beyond token type, value, line, and column.

### Token Types

| Type           | Pattern          | Value field |
|----------------|------------------|-------------|
| `STORE_LIT`    | `yami`           | none        |
| `STORE_CPY`    | `Yami`           | none        |
| `STORE_EXP`    | `YAMI`           | none        |
| `FLOW_LAB`     | `hikari`         | none        |
| `FLOW_JMP`     | `Hikari`         | none        |
| `FLOW_COND_JMP`| `HIKARI`         | none        |
| `IO_PRINT`     | `rei`            | none        |
| `IO_READ`      | `Rei`            | none        |
| `IO_DEC_PRINT` | `REI`            | none        |
| `NUM`          | `-?[0-9]+`       | int32       |
| `OP`           | `[+\-*/%]`       | char        |
| `LABEL`        | `[a-zA-Z]{1,31}` | string      |
| `EOF_TOKEN`    | end of input     | none        |

Token enum types with a numeric value below 100 correspond directly to an IR instruction. Token enum types 100 and above are structural tokens that produce no IR output (`NUM`, `OP`, `LABEL`, `EOF_TOKEN`).

### Scanning Rules

Words are scanned by reading characters while `isalpha` returns true. The resulting string is compared against all nine keywords. If no keyword matches the word becomes a `LABEL` token. A word longer than 31 characters is a LexerError.

Numbers are scanned by reading an optional leading `-` followed by one or more digits. A leading `-` is only treated as a number sign when immediately followed by a digit; otherwise it is scanned as an operator.

Operators are single characters from the set `+`, `-`, `*`, `/`, `%`.

Comments begin with `#` and are discarded up to the end of the line.

Any other character is a LexerError.

### Label Constraints Enforced by the Lexer

The following constraints follow directly from the scanning rules:

- Labels consist only of `isalpha` characters because the word scanner stops at any non-alpha character.
- Labels are at most 31 characters because the scanner enforces a maximum word length of 32 bytes including the null terminator.
- Labels cannot begin with a digit or `-` because those trigger the number scanner first.
- Labels cannot contain operators or whitespace because those terminate the word scanner.
- Labels cannot be keywords because the keyword check runs before a LABEL token is produced.

---

## Parser

The parser runs in three passes over the token list.

### Pass 1: Pre-Scan

A single forward scan over the token list counts:

- Total IR instructions (token type < 100)
- Label definitions (`FLOW_LAB`)
- Jump instructions (`FLOW_JMP`, `FLOW_COND_JMP`)

These counts are used to allocate the IR array, label map, and patch list in a single allocation round before any IR is emitted.

### Pass 2: Code Generation

The parser maintains a `parser_state` carrying the token position, IR write index, label map, patch list, and a register normalization table (vector).

The main loop reads the leading token of each instruction and dispatches to the appropriate handler. `NUM`-led instructions are handled by `parse_num`, which reads the following keyword and dispatches to the correct form. All other instructions are handled inline in the main switch.

Register normalization happens during this pass. Every register address read from a token is passed through `patch_reg`, which maintains a list of seen register addresses and returns a normalized index starting at 1. Register 0 is always returned as 0 and never added to the table. The result is that the IR contains only small consecutive indices regardless of the register numbers used in source.

Labels do not produce IR instructions. When `hikari L` is encountered, the current IR index is recorded in the label map under the label name. A repeated label definition is rejected. When `Hikari L` or `HIKARI L S` is encountered, the label name, source location, and current IR index are recorded in the patch list and a placeholder IR instruction with `target = 0` is emitted.

### Pass 3: Backpatching

Each entry in the patch list is resolved against the label map using `strcmp`. If a label is not found a location-aware LabelError is raised. If found the `target` field of the corresponding IR instruction is set to the resolved IR index. After this pass the label map and patch list are freed.

---

## Intermediate Representation

The IR is a dense array of `ir_instruction` structs indexed by the instruction pointer.

```c
typedef struct {
    ir_type type;
    union {
        struct { uint32_t dest; int32_t literal;   } init_register;
        struct { uint32_t src;  uint32_t dest;      } copy_register;
        struct { uint32_t src1; uint32_t src2;
                 uint32_t dest; ir_operation op;    } calc_register;
        struct { size_t target; uint32_t cond;      } jmp;
        struct { uint32_t src;                      } io_out;
        struct { uint32_t dest;                     } io_in;
    };
} ir_instruction;
```

`io_out` is shared by `IO_OUT` and `IO_DOUT`. The `jmp` struct is shared by `JMP` and `COND_JMP`; for `JMP` the `cond` field is unused.

The `ir_arr` struct carries the instruction array, its length, and the register count produced by the normalization pass:

```c
typedef struct {
    ir_instruction* instruction;
    size_t          length;
    size_t          register_count;
} ir_arr;
```

`register_count` is the number of distinct non-zero registers used in the program. The VM allocates exactly `register_count` slots.

---

## Virtual Machine

### State

```c
typedef struct {
    const ir_arr* ir;
    size_t        pos;
    int32_t*      regs;
} vm_state;
```

`regs` is allocated with `calloc(ir->register_count, sizeof(int32_t))`. All registers are zero-initialized.

Register access goes through two helpers:

```c
get_register(state, reg)            // returns 0 if reg == 0, else regs[reg - 1]
set_register(state, reg, value)     // no-op if reg == 0, else regs[reg - 1] = value
```

The offset of `reg - 1` accounts for the zero register occupying index 0 in the normalized scheme while not occupying a slot in the array.

### Main Loop

```
ip = 0
steps = 0

loop:
    if ip >= ir.length: halt
    if max_steps > 0 and steps >= max_steps: report a runtime error and fail

    fetch ir[ip]
    ip++
    steps++

    INIT_REGISTER:  set(dest, literal)
    COPY_REGISTER:  set(dest, get(src))
    CALC_REGISTER:  set(dest, get(src1) op get(src2))
    JMP:            ip = target; continue
    COND_JMP:       if get(cond) != 0: ip = target; continue
    IO_OUT:         c = get(src); if c == 0: halt; putchar(c)
    IO_DOUT:        printf("%d\n", get(src))
    IO_IN:          c = getchar(); set(dest, c == EOF ? 0 : c)
```

### Arithmetic

All arithmetic operates on 32-bit signed integers. Addition, subtraction, and multiplication wrap modulo 2^32 using unsigned arithmetic, avoiding C signed-overflow undefined behavior. Division and modulo return `0` when the right operand is zero. The `INT32_MIN / -1` case wraps to `INT32_MIN`; the corresponding modulo result is `0`.

---

## Command-Line Interface

The interpreter is invoked as:

```
kage [options] <file>
kage [options] -e <code>
```

Argument parsing is handled by a manual forward scan over `argv`. Flags and the file argument may appear in any order. The first non-flag argument is treated as the file path.

| Flag | Description |
|------|-------------|
| `-h`, `--help` | Print usage and exit |
| `-v`, `--version` | Print version and exit |
| `-e <code>` | Execute the given string as Kage source |
| `-i <file>`, `--input <file>` | Read VM input from a file instead of standard input |
| `-o <file>`, `--output <file>` | Write VM output to a file instead of standard output |
| `--verbose` | Enable debug output: token dump, IR dump, run markers |
| `--max-steps N` | Fail if execution reaches N instructions without terminating (0 = unlimited) |

The version string is generated at build time from the CMake project version via `configure_file` and `version.h.in`.

---

## Diagnostics and Errors

The compiler and VM write diagnostics to standard error. Source locations are one-based (`line:column`). The examples below show the output for the indicated source or command. Compiler diagnostics may include a `help:` line with a suggested correction.

### Lexer errors

| Cause | Example source | Example output |
|---|---|---|
| Unexpected character | `@` | `LexerError (1:1): Unexpected character`<br>`  help: Remove this character or replace it with a valid Kage token.` |
| Word longer than 31 characters | `abcdefghijklmnopqrstuvwxyzABCDEF` | `LexerError (1:1): word exceeds maximum length of 31 characters`<br>`  help: Shorten this label to 31 ASCII letters or fewer.` |
| Integer greater than `INT32_MAX` | `1 yami 2147483648` | `LexerError (1:8): integer literal is outside the int32 range`<br>`  help: Use a value between -2147483648 and 2147483647.` |
| Integer less than `INT32_MIN` | `1 yami -2147483649` | `LexerError (1:8): integer literal is outside the int32 range`<br>`  help: Use a value between -2147483648 and 2147483647.` |
| Compiler cannot allocate memory while collecting tokens or diagnostics | No source-specific trigger | `Error: out of memory while collecting diagnostics` |

### Parser and label errors

| Cause | Example source | Example output |
|---|---|---|
| Instruction begins with an unexpected token | `+` | `ParseError (1:1): unexpected token`<br>`  help: Start an instruction with a register address or Kage keyword.` |
| A number is not followed by a storage keyword | `1 rei 0` | `ParseError (1:3): expected store keyword`<br>`  help: Follow a register address with yami, Yami, or YAMI.` |
| Required operand is missing | `1 yami` | `ParseError (1:7): expected NUM(102) but got EOF_TOKEN(105)`<br>`  help: Add a number or register address here (for example, 1).` |
| Register address is negative | `-1 yami 0` | `ParseError (1:1): register address must be non-negative`<br>`  help: Use register 0 or a positive register address.` |
| Jump references an undefined label | `Hikari missing` | `LabelError (1:8): undefined label missing`<br>`  help: Define this label with hikari before jumping to it.` |
| Label is defined more than once | `hikari loop` followed by `hikari loop` | `LabelError (2:8): duplicate label loop`<br>`  help: Rename this label or remove the earlier definition.` |
| Parser cannot allocate memory | No source-specific trigger | `Error: out of memory while collecting diagnostics` |
| Parser receives an empty token stream | Internal API only | `ParseError (1:1): token stream is empty` |

When an operand has the wrong type, the parser prints the expected and actual token names and numeric values, followed by a suggested correction. For example, `1 yami loop` produces `ParseError (1:8): expected NUM(102) but got LABEL(104)` and `  help: Add a number or register address here (for example, 1).`

The compiler continues lexing and parsing after recoverable errors, reports the collected diagnostics together, and does not execute the program if any compiler error was found. After a malformed instruction, parsing resumes at the next token that can begin an instruction. Consequently, later errors can still be found, though malformed input can sometimes make recovery ambiguous.

### Command-line errors

| Cause | Example command | Example output |
|---|---|---|
| Missing `-e` argument | `kage -e` | `Error: -e requires an argument` |
| Source specified more than once | `kage -e "1 yami 0" -e "2 yami 0"` | `Error: source code or file was specified more than once` |
| Missing `-i` argument | `kage program.kage -i` | `Error: -i requires an argument` |
| Input specified more than once | `kage program.kage -i a -i b` | `Error: input file was specified more than once` |
| Missing `-o` argument | `kage program.kage -o` | `Error: -o requires an argument` |
| Output specified more than once | `kage program.kage -o a -o b` | `Error: output file was specified more than once` |
| Missing `--max-steps` argument | `kage program.kage --max-steps` | `Error: --max-steps requires an argument` |
| `--max-steps` repeated | `kage program.kage --max-steps 1 --max-steps 2` | `Error: --max-steps was specified more than once` |
| Invalid `--max-steps` value | `kage program.kage --max-steps -1` | `Error: --max-steps requires a non-negative integer` |
| Unknown option | `kage --unknown` | `Error: unknown flag '--unknown'` |
| Unexpected extra file argument | `kage first.kage second.kage` | `Error: unexpected argument 'second.kage'` |
| No source specified after another option | `kage --verbose` | `Error: no code was given` |
| No source provided | `kage` | Prints help; this is not an error and exits successfully |

### Source-file errors

The path is substituted into `<path>`. Errors reported by the operating system include its message in place of `<system error>`, so that portion can vary by platform.

| Cause | Example output |
|---|---|
| Cannot open source file | `Error: cannot open source file <path>: <system error>` |
| Cannot seek or rewind source file | `Error: cannot seek source file <path>: <system error>` or `Error: cannot rewind source file <path>: <system error>` |
| Invalid or too-large source file | `Error: invalid source file size for <path>` |
| Source read fails | `Error: cannot read source file <path>: <system error>` |
| File changes size while being read | `Error: source file <path> changed while it was being read` |
| Cannot close source file | `Error: cannot close source file <path>: <system error>` |
| Memory allocation fails while reading | `Error: out of memory while reading <path>` |

For example, on systems that report `No such file or directory`, `kage missing.kage` prints `Error: cannot open source file missing.kage: No such file or directory`. If a source file fails to open or read, Kage reports the specific file error; it does not also print `Error: no code was given`.

### Runtime errors

Runtime errors occur after successful parsing and produce a failing process exit status.

| Cause | Example output |
|---|---|
| Input file cannot be opened | `Error: cannot open input file <path>: <system error>` |
| Output file cannot be opened | `Error: cannot open output file <path>: <system error>` |
| Input and output paths are identical | `Error: input and output paths must be different` |
| Register storage size overflows | `RuntimeError: program uses too many registers` |
| Register allocation fails | `RuntimeError: out of memory` |
| Configured instruction limit is reached | `RuntimeError: maximum instruction count (<limit>) reached` |
| Invalid arithmetic operation in the IR | `RuntimeError: invalid arithmetic operation` |
| Invalid instruction in the IR | `RuntimeError: invalid instruction` |
| VM input read fails | `RuntimeError: input read failed` |
| VM output write fails | `RuntimeError: output write failed` |
| Output file cannot be closed | `RuntimeError: failed to close output file <path>` |
| Standard output cannot be flushed | `RuntimeError: failed to flush standard output` |
| Input file cannot be closed | `RuntimeError: failed to close input file <path>` |

---

## Known Limitations

- `rei` and `Rei` operate on single ASCII bytes. Multi-byte UTF-8 input and output are not yet supported.
- Division by zero and arithmetic overflow are defined behavior and produce no runtime error or warning.
