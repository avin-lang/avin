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
        println(s"usage: ${process.programName()} FILE...")
        process.exit(2)
    val home = process.env("HOME").getOrElse("/")
    for f in files do println(s"$home: $f")
```

| Function | Meaning |
| --- | --- |
| `args() -> Vector[string]` | The arguments after the program's name, in order. Empty arguments are kept. |
| `programName() -> string` | The name the program was started by, as the platform gave it: usually the path it was run as. Empty when the program was started with no arguments at all. |
| `env(name: string) -> Option[string]` | The environment variable's value, `None` when it is not set; a variable set to nothing is `Some("")`. |
| `exit(code: int) -> Nothing` | Ends the program now with `code` as its exit status, after flushing what it has written. |

**`main` may take the arguments itself.** `main(args: Vector[string])` receives the vector `args()`
answers, and `main()` is the form for a program that does not read them. The parameter is exactly
`Vector[string]` with no default; any other parameter list is an error. `args()` and `programName()`
remain available everywhere.

```text
main(args: Vector[string]) -> int
    for f in args do println(f)
    0
```

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

### The environment and the platform

What a program asks about where it runs is a function of `avin.process`, beside `env`;
there is no module for the environment alone and no store of properties. Each fact the platform
knows is asked for by name, and a program's own settings are its own values.

| Function | Meaning |
| --- | --- |
| `environment() -> Map[string, string]` | Every environment variable and its value, read when called. A variable set to nothing is there with `""`. |
| `currentDir() -> Result[string, IoError]` | The directory relative paths are read from. |
| `homeDir() -> Option[string]` | The user's home directory, `None` where the platform names none. |
| `executable() -> Option[string]` | The absolute path of the running program's file, where the platform can tell. |
| `processors() -> int` | How many processors the program may run on: the number the futures' pool grows to. |
| `pid() -> long` | The process's identifier. |
| `os() -> Os` | The operating system the program was compiled for: `MacOS`, `Linux`, `Windows`, `Android`, `Wasi` or `Freestanding`. |

**Nothing sets a variable.** The environment is the program's to read; a program that runs another
gives it the variables it needs through its [command](#commands), which changes nothing in its own.
Text ends lines with `\n` on every platform, so there is no line separator to ask for.

### Commands

A `Command` is another program to run: a program and its arguments, the directory and
variables it runs with and where its streams go, made first and run when asked. Commands combine into
pipelines and sequences, and what combines is again a command.

```text
import avin.process.{Command, CommandError}

sources(dir: string) -> Result[Vector[string], CommandError]
    val listing = Command("ls", dir) | Command("grep", "\\.avin$")
    Ok(listing.output()?.lines())

main() -> int
    "make test".run() match
        Ok(status) if status.isSuccess() -> 0
        _ -> 1
```

| Making | Meaning |
| --- | --- |
| `Command(program: string, args: string*)` | One program and its arguments, each passed as it is: no shell reads them. A program without a `/` is looked for on `PATH`. |
| `Command.parse(line: string)` | The line split at runs of white space, the first word the program; no quoting and no shell. A line with no word traps. |
| `c.inDir(dir: string)` | The same command, run in `dir`. |
| `c.withEnv(name: string, value: string)` | The same command, with the variable set for it; the rest of the program's environment is passed on. |
| `c.withTimeout(d: Duration)` | The same command, stopped when it has run for `d`; its status is then `TimedOut`. |
| `c.toFile(file: string)`, `c.appendTo(file: string)` | The same command, its standard output written to the file, emptied first or added to. |
| `c.fromFile(file: string)`, `c.withInput(text: string)` | The same command, reading the file, or the text, as its standard input. |
| `a andThen b` | `a`, then `b` only where `a` succeeded; the status is the last one run. |
| `a orElse b` | `a`, then `b` only where `a` did not succeed. |
| `a followedBy b` | `a`, then `b` whatever `a`'s status. |

**`a | b` is a pipeline**: `a`'s standard output is `b`'s standard input, the two running at once,
and its status is `b`'s.

**A setting on a command made of parts is every part's** ([proposed](/spec/decisions/#proposed)):
a part runs in the command's directory and with its variables unless it sets its own, a time limit
bounds the whole and a part not started in time is not started, output redirected on a pipeline is
its last stage's and input its first's, and a sequence's redirected file is opened once, its parts
writing, or reading, one after the other.

| Running | Meaning |
| --- | --- |
| `c.run() -> Result[ExitStatus, IoError]` | Runs the command with the program's own streams, waits, and answers how it ended. A status other than success is a value, not a failure. |
| `c.output() -> Result[string, CommandError]` | Runs it with its standard output captured as text, each ill-formed UTF-8 run replaced with U+FFFD, and standard error the program's own; any status but success is `Failed(status)`. |
| `c.lines() -> Result[Iterator[string], IoError]` | Starts it and answers its standard output's lines as they arrive, by the rule [standard input](#standard-input) reads lines by. Its status is not checked. |
| `c.start() -> Result[Child, IoError]` | Starts it and answers at once. A `Child` has `waitFor() -> Result[ExitStatus, IoError]`, `isAlive()` and `kill()`, which sends signal 15 to every program still running and starts no part not yet started. |

A started `Child` is [sendable](/spec/actors/#what-may-be-sent) ([proposed](/spec/decisions/#proposed)):
it belongs to one thread at a time, as a [connection](#tcp) does. Handed to a thread's or a future's
body it is the body's, which may wait for it, stop it and start a sequence's later part, and the code
that handed it over traps at any use of it while the body runs, though the command has ended; when the
body returns, the next code to use it takes it back:

```text
avin: trap: `Child` belongs to another thread at main.avin:12:3
```

The iterator `lines()` answers is not sendable: the thread that reads a command's output starts it.

```text
data ExitStatus
    Exited(code: int)
    Signalled(signal: int)
    TimedOut

