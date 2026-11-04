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

`avin run` builds a program and runs it in one step:

```sh
./compiler/avin run examples/hello.avin
./compiler/avin run my-program -O0 -- first second
```

The program writes to the terminal as it runs, and `avin run` exits with the program's own status;
when a trap stops the program, the status is 128 and the signal's number, as a shell reports it.
Everything after `--` is passed to the program as its arguments. The executable is kept in `.avin/build/`
in the program's directory, and the next run of an unchanged program starts it without building it
again. Any change to the program's sources, the standard library, the runtime or the compiler
builds it afresh.

A program is a source file or a directory: `avin run my-program` builds every `.avin` and `.lavin`
file under `my-program/`. `build`, `run` and `test` all take `-O0` or `-O2`, and build at `-O2` by
default.

## Literate programs

A `.lavin` file is a Markdown document whose lines indented four columns are the program
([Literate source](/spec/declarations/#literate-source) has the rules). Every command that takes a
source file takes one, and a directory program may mix `.avin` and `.lavin` files freely.
`examples/digits.lavin` is one. Two commands run each half of a literate system on its own:

```sh
./compiler/avin tangle examples/digits.lavin
./compiler/avin tangle examples/digits.lavin -o digits.avin
./compiler/avin weave examples/digits.lavin -o digits.html
```

- **`avin tangle`** prints the program with the prose stripped and the four columns taken off, line
  for line with the document, so line 30 of what it prints is line 30 of the file. It shows exactly
  what the compiler reads, which is the first thing to check when a literate file misbehaves: a
  block indented that should not have been, or a fence that swallowed a function.
- **`avin weave`** renders the document as one HTML page, with its stylesheet inside it: the prose
  as Markdown with GitHub's extensions, and every program block, and every fenced block marked
  `avin`, coloured by the compiler's own lexer. Without `-o` the page goes to standard output.

Both read the file as a build does, so a file the compiler refuses (a tab in an indentation, a fence
never closed) is refused with the same diagnostic, and neither needs the library or the runtime.

## Testing a program

A test is a function marked `@test` that takes nothing and yields nothing. It passes by returning,
and fails by stopping, which a failed assertion does:

```avin
clamp(n: int, low: int, high: int) -> int = if n < low then low elif n > high then high else n

@test("clamps below the low bound")
below()
    assertEq(clamp(0, 1, 3), 1)

@test
leavesInRange()
    print("clamping 4")
    assert(clamp(4, 1, 3) == 4, "four is left as it is")
```

`assert(condition)` and `assert(condition, message)` stop the program when the condition is false;
`assertEq(actual, expected)` stops it when the two differ, showing both in their `Debug` form. Each
reports the place of the assertion itself.

`avin test` builds the program's tests into one executable and runs each test in a process of its
own, so a test that traps stops only itself:

```sh
./compiler/avin test my-program
./compiler/avin test my-program "low bound"
```

```text
running 2 tests

clamp.avin
  ok    clamps below the low bound  2ms
  FAIL  leavesInRange               1ms
        assertion failed: four is left as it is at clamp.avin:10:5
        > clamping 4

1 passed, 1 failed — 4ms
```

Tests are grouped by the file they are written in, in the order they are declared. A failing test
shows why it failed, then everything it printed, each line after `>`; a passing test's output is
not shown. The exit status is 1 when any test failed. A second argument keeps only the tests whose
name or file contains it. A program built for its tests need not have `main`, and `avin run` and
`avin build` never run its tests.

## The compiler's tests

```sh
sysl test compiler
```

This runs the compiler's unit tests and every program in `tests/`, which are built and run at `-O0`
and `-O2`.

## Editors

`avin lsp` is a language server for any editor with a Language Server Protocol client:
[Editors](/getting-started/editors/) says what it answers.

## Documentation

```sh
juicer build -s docs -d docs/public
juicer serve -s docs -L
```
