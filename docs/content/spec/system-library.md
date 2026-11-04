---
title: System library
summary: The standard library's modules for the world outside the program — its process, files and the rest — written in Avin over Sysl bindings.
weight: 23
---

The system library is the part of the standard library that talks to the operating system. Each
module is an ordinary Avin module under `avin`, imported like any other, and written in Avin: what
it needs from the platform it reaches through [Sysl bindings](/spec/sysl-bindings/), or, for what
only the runtime holds, through the runtime's intrinsics. Failures a correct program can meet are
values; misuse is a trap.

## `avin.process`

The running program's process: the arguments it was started with, its environment, and ending it.

```text
import avin.process

main()
    val files = process.args()
    if files.isEmpty()
        print(s"usage: ${process.programName()} FILE...")
        process.exit(2)
    val home = process.env("HOME").getOrElse("/")
    for f in files do print(s"$home: $f")
```

| Function | Meaning |
| --- | --- |
| `args() -> Vector[string]` | The arguments after the program's name, in order. Empty arguments are kept. |
| `programName() -> string` | The name the program was started by, as the platform gave it: usually the path it was run as. Empty when the program was started with no arguments at all. |
| `env(name: string) -> Option[string]` | The environment variable's value, `None` when it is not set; a variable set to nothing is `Some("")`. |
| `exit(code: int) -> Nothing` | Ends the program now with `code` as its exit status, after flushing what it has written. |

**The program's name is not an argument.** `args()` holds only what the user typed after it, so
`args()(0)` is the first real argument, and the name is asked for by `programName()` when a
program wants it, for a usage line.

**Arguments are strings.** Every argument is checked as UTF-8 when the program starts, before
`main`; a program given one that is not UTF-8 stops there.

**`exit` never returns.** Its type is `Nothing`, so it stands in any branch, as a trap does. What the
program has written to standard output is flushed first, so output that goes to a pipe or a file is
not lost. The status the parent sees is the platform's: on POSIX systems, the code's low eight bits.
Returning an `int` from `main` sets the status the same way without stopping early.

**Tests see no arguments.** `avin test` runs each test in a process of its own and chooses the test
by an argument of its own, which the test does not see: in a test, `args()` is empty.

`avin run <program> -- ARGS` passes everything after `--` to the program unchanged, flags and a
second `--` included.

## `avin.io`

Standard input, read as text. Standard output and standard error need no import: `print` and
`eprint` are in the prelude, beside the writers `StandardOutput` and `StandardError`.

```text
import avin.io

main()
    var total = 0
    for line in io.lines()
        line.trim().toInt() match
            Some(n) -> total += n
            None -> eprint(s"not a number: $line")
    print(total)
```

| Function | Meaning |
| --- | --- |
| `readLine() -> Option[string]` | The next line without its terminator, `None` at the end of the input. An empty line is `Some("")`. |
| `lines() -> Iterator[string]` | The lines left, each as `readLine` gives it, read as the iterator advances. |
| `readAll() -> string` | Everything left, terminators and all; empty when nothing is left. |

**A line ends at `\n`, and a `\r` just before it belongs to the terminator**, so input written with
either convention reads the same. A `\r` anywhere else is part of the line. The last line needs no
terminator: input ending `a\nb` has the lines `a` and `b`, and input ending `a\n` only `a`.

**The three share one buffer.** The runtime reads standard input in large blocks and keeps what it
has read ahead, so `readLine`, `lines()` and `readAll` can be mixed: each takes up where the last
stopped, and nothing read ahead is lost. Reading blocks until a line is complete or the input ends,
so a program reading a terminal sees each line as it is entered.

**Input is text.** Bytes that are not UTF-8 become U+FFFD, one for each ill-formed run, as they are
read; a line is always a valid string, and a program reading text is never stopped by a stray byte.
A read that fails ends the input, as its end does.

**Writing to standard error flushes standard output first.** Standard output is buffered, and
flushed when the program ends, exits or traps; standard error is not buffered. Each write to
standard error first flushes what the program has written to standard output, so when both go to one
terminal or one file they arrive in the order the program wrote them. `eprint(x)` renders `x` and its
new line first and writes them in one write, so a line is never split.

**Tests see no input.** `avin test` gives each test an empty standard input, so a test never waits on
the terminal it was started from. `avin run` hands the program its own standard input.