data CommandError
    Io(error: IoError)
    Failed(status: ExitStatus)
```

`isSuccess()` is `true` of `Exited(0)` alone. A program that is not there fails to start: `run`
answers `Err(NotFound)` and `output` `Err(Io(NotFound))`.

**A string runs as a command.** Importing `avin.process` adds `run()` and `output()` to `string`, as
an [extension](/spec/declarations/#extensions): `"ls -l".run()` is `Command.parse("ls -l").run()`.

**Combining is spelled with words where an operator cannot be.** `|` is free on any type but the
integers and bytes, so a pipeline is `a | b`. Running is a method rather than a postfix `!` or `!!`,
because no method is called postfix and `!` is the language's own; `#` is no operator character,
and `&&` and `||` are the language's, so sequencing is `andThen`, `orElse` and `followedBy`, and
redirection `toFile`, `appendTo` and `fromFile`.

**Waiting is safe for the collector.** A thread waiting in `run`, `output` or `waitFor` holds
nothing the collector waits on, as `await` does not.

## `avin.io`

Standard input, read as text, and the error every module that reads or writes shares. Standard
output and standard error need no import: `print`, `println`, `eprint` and `eprintln` are in the prelude, beside the
writers `StandardOutput` and `StandardError`; each is one function taking `Any*` (see
[Strings and formatting](/spec/strings-and-formatting/)).

### `IoError`

What every module that reads or writes shares: the error an input or output operation fails with.

```text
data IoError
    NotFound
    PermissionDenied
    AlreadyExists
    IsADirectory
    NotADirectory
    DirectoryNotEmpty
    InvalidUtf8(offset: long)
    Other(text: string)
```

| Variant | Meaning |
| --- | --- |
| `NotFound` | Nothing exists at the path, or a directory on the way to it is missing. |
| `PermissionDenied` | The program may not read, write or search what the path names. |
| `AlreadyExists` | Something is already where a new file or directory was to be made. |
| `IsADirectory` | A directory was used where a file was expected. |
| `NotADirectory` | A file was used where a directory was expected. |
| `DirectoryNotEmpty` | A directory to be removed still holds entries. |
| `InvalidUtf8(offset)` | Text read is not UTF-8; `offset` is the first byte where it stops being. |
| `Other(text)` | Any other failure, described in words. |

An `IoError` is a value like any other: it is matched by its variants, shows as one (`NotFound`,
`InvalidUtf8(2)`) wherever a value is displayed, and `e.message()` gives a sentence for a person
(`no such file or directory`). **The named cases have one sentence each, the same on every platform;
`Other` carries the platform's own wording** (`File name too long`), so a failure with no case of its
own still says what happened. An operation that can fail answers a `Result[T, IoError]`, so `?`
passes the failure on from a function that answers one too.

### Standard input

```text
import avin.io

main()
    var total = 0
    for line in io.lines()
        line.trim().toIntOption() match
            Some(n) -> total += n
            None -> eprintln(s"not a number: $line")
    println(total)
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

**Input is text, and reading it does not fail.** Bytes that are not UTF-8 become U+FFFD, one for each
ill-formed run, as they are read; a line is always a valid string, and a program reading text is
never stopped by a stray byte. A read that fails ends the input, as its end does. Standard input is a
stream a program consumes as it arrives, where a file is read whole: `fs.readText` answers
`InvalidUtf8` instead, since a file's bytes can be read again as bytes.

**Writing to standard error flushes standard output first.** Standard output is buffered, and
flushed when the program ends, exits or traps; standard error is not buffered. Each write to
standard error first flushes what the program has written to standard output, so when both go to one
terminal or one file they arrive in the order the program wrote them. `eprintln(x)` renders `x` (or all its arguments) and its
new line first and writes them in one write, so a line is never split.

**Tests see no input.** `avin test` gives each test an empty standard input, so a test never waits on
the terminal it was started from. `avin run` hands the program its own standard input.

### `Source`

A `Source` is text to be read from somewhere, so one function can read a file, a string
or standard input alike.

```text
import avin.io.{IoError, Source}

