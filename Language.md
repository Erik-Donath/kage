# Kage Language Reference

Kage is a minimal register-based esoteric programming language. Programs are sequences of instructions that operate on an infinite set of 32-bit integer registers. There are no variables, no types beyond integers, and no call stack. Only registers, arithmetic, jumps, and I/O.

---

## Registers

Every register holds a 32-bit signed integer in the range -2,147,483,648 to 2,147,483,647. Registers are identified by non-negative integers and are lazily initialized to `0`.

Register 0 is the zero register. Reading it always returns `0`. Writing to it is silently ignored. It cannot be repurposed.

Arithmetic overflow wraps around using two's complement. Division by zero produces `0`.

---

## Program Structure

A Kage program is a sequence of instructions. Whitespace (spaces, tabs, newlines) is ignored except as a separator between tokens.

### Comments

A comment begins with `#` and extends to the end of the line. Comments have no effect on the program. A comment may appear anywhere whitespace is valid, including in the middle of an instruction:

```kage
1 yami   # store 65 into register 1
65
```

This is identical to `1 yami 65`.

### Termination

The program terminates when a `rei` instruction reads a value of `0` from the given register, or when the last instruction has been executed.

---

## Keywords

Kage has exactly nine keywords organized into three families. Each family covers one domain: storage, control flow, and I/O. Case is significant. `yami`, `Yami`, and `YAMI` are three distinct instructions.

---

## The YAMI Family

YAMI instructions read and write registers.

### `R yami N`

Stores the literal integer `N` into register `R`. `N` may be negative. This is the only instruction that takes a literal value rather than a register address.

```kage
1 yami 42
2 yami -7
```

### `R Yami S`

Copies the value of register `S` into register `R`.

```kage
2 Yami 1
```

### `R YAMI S op T`

Computes `reg[S] op reg[T]` and stores the result in register `R`. The operator must be one of `+`, `-`, `*`, `/`, `%`.

```kage
3 YAMI 1 + 2
4 YAMI 3 * 2
```

| Operator | Operation      | Notes                         |
|----------|----------------|-------------------------------|
| `+`      | Addition       | Wraps on overflow             |
| `-`      | Subtraction    | Wraps on overflow             |
| `*`      | Multiplication | Wraps on overflow             |
| `/`      | Division       | Division by zero produces `0` |
| `%`      | Modulo         | Division by zero produces `0` |

---

## The HIKARI Family

HIKARI instructions control the flow of execution using labels. A label marks a position in the program. Labels are resolved before execution begins and are not register addresses.

### `hikari L`

Defines label `L` at the current position. Execution resumes from the instruction immediately after this definition when jumped to. Each label may only be defined once.

```kage
hikari loop
```

### `Hikari L`

Jumps to label `L` unconditionally.

```kage
Hikari loop
```

### `HIKARI L S`

Jumps to label `L` if `reg[S]` is not zero. If `reg[S]` is zero, execution continues with the next instruction.

```kage
HIKARI loop 3
```

---

## The REI Family

REI instructions handle input and output.

### `rei S`

Prints `reg[S]` as an ASCII character to standard output. If `reg[S]` is `0`, the program terminates immediately.

```kage
rei 1
```

### `Rei S`

Reads one ASCII character from standard input and stores its value in register `S`.

```kage
Rei 2
```

### `REI S`

Prints `reg[S]` as a signed decimal integer. Intended for debugging.

```kage
REI 1
```

---

## Labels

Labels are identifiers used by the HIKARI family.

- A label consists entirely of alphabetic characters. No digits, operators, underscores, or other characters are permitted.
- A label is at most 31 characters long.
- Labels are case-sensitive. `Start` and `start` are different labels.
- A label must not be identical to any of the nine keywords.

Valid: `loop`, `Start`, `exitPoint`, `A`

Invalid: `yami` (keyword), `loop2` (contains digit), `my_label` (contains underscore)

---

## Grammar

```
program     ::= instruction* EOF

instruction ::= NUM 'yami'   NUM
              | NUM 'Yami'   NUM
              | NUM 'YAMI'   NUM OP NUM
              | 'hikari'     LABEL
              | 'Hikari'     LABEL
              | 'HIKARI'     LABEL NUM
              | 'rei'        NUM
              | 'Rei'        NUM
              | 'REI'        NUM

OP          ::= '+' | '-' | '*' | '/' | '%'
NUM         ::= -?[0-9]+
LABEL       ::= [a-zA-Z]{1,31}   (not a keyword)
```

---

## Errors

| Error      | Cause                                                                |
|------------|----------------------------------------------------------------------|
| LexerError | Unknown character; word that is neither a keyword nor a valid label  |
| ParseError | Unexpected token or incomplete instruction                           |
| LabelError | Jump to a label that was never defined; label defined more than once |

All errors report the line and column where the problem was detected.

---

## Examples

### Hello World

```kage
1 yami 72
2 yami 101
3 yami 108
4 yami 111

rei 1
rei 2
rei 3
rei 3
rei 4
rei 0
```

### Counter (prints 1 through 5)

```kage
1 yami 1     # counter
2 yami 1     # increment
3 yami 5     # limit

hikari loop

REI 1

4 YAMI 1 + 2
1 Yami 4

5 YAMI 3 - 1
HIKARI loop 5

rei 0
```

### Echo (reads and reprints characters until null byte)

```kage
hikari top
Rei 1
rei 1
Hikari top
```
