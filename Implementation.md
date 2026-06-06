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

Labels do not produce IR instructions. When `hikari L` is encountered, the current IR index is recorded in the label map under the label name. When `Hikari L` or `HIKARI L S` is encountered, the label name and current IR index are recorded in the patch list and a placeholder IR instruction with `target = 0` is emitted.

### Pass 3: Backpatching

Each entry in the patch list is resolved against the label map using `strcmp`. If a label is not found a LabelError is raised. If found the `target` field of the corresponding IR instruction is set to the resolved IR index. After this pass the label map and patch list are freed.

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
    if max_steps > 0 and steps >= max_steps: halt

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

All arithmetic operates on 32-bit signed integers and wraps on overflow. Division and modulo return `0` when the right operand is zero.

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
| `--verbose` | Enable debug output: token dump, IR dump, run markers |
| `--max-steps N` | Halt after N instructions (0 = unlimited) |

The version string is generated at build time from the CMake project version via `configure_file` and `version.h.in`.

---

## Error Types

| Error        | When raised                                                    |
|--------------|----------------------------------------------------------------|
| `LexerError` | Unknown character; word exceeding 31 characters                |
| `ParseError` | Token sequence does not match any valid instruction form       |
| `LabelError` | Label used in a jump was never defined                         |

All errors carry the line and column of the offending token.

---

## Known Limitations

- `rei` and `Rei` operate on single ASCII bytes. Multi-byte UTF-8 input and output are not yet supported.
- Division by zero and arithmetic overflow are defined behavior and produce no runtime error or warning.
- CTest integration is planned for a future release.
