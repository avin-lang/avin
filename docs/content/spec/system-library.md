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

**Proposed.** What a program asks about where it runs is a function of `avin.process`, beside `env`;
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

**Proposed.** A `Command` is another program to run: a program and its arguments, the directory and
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

| Running | Meaning |
| --- | --- |
| `c.run() -> Result[ExitStatus, IoError]` | Runs the command with the program's own streams, waits, and answers how it ended. A status other than success is a value, not a failure. |
| `c.output() -> Result[string, CommandError]` | Runs it with its standard output captured as text and standard error the program's own; any status but success is `Failed(status)`. |
| `c.lines() -> Result[Iterator[string], IoError]` | Starts it and answers its standard output's lines as they arrive, by the rule [standard input](#standard-input) reads lines by. Its status is not checked. |
| `c.start() -> Result[Child, IoError]` | Starts it and answers at once. A `Child` has `waitFor() -> Result[ExitStatus, IoError]`, `isAlive()` and `kill()`. |

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
        line.trim().toInt() match
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

**Proposed.** A `Source` is text to be read from somewhere, so one function can read a file, a string
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
program's.

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
| `remove(file) -> Result[unit, IoError]` | Removes a file; never a directory. |
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

## `avin.time`

Measuring time and reading the clock. Every quantity is a count of whole microseconds, held in a
`long`; arithmetic that leaves its range, about 292,000 years, traps like any integer overflow.

**Three types, two clocks.** A `Duration` is a length of time, negative, zero or positive. An
`Instant` is a reading of the **monotonic** clock, which only moves forward and starts from an origin
that means nothing: one reading answers nothing, and the difference of two is the time between them.
A `WallTime` is a reading of the **wall** clock, as a `Duration` since 1970-01-01T00:00:00 UTC, which
anyone may set, so two readings may differ by a negative amount. The clocks are different types, so a
reading of one is never taken from a reading of the other: measure with `Instant`, stamp with
`WallTime`.

```text
import avin.time

val start = time.now()
work()
println(s"took ${start.elapsed()}")
```

**Making durations.** `microseconds`, `milliseconds`, `seconds`, `minutes` and `hours` each take a
`long` count; `zero()` is the empty duration. `inMilliseconds`, `inSeconds` and `inMinutes` answer
whole units, a fraction dropped toward zero, and `inSecondsReal` the seconds with their fraction.
Durations add, subtract and negate with `+`, `-` and unary `-` (they implement `Add`, `Sub` and
`Neg`), and `d * n` and `d / n` scale one by a `long`, a remainder dropped toward zero; `abs()`
drops the sign and `isNegative()` asks it.

