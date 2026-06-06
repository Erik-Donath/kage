# Kage

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language](https://img.shields.io/badge/Language-Kage-purple)](#)

An ordinary Eso-Lang.

## Overview

Kage is a minimal register-based esoteric programming language built around three keyword families:

| Family  | Keywords                   |
|---------|----------------------------|
| Yami    | `yami`, `Yami`, `YAMI`     |
| Hikari  | `hikari`, `Hikari`, `HIKARI` |
| Rei     | `rei`, `Rei`, `REI`        |

Programs operate on an infinite set of lazily initialized 32-bit registers, featuring arithmetic operations, conditional jumps, character I/O, and a dedicated zero register.

Read [Language.md](Language.md) for a complete language reference.
Read [Implementation.md](Implementation.md) for a breakdown of the interpreter internals.

## Usage

```sh
kage [options] <file>
kage [options] -e <code>

Options:
  -h, --help           Show this help message and exit
  -v, --version        Print version and exit
  -e <code>            Execute code directly
      --verbose        Enable debug output (token dump, IR dump)
      --max-steps N    Abort after N instructions (0 = unlimited)
```

## Building

Kage uses CMake and requires a C17-compatible compiler.

### Requirements

- CMake 3.20 or newer
- GCC, Clang, or another C17-compatible compiler

### Configure and Build

```sh
git clone https://github.com/Erik-Donath/kage.git
cd kage
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Run

```sh
./build/kage --help
./build/kage test.kage
./build/kage -e "1 yami 72 rei 1 rei 0"
./build/kage --verbose test.kage
./build/kage --max-steps 1000 test.kage
```

## License

Kage is licensed under the [MIT License](LICENSE).