count(src: Source) -> int = src.lines().count(l -> !l.isEmpty())

main() -> int
    Source.fromFile("notes.txt") match
        Ok(src) -> println(count(src))
        Err(e) -> eprintln(e.message())
    0
```

| Function | Meaning |
| --- | --- |
| `Source.fromFile(file: string) -> Result[Source, IoError]` | The file's text, read whole when the source is made, as `fs.readText` reads it: every failure, `InvalidUtf8` among them, is the `Err`. |
| `Source.fromString(text: string) -> Source` | The text itself. |
| `Source.stdin() -> Source` | Standard input, read as it is consumed and sharing `readLine`'s buffer. |
| `s.lines() -> Iterator[string]` | The lines left, by the rule standard input's lines follow. |
| `s.chars() -> Iterator[char]` | The characters left. |
| `s.readAll() -> string` | Everything left. |

**A source is read once.** Each of the three takes up where the last stopped, as standard input's
functions do; a second `lines()` over a source already read to its end has nothing. A source holds
nothing open, so there is nothing to close: a file's is its text, and standard input stays the
program's. **Standard input's characters come from the same buffer as its lines**, so `chars()`,
`lines()` and `io.readLine()` can be mixed, and bytes that are not UTF-8 are one U+FFFD for each
ill-formed run, character by character as in a line. Asking an exhausted iterator for more traps, as
with any iterator.

## `avin.fs`

Files and directories. A file is read or written whole, in one call.

```text
import avin.fs
import avin.io.IoError

copyNotes(from: string, to: string) -> Result[unit, IoError]
    val text = fs.readText(from)?
    fs.createDirAll(to)?
    fs.writeText(to + "/notes.txt", text)
```

| Function | Meaning |
| --- | --- |
| `readText(file) -> Result[string, IoError]` | The whole file as text; `InvalidUtf8` when it is not UTF-8. |
| `readBytes(file) -> Result[Array[byte], IoError]` | The whole file as bytes, in a new array. |
| `writeText(file, text) -> Result[unit, IoError]` | Makes the file hold exactly `text`, creating or replacing it. |
| `appendText(file, text) -> Result[unit, IoError]` | Adds `text` to the end of the file, creating it if it is not there. |
| `writeBytes(file, bytes) -> Result[unit, IoError]` | As `writeText`, with bytes. |
| `appendBytes(file, bytes) -> Result[unit, IoError]` | As `appendText`, with bytes. |
| `exists(file) -> bool` | Whether anything, file or directory, is at the path. |
| `isDir(file) -> bool` | Whether the path names a directory. |
| `isFile(file) -> bool` | Whether the path names something that is not a directory. |
| `remove(file) -> Result[unit, IoError]` | Removes a file; never a directory, which is `IsADirectory`. A link is removed, not what it names. |
| `removeDir(dir) -> Result[unit, IoError]` | Removes an empty directory; `DirectoryNotEmpty` otherwise. |
| `removeDirAll(dir) -> Result[unit, IoError]` | Removes a directory and everything in it; a path already gone is success. |
| `createDir(dir) -> Result[unit, IoError]` | Makes one directory, whose parent must exist; `AlreadyExists` if something is there. |
| `createDirAll(dir) -> Result[unit, IoError]` | Makes the directory and every missing one above it; a directory already there is success; a file where `dir` itself would go is `AlreadyExists`, and a file where one of the directories above it would go is `NotADirectory`. |
| `createTempDir(prefix) -> Result[string, IoError]` | Makes a new, empty directory in the platform's place for temporary files, its name starting with `prefix`, and answers its path. |
| `listDir(dir) -> Result[Vector[string], IoError]` | The names of the directory's entries, files and directories alike, sorted by code point; `NotFound` or `NotADirectory` when there is no directory to list. |

**Paths are strings**, and the questions follow symbolic links: `isDir` of a link to a directory is
`true`. `removeDirAll` is the exception that matters: a link inside the tree is removed, never
followed, so it deletes nothing outside the directory it was given.

**A temporary directory is the program's to remove.** `createTempDir` makes the directory rather than
inventing a name, so no other program can take it in between, and nothing removes it at the end:
`removeDirAll` is the call.

**Bytes are copied.** The array `readBytes` answers is new, and changing it changes nothing on disk;
the array given to `writeBytes` is read once and left as it was.

**A listing is names, sorted.** `listDir` answers each entry's name alone, without the directory
before it, and leaves out `.` and `..`; what is inside a subdirectory is not listed. The names are
sorted by code point, as strings order ([collections](/spec/collections/#sorting)), so `Zed` comes
before `a`, `a` before `a.txt`, and a name beyond ASCII after both; two listings of an unchanged
directory agree, whatever order the file system keeps. A name that is not UTF-8 makes the whole
listing `InvalidUtf8` rather than leaving that name out.

```text
for name in fs.listDir(dir)?
    if name.endsWith(".txt") then println(name)
