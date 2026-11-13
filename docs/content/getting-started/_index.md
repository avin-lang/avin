---
title: Getting started
summary: Install Avin, run a program, and run the tests.
weight: 5
---

Avin is early in its [roadmap](/compiler/roadmap/): the compiler builds whole programs to native
executables, but the language it accepts is still a small part of the [specification](/spec/).

## Install

On an Apple silicon Mac, with Homebrew:

```sh
brew install avin-lang/tap/avin
avin --version
```

The release is also a tarball, `avin-<version>-darwin-arm64.tar.gz`, laid out as a prefix: put its
`bin/` on your `PATH`, and `avin` finds the standard library and runtime in `share/avin` beside
itself, through any symbolic link to it. It never writes there: the runtime archives and binding
shims it builds are kept in `~/Library/Caches/avin/<version>` (`$XDG_CACHE_HOME/avin` or
`~/.cache/avin` elsewhere; `AVIN_CACHE` names another directory), and so are a program's executables
and what its builds keep for the next, in a directory of the program's own under `build/` there.
Nothing is written beside the program.

What an installed `avin` runs:

- **clang**, which links every program: Xcode's command-line tools (`xcode-select --install`)
  provide it.
- **sysl**, which builds the shims of the Sysl bindings a program reaches, and the runtime archive
  when the shipped one does not match the runtime's sources. A program that reaches no binding needs
  no Sysl at all, since the runtime ships prebuilt; the standard library's string case mapping,
  files, processes and threads are bindings. Homebrew installs it with Avin.

A missing tool is reported by name, with what installs it.

## Build from source

Working on the compiler, the library or the runtime needs:

- Sysl 0.10.7, with a checkout of Sysl's `parsing` package at `../../sysl-lang/parsing` relative to
  the repository: the repository is cloned into a directory named for its organization
  (`avin-lang/avin`), beside a `sysl-lang` directory holding `sysl-lang/parsing`
- clang on `PATH`

From the repository root:

```sh
sysl build compiler -o compiler/avin
./compiler/avin build examples/hello.avin
./hello
```

`avin build file.avin` writes `./file`, and `-o` chooses another path. `avin check` checks a program
without building it, and `avin emit-llvm file.avin -o file.ll` writes its LLVM IR. A checkout's
`compiler/avin` finds the standard library and runtime in the checkout; `AVIN_ROOT` names another
location, and is only for development: an installed `avin` needs no setting.

`avin run` builds a program and runs it in one step:

```sh
./compiler/avin run examples/hello.avin
./compiler/avin run my-program -O0 -- first second
```