**Readings.** `now()` reads the monotonic clock and `wallClock()` the wall clock; `fromEpoch(d)`
makes the wall-clock time `d` after the start of 1970. On an `Instant`, `elapsed()` is the time from
it to now, `later - earlier` (or `later.since(earlier)`) the time from another reading, and
`start + d` and `start - d` move it by a duration; a `WallTime` has the same `+`, `-` and `since`, and
the `sinceEpoch` duration. These are [operator methods](/spec/operators-and-control-flow/#methods-named-by-symbols)
of the reading types, so a reading of one clock less a reading of the other does not compile.

**Order, equality and text.** All three types compare by value and are ordered, so `<`, `==` and
`max` work on them. A duration displays in the largest unit it fills, with its fraction and no
trailing zeros: `3us`, `1.5ms`, `90.5s`, `-500ms`; its `Debug` form wraps that as `Duration(1.5ms)`.
A `WallTime` displays as an ISO 8601 UTC timestamp with microseconds, `2026-11-04T12:34:56.000045Z`,
in the proleptic Gregorian calendar.

## `avin.random`

**Proposed.** Pseudo-random numbers: a generator a program seeds to replay a run, or lets the platform
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
| `between(min, max)` | From `min` up to `max`, not including it, for `int`, `long` and `real`; `min >= max` traps. |
| `nextReal() -> real` | From `0.0` up to `1.0`, not including it, every multiple of 2^-53 equally likely. |
| `nextBoolean() -> bool` | `true` or `false`, equally likely. |
| `nextGaussian() -> real` | Normally distributed, mean `0.0` and standard deviation `1.0`. |
| `nextBytes(n: int) -> Array[byte]` | A new array of `n` bytes. |
| `alphanumeric() -> Iterator[char]` | An endless run of characters from `A`–`Z`, `a`–`z` and `0`–`9`, equally likely. |
| `shuffle[T](xs: Iterable[T]) -> Vector[T]` | The elements in a new order, every order equally likely. |
| `setSeed(seed: long)` | Starts the generator again as `Random(seed)` would. |

**The companion is a generator too.** `Random.nextInt(100)` and every other method called on
`Random` itself use one generator the program shares, seeded by the platform on first use and safe to
call from any thread. A `Random` a program makes is its own: it takes no lock, so threads that share
one share it through a `Mutex`.

**The generator is named.** It is PCG32 (PCG-XSH-RR, 64 bits of state, 32 of output), so a seeded run
can be checked against the published algorithm. A bound is reached without bias, by rejection rather
than a remainder. **None of this is fit for secrets**: anyone who sees a few outputs can compute the
rest, so a key, a token or a password is never taken from it.

## `avin.net`

**Proposed.** TCP connections, URLs, and a small HTTP client. Every operation that can fail answers a
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

| Function | Meaning |
| --- | --- |
| `TcpStream.connect(host: string, port: int) -> Result[TcpStream, IoError]` | A connection to the first of the host's addresses that answers. |
| `s.read(max: int) -> Result[Array[byte], IoError]` | Up to `max` bytes, waiting for at least one; empty when the other end has finished sending. |
| `s.readLine() -> Result[Option[string], IoError]` | The next line, as standard input reads one, `None` at the end. |
| `s.write(bytes: Array[byte])`, `s.writeText(text: string)` | Sends all of it. |
| `s.setTimeout(d: Duration)` | A read or write that waits longer fails with `TimedOut`. |
| `s.shutdownOutput()` | Tells the other end nothing more will be sent. |
| `s.close()` | Ends the connection; closing again does nothing. |
| `TcpListener.bind(host: string, port: int) -> Result[TcpListener, IoError]` | Listens for connections; port `0` lets the platform choose. |
| `l.port() -> int` | The port it listens on. |
| `l.accept() -> Result[TcpStream, IoError]` | Waits for the next connection. |

A connection and a listener are closed by `close()`; one the program forgets is closed when the
collector finds it unreachable, so a lost one leaks nothing for long, but only `close()` says when.
`IoError` gains the variants a connection meets: `ConnectionRefused`, `ConnectionReset` and
`TimedOut`.

### `Url`

`Url.parse(text) -> Result[Url, UrlError]` reads an absolute URL into its `scheme`, `host`, `port`
(an `Option[int]`), `path`, `query` (an `Option[string]`) and `fragment`; `u.portOrDefault()` gives
80 for `http` and 443 for `https` where none is written, and `u.queryParams() -> Vector[(string,
string)]` the decoded pairs. `Url.encode(text)` and `Url.decode(text)` percent-encode and decode a
component. A `Url` displays as the text it was parsed from, normalized: the scheme and host in lower
case, a default port left out.

### `http`

| Function | Meaning |
| --- | --- |
| `http.get(url: string) -> Result[Response, HttpError]` | Fetches the URL. |
| `http.post(url: string, body: string, contentType: string) -> Result[Response, HttpError]` | Sends `body` and answers the response. |
| `http.send(request: Request) -> Result[Response, HttpError]` | Any method, headers, body and timeout, built from `Request(method, url)` with `withHeader`, `withBody` and `withTimeout`. |

A `Response` has its `status`, its `headers` as a vector of name and value pairs, `header(name)`
looking a name up without regard to case, its `body` as an `Array[byte]` and `text()` reading that as
UTF-8. **A status is an answer, not a failure**: `404` is an `Ok`. `HttpError` is `Io(IoError)`,
`BadUrl(UrlError)`, `Malformed(text)` for a reply that is not HTTP, and `Unsupported(text)`. The
client speaks HTTP/1.1, one request to a connection, reads bodies by `Content-Length`, chunked or to
the connection's end, and follows up to five redirects of a `GET` or `HEAD`. A request for `https`
is `Unsupported` where the library has no TLS; TLS lives in a Sysl package of its own, which the
library reaches through a binding to a package named in its `sysl-dependencies`, as any program
reaches one.

## Concurrency

**Proposed.** Futures and promises stay in [`avin.threads`](/spec/threads/#futures), beside the
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
program adds as a dependency.