```

### Open files

A file held open is a `File`, for a program that reads or writes a file piece by piece, follows one
another program is writing, or adds to a log.

```text
import avin.fs
import avin.threads.sleep

follow(log: string) -> Result[unit, IoError]
    val f = fs.open(log)?
    f.seekToEnd()?
    while true
        f.readCompleteLine()? match
            Some(line) -> println(line)
            None -> sleep(250.millis())
```

| Function | Meaning |
| --- | --- |
| `open(file) -> Result[File, IoError]` | The existing file, for reading, at its start; `IsADirectory` for a directory. |
| `create(file)` | For writing, made if it is not there and emptied if it is. |
| `append(file)` | For writing, made if it is not there; every write goes to the end, whatever other handles append to the same file. |
| `openUpdate(file)` | The existing file, for reading and writing, keeping what it holds. |
| `createUpdate(file)` | For reading and writing, made if it is not there and emptied if it is. |
| `readBytesFrom(file, offset) -> Result[Array[byte], IoError]` | Every byte from `offset` on, empty at or past the end: what was appended since a program last read to `offset`. A negative offset traps. |
| `readTail(file, maxBytes) -> Result[string, IoError]` | The last `maxBytes` bytes as text, reading only those. |
| `readTailLines(file, count) -> Result[Vector[string], IoError]` | The last `count` lines, without terminators, fewer where there are fewer. |

| Method on `File` | Meaning |
| --- | --- |
| `size()` | The bytes the file holds now, other handles' writes included. |
| `position()`, `seek(offset)`, `seekToEnd()` | The position counted from the start; `seek` past the end is allowed (a read there is empty, a write leaves zero bytes between), a negative offset is an `Err`; `seekToEnd` goes to the end as it is now. |
| `read(max)` | Up to `max` bytes, fewer only at the end of the file; empty at the end. Negative `max` traps. |
| `readAll()`, `readText()` | The rest of the file as bytes, or as text (`InvalidUtf8`, the offset counted from the position the read began at). |
| `readLine()`, `readCompleteLine()` | The next line, `None` at the end. `readLine` also answers a last line with no `\n`; `readCompleteLine` answers only a line ended by `\n`, and leaves later bytes unread, so a line another program is still writing arrives whole. |
| `write(bytes)`, `writeText(text)` | Writes at the position, buffered. |
| `flush()`, `close()` | `flush` hands what is written to the platform, so other handles and programs see it. `close` flushes and releases; closing twice is fine. |

**One position serves reading and writing**, and lines, `read` and `position` agree on it: bytes the
file has buffered ahead are never lost to a `seek`, a `write` or a `read`. **A read at the end does
not end the file's life**: a file that grew meanwhile gives the new bytes to the next read, so no
reopening is needed to follow it. **A file closed by `close` answers every call with an `Err`**, as does
reading a file opened only to write and writing one opened only to read; none of them is an empty
answer that looks like the end of the file. A forgotten file is closed when the collector frees it. A
file belongs to the thread that opened it and is not handed to another. Calls that read and write wait
as the calls of `avin.fs` above do, which is to say they do not park a lightweight thread.

**A tail starts at a whole character.** When `readTail` begins in the middle of a UTF-8 sequence it
skips the rest of that sequence, so the text never starts with a stray byte; bytes that are not UTF-8
elsewhere become U+FFFD, since a log's last lines are read to be looked at and not to be refused.
Lines are decoded the same way, a `\r` before the `\n` going with it.

## `avin.time`

Clocks, lengths of time, the calendar and time zones, in eight types: `Instant` (a point on the
timeline), `Duration` (an exact length), `Date`, `Time` and `DateTime` (calendar readings with no
zone), `Zone`, `Zoned` (a reading in a zone) and `Period` (months and days). `now()` reads the wall
clock as an `Instant`; `monotonic()` reads the monotonic clock as a `Duration`, so an elapsed time is
`monotonic() - start`. The module has a chapter of its own: [dates and times](/spec/time/).

```text
import avin.time

