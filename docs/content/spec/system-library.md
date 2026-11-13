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
  if files.isEmpty
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
| `exit(code: int) -> Nothing` | Ends the program now with `code` as its exit status, after flushing what it has written and running its [exit hooks](#exit-hooks). |
| `onExit(@sendable body: () -> unit) -> ExitHook` | Registers `body` to run as the program ends; see [exit hooks](#exit-hooks). |
| `hook.cancel() -> bool` | Takes the hook out so it does not run, answering whether it was still registered. |

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
not lost, and then its [exit hooks](#exit-hooks) run. The status the parent sees is the platform's: on POSIX systems, the code's low eight bits.
Returning an `int` from `main` sets the status the same way without stopping early.

### Exit hooks

A program that changes something outside itself, a terminal put into raw mode on the alternate
screen, a lock file, puts it back in an exit hook, which runs however the program ends:

```text
import avin.process

main(args: Vector[string]) -> int
  print("\u001b[?1049h")
  process.onExit(() -> print("\u001b[?1049l"))
  draw(args)
  0
```

**They run when the program ends, once each, newest first.** The thread that ends the program runs
them, after it has flushed what the program wrote: the main thread when `main` returns, the thread
that calls `exit`, wherever it is, and the thread that traps, after the trap's report, so a hook
writing to standard error writes after it. A trap is any of them: a failed check, `fail`, an
actor's failure escalated past every actor (reported naming the actor), running out of memory. A
trap an actor's failure contains does not end the program and runs none. A test's process runs
them as a program does. The program then ends as it was ending: with `main`'s status, with
`exit`'s, or as a trap.

**What does not run them.** Running out of stack stops the program from a handler on a stack of its
own, where no Avin code can run, so no hook runs. A signal the program does not
[watch](#avinsignal) ends it as the signal always has, with none run; a program that wants its hooks
to run on `Terminate` or `Interrupt` watches the signal and calls `exit`. A program killed (`Kill`)
runs nothing.

**A hook may run on another thread than the one that registered it**, while the program's other
threads go on running, so its body is `@sendable`: it captures only sendable values, as a thread's
body does. Hooks may be registered and cancelled from any thread, an actor or a future. A hook may
start threads and wait for them; it should not wait for one of the program's own threads, which
may be blocked or have stopped. A thread that traps or calls `exit` while the hooks run waits for
the end, and its trap is not reported: the first ending is the program's.

**`cancel()`** answers `true` once, for a hook still registered, which then does not run, and
`false` after it has run, has begun to, or was cancelled. A hook registered while the hooks run is
run too, at once after the one that registered it.

**A hook that traps or calls `exit` never goes on.** A trap is reported as `avin: trap in an exit
hook: …`, the hooks after it still run, and the program then ends as a trap ends it, even where
`main` returned. An `exit` inside a hook ends only that hook: the program keeps the status of the
ending that ran the hooks. A hook that traps while holding a mutex leaves what the mutex guards in
doubt, so the program stops at once, running no more hooks.

**Tests see no arguments.** `avin test` runs each test in a process of its own and chooses the test
by an argument of its own, which the test does not see: in a test, `args()` is empty.

`avin run <program> -- ARGS` passes everything after `--` to the program unchanged, flags and a
second `--` included. Everything before the `--` is avin's, and its options may come before or
after the program: `avin run -O2 main.avin`, `avin run main.avin -O2` and `avin -O2 main.avin` are
one command.

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
| `totalMemory() -> Result[long, IoError]` | The physical memory the machine has, in bytes: what is installed, not what is free. |
| `residentBytes(pid: long) -> Result[long, IoError]` | The memory the process `pid` holds in RAM now, in bytes; `residentBytes(pid())` is the program's own. A process that is not there, a reaped child among them, and a `pid` of zero or less are `Err(NotFound)`; one the program may not look at is `Err(PermissionDenied)`. |
| `groupMembers(pgid: long) -> Result[Vector[long], IoError]` | The live processes whose process group is `pgid`, in no particular order: the leader, the programs it started and their children that stayed in the group. Ended programs not yet waited for are listed, waited-for ones are not. A group nobody is in is `Ok([])`; a `pgid` of zero or less, or past the largest process id, is `Err(Other)`. A snapshot. |
| `children(pid: long) -> Result[Vector[long], IoError]` | The live processes whose parent is `pid`, in no particular order, direct children only: `children(pid())` is what this program started and has not waited for. No children, or no such process, is `Ok([])`; a `pid` of zero or less, or past the largest process id, is `Err(Other)`. |
| `signal(pid: long, sig: int) -> Result[unit, IoError]` | Sends the signal to the process `pid`; a negative `pid` is the group `-pid`, zero the program's own group, and `sig` zero sends nothing and answers whether one could be sent. No such process is `Err(Other("No such process"))`. |
| `newSession() -> Result[long, IoError]` | Makes the program the leader of a new session and group with no controlling terminal, as a daemon does, answering its number; refused for a program that already leads a group. |
| `exec(program: string, args: Vector[string]) -> Result[Nothing, IoError]` | Replaces the program with `program` run with `args`, in the same process; see [becoming another program](#becoming-another-program). Answers only if that failed. |
| `exec(program: string, args: Vector[string], environment: Map[string, string]) -> Result[Nothing, IoError]` | The same, with `environment` as the whole of the new program's environment. |
| `waitPid(pid: long) -> Result[ExitStatus, IoError]` | Waits for this program's child `pid` to end and answers how it ended, reaping it; see [becoming another program](#becoming-another-program). |
| `tryWaitPid(pid: long) -> Result[Option[ExitStatus], IoError]` | Whether the child `pid` has ended, without waiting: `None` while it runs, its status (reaping it) once it has. |
| `os() -> Os` | The operating system the program was compiled for: `MacOS`, `Linux`, `Windows`, `Android`, `Wasi` or `Freestanding`. |

Listing a group replaces a `pgrep -g` subprocess, for instance to signal or count everything a
command left running:

```avin
match Command("/bin/sh", "-c", "sleep 30 & wait").inNewGroup.start()
  Ok(child) ->
    val members = process.groupMembers(child.pid())   // the shell and its `sleep`
    val direct = process.children(child.pid())        // just the `sleep`
  Err(e) -> eprint(e.message)
```

**Nothing sets a variable.** The environment is the program's to read; a program that runs another
gives it the variables it needs through its [command](#commands), which changes nothing in its own.
Text ends lines with `\n` on every platform, so there is no line separator to ask for.

### Becoming another program

A long-running program that upgrades itself in place saves its state, becomes the newly installed
version of itself with `exec`, keeping its process id, and the new image takes up the children the
old one started, waiting for them by pid:

```text
import avin.process

upgrade(state: string, jobs: Vector[long]) -> Result[unit, IoError]
  fs.writeText(state, saved())?
  process.exec(installed, ["--resume", state] ++ jobs.map(j -> s"$j"))?
  Ok(())

resume(jobs: Vector[long])
  for pid in jobs do report(pid, process.waitPid(pid))
```

**`exec` comes back only when it failed.** On success the process goes on, the same id, the same
parent and the same working directory, running `program` from its beginning; there is nothing left
to return to, so its `Ok` side is `Nothing`, and `?` passes the failure on. A program that is not
there is `Err(NotFound)`, one that may not be run (a directory among them) `Err(PermissionDenied)`,
and the program then carries on exactly as it was: everything that can fail is checked before
anything is given up.

**What goes and what stays.** What the program has written to standard output is flushed first.
Then nothing of it is left: its other threads vanish wherever they were, its
[exit hooks](#exit-hooks) do not run, and its [signal watches](#avinsignal) end, each signal doing
again what it does by default. Its standard streams stay; every other descriptor the library opens
closes across the `exec`, as across a [command's](#commands) start. Its children stay its children.

**Where the program is looked for.** A name with a `/` is a path; a bare name is looked for on the
`PATH`. `exec(program, args)` gives the new program this program's environment;
`exec(program, args, environment)` gives it exactly `environment`, nothing inherited, and looks for
a bare name on the `PATH` that holds, or the system's default search path where it holds none.
`process.environment().updated("NAME", "value")` adds to this program's own.

**`waitPid` and `tryWaitPid` wait for a child no `Child` holds**: one [detached](#commands), or one
an earlier image of the process started before it became this program. They answer the
`ExitStatus` a `Child`'s `waitFor` answers, `Signalled` for a child a signal ended, and the child is
reaped by the answer. A pid that is not this program's child, a child already waited for, the
program's own pid, and a pid of zero or less or past the largest process id are each
`Err(NotFound)`. A thread waiting in `waitPid` leaves its worker to the program's other threads, as
every [blocking](/spec/threads/#lightweight-threads) wait does. A child a `Child` still holds is
its `Child`'s to wait for: one reaped by `waitPid` first is no longer there for `waitFor`.

### Commands

A `Command` is another program to run: a program and its arguments, the directory and
variables it runs with and where its streams go, made first and run when asked. Commands combine into
pipelines and sequences, and what combines is again a command.

```text
import avin.process.{Command, CommandError}

sources(dir: string) -> Result[Vector[string], CommandError]
  val listing = Command("ls", dir) | Command("grep", "\\.avin$")
  Ok(listing.output()?.lines)

main() -> int
  "make test".run() match
    Ok(status) if status.isSuccess -> 0
    _ -> 1
```

| Making | Meaning |
| --- | --- |
| `Command(program: string, args: string*)` | One program and its arguments, each passed as it is: no shell reads them. A program without a `/` is looked for on `PATH`. |
| `Command.parse(line: string)` | The line split at runs of white space, the first word the program; no quoting and no shell. A line with no word traps. |
| `c.inDir(dir: string)` | The same command, run in `dir`. |
| `c.withEnv(name: string, value: string)` | The same command, with the variable set for it; the rest of the program's environment is passed on. |
| `c.clearEnv` | The same command, started from an empty environment: none of the program's variables is passed on and none set by an earlier `withEnv` stays. A bare program name is then looked for on the `PATH` the command was given, or on the system's default search path where it has none, never the running program's. |
| `c.withoutEnv(name: string)` | The same command without the variable: the program's value is not passed on and an earlier `withEnv` of it is dropped. A name that is not set is no error. |
| `c.withEnvironment(vars: Map[string, string])` | The same command with exactly these variables: `clearEnv`, then a `withEnv` for each. |

Environment changes apply in the order called; on a pipeline or a sequence the command's come first and each part's own after them, so a part's `clearEnv` discards what the command set.
| `c.withTimeout(d: Duration)` | The same command, stopped when it has run for `d`; its status is then `TimedOut`. |
| `c.toFile(file: string)`, `c.appendTo(file: string)` | The same command, its standard output written to the file, emptied first or added to. |
| `c.fromFile(file: string)`, `c.withInput(text: string)` | The same command, reading the file, or the text, as its standard input. |
| `c.errorsToFile(file: string)`, `c.errorsAppendTo(file: string)` | The same command, its standard error written to the file, emptied first or added to. |
| `c.errorsToOutput()` | The same command, its standard error going where its standard output goes: into the same open file, so neither empties what the other wrote, and into what `output()` and `lines()` capture. |
| `c.inNewGroup` | The same command, started in a process group of its own, which every program it starts joins ([proposed](/spec/decisions/#proposed)). |
| `a andThen b` | `a`, then `b` only where `a` succeeded; the status is the last one run. |
| `a orElse b` | `a`, then `b` only where `a` did not succeed. |
| `a followedBy b` | `a`, then `b` whatever `a`'s status. |

**`a | b` is a pipeline**: `a`'s standard output is `b`'s standard input, the two running at once,
and its status is `b`'s.

**A setting on a command made of parts is every part's** ([proposed](/spec/decisions/#proposed)):
a part runs in the command's directory and with its variables unless it sets its own, a time limit
bounds the whole and a part not started in time is not started, output redirected on a pipeline is
its last stage's and input its first's, and a sequence's redirected file is opened once, its parts
writing, or reading, one after the other. A started program is handed its three standard streams and
nothing else: the files, sockets and pipes the program has open, and the runtime's own, are closed in
it.

| Running | Meaning |
| --- | --- |
| `c.run() -> Result[ExitStatus, IoError]` | Runs the command with the program's own streams, waits, and answers how it ended. A status other than success is a value, not a failure. |
| `c.output() -> Result[string, CommandError]` | Runs it with its standard output captured as text, each ill-formed UTF-8 run replaced with U+FFFD, and standard error the program's own; any status but success is `Failed(status)`. |
| `c.lines() -> Result[Iterator[string], IoError]` | Starts it and answers its standard output's lines as they arrive, by the rule [standard input](#standard-input) reads lines by. Its status is not checked. |
| `c.start() -> Result[Child, IoError]` | Starts it and answers at once, with the `Child` below. |

| `Child` | Meaning |
| --- | --- |
| `waitFor() -> Result[ExitStatus, IoError]` | Waits for the command to end; the same answer every time. |
| `isAlive() -> bool` | Whether it is still running, without waiting. |
| `kill()` | Sends signal 15 to every program still running and starts no part not yet started. |
| `close() -> Result[unit, IoError]` | A `Child` is an [`io.Close`](#close): stops the command as `kill` does where it has not been seen to end, then waits for it, the wait's `Err` being the answer. One already waited for is only waited for again, which answers at once; a detached one is let be, and closing it does nothing. |
| `pid() -> long` | The process id of the program started last: the one program, a pipeline's last stage, a sequence's latest part; `0` where none started, its time having run out first. |
| `group() -> Option[long]` | The process group of a command started `inNewGroup`: the number its first program leads it by. |
| `signal(sig: int) -> Result[unit, IoError]` | Sends `sig` to the program `pid()` names. A command seen to have ended, by `waitFor` or by `isAlive` answering `false`, is `Err(Other("No such process"))`, its id being the platform's to give again. |
| `signalGroup(sig: int) -> Result[unit, IoError]` | Sends `sig` to every process in the command's group, the programs those programs started that stayed in it among them, however the command ended; an empty group is `Err(Other("No such process"))`, also while its last member has ended but is not yet waited for, and a command not started `inNewGroup` `Err(Other("the command has no group of its own"))`. |
| `detach() -> long` | Lets go of the command without stopping it, so it outlives its `Child` and the program, and answers `pid()`. |
| `residentBytes() -> Result[long, IoError]` | The memory the program `pid()` names holds in RAM now, as `process.residentBytes(pid())` answers it; a command seen to have ended, or with no program started, is `Err(NotFound)`. |

A started `Child` is [sendable](/spec/actors/#what-may-be-sent) ([proposed](/spec/decisions/#proposed)):
it belongs to one thread at a time, as a [connection](#tcp) does. Handed to a thread's or a future's
body it is the body's, which may wait for it, stop it and start a sequence's later part, and the code
that handed it over traps at any use of it while the body runs, though the command has ended; when the
body returns, the next code to use it takes it back:

```text
avin: trap: `Child` belongs to another thread at main.avin:12:3
```

**What a `Child` knows of its processes any thread may ask** ([proposed](/spec/decisions/#proposed)):
`pid()`, `group()`, `signal` and `signalGroup` read what was recorded as each program started and
touch none of the command's handles, so one thread may signal a child while another waits for it,
the waiter seeing the status the signal caused. `signal` and `kill` differ: `signal` reaches one
program and stops nothing else, so a sequence goes on as its status says, and `kill` reaches every
program and starts nothing more.

**One group for a whole command.** A command started `inNewGroup` puts every program it starts in one
group, the first program leading it, the outermost command's setting deciding for its parts. A
sequence's later part started once every member of the group has ended, the group being gone, leads
a new one, which `group()` then answers. A group outlives its leader while any member is left, so
`signalGroup` after `waitFor` still reaches what the command left running:

```text
import avin.io.IoError
import avin.process.{Command, ExitStatus}
import avin.signal
import avin.threads

stopped() -> Result[ExitStatus, IoError]
  val job = Command("sh", "-c", "worker & worker & wait").inNewGroup.toFile("job.log").errorsToOutput().start()?
  val waiter = threads.spawn(() -> job.waitFor())
  job.signalGroup(signal.Terminate)?
  waiter.join()
```

**A started command does not outlive its `Child` unless it is detached.** A `Child` collected while
its command runs stops it. `detach()` lets go of every program of the command still running without
stopping it ([proposed](/spec/decisions/#proposed)): neither collecting the `Child` nor the program's
end stops them, so a server a command line program starts and leaves running is started and
detached. It answers `pid()`, which `process.signal` can still reach, and so can the `Child`'s own
`pid()`, `group()`, `signal` and `signalGroup`, which read only what was recorded as the programs
started. A sequence's later part not yet started is never started. A detached command is no longer
the `Child`'s to wait for: `waitFor`, `isAlive` and `kill` trap, and a second `detach` answers the
`pid()` again.

```text
avin: trap: a detached `Child` is not waited for, asked after or stopped at main.avin:9:5
```

**Nothing reaps a detached program.** A detached program that ends while the program that started it
still runs stays a zombie, holding its process id, until that program ends, when the system adopts
and reaps it; since the `Child` no longer sees it end, `signal` may then reach whatever was given its
id after, and `signal(0)` answers `Ok` of a zombie. Detaching a command already waited for lets go of
nothing. A program that leaves its terminal's group as well starts the command `inNewGroup`, or
detaches itself with `newSession()`.

The iterator `lines()` answers is not sendable: the thread that reads a command's output starts it.

```text
data ExitStatus
  Exited(code: int)
  Signalled(signal: int)
  TimedOut

data CommandError
  Io(error: IoError)
  Failed(status: ExitStatus, command: string)
```

A `CommandError` is an [`Error`](/spec/errors/#the-error-trait), as `IoError` is, and shows as its
`message`: an `Io` error's own message, or the command's programs, as `ls`, `ls | wc` or
`make && ./app`, and how it ended: `false exited with status 1`, `sleep killed by signal 15
(Terminate)` (the name of a signal of `avin.signal`, `unknown` for any other number) or `sleep timed
out`. `Failed`'s `command` is the programs alone; one built with an empty `command` leaves it out.

`isSuccess` is `true` of `Exited(0)` alone. A program that is not there fails to start: `run`
answers an `IoError` of kind `NotFound`, `cannot run nosuch: no such file or directory`, and `output`
the same error in `Io`.

**A string runs as a command.** Importing `avin.process` adds `run()` and `output()` to `string`, as
an [extension](/spec/declarations/#extensions): `"ls -l".run()` is `Command.parse("ls -l").run()`.

**Combining is spelled with words where an operator cannot be.** `|` is free on any type but the
integers and bytes, so a pipeline is `a | b`. Running is a method rather than a postfix `!` or `!!`,
because no method is called postfix and `!` is the language's own; `&&` and `||` are the
language's, so sequencing is `andThen`, `orElse` and `followedBy`, and redirection `toFile`,
`appendTo` and `fromFile`, words that say what each does where `#&&` or `#>` would not.

**Waiting is safe for the collector.** A thread waiting in `run`, `output` or `waitFor` holds
nothing the collector waits on, as `await` does not.

## `avin.cli`

A program's command line, read against what the program takes. A `Parser` declares the flags, the
options with a value and the positional arguments, each with a line of help; `parse` reads `main`'s
`args` into `Args`, whose values are asked for by name and come back as the types they were
declared with. The help and the usage line are written from the same declarations.

```text
import avin.cli

val parser = cli.Parser("wc", "Count lines, words and characters.", version = "1.0")
  .flag("lines", short = 'l', help = "count lines")
  .flag("words", short = 'w', help = "count words")
  .int("top", help = "list the N commonest words", value = "N")
  .choice("format", ["text", "json"], default = "text")
  .positionals("file", help = "the files to count", required = true)

main(args: Vector[string]) -> int
  val opts = parser.parseOrExit(args)
  for f in opts.strings("file")
    if opts.flag("lines") then println(s"$f: lines")
  println(s"top ${opts.int("top")} as ${opts.string("format")}")
  0
```

`wc -lw --top=3 a.txt b.txt` gives both flags, 3 and the two files; `wc --help` writes

```text
Usage: wc [options] <file>...

Count lines, words and characters.

Arguments:
  <file>...                 the files to count

Options:
  -l, --lines               count lines
  -w, --words               count words
      --top <N>             list the N commonest words
      --format <text|json>  (default: text)
  -h, --help                show this help and exit
      --version             show the version and exit
```

and `wc --top many a.txt` writes to standard error, and exits with 2:

```text
wc: `--top` needs an int, not `many`
Usage: wc [options] <file>...
Run `wc --help` for the options.
```

| Declaration on `Parser` | Declares |
| --- | --- |
| `Parser(name, about = "", version = "")` | A parser of no options and no arguments. `name` is the program's name in the usage line and the messages, `about` a sentence for the help; a `version` that is not empty gives the program `--version`. |
| `flag(name, short, help)` | `--name`, given or not, any number of times. |
| `string(name, short, help, default = "", required = false, repeated = false, value = "")` | `--name` with a text value. `value` names the value in the help (`--out <file>`), the option's name when empty. |
| `int(…, default = 0, …)`, `real(…, default = 0.0, …)` | `--name` with an `int` or a `real` value, refused at parse time when it is not one. |
| `choice(name, choices, short, help, default = "", required = false, repeated = false)` | `--name` whose value is one of `choices` exactly as written; the help shows them, `<text\|json>`. |
| `positional(name, help = "", required = true)` | One positional argument, after those declared before it. |
| `positionals(name, help = "", required = false)` | Every argument left, none or more, or one or more where `required`. It is the last declared. |

Each declaration answers a new parser and leaves the one it was called on as it was, so a parser is
an ordinary value: built once at the top of a module, or in a function.

| Reading | Meaning |
| --- | --- |
| `parse(args) -> Result[Args, ArgsError]` | The arguments read, or why not. |
| `parseOrExit(args) -> Args` | As `parse`, and where it gives no `Args` the program ends: the help or the version to standard output and status 0, a mistake to standard error, as the name and the reason, the usage line and where the help is, and status 2. |
| `usage -> string`, `help -> string` | The usage line, and the whole help: usage, `about`, the arguments, then the options in the order declared, each with its help, its default where it has one, and whether it is required or may be repeated. |
| `a.flag(name) -> bool`, `a.count(name) -> int` | Whether the flag was given, and how many times: 3 for `-vvv`. |
| `a.string(name)`, `a.int(name)`, `a.real(name)` | The option's last value, else its default; the text of a choice or of a positional argument (empty where an optional one was left off). |
| `a.strings(name)`, `a.ints(name)`, `a.reals(name)` | Every value of a repeated option in order, or its default alone where none was given and it has one; every argument a `positionals` took. |
| `a.given(name) -> bool` | Whether the option or argument was on the command line, rather than taking its default. |

**The arguments are read as a command line is.** An argument starting with `-` is an option, except
`-` alone, which is positional, as is the empty argument. `--name=value` and `--name value` give a
value alike. A single `-` starts one or more one-letter options, `-lw`, the last of which may take
the rest of the argument or the next one as its value: `-n5`, `-n 5`, `-lwn5`. An option's value is
the next argument whatever it starts with, so `--top -1` is −1 and `--out --verbose` writes to a
file named `--verbose`. Every argument after `--` is positional. Options and positional arguments
may come in any order.

**A value given twice replaces the first,** so a later `--out` wins, unless the option is
`repeated`, when each is kept. A flag may always be repeated, and `count` says how often.

**Every parser has `--help` and `-h`,** unless the program declares an option named `help` or with
the letter `h` itself, which then is the program's own; `--version` likewise, where the parser has
a version. Each is acted on where it stands: `--help` after a mistake is never reached, and a
mistake after `--help` is not looked at.

**A mistake on the command line is an `ArgsError`, never a trap.** `ArgsError` is
`HelpRequested(text)`, `VersionRequested(text)` or `Invalid(reason, usage)`, and an
[`Error`](/spec/errors/), so a `main` answering a `Result` may pass it on with `?` and the program
ends with `error: ` and the reason. The reasons name what the user wrote, as they wrote it:

| Command line | Reason |
| --- | --- |
| `--x`, `-x` | ``unknown option `--x` ``, ``unknown option `-x` `` |
| `--verbose=yes` | `` `--verbose` takes no value `` |
| `--out` at the end | `` `--out` needs a value `` |
| `--count abc`, `--count 2147483648` | `` `--count` needs an int, not `abc` `` |
| `--scale fast` | `` `--scale` needs a number, not `fast` `` |
| `--format xml` | `` `--format` must be one of `text`, `json`, not `xml` `` |
| a required option left off | ``missing required option `--to` `` |
| a required argument left off | ``missing argument `<file>` `` |
| one argument more than declared | ``unexpected argument `extra` `` |

**A mistake in the declarations, or in asking for a value, is the program's, and traps.** Two
options of one name or one letter, a name written with its dashes, a choice with no choices or a
default not among them, an argument after the one taking the rest or a required one after an
optional one trap where they are declared; asking `Args` for a name never declared, or as another
type than it was declared with (`a.int("out")` for a string option), traps where it is asked.

## `avin.signal`

Signals the program watches for: a number another process, or the platform, sends to ask it to
stop, to say its terminal has gone, or for a reason of their own. A watched signal no longer does
what it did, ending the program among them; the program asks, where it chooses, whether it has
arrived, and no code of its own runs when it does. Signals are sent with `process.signal` and
`Child.signal`.

```text
import avin.io.IoError
import avin.signal

serve() -> Result[unit, IoError]
  val stop = signal.watch(signal.Terminate)?
  while !stop.take()
    serveOne()
  cleanUp()
  Ok(())
```

| Constant | Signal |
| --- | --- |
| `Hangup`, `Interrupt`, `Quit` | 1, 2, 3: the terminal gone, Ctrl-C, Ctrl-\. |
| `Kill` | 9: ends a program at once; it cannot be watched. |
| `BrokenPipe`, `Alarm` | 13, 14. |
| `Terminate` | 15: asks a program to stop; what `Child.kill` sends. |
| `WindowChange` | 28: the terminal was resized. |
| `User1`, `User2` | The user-defined signals, numbered as the platform numbers them. |
| `Stop` | Freezes a program until a `Continue` arrives; numbered as the platform numbers it. It cannot be watched, as `Kill` cannot ([proposed](/spec/decisions/#proposed)). |
| `Continue` | Lets a stopped program run again; one that was not stopped is not disturbed. Numbered as the platform numbers it. |
| `TerminalStop` | Ctrl-Z at the terminal; a program that watches it is not stopped. Numbered as the platform numbers it. |
| `Child` | A program this one started has ended, stopped or continued. The platform ignores it unless it is watched; numbered as the platform numbers it. |

Sending `Stop` and then `Continue` with `process.signal` or `Child.signal` pauses and resumes one
program; a negative `pid`, or `signalGroup` on a command started `inNewGroup`, pauses a child and
every program it started in the group together. A status a signal ended names each of these, as
`killed by signal 19 (Stop)`.

| Function | Meaning |
| --- | --- |
| `watch(sig: int) -> Result[Watch, IoError]` | Starts watching `sig`. A number the platform does not have, `Kill` and `Stop` are `Err(Other("Invalid argument"))`. |
| `w.take() -> bool` | Whether it arrived since the last `take` or since the watch began, marking it seen. Arrivals between two takes are one `true`. |
| `w.wait()` | Parks until it arrives, then takes it. |
| `w.wait(limit: Duration) -> bool` | The same, giving up after `limit`, and answering whether it arrived. |
| `w.signal() -> int` | The signal watched. |

**Two watches of one signal each see every arrival**, so a library watching for its own reasons
takes nothing from the program. When every watch of a signal has been collected, the signal does
again what it did before the first. **A wait looks every 20 milliseconds** ([proposed](/spec/decisions/#proposed)),
its lightweight thread parked between looks, so it answers at most that late. A `Watch` is sendable,
belonging to one thread at a time as a connection does, so a thread of its own may wait on it.

## `avin.io`

Standard input, read as text, and the error every module that reads or writes shares. Standard
output and standard error need no import: `print`, `println`, `eprint` and `eprintln` are in the prelude, beside the
writers `StandardOutput` and `StandardError`; each is one function taking `Any*` (see
[Strings and formatting](/spec/strings-and-formatting/)).

### `IoError`

What every module that reads or writes shares: the error an input or output operation fails with.
It says what kind of failure it was, what was being done, and what it was done to.

```text
data IoError(kind: ErrorKind, operation: string = "", subject: string = "")

data ErrorKind
  NotFound
  PermissionDenied
  AlreadyExists
  IsADirectory
  NotADirectory
  DirectoryNotEmpty
  InvalidUtf8(offset: long)
  ConnectionRefused
  ConnectionReset
  TimedOut
  Other(text: string)
```

| Kind | Meaning | `description` |
| --- | --- | --- |
| `NotFound` | Nothing exists at the path, or a directory on the way to it is missing. | `no such file or directory` |
| `PermissionDenied` | The program may not read, write or search what the path names. | `permission denied` |
| `AlreadyExists` | Something is already where a new file or directory was to be made. | `already exists` |
| `IsADirectory` | A directory was used where a file was expected. | `is a directory` |
| `NotADirectory` | A file was used where a directory was expected. | `not a directory` |
| `DirectoryNotEmpty` | A directory to be removed still holds entries. | `directory not empty` |
| `InvalidUtf8(offset)` | Text read is not UTF-8; `offset` is the first byte where it stops being. | `not UTF-8 at byte 2` |
| `ConnectionRefused` | Nothing listens at the address a connection was made to. | `connection refused` |
| `ConnectionReset` | The other end reset or closed a connection while it was in use. | `connection reset by peer` |
| `TimedOut` | A read or write on a connection waited past its timeout. | `timed out` |
| `Other(text)` | Any other failure, described in words. | `text` |

**An `IoError` names its subject.** `operation` is what was being done, in words that follow
`cannot`: `read`, `write`, `append to`, `open`, `create`, `remove`, `remove directory`, `rename`,
`create directory`, `list`, `read metadata of`, `connect to`, `listen on`, `accept on`, `read from`,
`write to`, `run`, `wait for`, `signal`. `subject` is what it was done to: a path, `old to new` for a
rename, `host:port` or a socket's path for a connection, a program's name, `process 4242`. Every
error the standard library makes has both, so it is an [`Error`](/spec/errors/#the-error-trait)
whose `message`, and so its display, is a sentence:

```text
fs.readText("nope.txt")          // Err(cannot read nope.txt: no such file or directory)
TcpStream.connect("localhost", 1) // Err(cannot connect to localhost:1: connection refused)
Command("nosuch").run()          // Err(cannot run nosuch: no such file or directory)
```

The sentence is `cannot <operation> <subject>: <description>`; an error with no subject leaves it
out (`cannot read: …`), and one with no operation either is the kind's description alone, so a
program's own `IoError(Other("disk on fire"))` shows as `disk on fire`. **The named kinds have one
description each, the same on every platform; `Other` carries the platform's own wording** (`File
name too long`), so a failure with no kind of its own still says what happened.

**A program tells failures apart by the kind**, which a pattern reaches inside the error, a guard or
an `==` reads, and the operation and subject come along to be used:

```text
fs.readText(path) match
  Ok(text) -> use(text)
  Err(IoError(NotFound, _, _)) -> useDefaults()
  Err(e) if e.kind == PermissionDenied -> eprintln(s"$e; try sudo")
  Err(e) -> fail(e.message)
```

An operation that can fail answers a `Result[T, IoError]`, so `?` passes the failure on from a
function that answers one too, and `context` wraps it in what the program was doing. Where this
chapter says an operation answers `Err(NotFound)` or `Err(Other("No such process"))`, it means an
`IoError` of that kind, naming the operation and its subject.

### Standard input

```text
import avin.io

main()
  var total = 0
  for line in io.lines()
    line.trim.toIntOption() match
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

**`flush()` sends a partial line on its way.** `flush()` in the prelude (and `StandardOutput.flush()`,
and `StandardError.flush()`, which has nothing to send) sends everything written to standard output so
far out of the buffer whether or not a line is complete, so a program that draws a frame without a
new line can show it. Nothing buffered is not an error. Reading standard input flushes standard
output first when it must wait for more, so a prompt written with `print` is on the terminal before
the program blocks for the answer.

**Tests see no input.** `avin test` gives each test an empty standard input, so a test never waits on
the terminal it was started from. `avin run` hands the program its own standard input.

### `Reader`

A `Reader` is bytes read from somewhere a piece at a time, so one function can read a file, a
connection, or a source the program writes itself, holding no more of the input than it asks for:

```text
trait Reader
  read(max: int) -> Result[Array[byte], IoError]
```

`read(max)` gives up to `max` bytes, `max` being at least one, waiting for at least one where the
source is still going, and an empty array once it has ended. `fs.File` is a reader by its own
`read`, and so is every `net.ByteStream`: `TcpStream`, `UnixStream` and `TlsStream`; a class of the program's own is one by
naming `io.Reader` (or `Reader`, imported) after `:` and overriding `read`. The
JSON package's `JsonReader` ([`dev.avinlang.json`](/spec/json/)) reads JSON from any reader.

### `Close`

A `Close` is something held outside the program that `close()` gives back:

```text
trait Close
  close() -> Result[unit, IoError]
```

Every resource the library hands out is one: `fs.File`, `fs.TempDir`, every `net.ByteStream`
(`TcpStream`, `UnixStream`, `TlsStream`), `TcpListener`, `UnixListener` and `process.Child`. **Closing
a closed one does nothing and answers `Ok`**, so a [`defer`](/spec/operators-and-control-flow/#defer)
beside the code that took it gives it back on every way out of the block, and the success path may
close it first to see the `Err`, a flush that failed or a directory that could not be removed:

```text
save(name: string, text: string) -> Result[unit, IoError]
  val f = fs.create(name)?
  defer f.close()
  f.writeText(text)?
  f.close()
```

A generic function takes any resource as `[R: Close]`, and a class of the program's own is one by
naming `io.Close` after `:` and overriding `close`. A `Channel`'s `close()` ends what it carries rather
than giving anything back, and is not one; a Sysl or C handle's built-in `close()` answers `unit`.

### `Source`

A `Source` is text to be read from somewhere, so one function can read a file, a string
or standard input alike.

```text
import avin.io.{IoError, Source}

count(src: Source) -> int = src.lines().count(l -> !l.isEmpty)

main() -> int
  Source.fromFile("notes.txt") match
    Ok(src) -> println(count(src))
    Err(e) -> eprintln(e.message)
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
| `isDir(file) -> bool` | Whether the path names a directory, one the program may not read included. |
| `isFile(file) -> bool` | Whether the path names something that is not a directory. |
| `remove(file) -> Result[unit, IoError]` | Removes a file; never a directory, which is `IsADirectory`. A link is removed, not what it names. |
| `removeDir(dir) -> Result[unit, IoError]` | Removes an empty directory; `DirectoryNotEmpty` otherwise. |
| `removeDirAll(dir) -> Result[unit, IoError]` | Removes a directory and everything in it; a path already gone is success. |
| `rename(from, to) -> Result[unit, IoError]` | Moves a file or directory in one step, replacing what is at `to`: a file replaces a file, a directory an empty directory (`DirectoryNotEmpty` if it holds entries); a file onto a directory is `IsADirectory`, a directory onto a file `NotADirectory`; a missing source or a missing directory above `to` is `NotFound`. The same path on both sides succeeds and changes nothing. Both names must be on one file system, or nothing moves. |
| `createDir(dir) -> Result[unit, IoError]` | Makes one directory, whose parent must exist; `AlreadyExists` if something is there. |
| `createDirAll(dir) -> Result[unit, IoError]` | Makes the directory and every missing one above it; a directory already there is success; a file where `dir` itself would go is `AlreadyExists`, and a file where one of the directories above it would go is `NotADirectory`. |
| `createTempDir(prefix) -> Result[string, IoError]` | Makes a new, empty directory in the platform's place for temporary files, its name starting with `prefix`, and answers its path. |
| `tempDir(prefix) -> Result[TempDir, IoError]` | Makes the directory as `createTempDir` does and holds it as an [`io.Close`](#close): `t.path` is its path, `t.close()` removes it and everything in it (closing again does nothing and answers `Ok`; one that failed may be closed again), and `t.isClosed` says whether it has been removed. One never closed stays on the disk. |
| `metadata(file) -> Result[Metadata, IoError]` | One look at a path: a `Metadata` of `size` (bytes), `modified` (an `avin.time.Instant`), `isDir` and `isFile`. A symbolic link is followed; a missing path is `NotFound`. For a directory `size` is the file system's bookkeeping, not a count of entries. |
| `size(file) -> Result[long, IoError]` | The size of the file in bytes. |
| `modified(file) -> Result[Instant, IoError]` | When the contents of the file or directory last changed, to the file system's resolution. |
| `setModified(file, when) -> Result[unit, IoError]` | Sets the modification time (the access time moves with it); reading it back may round to the file system's resolution. |
| `listDir(dir) -> Result[Vector[string], IoError]` | The names of the directory's entries, files and directories alike, sorted by code point; `NotFound` or `NotADirectory` when there is no directory to list. |

**Paths are strings**, and the questions follow symbolic links: `isDir` of a link to a directory is
`true`. `removeDirAll` is the exception that matters: a link inside the tree is removed, never
followed, so it deletes nothing outside the directory it was given.

**Size and age** come from one `stat`. A cache that keeps what was used recently and reports what it
freed:

```text
import avin.fs
import avin.io.IoError
import avin.time
import avin.time.Duration

evictOlderThan(dir: string, age: Duration) -> Result[long, IoError]
  var freed: long = 0
  for name in fs.listDir(dir)?
    val file = dir + "/" + name
    val m = fs.metadata(file)?
    if m.isFile && time.now() - m.modified > age
      fs.remove(file)?
      freed += m.size
  Ok(freed)
```

Recording that an entry was used is `fs.setModified(file, time.now())`, which writes nothing to the file.

**A temporary directory is the program's to remove.** `createTempDir` makes the directory rather than
inventing a name, so no other program can take it in between, and nothing removes it at the end:
`removeDirAll` is the call, or `tempDir` and a `defer t.close()` beside it, which removes it on every
way out of the block.

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

### Paths

A path is a `string`, and `avin.fs` has the functions that take one apart and put one together.
They answer from the text alone and touch no disk. Paths are POSIX: `/` separates components, and a
path that starts with `/` starts at the root.

```text
import avin.fs

backupOf(file: string) -> string
  val dir = fs.parent(file).getOrElse(".")
  val stem = fs.stem(file).getOrElse("file")
  fs.join([dir, "backup", stem + ".bak"])
```

| Function | Meaning |
| --- | --- |
| `isAbsolute(p)`, `isRelative(p)` | Whether `p` starts with `/`; the empty path is relative. |
| `components(p) -> Vector[string]` | The names between the separators, empty ones dropped: `/a//b/` has `a` and `b`. The root is not a component; `.` and `..` are kept. |
| `join(a, b) -> string` | `b` after `a` with exactly one `/` between, however many `a` ends with. An empty side adds nothing; an absolute `b` replaces `a`: `join("/home/me", "/etc")` is `/etc`. |
| `join(parts) -> string` | A `Vector[string]` joined in turn by the same rule; empty for no parts. |
| `parent(p) -> Option[string]` | Everything above the last component: `/a/b` has `/a`, `/a` has `/`, `a/b/` has `a`. The root, the empty path and a relative path of one component have `None`. |
| `fileName(p) -> Option[string]` | The last component; `None` for the root, the empty path, `.` and `..`. |
| `extension(p) -> Option[string]` | What follows the last `.` of the file name: `gz` for `a.tar.gz`, the empty string for `a.`. `None` with no dot, and for a name whose only dot is its first: `.profile` has none, `.config.json` has `json`. |
| `stem(p) -> Option[string]` | The file name without its extension and its dot: `a.tar` for `a.tar.gz`, `.profile` for `.profile`. |
| `normalize(p) -> string` | `.` and `..` resolved in the text, doubled and trailing `/` dropped. A `..` at the root is dropped (`/..` is `/`), one leading a relative path is kept (`a/../..` is `..`), and a path that cancels to nothing is `/` or `.`. |
| `relativeTo(p, base) -> Option[string]` | `p` written from `base`, both normalized: `../c` for `/a/c` from `/a/b`, `.` for a path from itself. `None` when one is absolute and the other not, or when `base` climbs above what it names with `..`. |

The questions below ask the file system.

| Function | Meaning |
| --- | --- |
| `absolute(p) -> Result[string, IoError]` | `p` from the root, normalized, a relative path read against the current directory. Links are not resolved. |
| `canonicalize(p) -> Result[string, IoError]` | The one absolute path the file system gives `p`, every link resolved; `NotFound` when nothing is there. |
| `isLink(p) -> bool` | Whether `p` is a symbolic link, dangling or not. |
| `symlink(target, link) -> Result[unit, IoError]` | Makes `link` a symbolic link holding `target` as written; a relative target is read from the link's directory. `AlreadyExists` when something is at `link`. |
| `readLink(link) -> Result[string, IoError]` | What a link holds, as written; a path that is not a link is an `Err`. |
| `walk(root) -> Result[Vector[Entry], IoError]` | Every entry below the directory `root`, as described below. |
| `matches(pattern, p) -> bool` | Whether `p` fits the pattern, as described below. |
| `glob(pattern) -> Vector[string]` | The paths that exist and fit the pattern. |

**`normalize` reads only the text, and the difference matters.** Where `a/lib` is a link to
`src/lib`, `normalize("a/lib/../main.c")` is `a/main.c`, while the file system climbs out of
`src/lib` into `src`: `canonicalize` of the same path names `src/main.c`. Whatever decides access to
a file is decided on `canonicalize`.

**A walk is the whole tree, sorted.** `walk` answers an `Entry(path, relative, depth, isDir, isLink,
error)` for each file, directory and link below the root, the root itself left out: `relative` is the
path from the root and `path` the root joined with it; `depth` is 1 for the root's own entries. The
order is depth first, a directory before what it holds, and each directory's names in code-point
order, as `listDir` sorts them, so two walks of an unchanged tree agree. **A symbolic link is listed
and never followed**: its `isLink` is true and its `isDir` false, whatever it names, so a link back up
the tree is one entry and not a loop. The root itself is followed when it is a link. **A directory the
walk cannot list is still an entry**, with `isDir` true and the reason in `error` (for example
`Some(PermissionDenied)`), and the walk goes on past it; only a root that cannot be listed makes the
answer an `Err`: `NotFound`, `NotADirectory` or `PermissionDenied`.

```text
import avin.fs
import avin.io.IoError

sourcesIn(dir: string) -> Result[Vector[string], IoError]
  val found = fs.walk(dir)?.filter(e -> !e.isDir && fs.matches("**/*.avin", e.relative))
  Ok(found.map(e -> e.path))
```

**A pattern is matched component by component**, so doubled and trailing `/` do not matter, and an
absolute pattern fits only an absolute path. Within a component:

| Pattern | Fits |
| --- | --- |
| `*` | any run of characters, none included |
| `?` | one character: `?.txt` fits `文.txt` |
| `[abc]`, `[a-z]` | one character of the set; `]` first in the set, and `-` first or last, are members |
| `[!a-z]`, `[^a-z]` | one character not in the set |
| `\x` | the character `x` itself |
| `**`, as a whole component | any number of whole components, none included: `src/**/main.c` fits `src/main.c` and `src/a/b/main.c` |

**A name starting with `.` is hidden**: only a component that starts with a literal `.` fits it, so
`*` does not fit `.git`, nor do `?git` and `[.]git`, while `.*` and `\.git` do; and `**` never passes
through a hidden name, so `**/*.c` stays out of `.git`. `*/x` does not fit `../x`, for the same reason.
**Every pattern is valid**: a `[` with no closing `]` is a literal `[`, and a `\` at the end a literal
`\`. A `**` inside a component, as in `a**b`, is two `*`.

**`glob` answers what is there.** A relative pattern is read from the current directory and answers
relative paths; an absolute one answers absolute paths. The answer is in code-point order, each path
once, and every path in it fits the pattern by `matches`. A `**` goes through directories as `walk`
does, never through a link; a link a literal or wildcard component names is followed. A literal last
component answers a dangling link too. A directory that cannot be read contributes nothing: `glob`
has no `Err`. The empty pattern answers nothing, and `/` answers `/`.

```text
for file in fs.glob("tests/**/*.avin")
  println(file)
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
      None -> sleep(250.millis)
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
| `flush()`, `close()` | `flush` hands what is written to the platform, so other handles and programs see it. `close` flushes and releases, answering the flush's `Err`; closing twice is fine and answers `Ok`. A file is an [`io.Close`](#close). |

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
| `round(x)` | The whole number nearest `x` as a `long`, a tie going away from zero: `round(2.5)` is `3`, `round(-2.5)` is `-3`. It converts as `long(x)` does: NaN is `0`, a value past the range, an infinity included, is the nearest bound. |
| `trunc(x)` | `x` with its fraction dropped, towards zero, as a `real`; `trunc(-0.5)` is `-0.0`. |
| `log2(x)` | The logarithm to base 2, exact at every power of two. |
| `fma(x, y, z)` | `x * y + z` rounded once. The language never fuses `x * y + z` by itself, so `fma` is how a program asks for it. |
| `copySign(x, s)` | The magnitude of `x` with the sign of `s`, a negative zero's sign included. |
| `signBit(x)` | Whether the sign bit of `x` is set: `true` for a negative value, a negative zero and a negated NaN. |
| `ulp(x)` | The gap from `abs(x)` to the next `real` above: `real.MinPositiveValue` at zero and through the subnormals, infinity at an infinity, NaN at NaN, and at the greatest finite value the gap below it. |
| `nextUp(x)`, `nextDown(x)` | The next `real` above, or below: `nextUp(0.0)` is `real.MinPositiveValue`, the greatest finite value goes to infinity, and an infinity going outward and NaN stay as they are. |

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
O(log n); it draws exactly as `rng.chooseWeighted(pairs)` would. `size` is the number of items.

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

## `avin.crypto`

Content digests and keyed digests: SHA-256, SHA-512, SHA-1 and MD5, HMAC over the first two, and
hexadecimal text for any digest.

```text
import avin.crypto.{sha256, hmacSha256, toHex, Sha256}

main()
  println(toHex(sha256("abc")))
  println(toHex(hmacSha256("key", "message")))
  val file = Sha256()
  file.update("a piece")
  file.update("and another")
  println(toHex(file.finish()))
```

A digest is a `Vector[byte]`, the bytes the standard defines, so it compares by value, can key a map
or a cache, and is written as text only on request. Text is hashed as its UTF-8 bytes.

| Function | Meaning |
| --- | --- |
| `sha256(x)`, `sha512(x)` | The 32-byte and 64-byte digest of `x`, a `Vector[byte]` or a `string`. |
| `sha1(x)`, `md5(x)` | The 20-byte and 16-byte digest of `x`. **Broken for security** (below). |
| `hmacSha256(key, message)`, `hmacSha512(key, message)` | The 32-byte and 64-byte HMAC (RFC 2104) of `message` under `key`, both `Vector[byte]` or both `string`. Any key length is allowed, the empty key included; one longer than the algorithm's block is hashed first, as the RFC says. |
| `toHex(bytes) -> string` | Lower-case hexadecimal, two digits for each byte. |

**A hasher takes its input in pieces**, for data too large to hold whole, such as a file read in
chunks. `Sha256()`, `Sha512()`, `Sha1()` and `Md5()` each have `update(bytes)` and `update(text)`,
any number of times, and `finish()`, the digest of everything added. `finish()` answers the same
digest each time it is called, and `update` after `finish` traps. Hashing in any pieces gives the
one-shot digest. A hasher belongs to the thread that made it, as a handle does.

**`sha1` and `md5` are for formats that already use them**: a checksum in a listing, a cache key
nobody attacks. Two different inputs with the same digest can be made for both, so neither signs,
authenticates or names untrusted content; use `sha256` or `sha512` for those. HMAC does not rest on
collision resistance, but only the SHA-2 variants are offered. Comparing a received tag with a
computed one is a comparison of two `Vector[byte]`, which is not constant-time.

The digests are bound to Sysl's `sysl.crypto`, so they are the same code Sysl programs use.

## `avin.net`

TCP connections, TLS over them, servers, URLs, and a small HTTP client and server that speak `https`
too. Every operation that can fail answers a `Result[T, IoError]`, and a connection that is reading
or writing waits safe for the collector.

```text
import avin.net.http

main() -> int
  http.get("https://example.com/") match
    Ok(r) if r.status == 200 -> println(r.text)
    Ok(r) -> eprintln(s"status ${r.status}")
    Err(e) -> eprintln(e.message)
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
| `s.setTimeout(d: Duration) -> Result[unit, IoError]` | A read or write that waits longer for the other end fails with `TimedOut`; zero or less takes the limit away. Each wait has the whole limit, so a write that waits for room several times fails only where one wait runs out. |
| `s.shutdownOutput() -> Result[unit, IoError]` | Tells the other end nothing more will be sent; reading goes on. |
| `s.close() -> Result[unit, IoError]` | Ends the connection and answers `Ok`; closing again does nothing. Every connection and listener is an [`io.Close`](#close). |
| `TcpListener.bind(host: string, port: int) -> Result[TcpListener, IoError]` | Listens on the host's first address (`127.0.0.1` for this machine alone, `0.0.0.0` for all of its addresses); port `0` lets the platform choose. |
| `l.port() -> int` | The port it listens on. |
| `l.accept() -> Result[TcpStream, IoError]` | Waits for the next connection. |
| `l.close() -> Result[unit, IoError]` | Stops listening and answers `Ok`; closing again does nothing. |

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
    Err(e) -> eprintln(e.message)
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
| `l.close() -> Result[unit, IoError]` | Stops listening and answers `Ok`; closing again does nothing. The socket file stays; `fs.remove` or `bindReplacing` clears it. |

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
whole milliseconds, a part of one counting as one, and the longest duration is a limit that never
runs out.

**Waiting on a connection holds no operating system thread.** An `accept`, a `connect`, a read or a
write that has to wait for the other end parks a [lightweight thread](/spec/threads/#lightweight-threads)
on the runtime's poller, and its worker runs other threads until the socket is ready: a server that
gives each connection a thread of its own serves far more connections than it has workers. `main`,
which is not a lightweight thread, waits on the socket alone. Either way the wait is safe for the
collector and a timeout ends it. A wait ends when the other end acts: a read whose peer has closed
answers the end of the input (an empty array, `None` from `readLine`), and a write to a peer that has
closed fails with `ConnectionReset`, never ending the program. Nothing else can end a wait early,
since a connection [belongs to one thread](#tcp): another thread that tries to close a connection or
listener a thread is parked on traps, as any use of it from there does.

**A connection belongs to one thread at a time**, as every [handle](/spec/sysl-bindings/#handles)
does: a server accepts a connection and hands it to the thread or future that serves it, the bytes
read ahead going with it, and the code that handed it over traps at any operation on it while the
body runs, a read served from those bytes alone and one on a connection the body has closed
included ([proposed](/spec/decisions/#proposed)); a listener's `port`, `accept` and `close` are
checked the same way:

```text
avin: trap: `TcpStream` belongs to another thread at main.avin:16:25
```

**`ByteStream` is what the three connections share**: a trait with `read`, `readLine`, `readAll`,
`write`, `writeText`, `setTimeout`, `shutdownOutput` and `close`, which `TcpStream`, `UnixStream` and
`TlsStream` implement, so code that speaks a protocol, as the HTTP client and server do, takes a
`ByteStream` and runs over any of them. It is [sendable](/spec/actors/#what-may-be-sent), and it is
an [`io.Reader`](#reader) by its `read`, so whatever reads a `Reader` reads any connection, a TLS one
included. `ReadAhead`,
the bytes a connection has read beyond what its caller asked for and the line reading over them, is
public for a connection of one's own.

### TLS

`avin.net.tls` is TLS over a `TcpStream`, at either end, by OpenSSL, which the library reaches
through the Sysl package `openssl` named in its own `sysl-dependencies`. OpenSSL never touches the
socket: it works on two buffers in memory, the bytes it has to send are written on the TCP
connection, and the bytes that arrive there are handed to it. So a `TlsStream` waits exactly as its
`TcpStream` does, a lightweight thread parked on the poller, with the same per-wait limit and the
same [cancellation](/spec/threads/#cancellation). A program that reaches none of this builds and
links no OpenSSL.

```text
import avin.io.IoError
import avin.net.tls.{TlsError, TlsStream}

main() -> Result[unit, TlsError | IoError]
  val s = TlsStream.connect("example.com", 443)?
  defer s.close()
  s.writeText("HEAD / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n")?
  println(s.readLine()?.getOrElse(""))
  Ok(())
```

| Member | Meaning |
| --- | --- |
| `TlsStream.connect(host, port, settings: TlsClient = TlsClient()) -> Result[TlsStream, TlsError]` | A TCP connection as `TcpStream.connect` makes one, then TLS as the client, the certificate checked for `host`. |
| `TlsStream.client(tcp: TcpStream, host, settings = TlsClient()) -> Result[TlsStream, TlsError]` | TLS as the client over a connection the caller made, its handshake waiting with `tcp`'s limit. Where it fails, `tcp` is the caller's to close. |
| `TlsStream.server(tcp: TcpStream, settings: TlsServer) -> Result[TlsStream, TlsError]` | TLS as the server over an accepted connection, presenting the settings' certificate. |
| `s.read`, `readLine`, `readAll`, `write`, `writeText`, `setTimeout` | As a `TcpStream`'s, of plaintext; the limit is the TCP connection's, for each of its waits. |
| `s.shutdownOutput()` | Closes TLS (a `close_notify`) and the TCP connection's sending side; reading goes on. |
| `s.close() -> Result[unit, IoError]` | Closes TLS as far as the peer can be told without waiting, then the connection; the `Err` is a failure to send that notice, the connection being closed either way. Again does nothing and answers `Ok`. |
| `TlsClient(trusted = [])`, `c.trust(pem: string) -> TlsClient` | The machine's trusted authorities and those added: `trust` answers a client that also trusts the certificates in `pem`. |
| `TlsServer(certificate, key)`, `TlsServer.of(certificate, key) -> Result[TlsServer, TlsError]` | A server's certificate (a chain, leaf first) and private key as PEM text; `of` checks they can be used and go together, and one made directly is checked at its first connection. |

**What a client checks, and what cannot be turned off.** The server's certificate must chain to an
authority the client trusts: the machine's store, read through OpenSSL's default paths (on macOS
the `cert.pem` of Homebrew's OpenSSL, not the Keychain), and the authorities `trust` adds. Its name
must cover the host connected to, a name or an address. The host's name is sent for the server to
choose a certificate by, and the oldest protocol spoken is TLS 1.2. `trust` only adds: there is no
way to accept a certificate these refuse, a private authority, a corporate one and a test's own
being the same case. The choices are [proposed](/spec/decisions/#proposed).

**After the handshake a connection is a `ByteStream`**: a failure of TLS itself while reading or
writing is an `IoError` of kind `Other` whose text begins `TLS:`, and the connection's end, by the peer's
`close_notify` or by the TCP connection closing, reads as the end of a TCP connection's does.

`TlsError` says why a connection could not be made: `Transport(IoError)`, the connection beneath
could not be made, broke or timed out; `Certificate(text, why)`, the peer's certificate was refused,
and `why` is a `TlsRefusal` saying how: `HostMismatch` (for another host or IP address), `Expired`,
`NotYetValid`, `Untrusted` (an unknown issuer, or a self-signed certificate) or `Other(code)`, OpenSSL's
number for a rarer verdict; `refusal()` answers it as an `Option[TlsRefusal]`, `None` for any other
error, and the `message` of the error and of the refusal names it ("the certificate is for another
host", "the certificate has expired", "the certificate is not yet valid", "the certificate is not
trusted", "the certificate was refused (code N)"); `Handshake(text)`, the two ends did not agree,
as with a peer that does not speak TLS or ends the connection in the handshake; and
`Credentials(text)`, a certificate or key given to this end cannot be used. `message` says it in
words. Each `TlsStream`, `TlsClient` and `TlsServer` is sendable, a stream belonging to one thread
at a time as a `TcpStream` does.

### Servers

`TcpServer` is a listener that serves each connection it accepts in an
[actor](/spec/actors/) of its own, so a connection that waits holds no operating system thread and
a bug in the code serving one ends that connection alone:

```text
import avin.net.TcpServer

main()
  val server = TcpServer.start("0.0.0.0", 7000, (s, _) ->
    while s.readLine() is Ok(Some(line))
      s.writeText(line + "\n")).ok.get
  server.join()
```

| Member | Meaning |
| --- | --- |
| `TcpServer.start(host, port, serve: @sendable (TcpStream, TcpServer) -> unit, maxConnections = 1024, timeout = Duration.Zero, failed: @sendable (TcpStream) -> unit = _ -> ()) -> Result[TcpServer, IoError]` | Listens as `TcpListener.bind` does and serves each connection by `serve`, given the connection and the server. |
| `s.port() -> int` | The port it listens on, the one the platform chose for port 0. |
| `s.connections() -> int` | How many connections it holds now. |
| `s.isStopping() -> bool` | Whether it has been asked to stop, for code serving a connection to finish early. |
| `s.idle[T: Sendable](@sendable wait: () -> T) -> Option[T]` | Runs `wait`, a connection's wait for its next piece of work, on a thread of its own; `Some` of its result, or `None` where the server stops first, which cancels `wait`. |
| `s.stop()` | Stops accepting and returns at once, cancelling the parked `accept` and every wait in `idle`; stopping again does nothing. |
| `s.join()` | Waits until it has stopped and every connection it held has ended. |
| `s.shutdown()` | `stop()`, then `join()`. |

- **Each connection is served in an actor's first turn**, the connection handed to it, and closed
  once `serve` returns. The actor runs on a pool of the server's own with a worker for each
  connection it may hold, so a connection parked on its socket never keeps another waiting for a
  worker; the workers are lightweight threads, idle ones parked.
- **A trap in `serve` fails that connection's actor alone** (supervised with `Stop`): the trap is
  reported naming the actor, `avin: trap in Connection#7: …`, then `failed` runs on the
  connection, which is closed, and the server goes on. A trap that leaves shared state in doubt, as
  [supervision](/spec/actors/#supervision) says, still stops the program.
- **At most `maxConnections` are held at once.** The next connection waits unaccepted, in the
  listener's backlog, until one ends; a limit less than one traps. Each connection's timeout is
  `timeout` when `serve` begins, none for zero, and `serve` may change it.
- **A graceful shutdown.** `stop()` stops accepting at once: the acceptor's parked `accept` is
  [cancelled](/spec/threads/#cancellation), no connection being made to wake it, and the listener
  is closed, so a new connection is refused. The connections held go on until `serve` returns;
  `join()` waits for them. Code serving a connection calls `stop()`, since `shutdown()` there would
  wait for itself. The server may be shared between threads, and any of them may stop it and wait
  for it.
- **A connection idle between pieces of work ends at once.** `serve` waits for the next one
  through `server.idle(() -> s.readLine())`: `wait` runs on a thread of a scope the calling thread
  runs, and stopping the server cancels that scope, so `wait` ends wherever it waits, however long
  its timeout, and `idle` answers `None`; once the server is stopping, `idle` answers `None` without
  running `wait`. The connection `wait` reads is handed back when `idle` returns, as a scope hands
  back the handles of its threads. Code outside `idle`, a request being answered, is never
  cancelled, so it finishes. A trap in `wait` stops the program, as a trap in any thread does,
  rather than failing the connection's actor alone.

```text
serveLines(s: TcpStream, server: TcpServer)
  while server.idle(() -> s.readLine()) is Some(Ok(Some(line)))
    s.writeText(handle(line) + "\n")
```

### HTTP server

`http.serve` is an HTTP/1.1 server on a `TcpServer`, each request answered by a handler from a
`ServerRequest` to a `Response`; a `Router` is a handler that chooses another by method and path:

```text
import avin.net.http
import avin.net.http.{Response, Route, Router}

main()
  val router = Router(
    [
      Route.get("/hello/:name", r -> Response.text(200, s"hello, ${r.param("name")}\n")),
      Route.post("/echo", r -> Response.bytes(200, r.body, "application/octet-stream"))
    ]
  )
  http.serve("0.0.0.0", 8080, router.handle).ok.get.join()
```

`http.serve(host, port, handler: @sendable (ServerRequest) -> Response, maxConnections = 1024,
readTimeout = seconds(30), writeTimeout = seconds(30), idleTimeout = seconds(5), maxHeaderBytes =
16384, maxBodyBytes = 1048576, tls: Option[TlsServer] = None) -> Result[TcpServer, IoError]` listens as `TcpServer.start` does. Each
wait while a request is read may last `readTimeout`, each wait while a response is written
`writeTimeout`, and the wait for the next request on a connection kept alive `idleTimeout`; zero
takes a limit away. A `maxHeaderBytes` less than one or a `maxBodyBytes` less than zero traps. The
wait for a connection's next request, its first among them, is made through `idle`, so a shutdown
closes a connection waiting there at once, unanswered; a request already being read or answered
finishes, its response saying `Connection: close`, and a request read once the server is stopping
is answered `503`.

**With `tls`, the server speaks HTTPS.** Each connection's actor begins with a TLS handshake
presenting the `TlsServer`'s certificate, each of its waits lasting at most `readTimeout`, and
serves its requests over the `TlsStream` exactly as over TCP, kept alive and pipelined alike. A
connection whose handshake fails, a client speaking plain HTTP among them, is closed, and the server
goes on. A handler that traps on a TLS connection has it closed without the `500`, since the answer
would have to be written through TLS the handler's turn held.

A `ServerRequest` has its `method`, its `target` as the request line gave it, its `version`
(`HTTP/1.1` or `HTTP/1.0`), its `headers` in order with `header(name)` looking one up without regard
to case, its `body` with `text` reading it as UTF-8, and the `params` its route bound, with
`param(name)` trapping where the route has none of that name. From the target come `path`, its
escapes decoded, `segments`, the path's segments each decoded (so `/a%2Fb` is the one segment
`a/b`, and `/` one empty one), `query` as written, and `queryParams` as `Url.queryParams` reads
them. A target is a path beginning with `/` or an absolute `http` URL; `ServerRequest(method,
target)` traps on any other, so a handler can be tried without a connection.

A `Response` is the client's: `Response.text(status, text)` (`text/plain; charset=utf-8`),
`Response.json(status, text)` (`application/json`, the text sent as given and not checked to be
JSON), `Response.bytes(status, body, contentType)`, `Response.empty(status)` and `withHeader(name, value)`,
which traps on a name that is not a token or a value holding a line break; `http.reasonPhrase(status)`
is RFC 9110's phrase for a status, empty for one it does not name.

`Route(method, pattern, handler)`, or `Route.get`, `post`, `put`, `delete` and `patch`, routes a
method and a pattern; a method that is not a token or a pattern not beginning with `/` traps.
`Router(routes)` tries its routes in order. A pattern's segments match a request's decoded segments
one for one, a segment `:name` matching any one that is not empty and binding it, so Unicode paths
route whether escaped or not. A `GET` route answers `HEAD` too where no `HEAD` route matches. A path
no route matches is answered `404`, and one that routes match for other methods only `405`, with
`Allow` naming those methods, `HEAD` beside `GET`.

**The connection.** Requests are read one after another, so several are kept alive on one
connection and pipelined ones are answered in order: in HTTP/1.1 unless either side says
`Connection: close`, in HTTP/1.0 only on `Connection: keep-alive`, which the response then repeats.
Empty lines before a request line are passed over, and a line may end in a bare `\n`. A body is read
by `Content-Length` or chunked (extensions and trailers passed over); an `Expect: 100-continue` is
answered `100 Continue` once the body is known to fit. The server writes `Content-Length` and
`Connection` itself, in place of any the handler gave, every response as `HTTP/1.1`, and no body for
`HEAD`, `204` and `304`. A handler's status outside 200 to 599, or a header the response cannot
carry, traps.

**A request the server cannot take is answered and the connection closed**, gently: its side is shut
and what the client still sends read and dropped for a moment, so the answer is not lost to a reset.

| Status | When |
| --- | --- |
| `400` | A request line that is not three words, a method that is not a token, a malformed version, a target that is not one (a fragment, an escape that is not UTF-8 among them), no `Host` in HTTP/1.1 or two, a header without a colon, with a name that is not a token or folded onto a second line, a `Content-Length` that is not digits or two that differ, one beside `Transfer-Encoding`, a coding list not ending in `chunked`, a malformed chunk |
| `408` | A wait while a request is read outlasts `readTimeout` |
| `413` | A body longer than `maxBodyBytes`, by its length before it is read, or as its chunks arrive |
| `414` | A request line longer than `maxHeaderBytes` |
| `417` | An `Expect` other than `100-continue` |
| `431` | The request line and headers together, or a chunked body's trailers, longer than `maxHeaderBytes` |
| `500` | The handler traps: it fails its connection alone, as a `TcpServer`'s `serve` does (over TLS the connection is closed unanswered) |
| `501` | A transfer coding besides `chunked` |
| `503` | A request read once the server is stopping |
| `505` | A version other than 1.0 and 1.1 |

A connection waiting for a first request waits `readTimeout`, and for a later one `idleTimeout`,
then is closed without an answer. **Shutting down**, the server answers the request in flight with
`Connection: close`, and one arriving on a kept-alive connection `503`; a connection waiting for its
next request ends when its idle limit runs out, and `join()` waits for that.

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
| `u.portOrDefault -> Option[int]` | The port, or where none is written the scheme's default: 80 for `http` and `ws`, 443 for `https` and `wss`. |
| `u.queryParams -> Vector[(string, string)]` | The query split at each `&` (an empty piece skipped) and each piece at its first `=`, decoded, a `+` standing for a space as a form writes one; no `=` gives an empty value, no query no pairs. |
| `u.resolve(ref: string) -> Result[Url, UrlError]` | The reference, relative or absolute, resolved against `u` by RFC 3986 §5.2: its missing components taken from `u` and its dot segments removed, strictly, so `http:g` stays `http:g`. |
| `u.resolve(ref: Url) -> Url` | An absolute reference with its dot segments removed. |
| `u.withScheme(s)`, `withUserInfo(i)`, `withHost(h)`, `withPort(p)`, `withPath(p)`, `withQuery(q)`, `withFragment(f)` | The URL with one component replaced, as written with its escapes, held as `parse` holds it; the optional ones take a value or an `Option`, `None` taking the component away. A component the URL cannot hold traps: one with a character it may not, a port outside 0 to 65535, user information or a port without a host, a path beside a host not beginning with `/`. |
| `Url.encode(text) -> string` | Every byte of the text's UTF-8 but a letter, a digit, `-`, `.`, `_` and `~` written `%XX` in upper case, so the result can stand as any component. |
| `Url.decode(text) -> Result[string, UrlError]` | Each `%XX` replaced by its byte and the bytes read as UTF-8; a `+` is kept. |

`UrlError` says why text is not a URL, with `at`, the byte offset of what is wrong: `MissingScheme`
(a relative reference, or text beginning with `:`), `BadScheme(at)`, `BadCharacter(at)`,
`BadPercent(at)`, `BadHost(at)`, `BadPort(at)` (not digits, or above 65535) and `InvalidUtf8(at)`;
`message` says it in words. **Every component's escapes must decode to UTF-8 for the text to be a
URL**, so `parse` refuses `http://h/%FF` with `InvalidUtf8(9)` and `queryParams` never fails on a
`Url` it read.

### `http`

| Function | Meaning |
| --- | --- |
| `http.get(url: string, headers = []) -> Result[Response, HttpError]` | Fetches the URL. |
| `http.head(url: string, headers = [])` | Asks for the URL's headers; the response has no body. |
| `http.delete(url: string, headers = [])` | Asks the server to delete the URL. |
| `http.post(url: string, body: string, contentType: string, headers = []) -> Result[Response, HttpError]` | Sends `body`, as UTF-8, and answers the response. |
| `http.put(url, body, contentType, headers = [])` | The same with `PUT`. |
| `http.patch(url, body, contentType, headers = [])` | The same with `PATCH`. |
| `http.send(request: Request) -> Result[Response, HttpError]` | Any method, headers, body, timeout and TLS settings, built from `Request(method, url, headers)` with `withHeader`, `withHeaders`, `withQueryParam`, `withBody`, `withTimeout` and `withTls`. |

Every verb answers a `Result[Response, HttpError]`, and `headers`, a `Vector[(string, string)]`, are
sent after the ones the client writes, in order. `post`, `put` and `patch` send `Content-Type:
contentType` first, so a `Content-Type` among `headers` follows it. `withQueryParam(name, value)`
adds `name=value` to the request's URL, each encoded by `Url.encode`, after any query it has (with
`?` where it has none, `&` otherwise, nothing after a trailing `?` or `&`) and before its fragment.

A `Response` has its `status` and `reason`, its `headers` as a vector of name and value pairs,
`header(name)` looking a name up without regard to case, its `body` as an `Array[byte]` and `text`
reading that as UTF-8, a sequence that is not read as U+FFFD. **A status is an answer, not a failure**: `404` is an `Ok`. `HttpError` is `Io(IoError)`,
`BadUrl(UrlError)`, `Malformed(text)` for a reply that is not HTTP, `Unsupported(text)` for a scheme
other than `http` and `https`, and `Tls(TlsError)` for TLS that could not be set up (a connection
that could not be made, broke or timed out being `Io`, for `https` as for `http`). The
client speaks HTTP/1.1, one request to a connection, reads bodies by `Content-Length`, chunked or to
the connection's end, and follows up to five redirects of a `GET` or `HEAD` (statuses 301, 302,
303, 307 and 308), each `Location` resolved against the URL that answered by `Url.resolve`; the sixth
is the answer. It writes `Host`, `Content-Length` (for a body, and for a `POST`, a `PUT` and a
`PATCH` even when theirs is empty) and `Connection: close` unless the request gives them, then the
request's own headers in order. `HEAD`, an interim
`1xx` (which is skipped for the answer after it), `204` and `304` have no body. A method is a token
and a header's name one too, and a header's value holds no line break; a request where one does not
traps as it is made, whether the header came to `Request`'s constructor, to a verb, or by
`withHeader` or `withHeaders`, so nothing is sent. A request's timeout bounds each read and write on its connection.

**An `https` URL just works**: the client makes the TCP connection to the URL's port, 443 where it
gives none, sets the request's timeout on it, so the handshake's waits are bounded too, and speaks
the request over a [`TlsStream`](#tls), the certificate checked for the URL's host against the
machine's trusted authorities and those of the request's `TlsClient`, which `withTls(client)` sets
(`TlsClient()` unless it is set). A redirect between `http` and `https` is followed like any other,
keeping the request's TLS settings. TLS lives in the Sysl package `openssl`, which the library
names in `library/library.hocon`, its own `sysl-dependencies`; a program's shims depend on it only
where a binding the program compiles reaches it, which `avin.net.http` and `avin.net.tls` do and
`avin.net` alone does not ([proposed](/spec/decisions/#proposed)).

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

JSON is not part of the standard library: it is the package [`dev.avinlang.json`](/spec/json/), in
its own repository, which a program adds to its dependencies. `import avin.json` is an error naming
the package.
