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
(`no such file or directory`). An operation that can fail answers a `Result[T, IoError]`, so `?`
passes the failure on from a function that answers one too.

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
| `createDirAll(dir) -> Result[unit, IoError]` | Makes the directory and every missing one above it; a directory already there is success, a file in the way an error. |
| `createTempDir(prefix) -> Result[string, IoError]` | Makes a new, empty directory in the platform's place for temporary files, its name starting with `prefix`, and answers its path. |

**Paths are strings**, and the questions follow symbolic links: `isDir` of a link to a directory is
`true`. `removeDirAll` is the exception that matters: a link inside the tree is removed, never
followed, so it deletes nothing outside the directory it was given.

**A temporary directory is the program's to remove.** `createTempDir` makes the directory rather than
inventing a name, so no other program can take it in between, and nothing removes it at the end:
`removeDirAll` is the call.

**Bytes are copied.** The array `readBytes` answers is new, and changing it changes nothing on disk;
the array given to `writeBytes` is read once and left as it was.

Listing a directory's entries is not yet offered: it needs a list of names to come back from the
platform, which a binding cannot yet carry.