val start = time.monotonic()
work()
println(s"took ${time.monotonic() - start}")
```

## `avin.math`

The elementary functions on `real`, and the constants `Pi` and `E`. They are functions
of the module rather than methods, called through it or imported by name:

```text
import avin.math
import avin.math.{Pi, sqrt}

main()
    println(sqrt(2.0))
    println(math.atan2(1.0, -1.0) / Pi)
    println(math.log(0.0))
```

| Function | Meaning |
| --- | --- |
| `sqrt(x)`, `cbrt(x)` | The square root, NaN for a negative `x`; the cube root, negative for a negative `x`. |
| `exp(x)`, `expm1(x)` | `E` to the power `x`; `exp(x) - 1.0`, accurate near zero. |
| `log(x)`, `log10(x)`, `log1p(x)` | The natural logarithm; to base 10; `log(1.0 + x)`, accurate near zero. Negative infinity at zero, NaN below it. |
| `pow(x, y)` | `x` to the power `y`. `pow(x, 0.0)` is `1.0` for every `x`, NaN included; a negative `x` with a `y` that is not whole is NaN. |
| `hypot(x, y)` | `sqrt(x * x + y * y)`, without overflow or underflow on the way. |
| `sin(x)`, `cos(x)`, `tan(x)` | Of an angle in radians. |
| `asin(x)`, `acos(x)`, `atan(x)` | The angle in radians: `asin` and `atan` from `-Pi / 2` to `Pi / 2`, `acos` from `0.0` to `Pi`; NaN outside `-1.0` to `1.0` for the first two. |
| `atan2(y, x)` | The angle in radians, from `-Pi` to `Pi`, of the point `(x, y)`; `y` comes first. |
| `sinh(x)`, `cosh(x)`, `tanh(x)` | The hyperbolic functions. |
| `floor(x)`, `ceil(x)`, `rint(x)` | The whole number below, above, or nearest, as a `real`; `rint` takes a tie to the even one, so `rint(2.5)` is `2.0`. |
| `abs(x)`, `signum(x)` | The magnitude, `abs(-0.0)` being `0.0`; `1.0`, `-1.0`, or `x` itself for a zero or NaN. |
| `max(x, y)`, `min(x, y)` | NaN when either is; `-0.0` is less than `0.0`. |
| `toRadians(x)`, `toDegrees(x)` | An angle converted. |

**Nothing traps.** Every function follows IEEE 754: an argument outside a function's domain gives
NaN, a pole gives the infinity its limit approaches, and the infinities go where their limits do
(`sqrt` of positive infinity is positive infinity, `exp` of negative infinity is `0.0`). The limits
of `real` itself, its infinities and NaN, are on the type: `real.PositiveInfinity`, `real.NaN`.

## `avin.random`

Pseudo-random numbers: a generator a program seeds to replay a run, or lets the platform
seed.

```text
import avin.random.Random

main()
    val dice = Random(42)
    println(dice.between(1, 7))
    println(Random.nextInt(100))
    println(Random.shuffle([1, 2, 3, 4]))
```

`Random(seed: long)` is a generator whose numbers are fixed by its seed, the same on every platform
and in every release; `Random()` is seeded by the platform, differently each time.

| Method | Meaning |
| --- | --- |
| `nextInt() -> int`, `nextLong() -> long` | Any value of the type, each equally likely. |
| `nextInt(bound: int) -> int`, `nextLong(bound: long) -> long` | From `0` up to `bound`, not including it; a bound that is not positive traps. |
| `between(min, max)` | From `min` up to `max`, not including it, for `int`, `long` and `real`; `min >= max` traps, and for `real` so do a NaN bound and an infinite `max - min`. |
| `nextReal() -> real` | From `0.0` up to `1.0`, not including it, every multiple of 2^-53 equally likely. |
| `nextBoolean() -> bool` | `true` or `false`, equally likely. |
| `nextGaussian() -> real` | Normally distributed, mean `0.0` and standard deviation `1.0`. |
| `nextBytes(n: int) -> Array[byte]` | A new array of `n` bytes; a negative `n` traps. |
| `alphanumeric() -> Iterator[char]` | An endless run of characters from `A`–`Z`, `a`–`z` and `0`–`9`, equally likely. |
| `shuffle[T](xs: Iterable[T]) -> Vector[T]` | The elements in a new order, every order equally likely. |
| `choose[T](items: Seq[T]) -> T` | One of the items, each equally likely; an empty sequence traps. |
| `chooseWeighted(weights: Seq[real]) -> int` | An index, with probability proportional to its weight; a zero weight is never chosen. An empty sequence, a negative, NaN or infinite weight, and weights whose sum is zero or overflows trap. |
| `chooseWeighted[T](pairs: Seq[(T, real)]) -> T` | The first of a pair, chosen by the second as above. |
| `setSeed(seed: long)` | Starts the generator again as `Random(seed)` would. |

**Repeated draws from one distribution use `Weighted`.** `Weighted(pairs)` keeps the running totals
of the weights, built once with the same checks, and `sample(rng)` draws an item by binary search, in
O(log n); it draws exactly as `rng.chooseWeighted(pairs)` would. `size()` is the number of items.

```text
val weather = Weighted([("sun", 6.0), ("rain", 3.0), ("snow", 1.0)])
val rng = Random(7)
println(weather.sample(rng))
```

A chain is a weights row per state: `state = rng.chooseWeighted(rows(state))`.

**The companion is a generator too.** `Random.nextInt(100)` and every other method called on
`Random` itself use one generator the program shares, seeded by the platform on first use and safe to
call from any thread. A `Random` a program makes is its own: it takes no lock, so threads that share
one share it through a `Mutex`.

**The generator is named.** It is PCG32 (PCG-XSH-RR, 64 bits of state, 32 of output) on the
reference implementation's stream 54, seeded as the reference seeds it, so a seeded run can be checked
against the published algorithm: `Random(42).nextInt()` is its first published output, `0xa15c02b7`,
which as an `int` is `-1587805513`. A `long` is two draws, the first the high half. A bound is reached without bias, by rejection rather
than a remainder. **None of this is fit for secrets**: anyone who sees a few outputs can compute the
rest, so a key, a token or a password is never taken from it.

## `avin.net`

TCP connections, URLs, and a small HTTP client. Every operation that can fail answers a
`Result[T, IoError]`, and a connection that is reading or writing waits safe for the collector.

```text
import avin.net.http

