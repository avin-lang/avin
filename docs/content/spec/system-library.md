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

**`main` may take the arguments itself.** `main(args: Vector[string])` receives the vector `args()`
answers, and `main()` is the form for a program that does not read them. The parameter is exactly
`Vector[string]` with no default; any other parameter list is an error. `args()` and `programName()`
remain available everywhere.

```text
main(args: Vector[string]) -> int
    for f in args do print(f)
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

## `avin.io`

Standard input, read as text, and the error every module that reads or writes shares. Standard
output and standard error need no import: `print` and `eprint` are in the prelude, beside the
writers `StandardOutput` and `StandardError`.

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

**Input is text, and reading it does not fail.** Bytes that are not UTF-8 become U+FFFD, one for each
ill-formed run, as they are read; a line is always a valid string, and a program reading text is
never stopped by a stray byte. A read that fails ends the input, as its end does. Standard input is a
stream a program consumes as it arrives, where a file is read whole: `fs.readText` answers
`InvalidUtf8` instead, since a file's bytes can be read again as bytes.

**Writing to standard error flushes standard output first.** Standard output is buffered, and
flushed when the program ends, exits or traps; standard error is not buffered. Each write to
standard error first flushes what the program has written to standard output, so when both go to one
terminal or one file they arrive in the order the program wrote them. `eprint(x)` renders `x` and its
new line first and writes them in one write, so a line is never split.

**Tests see no input.** `avin test` gives each test an empty standard input, so a test never waits on
the terminal it was started from. `avin run` hands the program its own standard input.

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
    if name.endsWith(".txt") then print(name)
```
