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
| `waitPidCpu(pid: long) -> Result[(ExitStatus, CpuTime), IoError]` | `waitPid`, answering with the status the [CPU time](#cpu-time) the child used, its reaped descendants' included. |
| `tryWaitPidCpu(pid: long) -> Result[Option[(ExitStatus, CpuTime)], IoError]` | `tryWaitPid`, answering with the status the child's CPU time. |
| `cpuTime() -> CpuTime` | The [CPU time](#cpu-time) the program has used so far, every thread's together. |
| `childrenCpuTime() -> CpuTime` | The CPU time of the program's children that have ended and been reaped, each with the children it reaped in turn. |
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

### CPU time

`CpuTime(user: Duration, system: Duration)` is how long a processor ran a process's own code and
the kernel's code on its behalf, each in whole microseconds as the kernel accounts for them; `total`
is the two added, and `CpuTime` is `Add`, with `CpuTime.Zero` its zero. The kernel keeps the count
per process, and **a process's count is handed to its parent when the parent reaps it**, folded into
the parent's own children's count. So the CPU of a whole tree reaches the program through the one
child it started, once every process in the tree was reaped by its own parent: a shell running a
loop of programs that each exit has their time as well as its own.

```avin
match Command("/bin/sh", "-c", "make -j4").start()
  Ok(job) ->
    _ = job.waitFor()
    println(s"${job.cpuTime().total.toSecondsReal} s of CPU")   // make, the compilers it ran, theirs
  Err(e) -> eprint(e.message)
```

**Only what was reaped counts.** A program still running counts nothing, and so does one that has
ended and not been waited for, and a grandchild that outlives its parent, or that its parent never
waited for, never reaches it. `cpuTime()` is the program's own, every thread's, and only grows;
`childrenCpuTime()` is every reaped child's added. `waitPidCpu` and `tryWaitPidCpu` answer one
child's, as `waitPid` and `tryWaitPid` answer its status, with the same errors; a `Child`'s
`cpuTime()` answers its command's. A program that only slept used next to none: what starting it
took.

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
| `c.pipeInput()`, `c.pipeOutput()`, `c.pipeErrors()` | The same command, its standard input, output or error a pipe the program writes or reads through the `Child` that `start()` answers ([pipes](#pipes-to-a-started-command)). With `errorsToOutput()`, standard error goes into the output's pipe. |
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
| `cpuTime() -> CpuTime` | The [CPU time](#cpu-time) the command's programs used, each with its reaped descendants', a pipeline's stages and a sequence's parts added. After `waitFor` it is the whole command's; before, what the programs seen to have ended used. A detached program counts nothing. |
| `stdin() -> ChildInput` | The pipe the command reads its standard input from, for a command started `pipeInput()`; the same one every time. A command that asked for none traps. |
| `stdout() -> ChildOutput`, `stderr() -> ChildOutput` | The pipe the command writes its standard output, or its standard error, into, for a command started `pipeOutput()` or `pipeErrors()`. A command that asked for none traps. |

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

#### Pipes to a started command

A command started with `pipeInput()`, `pipeOutput()` or `pipeErrors()` talks to the program over
pipes while it runs, a request written and its reply read, as an editor talks to a language server
([settled](/spec/decisions/#settled)):

```text run
import avin.process.Command

main() -> int
  val doubler = Command("/bin/sh", "-c", "while read n; do echo $((n * 2)); done").pipeInput().pipeOutput().start().ok.get
  val requests = doubler.stdin()
  val replies = doubler.stdout()
  for n in [1, 20, 300]
    _ = requests.writeText(s"$n\n")
    println(s"$n doubled is ${replies.readLine().ok.get.get}")
  _ = requests.close()
  println(s"after its input ended: ${replies.readLine().ok.get}, ${doubler.waitFor().ok.get}")
  0
```

```text output
1 doubled is 2
20 doubled is 40
300 doubled is 600
after its input ended: None, Exited(0)
```

`stdin()` is a `ChildInput`, which is an `avin.net.StreamWriter`, and `stdout()` and `stderr()` are
`ChildOutput`s, each an `avin.net.StreamReader`: the two halves of a [split connection](#split-connections)
in all but name, so code that speaks a protocol over a connection speaks it over a child's standard
streams unchanged, and anything that takes an `io.Reader` or an `io.ByteWriter` takes them.

| `ChildOutput` | Meaning |
| --- | --- |
| `read(max: int)` | Up to `max` bytes of what has arrived, waiting only while nothing has; an empty array once the output has ended, as often as asked. |
| `readLine()`, `readAll()` | A line, `None` at the end, or everything until the end, as a connection reads them. |
| `readAvailable(max: int)` | `Some` of what `read(max)` would answer without waiting, `None` where nothing has arrived. Never waits. |
| `readable()` | Whether a read would answer now: a `select` waits on it with an `out.readable()` arm, a `Selector` with `readable(out, key)`. |
| `setTimeout(d)`, `close()` | A wait longer than `d` answers `TimedOut`; closing it tells a command that still writes that its reader has gone. |

| `ChildInput` | Meaning |
| --- | --- |
| `write(bytes)`, `writeText(text)` | Writes all of it, waiting for the command to read where the pipe is full. |
| `writeSome(bytes)` | Writes what goes without waiting, after waiting for room for the first byte, and answers how many. |
| `writable()` | Whether a write would write something now; a `select` waits on it with an `in.writable()` arm. |
| `setTimeout(d)`, `close()` | A wait longer than `d` answers `TimedOut`; closing it ends the command's input, which it reads once it has read everything written before. |

**A wait parks.** Each end is non-blocking, and a read or write that has to wait waits as a
connection's does: a lightweight thread parks on the runtime's poller, leaving its worker to the
others, and any other thread waits on the descriptor alone, safe for the collector.

**Nothing ends the program.** A write to a command that has ended, or has closed its input, answers
`Err(IoError(ConnectionReset, "write to", "the standard input of …"))`, never SIGPIPE. A command that
ends, or is killed, with output unread leaves it in the pipe: `waitFor` answers its status, and the
reads that follow answer what it wrote and then the end. Output ends when every program holding the
stream has ended or closed it, a program the command started and left running among them. A
command's [time limit](#commands) bounds every wait on its pipes too: one that would wait past it
answers `TimedOut`, and `waitFor` then stops the command and answers `TimedOut`.

**A full pipe waits for its reader.** A pipe holds 64 KiB; a command writing more waits until the
program reads, and the program writing more than the command reads waits for it. A program that writes
a command's input and reads its output, both larger than that, does the two at once: in two threads,
or in one through a `select` on `in.writable()` and `out.readable()` with `writeSome` and
`readAvailable`. Waiting for a command with `waitFor` reads nothing, so a command that writes more
than a pipe holds is read before it is waited for.

**Each end belongs to the code it is handed to, not to the `Child`.** An end is used by one thread at a
time and checked at every operation, as a connection is; handed to a thread, it is that thread's until
the thread returns. Handing the `Child` over hands over none of its pipes, so a thread may wait for the
command while the program writes and reads them. `close()` on the `Child` stops and waits for the
command and leaves the pipes to their own `close()`; an end closes too when the collector frees it.
The pipes do not keep the command running: it is the `Child`'s, which stops it when collected.

**Only the command `start()` starts hands its pipes over.** `run()`, `output()` and `lines()` of a
command asking for a pipe trap, and so does a part of a pipeline or a sequence asking for one: a
pipeline's input pipe is its first stage's, its output pipe its last stage's, and its error pipe every
stage's; a sequence's parts write into its output pipe one after the other. The program's ends of the
pipes are closed on exec, so no other command started meanwhile, from any thread, inherits one.

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
| `command(sub)` | The command `sub.name`, whose options and arguments the parser `sub` declares, listed in the help with `sub.about`. A parser with commands takes no positional arguments. |

Each declaration answers a new parser and leaves the one it was called on as it was, so a parser is
an ordinary value: built once at the top of a module, or in a function.

| Reading | Meaning |
| --- | --- |
| `parse(args) -> Result[Args, ArgsError]` | The arguments read, or why not. |
| `parseOrExit(args) -> Args` | As `parse`, and where it gives no `Args` the program ends: the error's `report`, the help or the version to standard output and status 0, a mistake to standard error, as the name and the reason, the usage line and where the help is, and status 2. |
| `e.report -> string`, `e.exitCode -> int` | For an `ArgsError`, the whole text `parseOrExit` writes, each line ending in a newline, and the status it exits with. |
| `usage -> string`, `help -> string` | The usage line, and the whole help: usage, `about`, the arguments, then the options in the order declared, each with its help, its default where it has one, and whether it is required or may be repeated. |
| `a.flag(name) -> bool`, `a.count(name) -> int` | Whether the flag was given, and how many times: 3 for `-vvv`. |
| `a.string(name)`, `a.int(name)`, `a.real(name)` | The option's last value, else its default; the text of a choice or of a positional argument (empty where an optional one was left off). |
| `a.strings(name)`, `a.ints(name)`, `a.reals(name)` | Every value of a repeated option in order, or its default alone where none was given and it has one; every argument a `positionals` took. |
| `a.given(name) -> bool` | Whether the option or argument was on the command line, rather than taking its default. |
| `a.command -> string`, `a.commandArgs -> Args` | The command given, empty where the parser has none, and the arguments after it as the command's parser read them. |

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
`HelpRequested(text)`, `VersionRequested(text)` or `Invalid(reason, usage, program, hint)`, and an
[`Error`](/spec/errors/), so a `main` answering a `Result` may pass it on with `?` and the program
ends with `error: ` and the reason. `program` is the name of the parser that found the mistake,
`todo add` for a command's, and `hint` the line saying where its help is (empty where the program
declared both `--help` and `help` itself). A program that reads one command line after another, a
shell or a script runner, calls `parse` rather than `parseOrExit`, writes the error's `report` and
carries on:

```text
for line in io.lines()
  parser.parse(words(line)) match
    Ok(opts) -> run(opts)
    Err(e) -> if e.exitCode == 0 then print(e.report) else eprint(e.report)
```

The reasons name what the user wrote, as they wrote it:

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
| no command, to a parser with commands | ``missing command; the commands are `add`, `list`, `help` `` |
| `lst` | ``unknown command `lst`; did you mean `list`? `` |
| `frobnicate` | ``unknown command `frobnicate`; the commands are `add`, `list`, `help` `` |

**A mistake in the declarations, or in asking for a value, is the program's, and traps.** Two
options of one name or one letter, a name written with its dashes, a choice with no choices or a
default not among them, an argument after the one taking the rest or a required one after an
optional one, two commands of one name, a command whose name is not one word or starts with `-`,
and a parser given both commands and positional arguments trap where they are declared; asking
`Args` for a name never declared, or as another type than it was declared with (`a.int("out")`
for a string option), or for `commandArgs` where the parser has no commands, traps where it is
asked.

### Commands

A program of several commands gives its parser one parser for each, declaring that command's
own options and arguments. A command may have commands of its own, as `git remote add` does.

```text
val add = cli.Parser("add", "Add a task.")
  .choice("priority", ["high", "normal", "low"], short = 'p', default = "normal")
  .positionals("words", help = "the task's title", required = true)

val parser = cli.Parser("todo", "Keep a to-do list.")
  .string("file", short = 'f', help = "the list", default = "todo.txt")
  .command(add)
  .command(cli.Parser("list", "List the tasks.").flag("all", short = 'a'))

main(args: Vector[string]) -> int
  val opts = parser.parseOrExit(args)
  val file = opts.string("file")
  opts.command match
    "add" -> addTask(file, opts.commandArgs.string("priority"), opts.commandArgs.strings("words"))
    _ -> listTasks(file, opts.commandArgs.flag("all"))
```

**The first positional argument names the command, and every argument after it is the
command's.** The program's own options come before the command word, `todo --file t.txt add -p
high milk`, and the command's after it; an option of the program's after the word is unknown to
the command (`todo list --file x` is ``unknown option `--file` ``), so a command may use a name
the program does too. An option's value is never a command, so `todo --file list add` writes to
the file `list`. `--` before the word ends only the program's options; the command reads its own.

**While a command reads its arguments, its parser is named after the program and the command,**
so its usage line, its help and its mistakes say `todo add`: `todo add --help` writes

```text
Usage: todo add [options] <words>...

Add a task.

Arguments:
  <words>...                        the task's title

Options:
  -p, --priority <high|normal|low>  (default: normal)
  -h, --help                        show this help and exit
```

and `todo --help` lists the commands, with their `about`, before the options:

```text
Usage: todo [options] <command> ...

Keep a to-do list.

Commands:
  add                Add a task.
  list               List the tasks.
  help <command>     show the help of a command

Options:
  -f, --file <file>  the list (default: todo.txt)
  -h, --help         show this help and exit
```

**Every parser with commands has the command `help`,** unless the program declares a command of
that name, which then is its own: `todo help` is `todo --help`, `todo help add` is `todo add
--help`, and `todo help remote add` names a command of a command. `--version` is the program's,
before the command word.

**A command not given or not known is a mistake.** The reason for an unknown command names the
command it was most likely meant to be: the nearest within two edits (fewer than the word has
characters), or else the one command the word begins, `rem` for `remote`; otherwise it lists the
commands. `todo lst` writes to standard error, and exits with 2:

```text
todo: unknown command `lst`; did you mean `list`?
Usage: todo [options] <command> ...
Run `todo --help` for the commands.
```

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

Standard input, read as text or as bytes, standard output and error written as bytes, the error
every module that reads or writes shares, and the traits
that let one function read bytes from, or write them to, a file and a connection alike. Standard
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
  NoTerminal
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
| `NoTerminal` | The program has no controlling terminal to read from or write to. | `there is no terminal` |
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
| `inputs(paths: Vector[string]) -> Iterator[Result[(string, string), IoError]]` | The files `paths` names, or standard input when it is empty, each as its name and all its text. |
| `inputLines(paths: Vector[string]) -> Iterator[Result[(string, string), IoError]]` | The same a line at a time: an input's name and one of its lines. |

**`inputs` and `inputLines` are for a tool that reads the files it is given, or standard input.** A
path of `-` names standard input wherever it stands, and no paths at all is one input, standard
input; a name comes back as it was written, so standard input is `"-"`. An input is read when the
iterator reaches it, and one that cannot be read is an `Err` naming it in its place: the iterator
goes on, so a tool reports each failure and carries on. Standard input is read to its end, so a
second `-` finds it empty; it shares the buffer of `readLine`, `lines()` and `readAll`.

```avin
for r in io.inputLines(args)
  r match
    Ok((name, line)) -> println(s"$name:$line")
    Err(e) -> eprintln(e.message)
```

**A line ends at `\n`, and a `\r` just before it belongs to the terminator**, so input written with
either convention reads the same. A `\r` anywhere else is part of the line. The last line needs no
terminator: input ending `a\nb` has the lines `a` and `b`, and input ending `a\n` only `a`.

**The three share one buffer.** The runtime reads standard input in large blocks and keeps what it
has read ahead, so `readLine`, `lines()` and `readAll` can be mixed: each takes up where the last
stopped, and nothing read ahead is lost. Reading blocks until a line is complete or the input ends,
so a program reading a terminal sees each line as it is entered.

**Threads read standard input one at a time, and each read is whole.** A `readLine`, a step of an
iterator and `readAll` each take what they read in one step: a line goes whole to exactly one reader,
never split between two or given to both, and a reader that finds the input ended gets `None`. A
lightweight thread waiting for its turn, or for the input itself, parks and leaves its worker to the
others. An iterator over standard input takes the line (or character) `hasNext` answers for and keeps
it for `next`, so a `true` from `hasNext` is never undone by another thread; a `readLine` after
`hasNext` reads the line after the one the iterator holds.

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

**A line printed is never split, whichever threads print.** `println(x)` (with any number of
arguments) renders all its arguments and its new line first, then adds them to standard output's
buffer in one step, and `print(x)` does the same with no new line: one call, one piece of output.
Another thread's output lands before the piece or after it, never inside it, however long the piece
is and wherever standard output leads (a terminal, a pipe, a file); a flush sends the buffer's
pieces out whole and in order. Rendering is done before the step, so a value slow or large to render
holds no other thread up. Output built from several calls (`print(a)` then `println(b)`, or writes
to `StandardOutput` piece by piece) is several pieces, and another thread's may land between them.

**`flush()` sends a partial line on its way.** `flush()` in the prelude (and `StandardOutput.flush()`,
and `StandardError.flush()`, which has nothing to send) sends everything written to standard output so
far out of the buffer whether or not a line is complete, so a program that draws a frame without a
new line can show it. Nothing buffered is not an error. Reading standard input flushes standard
output first when it must wait for more, so a prompt written with `print` is on the terminal before
the program blocks for the answer.

**Tests see no input.** `avin test` gives each test an empty standard input, so a test never waits on
the terminal it was started from. `avin run` hands the program its own standard input.

**A closed standard stream is `/dev/null`.** A program started with standard input, output or error
closed (`prog <&-`) finds `/dev/null` there: the runtime opens it on each of the three that is
missing before it opens anything of its own, so none of its own descriptors takes the number. A
closed standard input reads as empty; what is written to a closed output or error goes nowhere.

### Standard streams as bytes

```text
import avin.io

// Copies standard input to standard output byte for byte.
main()
  io.copy(io.stdin(), io.stdout()) match
    Ok(n) -> eprintln(s"$n bytes")
    Err(e) -> eprintln(e.message)
```

| Function | Meaning |
| --- | --- |
| `stdin() -> StandardInputBytes` | Standard input as a [`Reader`](#reader) of bytes and a `Readiness`, with `readAvailable(max)`. |
| `stdout() -> StandardOutputBytes` | Standard output as a [`ByteWriter`](#bytewriter). |
| `stderr() -> StandardErrorBytes` | Standard error as a `ByteWriter`. |

The three answers are objects, `Sendable`, so a thread of their own can read or write: a server's
writing thread takes `stdout()` as a `W: ByteWriter & Sendable`. They are for input whose length is
counted in bytes, a `Content-Length` header most of all, and for input and output that is not text.

**Bytes and lines share one buffer.** `stdin().read(max)` takes from the buffer `readLine`,
`lines()`, `readAll` and `Source.stdin()` take from, so a program reads a header as a line and the
body after it as bytes, or the other way round, and no byte is lost or read twice. A read answers
the bytes already read ahead where there are any, else waits for one read of the input, so it may
answer fewer than `max`; it never waits for more once it has some. Byte reads take the same lock as
the text reads, one thread at a time, and a lightweight thread waiting for input parks on the
runtime's poller, leaving its worker to the others.

**The end of input is an empty array, as often as it is asked.** Standard input may be a terminal,
a pipe, a file or `/dev/null`. A read that fails answers `Err(IoError(Other(m), "read", "standard
input"))` with the platform's sentence (standard input that is a directory: `is a directory`), where
the text readers see the end. A `max` below one traps, as a connection's read does.

**Standard input can be waited on with channels, timers and connections.** `stdin()` is a
[`Readiness`](/spec/threads/#select), so `stdin().readable() -> …` is an arm of a `select` beside
channel arms and an `after`, and `Selector.readable(stdin(), key)` puts it among connections. Its
`readable()` answers at once whether a read would answer without waiting: bytes are in the shared
buffer, those a `readLine` read past its line among them, or have arrived on the descriptor, or the
input has ended or failed, or it is a file, whose read never waits; it answers `false` while
another thread is in the middle of reading it. `readAvailable(max) -> Result[Option[Array[byte]],
IoError]` reads only what is there and never waits: `Some` of what `read(max)` would answer now,
an empty array once the input has ended, or `None` where a read would wait, so a loop that is
also drawing a screen or keeping time takes what has come and goes on.

```text
import avin.io
import avin.threads.Channel
import avin.time.millis

// Echoes what is typed, a piece at a time as it comes, and says each tick, until the input ends.
run(ticks: Channel[int])
  var open = true
  while open
    select
      io.stdin().readable() -> io.stdin().readAvailable(4096) match
        Ok(Some(b)) -> if b.length == 0 then open = false else io.stdout().write(b).getOrElse(())
        Ok(None) -> ()
        Err(_) -> open = false
      ticks.receive() as Some(t) -> println(s"tick $t")
      after millis(500) -> println("idle")
```

Standard input belongs to no thread, so any thread may wait on it, where a connection may be named
in a select only by the thread it belongs to. Waiting takes no lock: a select waits while another
thread reads, and bytes that read leaves in the buffer end the wait as bytes arriving do. Several
threads may wait on it at once; each is woken, and the reads that follow share the bytes, each byte
going to one of them.

**A byte write goes out whole before it returns.** Standard output's buffer, what `print` and
`println` have written so far, is sent on first, so the bytes land after it; the write holds
standard output for its whole length, so another thread's printed line lands before it or after it,
never inside it. There is nothing to flush afterwards. A write to standard error flushes standard
output first, as `eprint` does. An empty write writes nothing and answers `Ok`.

**A write to a reader that has gone fails; it does not end the program.** A byte write to a pipe or
socket whose reader has closed answers `Err(IoError(ConnectionReset, "write", "standard output"))`
(or `"standard error"`), and every later byte write answers the same; the program goes on, however
many of its threads are writing or running. A child the program starts writes to the same output
as any program does, and is ended by its broken pipe as usual. `print`
is not changed by it: a program that only prints still ends quietly when its reader goes, as a
filter in a pipeline should.

### Secrets typed at the terminal

```text
import avin.io

main()
  io.readSecret("Password: ") match
    Ok(password) -> login(password)
    Err(e) -> eprintln(e.message)
```

| Function | Meaning |
| --- | --- |
| `readSecret(prompt: string) -> Result[string, IoError]` | One line typed at the controlling terminal with echo off, after `prompt` is written to the terminal. Bytes that are not UTF-8 become U+FFFD. |
| `readSecretBytes(prompt: string) -> Result[Array[byte], IoError]` | The same line as bytes the caller owns and can overwrite when done with them. |
| `readSecretOrLine(prompt: string) -> Result[string, IoError]` | `readSecret`, or where the program has no terminal, `prompt` written to standard error and a line read from standard input, shown as it arrives. |

**The line is read from the terminal, not from standard input.** The terminal is the one the program
was started from (`/dev/tty`), so a program whose input or output is a pipe can still ask, as `sudo`
and `ssh` do. The prompt goes to the terminal too, and what the program has written to standard
output is flushed before it. A program with no controlling terminal gets
`Err(IoError(NoTerminal, "read", "the terminal"))`; `readSecretOrLine` is the call for a program a
script may run with its password piped in.

**Echo is off and the terminal is put back exactly as it was found.** Line editing stays on, so the
user can still correct what they type; the newline the user's Enter did not echo is written once the
terminal is back. The terminal is put back on every way out: a line, the end of input, an error, a
cancelled thread's stop and a signal. Ctrl-C, a hangup, terminate or quit signal arriving during
the read puts the terminal back first and then reaches the program as it would have: the program
ends, or, where it watches the signal, sees the read fail with `Other("interrupted")` and the signal
in its watch. Typed-ahead input is discarded when the read starts, so nothing typed before the prompt
is taken for the secret. A stop from the terminal (Ctrl-Z) puts the terminal back before the program
stops, so a stopped program leaves the user's shell with echo on; when it is continued, echo goes off
again, the prompt is written again and the read goes on; what was typed before the Ctrl-Z is
discarded by the terminal, as it is for any signal character.

**The line has no terminator.** A trailing `\n` and a `\r` before it are dropped. A line ended by the
terminal's end-of-file character counts if it has bytes in it; the end of input before any byte is
`Err(IoError(Other("end of input"), _, _))`. An empty line is an empty secret. A line is at most what
the terminal itself keeps of one: 1023 bytes on macOS, 4095 on Linux.

**It never runs at the same time as a read of standard input, or another secret.** The read takes
the lock every reader of standard input takes: a thread that asks waits for a reader of either, and
the other way round. A thread cancelled while it waits for the line is stopped once the line has
been read, as one reading standard input is. A secret asked for with no way to clear it is still a
`string`; `readSecretBytes` is the form to overwrite, and the runtime clears its own copies.

### `Reader`

A `Reader` is bytes read from somewhere a piece at a time, so one function can read a file, a
connection, or a source the program writes itself, holding no more of the input than it asks for:

```text
trait Reader
  read(max: int) -> Result[Array[byte], IoError]
```

`read(max)` gives up to `max` bytes, `max` being at least one, waiting for at least one where the
source is still going, and an empty array once it has ended. A stream's read allocates the bytes
it answers, not `max`, so a small message read with a large `max` costs what the message does.
`fs.File` is a reader by its own
`read`, and so is every `net.ByteStream`: `TcpStream`, `UnixStream` and `TlsStream`; a class of the program's own is one by
naming `io.Reader` (or `Reader`, imported) after `:` and overriding `read`. The
JSON package's `JsonReader` ([`dev.avinlang.json`](/spec/json/)) reads JSON from any reader.

### `ByteWriter`

A `ByteWriter` is somewhere bytes are written, so one function can write to a file, a connection,
a streamed HTTP body, or a sink the program makes itself:

```text
trait ByteWriter
  write(bytes: Array[byte]) -> Result[unit, IoError]
```

`write(bytes)` writes all of `bytes` or answers why it could not; an empty array writes nothing.
`fs.File` is a byte writer by its own `write`, and so are every `net.ByteStream` (`TcpStream`,
`UnixStream` and `TlsStream`), the `StreamWriter` half of a [split connection](#split-connections)
and a streamed response's `BodyWriter`; a write to any of them once it is closed is an `Err`. A
class of the program's own is one by naming `io.ByteWriter` after `:` and overriding `write`, and a
generic function takes any of them as `[W: ByteWriter]`.

It writes bytes, and the prelude's [`Writer`](/spec/strings-and-formatting/) is where values render
as text; the two names differ, so a program sees both unqualified and one class may be both, its two
`write`s told apart by what they take. A byte writer is not a `Close`, as a `Reader` is not: a
streamed body is ended by its response, not closed, and code holding a resource that is both bounds
it by both. Nor does it flush: a file's `flush()` is the file's own, and whoever opened the file
closes it, which flushes it.

`io.copy(from: Reader, to: ByteWriter) -> Result[long, IoError]` reads `from` to its end and writes
everything it gives to `to`, a piece of at most 64 KiB at a time, so a copy of any length holds no
more than one piece. It answers how many bytes it copied, or the first read's or write's `Err`, after
which nothing more is read or written. It neither closes, flushes nor rewinds either side:

```text
upload(name: string, to: TcpStream) -> Result[long, IoError]
  val f = fs.open(name)?
  defer f.close()
  io.copy(f, to)
```

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
| `write(bytes)`, `writeText(text)` | Writes at the position, buffered. A file is an [`io.ByteWriter`](#bytewriter) by `write`, and an [`io.Reader`](#reader) by `read`. |
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
rest, so a key, a token or a password is never taken from it. Those come from `SecureRandom`.

### `SecureRandom`

```text
import avin.random.SecureRandom

main()
  val session = SecureRandom.urlToken()
  val key = SecureRandom.nextBytes(32)
  println(SecureRandom.between(0, 10000))
```

**`SecureRandom` is random numbers fit for secrets.** Every draw is read from the operating system's
cryptographic generator (`getentropy`, on macOS, Linux and the BSDs), so no number of outputs says
anything about the next. It is an object, not a class: there is no seed, nothing to replay, and no
state, so any thread may call it at once. Where the operating system gives no bytes, the draw traps,
since nothing could stand in for them.

| Function | Meaning |
| --- | --- |
| `nextBytes(n: int) -> Array[byte]` | A new array of `n` bytes, any count, past the system's 256-byte request limit included; a negative `n` traps. |
| `hexToken(byteCount: int = 32) -> string` | `byteCount` random bytes as lower-case hexadecimal, two digits a byte: 64 characters by default. |
| `urlToken(byteCount: int = 32) -> string` | `byteCount` random bytes as unpadded base64url, which a URL, a header, a cookie and a file name carry as it is: 43 characters by default. |
| `nextInt()`, `nextLong()`, `nextInt(bound)`, `nextLong(bound)`, `between(min, max)` for `int` and `long`, `nextReal()`, `nextBoolean()`, `alphanumeric()`, `shuffle(xs)`, `choose(items)` | As `Random`'s, with the same bounds, the same traps and no bias. |

32 bytes is 256 bits, beyond any guessing. `SecureRandom` has no `nextGaussian`, `chooseWeighted` or
`setSeed`: no secret is drawn from those distributions, and there is no seed to set.

## `avin.crypto`

Content digests and keyed digests: SHA-256, SHA-512, SHA-1 and MD5, HMAC over the first two,
PBKDF2, comparison of secrets in constant time, SASLprep for passwords, and hexadecimal text for any
digest.

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
| `pbkdf2HmacSha256(password, salt, iterations, length)`, `pbkdf2HmacSha512(...)` | `length` bytes of key (RFC 8018 PBKDF2 over HMAC-SHA-256 or -SHA-512) as a `Vector[byte]`; `password` and `salt` both `Vector[byte]` or both `string` (UTF-8), either empty. `iterations` below 1, `length` below 1, or `length` over 8160 (SHA-256) or 16320 (SHA-512) trap. |
| `constantTimeEquals(a, b) -> bool` | Whether `a` and `b`, both `Vector[byte]` or both `string` (compared as UTF-8 bytes), are the same, taking as long wherever they first differ. Different lengths answer `false` at once. |
| `toHex(bytes) -> string` | Lower-case hexadecimal, two digits for each byte: [`avin.encoding`](#avinencoding)'s, which this module offers too. |

**A hasher takes its input in pieces**, for data too large to hold whole, such as a file read in
chunks. `Sha256()`, `Sha512()`, `Sha1()` and `Md5()` each have `update(bytes)` and `update(text)`,
any number of times, and `finish()`, the digest of everything added. `finish()` answers the same
digest each time it is called, and `update` after `finish` traps. Hashing in any pieces gives the
one-shot digest. A hasher belongs to the thread that made it, as a handle does.

**`sha1` and `md5` are for formats that already use them**: a checksum in a listing, a cache key
nobody attacks. Two different inputs with the same digest can be made for both, so neither signs,
authenticates or names untrusted content; use `sha256` or `sha512` for those. HMAC does not rest on
collision resistance, but only the SHA-2 variants are offered.

**A secret someone sent is compared with `constantTimeEquals`, never with `==`.** `==` stops at the
first byte that differs, so how long a rejection takes tells a patient attacker how much of a guessed
MAC tag, bearer token or password digest was right, and lets the guess be built a byte at a time.
`constantTimeEquals` reads every byte and decides once, at the end. A length is not the secret, so
different lengths answer at once. For two digests of public data, `==` is fine.

```text
val expected = hmacSha256(key, body)
val ok = fromBase64(header) match
  Ok(sent) -> constantTimeEquals(sent, expected)
  Err(_) -> false
```

**The iteration count belongs to the protocol**: SCRAM-SHA-256 (PostgreSQL, MongoDB) sends it, and a
client using its own derives a different key. It buys the cost of guessing a password and nothing
else.

```text
val salted = pbkdf2HmacSha256(password, salt, iterations, 32)
val clientKey = hmacSha256(salted, "Client Key")
```

**SCRAM prepares the password with SASLprep before deriving its key**, so that the same password
typed on two machines, as two sequences of characters, gives one key. `saslPrep(text) ->
Option[string]` is RFC 4013's profile of stringprep (RFC 3454): each space other than U+0020
becomes U+0020 and the characters mapped to nothing (the soft hyphen, the zero-width joiners, the
variation selectors) are removed; the result is put in normalization form
[NFKC](/spec/types/#normalization); and it is `None` where that holds a prohibited character
(controls, private use, non-characters, the characters that change the display direction, tags),
a code point unassigned in Unicode 3.2 (the RFC's version, as for a stored string), or right-to-left
text that also holds a left-to-right character or does not begin and end with a right-to-left one.
The tables are the RFC's, held as Unicode 3.2 fixed them; normalization is by the current Unicode
Character Database, which differs from 3.2's only for a few CJK compatibility ideographs Unicode
has corrected since.

```text run
import avin.crypto.saslPrep

main()
  println(saslPrep("I\u{ad}X")) // Some(IX)
  println(saslPrep("\u{2168}")) // Some(IX)
  println(saslPrep("pass\u{a0}word")) // Some(pass word)
  println(saslPrep("\u{7}")) // None
  println(saslPrep("\u{627}1")) // None
```

```text output
Some(IX)
Some(IX)
Some(pass word)
None
None
```

PostgreSQL's client falls back to the password as it was written where SASLprep refuses it, and so
may a client written with this: `saslPrep(password).getOrElse(password)`.

The digests, the key derivation and the comparison are bound to Sysl's `sysl.crypto`, so they are the
same code Sysl programs use.

## `avin.uuid`

Universally unique identifiers (RFC 9562), 128 bits written as 32 hexadecimal digits in groups of
8-4-4-4-12.

```text
import avin.uuid.Uuid

main()
  val id = Uuid.random()
  println(id)
  assertEq(Uuid.parse(id.toString()), Some(id))
```

`Uuid` is a value: it compares and hashes by its bits, orders by them as sixteen unsigned bytes
(the first most significant), is a `Map` key as it stands, and is `Sendable`.

| Member | Meaning |
| --- | --- |
| `Uuid.random()` | A version 4 identifier from `SecureRandom`, with the version and variant bits set. |
| `Uuid.parse(text) -> Option[Uuid]` | The canonical 8-4-4-4-12 form in either case; `None` for anything else (braces, a `urn:uuid:` prefix, missing hyphens and white space are not accepted). |
| `Uuid.fromBytes(bytes) -> Option[Uuid]` | Sixteen bytes, the first most significant; `None` for any other length. |
| `Uuid.Nil`, `Uuid.Max` | All zero bits and all one bits. |
| `bytes()`, `toString()` | The sixteen bytes, and the lower-case canonical text. |
| `version()`, `variant()` | The version (0 to 15), and the variant: 2 for RFC 9562's layout, 0, 6 and 7 for the reserved ones. |

A `Uuid` implements `FromParam` in [`avin.net.http`](#avinnethttp), so `paramAs[Uuid]` and
`queryAs[Uuid]` read one from a route or query parameter, an invalid one answering `400`.

## `avin.diff`

The differences between two sequences, found with Myers' O(ND) algorithm in its linear-space form
(the search runs from both ends until the paths meet in a middle snake, which splits the problem in
two), so the memory used is proportional to the sizes of the inputs and the time to their sizes times
the number D of differences. The script is a shortest one; a run of changes lists its deletes before
its inserts, as `diff -u` prints them.

```text
import avin.diff.{diff, apply, unifiedDiff}

main()
  val edits = diff([1, 2, 3, 4], [1, 3, 4, 5])
  assertEq(apply([1, 2, 3, 4], edits), [1, 3, 4, 5])
  print(unifiedDiff("a\nb\n", "a\nc\n", "old.txt", "new.txt"))
```

| Member | Meaning |
| --- | --- |
| `Edit[T]` | `Keep(value)`, `Insert(value)` or `Delete(value)`; a `Keep` holds the element of the first sequence. |
| `diff[T: Eq](a, b) -> Vector[Edit[T]]` | The shortest script turning `a` into `b`. |
| `apply(a, edits) -> Vector[T]` | The sequence the edits make of `a`; traps, naming the edit, where a `Keep` or `Delete` does not match the next element of `a`, or where `a` is not used up. |
| `changes(edits) -> int` | The number of inserts and deletes. |
| `Line(text, terminated)` | A line without its `\n`, and whether one ended it; only a text's last line can be unterminated. A `\r` stays in the line. |
| `splitLines(text)`, `joinLines(lines)` | Text to lines and back; `joinLines(splitLines(t))` is `t`. |
| `LineOptions(ignoreTrailingSpace = false, finalNewline = true)` | Ignore white space (a `\r` too) at the ends of lines; with `finalNewline` a last line with no `\n` differs from the same line with one, as in `diff`. |
| `diffLines(a, b, options) -> Vector[Edit[Line]]` | The shortest script over the lines of two texts. |
| `Hunk(aStart, aCount, bStart, bCount, edits)` | A hunk, the starts 0-based. |
| `hunks(edits, context) -> Vector[Hunk[T]]` | Each run of changes with `context` unchanged elements on either side; runs with no more than `2 * context` unchanged elements between them share a hunk. Traps on a negative `context`. |
| `renderUnified(edits, fromName, toName, context = 3) -> string` | The edits of lines as `diff -u` writes them: `---` and `+++` headers, `@@ -a,b +c,d @@` (a count of 1 left out, an empty range named by the line before it), ` `, `-` and `+` lines and `\ No newline at end of file`; empty when there are no changes. |
| `unifiedDiff(a, b, fromName, toName, context = 3, options)` | `renderUnified(diffLines(a, b, options), …)`, byte for byte `diff -u`'s output (with `--label`) for files holding the texts. |

## `avin.encoding`

Bytes written as text and read back, as RFC 4648 defines them: hexadecimal, base64 with its `=`
padding, and base64url, the URL- and filename-safe alphabet written without padding.

```text
import avin.encoding.{toBase64, fromBase64, toBase64Url, fromHex}

main()
  println(toBase64("hello"))
  println(toBase64Url("hello"))
  println(fromBase64("aGVsbG8="))
  println(fromHex("68656c6c6f"))
```

| Function | Meaning |
| --- | --- |
| `toHex(x) -> string` | `x`, a `Vector[byte]` or the UTF-8 bytes of a `string`, as lower-case hexadecimal, two digits a byte. |
| `fromHex(text) -> Result[Vector[byte], DecodeError]` | The bytes the digits spell, two a byte, upper or lower case. |
| `toBase64(x) -> string` | `x` in base64: `A`–`Z`, `a`–`z`, `0`–`9`, `+` and `/`, padded with `=` to a multiple of four characters. |
| `fromBase64(text) -> Result[Vector[byte], DecodeError]` | The bytes padded, standard base64 spells. |
| `toBase64Url(x) -> string` | `x` in base64url: `-` and `_` in place of `+` and `/`, and no padding. |
| `fromBase64Url(text) -> Result[Vector[byte], DecodeError]` | The bytes unpadded base64url spells. |

Encoding never fails, and the empty input encodes as the empty text and decodes back from it.

**Decoding is strict, so each byte sequence has exactly one spelling.** A decoder refuses whitespace
and line breaks, the other alphabet's characters, a missing, extra or misplaced `=`, any `=` in
base64url, and a last digit with bits set below the last whole byte, which an encoder always leaves
zero (`Zh==` is refused; `Zg==` is `f`). Two different texts therefore never decode to the same
bytes, which matters for a token or a signature carried as text. Text wrapped across lines has its
line breaks taken out before it is decoded.

**A refusal is a `DecodeError`**, an `Error` whose `at` is a byte offset in the text. The checks run
in order, the characters first, then the padding, the length and the unused bits, so the offset
named is of the first thing wrong.

| Case | Meaning |
| --- | --- |
| `BadCharacter(at)` | A character the encoding does not use. |
| `BadPadding(at)` | An `=` where padding cannot stand: before the last two characters, followed by a character that is not `=`, more than two of them, or any in base64url. |
| `Truncated(at)` | The text ends inside a unit: an odd number of hexadecimal digits, base64 whose length is not a multiple of four, base64url one character past one. `at` is the text's length. |
| `UnusedBits(at)` | The last digit of the data sets bits no bytes would set. |

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
| `s.timeout -> Duration` | The limit `setTimeout` set, in whole milliseconds, a part of one counting as one; zero where there is none. |
| `s.shutdownOutput() -> Result[unit, IoError]` | Tells the other end nothing more will be sent; reading goes on. |
| `s.close() -> Result[unit, IoError]` | Ends the connection and answers `Ok`; closing again does nothing. Every connection and listener is an [`io.Close`](#close). |
| `s.readable() -> bool` | Whether a read would answer now, without waiting: bytes a `readLine` read ahead are still to be read, bytes have arrived, the other end has finished sending or reset the connection, or the connection is closed. Never waits. A [`select`](/spec/threads/#select)'s `s.readable()` arm waits for it. |
| `s.readinessDescriptor() -> int` | The socket's descriptor, which that arm watches on the runtime's poller; `-1` once closed. |
| `s.writeSome(bytes: Array[byte]) -> Result[int, IoError]` | Sends what of `bytes` goes without waiting, after waiting for room for the first byte where there is none, and answers how many: at least one, unless `bytes` is empty. After `writable()` answered `true` it does not wait. |
| `s.writable() -> bool` | Whether a write would send something now, without waiting: the socket has room, or the connection has failed or is closed. Never waits. A [`select`](/spec/threads/#select)'s `s.writable()` arm waits for it. |
| `s.writabilityDescriptor() -> int` | The descriptor that arm watches; `-1` once closed. |
| `s.split() -> (StreamReader, StreamWriter)` | The connection divided into a reading half and a writing half, for two threads to use at once; see [split connections](#split-connections). |
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
(`read`, `readLine`, `readAll`, `write`, `writeSome`, `writeText`, `setTimeout`, `shutdownOutput`, `close`, `readable`, `readinessDescriptor`, `writable`, `writabilityDescriptor`, `split`) with the
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

**Looking up a host's name waits the same way, and a cancellation ends it.** The system's resolver
cannot be stopped halfway, so the runtime makes each lookup on a helper thread of its own, and the
`connect` or `bind` that asked waits for the answer as it waits on a socket: a lightweight thread
parks, `main` waits safe for the collector, and a [cancellation](/spec/threads/#cancellation) or a
`withTimeout`'s deadline stops the thread at once, so a connect's time limit covers the lookup too.
The lookup it abandoned runs on to the resolver's answer, which is thrown away. At most 64 helpers
run at once, which bounds how many abandoned lookups can still be running; a lookup asked for while
all 64 are busy waits for one to come free, and that wait too is ended by a cancellation. A numeric
address (`127.0.0.1`, `::1`) asks the network nothing and is resolved where it is asked.

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
`write`, `writeSome`, `writeText`, `setTimeout`, `shutdownOutput`, `close`, `readable`,
`readinessDescriptor`, `writable`, `writabilityDescriptor` and `split`, which `TcpStream`,
`UnixStream` and `TlsStream` implement, so code that speaks
a protocol, as the HTTP client and server do, takes a `ByteStream` and runs over any of them. It is
[sendable](/spec/actors/#what-may-be-sent), and it is an [`io.Reader`](#reader) by its `read`, so
whatever reads a `Reader` reads any connection, a TLS one included, and an
[`io.ByteWriter`](#bytewriter) by its `write`, so whatever writes to a `ByteWriter` writes to any
connection. It is a `threads.Readiness` by
`readable` and `readinessDescriptor`, so a [`select`](/spec/threads/#select) waits on any connection
beside its channels: the thread that owns a connection waits in one select for its peer's bytes
and for what other threads send it, and does every read and write itself. It is a
`threads.Writability` by `writable` and `writabilityDescriptor`, so a select's `writable()` arm waits
for room as a `readable()` arm waits for bytes. `Readiness` and `Writability` are traits of
their own, for a source or sink that is not a connection: its descriptor may be `-1`, and a select
then finds it ready only when it tries its arms.

#### Split connections

**`split()` divides a connection into a `StreamReader` and a `StreamWriter`**, each sendable and
each its own thread's, so one thread sends while another receives over the same connection. A
connection used by one thread at a time must stop writing to read: two ends that each send more
than the buffers between them hold before they read would both wait for ever. Split, the writer
waits for room and the reader for bytes at the same time, each parking only its own lightweight
thread on the poller, both on the connection's one descriptor.

```text
import avin.io.IoError
import avin.net.{StreamWriter, TcpStream}
import avin.threads.spawn

sendAll(w: StreamWriter, queries: Vector[string]) -> Result[unit, IoError]
  for q in queries do w.writeText(q + "\n")?
  w.close()

// Sends every query and reads every answer at once, so a long batch never waits for itself.
pipeline(c: TcpStream, queries: Vector[string]) -> Result[Vector[string], IoError]
  val (r, w) = c.split()
  val sending = spawn(() -> sendAll(w, queries))
  val answers = Buffer[string]()
  while answers.length < queries.length
    answers.addOne(r.readLine()?.getOrElse(""))
  sending.join()?
  r.close()?
  Ok(answers.toVector)
```

- **The halves.** A `StreamReader` is an `io.Reader`, an `io.Close` and a `threads.Readiness`, with
  `readLine`, `readAll`, `setTimeout` and `remoteAddress`; a `StreamWriter` is an `io.ByteWriter`,
  an `io.Close` and a `threads.Writability`, with `write`, `writeSome`, `writeText`, `setTimeout` and `remoteAddress`.
  Bytes the connection had read ahead go to the reader, and each half starts with the connection's
  timeout, then keeps its own.
- **Closing.** Closing the writer tells the other end nothing more will be sent, as
  `shutdownOutput` does; the reader reads on, and the other end reads the end of its input. Closing
  the reader stops its reading and leaves the writer sending. The connection closes once both halves
  are closed, or once both are unreachable. A closed half answers an error to every use but closing
  again, which does nothing and answers `Ok`; a TCP or Unix writer's close answers `Ok` even where
  the other end has gone, and a TLS writer's answers whether TLS's close notice went out. There is
  no way to join the halves again.
- **The connection split.** It is used up: every operation on it answers an error saying it has
  been split, it is readable and writable so that a select's arm runs and finds that out, its
  descriptors are `-1`, and closing it does nothing. Splitting it again, or splitting a closed
  connection, traps.
- **Each half belongs to one thread at a time**, as the connection did, its traps naming
  `StreamReader` or `StreamWriter`. After the peer resets the connection, each half answers
  `ConnectionReset`, or the reader the end of its input, at every use, and a parked half's wait ends
  with it. A half's thread may be cancelled while it waits, and the other half works on.
- **TLS.** The halves of a `TlsStream` share one TLS session. Each reaches it only under a lock that
  nothing holds while it waits for the connection, so the reader decrypts records as they arrive
  while the writer waits for room; what TLS has to send goes out in order, a record whole, whichever
  half made it, and a reader never waits for room to send anything of its own while the writer is
  sending. A failure to send is the writer's to answer whichever half was sending: the reader reads
  on, and the writer answers it at every use after, its close included, so a close notice the reader
  tried and failed to send is one that did not go out. A cancellation that stops a half inside an
  operation lets the locks go.

A `writeSome` hands a `TlsStream`, or its writer, at most one TLS record of plaintext. A
`TlsStream` sends what of the record the connection has room for; the rest goes before anything
else is sent, and `writable()` sends it as room comes and answers `true` once it has gone. So after
`writable()` answered `true`, its `writeSome` does not wait. A read of a `TlsStream` that has to
wait for input while such a rest is still to go waits for input or room, whichever comes first,
sending the rest as room comes, so a caller that only reads after its last `writeSome` still has
the whole record sent, and its peer's answer, given once that record has come, still arrives;
waiting for both, the read takes input while the peer is itself waiting to send. Its wait has the
connection's time limit, and a rest it did not send is kept for the next read or write. The writing half of a split one sends
the whole record before it answers, waiting for room for the rest where there is too little, so what
it answers has gone even where the writer writes nothing after it; its reading half reads
meanwhile, so the wait never holds up the peer's answers. Where the writer's time limit runs out
first, it still answers the plaintext TLS took, and the rest goes before anything else, `writable()`
sending it as room comes. Ciphertext a wait that failed did not send is kept, in order, to go first,
never dropped. `ReadAhead`, the bytes a connection has read
beyond what its caller asked for and the line reading over them, is public for a connection of
one's own: `pending` says whether any are still to be read, and `keep(bytes)` adds bytes that
arrived before anything asked for them.

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
| `s.readable() -> bool`, `s.readinessDescriptor() -> int` | Whether a read of plaintext would answer now: plaintext is buffered or TLS holds some decrypted, a whole record of it has arrived, or the peer has closed TLS or the connection. It hands TLS the ciphertext already on the socket, so part of a record is not taken for plaintext, and never waits for more; a failure it meets is answered by the next read. The descriptor is the TCP connection's. |
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
| `s.stopped() -> Channel[unit]` | A channel closed once the server is asked to stop and never sent on, so a `select` waiting for a connection's bytes or other work ends at once with an arm `s.stopped().receive()`, with no thread of its own. |
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

`type Handler = @sendable (ServerRequest) -> Response` names what answers a request.
`http.serve(host, port, handler: Handler, maxConnections = 1024,
readTimeout = seconds(30), writeTimeout = seconds(30), idleTimeout = seconds(5), maxHeaderBytes =
16384, maxBodyBytes = 1048576, tls: Option[TlsServer] = None, errors: ErrorPage = plainError) ->
Result[TcpServer, IoError]` listens as `TcpServer.start` does. Each
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
target)` traps on any other, so a handler can be tried without a connection. A request also carries
the values a middleware handed on, read by `value[T]()` (below).

A `Response` is the client's: `Response.text(status, text)` (`text/plain; charset=utf-8`),
`Response.html(status, text)` (`text/html; charset=utf-8`, the text sent as given),
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

**Typed parameters.** `paramAs[T](name)` reads a route parameter as a `T` and `queryAs[T](name)` a
query parameter, its first where it is given more than once, each answering `Result[T,
ParamError]`: `Missing(place, name)` where the query has none of that name, `Invalid(place, name,
text, kind)` where its text does not read as a `T` (`paramAs` traps on a name the route lacks, as
`param` does). `queryAs(name, default)` answers `default` for a missing one and `queryOption[T](name)`
`None`. A `T` is any `FromParam`, a trait of two static methods, `parse(text) -> Option[Self]` and
`kind()`, the words naming it in an error ("an int"); `string`, `int`, `long`, `real` and `bool`
implement it, and a program's own type may. A `ParamError`'s `message` is the sentence for the
client, as in ``the query parameter `n` is `x`, which is not an int``.

**Handlers that fail.** `ToResponse` is a failure a handler can answer with, its `toResponse()` the
response telling the client. `Response` implements it as itself and `ParamError` as a `400` with its
message; a program's own error type may too. `fromResult(answer)` makes a `Handler` of a function
answering `Result[Response, ToResponse]`, an `Err` answered with its failure's response, and each of
`Route.get`, `post`, `put`, `delete` and `patch` has an overload taking such a function, so `?` ends a
handler early:

```text
half(r: ServerRequest) -> Result[Response, ToResponse]
  val n = r.paramAs[int]("n")?
  if n % 2 != 0 then return Err(Response.text(422, "odd\n"))
  Ok(Response.text(200, s"${n / 2}\n"))

val route = Route.get("/half/:n", half)
```

**Groups and middleware.** `type Handler = @sendable (ServerRequest) -> Response`; a middleware is a
`(Handler) -> Handler`. `route.wrap(middleware)` passes a route's handler through one, wrapping again
putting the newer outside, so it runs first; `route.under(prefix)` puts a route's pattern after
`prefix`, a pattern of `/` becoming the prefix itself. `Route.group(prefix, routes, middleware = h ->
h)` answers each of `routes` under `prefix` and wrapped by `middleware`, and groups nest, an inner
group's middleware running inside the outer's. A prefix begins with `/`, does not end with one and
may bind parameters as a pattern does; one that does not hold traps.

**Values a middleware hands on.** `request.withValue(value)` answers the request carrying `value`,
and `request.value[T]()` answers `Some` of the value the request carries as its `T`, `None` where it
carries none; both take a `T: Sendable`, since a request may be answered on any thread. A request is
a value: `withValue` answers a new one and leaves the request it was called on as it was. The type is
the key, compared exactly, as a type pattern compares a generic type's arguments: handing on a
second value of one type replaces the first, a value handed on as an `int` is no `long`, one handed
on as a trait's type is read as that trait and not as its class, and `Vector[int]` is not
`Vector[string]`. Two values of one type are handed on as two types of the program's own, a
`data RequestId(value: string)` beside a `data TraceId(value: string)`. The values go wherever the
request goes: through a router's binding of parameters, groups, nested middleware (an inner one
replacing what an outer one handed on, if it hands on the same type) and the streamed and
event-stream handlers, whose `write` reads the request it was made from. An authenticating
middleware hands its user to the handlers inside it:

```text
import avin.net.http
import avin.net.http.{Handler, Response, Route, Router, ServerRequest}

data User(name: string)

// Answers `401` without a token, and hands the token's user to the handler with one.
auth(next: Handler) -> Handler = r -> r.header("Authorization") match
  Some(token) -> next(r.withValue(User(token)))
  None -> Response.text(401, "who are you?")

me(r: ServerRequest) -> Response = r.value[User]() match
  Some(u) -> Response.text(200, s"hello, ${u.name}\n")
  None -> Response.text(500, "the route is outside `auth`\n")

main()
  val router = Router(Route.group("/api", [Route.get("/me", me)], auth) :+ Route.get("/me", me))
  http.serve("0.0.0.0", 8080, router.handle).ok.get.join()
```

`GET /api/me` with an `Authorization` header is answered `hello` and the token; `/me`, outside the
group, finds no `User`.

**Error pages.** `type ErrorPage = @sendable (int) -> Response` renders the responses a router and a
server make themselves for an error status: `Router(routes, errors)` its `404` and `405`, and
`http.serve(…, errors)` every status of the table below. The status sent is the one given, whatever
the page answered, and a `405` still carries its `Allow` header. `http.plainError`, the default, answers the
status's reason phrase as plain text.

**What a middleware sees of a request.** `request.remote: Option[string]` is the client's IP address,
written numerically without its port (`127.0.0.1`, `::1`), over TCP and over TLS alike; `None` for a
request made without a connection. `request.route: Option[string]` is the whole pattern of the route
a router chose (`/api/users/:id` under its groups; a `GET` route's for a `HEAD` it answers), `None`
until one has, so a middleware wrapped around a whole router's `handle` sees `None`.
`request.withHeaderReplaced(name, value)` is the request with every header of `name`, any case,
replaced by one, added last, a name that is not a token or a value holding a line break trapping. A
`ByteStream`'s `remoteAddress` is where `remote` comes from: the other end's address for a
`TcpStream`, accepted or connected, and a `TlsStream`, and `None` for a `UnixStream`.

**Access log.** `http.accessLog(sink = standardError, format = PlainLines, clock = monotonic, ids =
newRequestId)` is a middleware writing one line to `sink`, a `@sendable (string) -> unit` called once
a line with no line end, for each request once its handler has answered:

```text
GET /items/7 200 12 1.5ms id=4f1c…e2 remote=127.0.0.1
{"id":"4f1c…e2","method":"GET","path":"/items/7","status":200,"bytes":12,"micros":1500,"remote":"127.0.0.1"}
```

The first is `PlainLines`, the second `JsonLines`. The path is the decoded one, its spaces, quotes,
backslashes and control characters written `%XX` in a plain line and escaped as JSON in a JSON one;
`bytes` is the body's length, or a streamed body's length said beforehand, `-` or `null` where it is
not known, as is an address. The request id is the client's `X-Request-Id` where
`http.isRequestId` takes it (1 to 128 ASCII letters, digits and `-._:+/=@`), and a new one from `ids`
otherwise, 32 hexadecimal digits from `SecureRandom` by default; the handler reads it in the request's
`X-Request-Id`, which replaces any the client sent, and the response carries it back unless the
handler set its own. The default sink writes each line to standard error in one write, so lines from
several connections never interleave. The time is read from `clock`, a `@sendable () -> Duration`,
before and after the handler. A handler that traps writes no line.

**Metrics.** `Metrics(buckets = defaultBuckets, clock = monotonic)` counts the requests its
`middleware` passes on, shared by every connection's actor under a lock of its own:
`http_requests_total` by `method`, `route` (the route's pattern, `""` where none was chosen) and
`status` class (`2xx`); `http_request_duration_seconds`, a histogram of `buckets` (5 ms to 10 s by
default), a time equal to a bound counted in that bound's bucket, with its `_sum` and `_count`; and
the gauge `http_requests_in_flight`. `render` is the Prometheus text format, series ordered by route,
method and status class; `handler` answers it as `200` of the type `MetricsContentType`
(`text/plain; version=0.0.4; charset=utf-8`), and `route(pattern = "/metrics")` mounts it.
`observe(method, route, status, took)` counts a request answered another way, and `inFlight` is the
gauge's value. Buckets not in increasing order, or one not above zero, trap.

```text
val metrics = Metrics()
val router = Router(Route.group("/api", routes, metrics.middleware) :+ metrics.route())
```

Wrapping a group or a route labels each request by its route; wrapping a router's `handle` counts the
`404`s and `405`s too, every request labelled `""`. A handler that traps is not counted, and leaves the
gauge one higher.

**Rate limiting.** `RateLimiter(rate, per, burst = rate, key = clientAddress, maxKeys = 10000, clock =
monotonic, errors = plainError)` gives each key a token bucket of `burst` tokens filled again at
`rate` every `per`. Its `middleware` takes a token for each request, keyed by `key` (the client's
address by default, `""` for a request with none), and answers one finding its bucket empty `429`,
rendered by `errors`, with `Retry-After` the seconds until the next token, rounded up and at least
one. `admit(key) -> Result[unit, Duration]` takes a token directly, `Err` saying how long until the
next. At most `maxKeys` buckets are held (`size` says how many): making one more forgets every bucket
full again, which a new one would equal, and where none is, the one nearest to full. A rate, burst
or `maxKeys` below one traps, and so does a `per` too short to give each token a whole microsecond.
The three middlewares compose as any do: `h -> metrics.middleware(limiter.middleware(h))` counts the
`429`s the limiter answers.

**Streamed responses.** `Response.stream(status, contentType, write)` answers a response whose body
`write` writes as it goes, through the `BodyWriter` it is given, each write sent to the client at
once; its `body` is empty. Its head says `Transfer-Encoding: chunked` and each write is a chunk, the
body ended by the empty one once `write` returns; to an HTTP/1.0 client it says neither, and the
body ends with the connection. `Response.stream(status, contentType, length, write)` says
`Content-Length: length` beforehand instead: a write past it traps, a negative length traps, and a
body ending short closes the connection. `BodyWriter.write(bytes)` and `write(text)` answer
`Result[unit, IoError]`: `Err` once the client has gone or the server is stopping, and for every
write after one that failed, so the code writing a stream ends there; a failed write closes the
connection. A `BodyWriter` is an [`io.ByteWriter`](#bytewriter) by `write(bytes)`, so
`io.copy(reader, w)` streams a file or any other `Reader` into a body. No body is written for
`HEAD`, `204` or `304`, and `write` is not run.

`Response.events(write)` is a stream of server-sent events, `200` of the type `text/event-stream`,
written through an `EventWriter`: `send(text, event = None, id = None)` sends an event, each line of
`text` (split at `\r\n`, `\r` or `\n`) a `data:` line after its `event:` and `id:` lines, a type or an
id holding a line break trapping; `comment(text)` sends a comment, which a client passes over; and
`retry(delay)` tells the client how long to wait before reconnecting. `Response.events(keepAlive,
write)` sets how often a comment is sent while `untilStopped` waits, fifteen seconds by default and
none for zero, so a client that has gone is noticed and one still there keeps the stream open.

**Waiting in a handler.** `request.isStopping()` says whether the server that read the request is
stopping, and `request.untilStopped(wait)` runs `wait`, a `@sendable () -> T`, on a thread of its own
and answers `Some` of its result, or `None` where the server stops first, cancelling `wait` wherever
it waits as `TcpServer.idle` does: a long poll answers at once rather than holding the shutdown. Where
the server is already stopping `wait` is not run. `BodyWriter` and `EventWriter` have both too, the
`EventWriter`'s sending its keep-alive comments while it waits and answering `None` once one cannot
be sent. A request made without a server never stops, and `wait` is then run as it is.

**The connection.** Requests are read one after another, so several are kept alive on one
connection and pipelined ones are answered in order: in HTTP/1.1 unless either side says
`Connection: close`, in HTTP/1.0 only on `Connection: keep-alive`, which the response then repeats.
Empty lines before a request line are passed over, and a line may end in a bare `\n`. A body is read
by `Content-Length` or chunked (extensions and trailers passed over); an `Expect: 100-continue` is
answered `100 Continue` once the body is known to fit. The server writes `Content-Length` and
`Connection` itself, in place of any the handler gave, every response as `HTTP/1.1`, and no body for
`HEAD`, `204` and `304`; a streamed response's framing is its own, as above. A handler's status
outside 200 to 599, or a header the response cannot carry, traps.

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
| `500` | The handler traps: it fails its connection alone, as a `TcpServer`'s `serve` does (over TLS, or once a streamed response's head is sent, the connection is closed unanswered) |
| `501` | A transfer coding besides `chunked` |
| `503` | A request read once the server is stopping |
| `505` | A version other than 1.0 and 1.1 |

A connection waiting for a first request waits `readTimeout`, and for a later one `idleTimeout`,
then is closed without an answer. **Shutting down**, the server answers the request in flight with
`Connection: close`, and one arriving on a kept-alive connection `503`; a connection waiting for its
next request ends when its idle limit runs out, and `join()` waits for that.

**Upgrades.** `Response.upgrade(protocol, take)` answers `101 Switching Protocols` with `Upgrade:
protocol` and `Connection: Upgrade` (the protocol a token, else it traps), and once that head is
sent the connection is no longer HTTP's: `take` is given an `Upgraded`, with its `stream`, the bytes
the client sent after its request that the server had already read (`ahead`), the `writeTimeout`, and
`isStopping()` and `untilStopped(wait)` as a request has them. No request is read after it, the
connection is closed when `take` returns, and a `take` that traps closes it with nothing written.

### WebSockets

`webSocket(request, session)` answers a WebSocket handshake (RFC 6455) from a handler, and
`WebSocket.connect(url)` opens one as a client; each end holds a `WebSocket` and the two speak alike:

```text
import avin.net.http
import avin.net.http.{Binary, Route, Router, Text, WebSocket, webSocket}

echo(ws: WebSocket)
  loop
    ws.receive() match
      Ok(Text(t)) -> _ = ws.send(t)
      Ok(Binary(b)) -> _ = ws.send(b)
      Err(_) -> return

main()
  val router = Router([Route.get("/echo", r -> webSocket(r, echo))])
  http.serve("0.0.0.0", 8080, router.handle).ok.get.join()
```

`webSocket(request, session, protocols = [], maxMessageBytes = 1048576, closeTimeout = seconds(5),
outboxSize = 64) -> Response` checks the handshake and answers `101` with its `Sec-WebSocket-Accept`
(`acceptKey(key)`, the base64 of the SHA-1 of the key and the protocol's GUID), then runs
`session` with the WebSocket, closing it with `1000` if `session` returns leaving it open. Of
`protocols`, the subprotocols the server speaks, the first the client offers in
`Sec-WebSocket-Protocol` is chosen, answered and found in `ws.protocol`; none where the client
offers none of them. A request that is not a handshake is answered without `session` running:
`405` for a method other than `GET`; `426` with `Upgrade: websocket` where it does not ask to
upgrade to a WebSocket; `426` with `Sec-WebSocket-Version: 13` for another version; `400` in
HTTP/1.0, without `upgrade` in `Connection`, or with a `Sec-WebSocket-Key` that is not sixteen bytes
of base64. A negative `maxMessageBytes` or `outboxSize` traps. A route is otherwise an ordinary route, under groups
and middleware alike, and it works over TLS as every route does.

`WebSocket.connect(url, headers = [], protocols = [], tls = TlsClient(), timeout = seconds(30),
maxMessageBytes = 1048576, outboxSize = 64) -> Result[WebSocket, WebSocketError]` opens `ws://` and `wss://` URLs,
the latter over TLS checked as an `https` request's is, sending `headers` and offering `protocols`
in the handshake; `timeout` limits each wait of the handshake and each write after it.

A `WebSocket` sends `send(text)` and `send(bytes)` as one message each, a message over 65536 bytes in
fragments of that many written one after another, `ping(payload = [])` (at most
125 bytes, else it traps), and `close(code = 1000, reason = "")`, which sends the close frame, waits
the close timeout for the other end's answer and closes the connection; a code is 1000 to 1003, 1007
to 1014 or 3000 to 4999 and a reason at most 123 bytes of UTF-8, either trapping where it is not.
`receive()` answers the next `Message`, `Text(text)` or `Binary(bytes)`, waiting as long as it
takes; `receive(limit)` answers `Ok(None)` where no whole message has arrived by then, keeping a
message that arrived in part. `isOpen()` says whether it can still be used, and `isStopping()` and
`untilStopped(wait)` are the server's, a client's never stopping. A `WebSocket` belongs to the code
it was given to, as its connection does.

While it receives, a WebSocket keeps the protocol by itself: a ping is answered with a pong at once
and a pong passed over, a message arriving in fragments is put back together with control frames
answered between them, and a close from the other end is answered with its code, the connection
closed, and the receive answers `Err(Closed(code, reason))` (`1005` for a close giving no code).
Every send and receive after the WebSocket is closed answers the same error. A server stopping
closes its WebSockets with `1001` at their next send, or at once where one is waiting in a receive,
which watches the server's `stopped()` channel. A thread cancelled while it receives closes the
connection, the WebSocket then closed with `1006`.

#### Sending from other threads

A `WebSocket` belongs to one thread, which does all of its connection's reads and writes. Other
threads send on it through `ws.sender()`, a sendable `WebSocketSender` with `send(text)`,
`send(bytes)`, `close(code = 1000, reason = "")` and `isOpen()`. A send queues one whole message in
the WebSocket's outbox, at most `outboxSize` messages; the owning thread writes them while it
receives, in the one wait that also watches its connection, so its senders' messages, its own, the
pongs and the close handshake all go on the wire from it, each message whole and never interleaved
with another, and each sender's in the order it queued them. A thread whose session only answers
what arrives, or whose messages all come from senders, loops in `ws.serve(onMessage) ->
WebSocketError`, which receives until the WebSocket closes, runs `onMessage` for every message and
answers why it closed. A broadcast chat server puts each session's sender in a room:

```text
import avin.net.http
import avin.net.http.{Route, Router, Text, WebSocket, WebSocketSender, webSocket}
import avin.threads.Mutex

class Room(): Sendable
  private val members = Mutex(Vector.empty[(int, WebSocketSender)]())
  private val count = Mutex(0)

  join(s: WebSocketSender) -> int
    val n = count.update(c -> c + 1)
    _ = members.update(v -> v :+ (n, s))
    n

  leave(n: int) = _ = members.update(v -> v.filter((k, _) -> k != n))

  broadcast(from: int, text: string)
    for (n, s) in members.get()
      if n != from then _ = s.send(text)

chat(ws: WebSocket, room: Room)
  val me = room.join(ws.sender())
  defer room.leave(me)
  _ = ws.serve(m -> m match
    Text(t) -> room.broadcast(me, t)
    _ -> ())

main()
  val room = Room()
  val router = Router([Route.get("/chat", r -> webSocket(r, ws -> chat(ws, room)))])
  http.serve("0.0.0.0", 8080, router.handle).ok.get.join()
```

A send answers `Ok` once its message is queued while the WebSocket is open; one the WebSocket closes
before writing is dropped. A full outbox, as a client that reads slowly leaves it, makes a send wait
up to the WebSocket's write timeout and then answer `Err(Broken(IoError(TimedOut, _, _)))`, the
message not queued and the WebSocket still open, so a slow client never holds up the threads
sending to every other one; the owner's own write to such a client fails at the same timeout and
closes it. A sender's `close` asks the owner to close the WebSocket, which it does, as its own
`close` would, once the messages queued before are written; every sender call from then on answers
`Err(Closed(code, reason))`. The owner's `close`, a close from the other end, a failure and a server
stopping end the outbox: what is queued is dropped, and every sender call, a send waiting for room
among them, answers why. Messages queued while the owner is not receiving wait until it next is.

| Close | When this end closes with it |
| --- | --- |
| `1002` | A client's frame not masked or a server's masked; a reserved bit set; an unknown opcode; a control frame fragmented or over 125 bytes; a continuation with no message to continue, or a new message inside a fragmented one; a close frame of one byte or with a code no endpoint may send; a 64-bit length with its top bit set |
| `1007` | A text message, or a close reason, that is not UTF-8, checked once the message is whole |
| `1009` | A message longer than `maxMessageBytes`, refused by its frames' lengths before their payloads are read |
| `1001` | The server is stopping |

A frame breaking the protocol fails the connection: the close frame is sent and the connection
closed at once. A `WebSocketError` is `Closed(code, reason)`, which also reports `1006` where the
other end went without a close; `Broken(error)`, the connection having failed; and, from `connect`,
`Refused(status, reason)` for an answer other than `101`, `BadHandshake(text)` for a `101` that is
not a WebSocket server's (an accept key not the key's, no `Upgrade: websocket`, a subprotocol not
offered), and `Unreachable(error)`, the URL, the connection or its TLS failing as an `HttpError`.

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
| Share costly resources, connections for one | [`Pool(create, maxSize)`](/spec/threads/#pools) and `pool.use(c -> …)`, which lends one for the call and always takes it back |

## JSON

JSON is not part of the standard library: it is the package [`dev.avinlang.json`](/spec/json/), in
its own repository, which a program adds to its dependencies. `import avin.json` is an error naming
the package.