main() -> int
    http.get("http://example.com/") match
        Ok(r) if r.status == 200 -> println(r.text())
        Ok(r) -> eprintln(s"status ${r.status}")
        Err(e) -> eprintln(e.message())
    0
```

### TCP

A connection and a listener are [sendable](/spec/actors/#what-may-be-sent): each holds a Sysl
handle, so a thread or an actor given one takes it over, and the code that gave it traps where it
uses it again.

| Function | Meaning |
| --- | --- |
| `TcpStream.connect(host: string, port: int) -> Result[TcpStream, IoError]` | A connection to the first of the host's addresses that answers; where none does, the last one's error. |
| `s.read(max: int) -> Result[Array[byte], IoError]` | Up to `max` bytes, at least one, waiting for at least one; empty when the other end has finished sending. |
| `s.readLine() -> Result[Option[string], IoError]` | The next line, as standard input reads one, `None` at the end. |
| `s.readAll() -> Result[Array[byte], IoError]` | Everything until the other end has finished sending. |
| `s.write(bytes: Array[byte])`, `s.writeText(text: string)` `-> Result[unit, IoError]` | Sends all of it. |
| `s.setTimeout(d: Duration) -> Result[unit, IoError]` | A read or write that waits longer fails with `TimedOut`; zero or less takes the limit away. |
| `s.shutdownOutput() -> Result[unit, IoError]` | Tells the other end nothing more will be sent; reading goes on. |
| `s.close()` | Ends the connection; closing again does nothing. |
| `TcpListener.bind(host: string, port: int) -> Result[TcpListener, IoError]` | Listens on the host's first address (`127.0.0.1` for this machine alone, `0.0.0.0` for all of its addresses); port `0` lets the platform choose. |
| `l.port() -> int` | The port it listens on. |
| `l.accept() -> Result[TcpStream, IoError]` | Waits for the next connection. |
| `l.close()` | Stops listening; closing again does nothing. |

```text
import avin.net.{TcpListener, TcpStream}
import avin.threads.spawn

main()
    TcpListener.bind("127.0.0.1", 0) match
        Ok(l) ->
            val server = spawn(() -> l.accept().flatMap(s -> s.readLine()))
            TcpStream.connect("127.0.0.1", l.port()).flatMap(c -> c.writeText("hello\n"))
            server.join() match
                Ok(Some(line)) -> println(line)
                _ -> eprintln("no line")
        Err(e) -> eprintln(e.message())
