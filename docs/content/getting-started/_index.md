---
title: Getting started
summary: Build the compiler, run a program, and run the tests.
weight: 5
---

Avin is early in its [roadmap](/compiler/roadmap/): the compiler builds whole programs to native
executables, but the language it accepts is still a small part of the [specification](/spec/).

## Prerequisites

- Sysl 0.8.13, with a checkout of `parsing` at `../sysl-lang/` relative to the repository
- clang on `PATH`

## Build and run

From the repository root:

```sh
sysl build compiler -o compiler/avin
./compiler/avin build examples/hello.avin
./hello
```

`avin build file.avin` writes `./file`, and `-o` chooses another path. `avin check` checks a program
without building it, and `avin emit-llvm file.avin -o file.ll` writes its LLVM IR. The compiler finds
the standard library and runtime beside itself; `AVIN_ROOT` names another location.

## Tests

```sh
sysl test compiler
```

This runs the compiler's unit tests and every program in `tests/`, which are built and run at `-O0`
and `-O2`.

## Documentation

```sh
juicer build -s docs -d docs/public
juicer serve -s docs -L
```