The program writes to the terminal as it runs, and `avin run` exits with the program's own status;
when a trap stops the program, the status is 128 and the signal's number, as a shell reports it.
Everything after `--` is passed to the program as its arguments. The executable is kept in the
program's own directory in the cache ([running and testing](/compiler/#running-and-testing)), and the
next run of an unchanged program starts it without building it again. Any change to the program's
sources, the standard library, the runtime or the compiler builds it again, and every build, `avin
build` and `avin test` included, keeps what it made of each function there too, so a rebuild after an
edit makes again only what the edit can change ([incremental builds](/compiler/#incremental-builds);
`AVIN_INCREMENTAL=off` turns it off). `avin clean` removes the program's cache and `avin clean --all`
every program's; either is always safe.

A program is a source file or a directory: `avin run my-program` builds every `.avin` and `.lavin`
file under `my-program/`. `build`, `run` and `test` all take `-O0` or `-O2`, and build at `-O2` by
default.

`run` is the default command and the current directory the default program, so inside a package
(or a directory of `.avin` files) these are all the same:

```sh
avin
avin run
avin run .
```

`avin hello.avin -O0 -- first second` is `avin run hello.avin -O0 -- first second`: a first argument
that is not a command name is the program to run. Options with no program before them, as in
`avin -O0` or `avin -- first`, are for the current directory's program. `parse`, `check`, `build`,
`emit-llvm`, `run` and `test` likewise take the current directory when no program is named (`avin
build`, `avin check`, `avin test`); `tangle` and `weave` always name their file. A directory with no
`avin.hocon` above it and no `.avin` or `.lavin` files is not a program, and `avin` there says so.

A command name is always the command. A file or directory named like one is reached by a path that
is not the bare word: `avin ./test`, or `avin run test`. A first argument that is neither a command
nor an existing path is refused with the list of commands.

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
  println("clamping 4")
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

With no program named, `avin test` tests the current directory. Its first argument is the program
when it names an existing file or directory, and the filter otherwise: `avin test "low bound"` filters
the current directory's tests, and `avin test . low` filters when a path called `low` also exists.

The report is written as the tests finish, at most twice a second, so a log read while the run is
alive is current. SIGTERM or SIGINT writes out the rest at once, lists every test and build still in
progress with the time it has taken, and exits with 143 or 130. `--prefix` puts each file's path and
a colon before every result line, for output that shares a log with other runs.

```text
running 2 tests

clamp.avin
  ok    clamps below the low bound  2ms
  FAIL  leavesInRange               1ms
        assertion failed: four is left as it is at clamp.avin:10:3
        > clamping 4

1 passed, 1 failed — 4ms
```

Tests are grouped by the file they are written in, in the order they are declared. A failing test
shows why it failed, then everything it printed, each line after `>`; a passing test's output is
not shown. The exit status is 1 when any test failed. A second argument keeps only the tests whose
name or file contains it. A program built for its tests need not have `main`, and `avin run` and
`avin build` never run its tests.

A test may answer with a `Result[unit, E]` instead, failing on an `Err`, and `@test` may say more
about running it: `should_trap` for a test that passes by trapping, `timeout: 5` for a limit of its
own, `ignore: "why"` for one that is checked and not run. `@setup` and `@teardown` mark functions run
around each of a module's tests, and `@setup_all` and `@teardown_all` once around them all. A file
whose header says `@tests` holds fixtures and helpers only the tests may use, and only `avin test`
builds. The [declarations chapter](/spec/declarations/#tests) has the details.

## Packages

`avin new` writes a package: a directory with an `avin.hocon` manifest, a module, a test and a
`.gitignore`. `--lib` makes a library, which has no `main`:

```sh
./compiler/avin new hello
./compiler/avin run hello
./compiler/avin test hello
./compiler/avin new geometry --lib
```

```hocon
package {
  name = "hello"
  version = "0.1.0"
}

dependencies {
  geometry { path = "../geometry" }
  json { git = "github.com/owner/json", version = "1.4.0" }
}
```

Every command takes a path anywhere in a package and compiles the whole package, with the packages
it depends on: `avin run hello/main.avin` and `avin test hello/tests` both mean `hello`. A
dependency's modules are imported by their own names. `avin build` names the executable after the
package. Fetched packages are kept in the machine's cache, and `avin.sum` beside the manifest records
what each one hashed to; commit it. `avin add github.com/owner/json@1.4.0` writes the dependency
into the manifest and fetches it, and without `@1.4.0` takes the highest release. `avin tidy` rewrites it to exactly what the package uses, and
`avin tidy --check` fails when it is not. The [packages chapter](/spec/packages/) has the rest.

A program of one file uses packages too, with no manifest: its first lines name them, after a
`// avin.hocon` line, in the comment lines under it, written exactly as a manifest's `dependencies`
block is. A `#!` line may come first, so the file runs as a command:

```text
#!/usr/bin/env avin
// avin.hocon
// dependencies {
//   json { git = "github.com/avin-lang/json", version = "0.1.0" }
// }

import dev.avinlang.json

main(args: Vector[string])
    ...
```

```sh
chmod +x tool.avin
./tool.avin data.json
```

`avin run`, `build`, `check` and `test` fetch what the header names into the same cache, and the
hashes go in `tool.avin.sum` beside the file. A blank line ends the header. Run through its `#!`
line, `./tool.avin data.json` is `avin ./tool.avin data.json`, and every argument after the file is
the program's own; `avin run tool.avin -O0 -- data.json` keeps the usual rule, with `-O0` avin's. The
[packages chapter](/spec/packages/#programs-of-one-file) has the rules.

## Formatting

`avin fmt` rewrites every `.avin` and `.lavin` file under the paths it is given (the current
directory when there are none) in Avin's one style, and `avin fmt --check` lists the files that are
not in it and fails, for a build or a commit hook:

```sh
./compiler/avin fmt hello
./compiler/avin fmt --check hello
```

Each block level is two spaces in, and a line is broken only past 120 characters, one item of a list
to a line; a sequence or set literal of short numbers, characters, strings, booleans or names is
instead filled, as many items to a line as fit, so a table stays a table. Comments and blank lines stay where they are, at most one blank line in a row, except
that a function's signature and its clauses stay together with no blank line among them. In a
literate file only the program is formatted, inside its four-column margin. A file that does not
parse is left alone and its errors are reported. The [decisions](/spec/decisions/) list the style
in full; the language server formats with the same rules.

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