```

### Unix domain sockets

A `UnixStream` and a `UnixListener` connect two programs on one machine by a path in the file system
rather than a host and port. A `UnixStream` has every reading and writing operation of a `TcpStream`
(`read`, `readLine`, `readAll`, `write`, `writeText`, `setTimeout`, `shutdownOutput`, `close`) with the
same meanings, including the bytes `readLine` reads beyond its line, and a `UnixListener` accepts
them as a `TcpListener` does. Both are sendable and owned by one thread at a time, and every wait is
safe for the collector, as TCP's are.

| Function | Meaning |
| --- | --- |
| `UnixStream.connect(path: string) -> Result[UnixStream, IoError]` | A connection to the program listening at `path`; a path with nothing at it is `NotFound`, and a socket file nobody listens on any more is `ConnectionRefused`. |
| `UnixListener.bind(path: string) -> Result[UnixListener, IoError]` | Listens at `path`, which must name nothing yet: a path already holding anything is `AlreadyExists`. |
| `UnixListener.bindReplacing(path: string) -> Result[UnixListener, IoError]` | As `bind`, but a socket file left behind by a program that is gone (a connection to it is refused) is removed first. A live listener's path and anything that is not a socket file is `AlreadyExists` and untouched. |
| `l.path() -> string` | The path it listens at. |
| `l.accept() -> Result[UnixStream, IoError]` | Waits for the next connection. |
| `l.close()` | Stops listening; closing again does nothing. The socket file stays; `fs.remove` or `bindReplacing` clears it. |

A path the platform's socket address cannot carry is an `Err`: one longer than 103 bytes on macOS (107
on Linux), an empty one, or one holding a zero byte. A missing directory on the way to a bound path is
an `Err` too. `bindReplacing` finds out whether a socket is live by connecting to it once, so a live
listener sees a connection that closes without a word.

A connection and a listener are closed by `close()`; one the program forgets is closed when the
collector finds it unreachable, so a lost one leaks nothing for long, but only `close()` says when.
A read, write or `accept` after `close()` answers `Other`, saying it is closed. `IoError` gains the
variants a connection meets: `ConnectionRefused`, `ConnectionReset` (a reset, or a write to a
connection the other end has closed) and `TimedOut`. A host that resolves to no address is `Other`,
and a port outside 0 to 65535 or a read of fewer than one byte traps at the call. Bytes a `readLine`
reads beyond its line are kept, and the next read of any kind takes them first. A timeout is kept in
whole milliseconds, a part of one counting as one.

**A connection belongs to one thread at a time**, as every [handle](/spec/sysl-bindings/#handles)
does: a server accepts a connection and hands it to the thread or future that serves it, the bytes
read ahead going with it, and the code that handed it over traps at any operation on it while the
body runs, a read served from those bytes alone and one on a connection the body has closed
included ([proposed](/spec/decisions/#proposed)); a listener's `port`, `accept` and `close` are
checked the same way:

```text
avin: trap: `TcpStream` belongs to another thread at main.avin:16:25
```

### `Url`

`import avin.net.Url` brings in a URL as RFC 3986 reads a URI. `Url.parse(text) -> Result[Url,
UrlError]` reads an absolute URL, one that begins with its scheme, and `Url(text)` reads one and
traps on text that is not, with the error's message and the text, as `Regex(p)` traps where
`Regex.compile(p)` answers a `Result`.

A `Url` is a `data` type of its components: `scheme`, `userInfo`, `host`, `port` (an `Option[int]`),
`path`, `query` and `fragment`, each optional one an `Option`. **An empty component is not an absent
one**: `http://h?` has the query `Some("")` and `http://h` has `None`; `file:///etc` has the host
`Some("")` and `mailto:a@b.c`, which has no authority, `None`. Components are kept as they were
written, percent-escapes and all, and `Url.decode` turns one into text. A host is a name, an IPv4
address, or an address in brackets, kept with them: an IPv6 address, with an optional zone written
`%25` and its name (`[fe80::1%25eth0]`), or an address of a later form (`[v7.x]`). A component may
hold characters beyond ASCII other than controls and spaces, kept as they are.

The scheme and host are held in lower case (a zone's name and a host's percent-escapes as written),
and a port that is the scheme's default is held as `None`, so URLs differing only in those are
equal, and a `Url` displays as the text it was read from with exactly those differences:
`Url("HTTP://Example.COM:80/A")` displays as `http://example.com/A`.

| Member | Meaning |
| --- | --- |
| `u.portOrDefault() -> Option[int]` | The port, or where none is written the scheme's default: 80 for `http` and `ws`, 443 for `https` and `wss`. |
| `u.queryParams() -> Vector[(string, string)]` | The query split at each `&` (an empty piece skipped) and each piece at its first `=`, decoded, a `+` standing for a space as a form writes one; no `=` gives an empty value, no query no pairs. |
| `u.resolve(ref: string) -> Result[Url, UrlError]` | The reference, relative or absolute, resolved against `u` by RFC 3986 §5.2: its missing components taken from `u` and its dot segments removed, strictly, so `http:g` stays `http:g`. |
| `u.resolve(ref: Url) -> Url` | An absolute reference with its dot segments removed. |
| `u.withScheme(s)`, `withUserInfo(i)`, `withHost(h)`, `withPort(p)`, `withPath(p)`, `withQuery(q)`, `withFragment(f)` | The URL with one component replaced, as written with its escapes, held as `parse` holds it; the optional ones take a value or an `Option`, `None` taking the component away. A component the URL cannot hold traps: one with a character it may not, a port outside 0 to 65535, user information or a port without a host, a path beside a host not beginning with `/`. |
| `Url.encode(text) -> string` | Every byte of the text's UTF-8 but a letter, a digit, `-`, `.`, `_` and `~` written `%XX` in upper case, so the result can stand as any component. |
| `Url.decode(text) -> Result[string, UrlError]` | Each `%XX` replaced by its byte and the bytes read as UTF-8; a `+` is kept. |

