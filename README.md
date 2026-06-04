# Kage

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Language](https://img.shields.io/badge/Language-Kage-purple)](#)

An ordinary Eso-Lang.

## Overview

Kage is a minimal register-based esoteric programming language built around three keyword families:

| Family | Keywords |
|----------|----------|
| Yami | `yami`, `Yami`, `YAMI` |
| Hikari | `hikari`, `Hikari`, `HIKARI` |
| Rei | `rei`, `Rei`, `REI` |

Programs operate on an infinite set of lazily initialized 32-bit registers, featuring arithmetic operations, conditional jumps, character I/O, and a dedicated zero register.

Read [Language.md](Language.md) for a complete Language breakdown.

## Building

Kage uses CMake and requires a C17-compatible compiler.

### Requirements

* CMake 3.20 or newer
* GCC, Clang, or another C17-compatible compiler

### Configure

Clone the repository and create a build directory:

```sh
git clone https://github.com/Erik-Donath/kage.git
cd kage

mkdir build
cd build

cmake ..
```

### Build

Compile the project with:

```sh
cmake --build .
```

This produces the `kage` executable in the build directory.

### Run

To execute a Kage program:

```sh
./kage ../test.kage
```

Replace `test.kage` with the path to your own source file.

## License

Kage is licensed under the [MIT License](LICENSE).