`UrlError` says why text is not a URL, with `at`, the byte offset of what is wrong: `MissingScheme`
(a relative reference, or text beginning with `:`), `BadScheme(at)`, `BadCharacter(at)`,
`BadPercent(at)`, `BadHost(at)`, `BadPort(at)` (not digits, or above 65535) and `InvalidUtf8(at)`;
`message()` says it in words. **Every component's escapes must decode to UTF-8 for the text to be a
URL**, so `parse` refuses `http://h/%FF` with `InvalidUtf8(9)` and `queryParams` never fails on a
`Url` it read.

### `http`

| Function | Meaning |
| --- | --- |
| `http.get(url: string) -> Result[Response, HttpError]` | Fetches the URL. |
| `http.post(url: string, body: string, contentType: string) -> Result[Response, HttpError]` | Sends `body` and answers the response. |
| `http.send(request: Request) -> Result[Response, HttpError]` | Any method, headers, body and timeout, built from `Request(method, url)` with `withHeader`, `withBody` and `withTimeout`. |

A `Response` has its `status` and `reason`, its `headers` as a vector of name and value pairs,
`header(name)` looking a name up without regard to case, its `body` as an `Array[byte]` and `text()`
reading that as UTF-8, a sequence that is not read as U+FFFD. **A status is an answer, not a failure**: `404` is an `Ok`. `HttpError` is `Io(IoError)`,
`BadUrl(UrlError)`, `Malformed(text)` for a reply that is not HTTP, and `Unsupported(text)`. The
client speaks HTTP/1.1, one request to a connection, reads bodies by `Content-Length`, chunked or to
the connection's end, and follows up to five redirects of a `GET` or `HEAD` (statuses 301, 302,
303, 307 and 308), each `Location` resolved against the URL that answered by `Url.resolve`; the sixth
is the answer. It writes `Host`, `Content-Length` (for a body, a `POST` and a `PUT`) and `Connection:
close` unless the request gives them, then the request's own headers in order. `HEAD`, an interim
`1xx` (which is skipped for the answer after it), `204` and `304` have no body. A method is a token
and a header's name one too, and a header's value holds no line break; `Request` and `withHeader`
trap where one does not. A request's timeout bounds each read and write on its connection. A request for `https`
is `Unsupported` where the library has no TLS; TLS lives in a Sysl package of its own, which the
library reaches through a binding to a package named in its `sysl-dependencies`, as any program
reaches one.

## Concurrency

Futures and promises stay in [`avin.threads`](/spec/threads/#futures), beside the
threads and locks they are built on; there is no separate module for them. What a program writing
with futures reaches for is there, under these names:

| To | Write |
| --- | --- |
| Run a computation on the pool | `Future(body)` |
| A future already complete | `Future.successful(x)`; a failure is `Future.successful(Err(e))`, since a future carries none of its own |
| Wait for the value | `f.await()`, or `f.await(timeoutMillis)` for an `Option` |
| Every value of several | `Future.all(fs)` |
| The first value of several | `Future.first(fs)` |
| A future for each element, then all of them | `Future.traverse(xs, f)`, which is `Future.all(xs.map(f))` started in order |
| Go on from a failure | on a `Future[Result[T, E]]`: `mapErr(f)`, and `orElse(other)`, which runs the by-name `other` where the first ended in `Err` |
| Run on threads of one's own | `Executor.fixed(n)`, a pool of `n` workers, and `ex.future(body)`; `Executor.shared()` is the pool `Future(body)` uses. A future's `map`, `flatMap` and `onComplete` run on the executor it was made on. `ex.shutdown()` lets its workers end once the queued work is done. |
| Say a body will wait on something other than a future | `threads.blocking(body)`: the pool starts a worker in its place while it waits, as it does for `await` |
| Pause a thread | `threads.sleep(d: Duration)`, safe for the collector while it sleeps |

## JSON

JSON is not part of the system library: reading and writing it is a package of its own, which a
program adds as a dependency. It is the [`json` package](/spec/json/), in `packages/json` of this
repository.
