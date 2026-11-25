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
| `setCurrentDir(dir) -> Result[unit, IoError]` | Makes `dir` the directory relative paths are read from, for the whole program; `NotFound`, `NotADirectory` or `PermissionDenied` leave it as it was. |
| `tempRoot() -> string` | The directory for temporary files: `TMPDIR` when it is an absolute path, else `/tmp`. |
| `cacheDir()`, `configDir()`, `dataDir()` `-> Option[string]` | Where the machine keeps a program's cache, settings and data; see below. A path only: nothing is made. |

**Well-known directories follow the platform's convention.** On macOS `cacheDir()` is
`~/Library/Caches`, and `configDir()` and `dataDir()` are both `~/Library/Application Support`: the
platform draws no line between settings and data, and the XDG variables are not read. Elsewhere each
is its freedesktop variable (`XDG_CACHE_HOME`, `XDG_CONFIG_HOME`, `XDG_DATA_HOME`) when that is set
to an **absolute** path, and otherwise a leaf under the home directory: `~/.cache`, `~/.config`,
`~/.local/share`. A variable that is empty or relative is ignored, as the specification requires. All
three are `None` when no home is known and the variable does not answer (`HOME` unset or empty).
`homeDir` joins its leaf with exactly one `/`. A program adds its own name:

```text
import avin.fs
import avin.process

settingsFile() -> Option[string] =
  process.configDir().map(d -> fs.join([d, "myprogram", "settings.conf"]))
```
| `executable() -> Option[string]` | The absolute path of the running program's file, where the platform can tell. |
| `processors() -> int` | How many processors the program may run on: the number the futures' pool grows to. |
| `pid() -> long` | The process's identifier. |
| `userId() -> long` | The user id the program acts as: the one the file system checks a permission against and the `owner` of a file it creates; `0` is the superuser. |
| `groupId() -> long` | The group id the program acts as, which a file's `group` is compared with. |
| `userName(id: long) -> Result[string, IoError]` | The name the system's user database gives user `id`: `userName(0)` is `Ok("root")`. An id with no entry, and one outside `0` to `0xffffffff`, is `Err(NotFound)`. |
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

### Configuration files and the environment

A server takes its settings from more places than its command line. **`parseLayered` gives each
option the first value found in four layers**: the command line, then the environment, then a TOML
file, then the option's default. The command line wins over the environment, the environment over
the file, and the file over the defaults, so a deployment sets what it must in a file, an operator
overrides one setting in the environment, and a person trying something overrides it again with a
flag. `Args.origin(name)` says which layer gave a value.

```text run
import avin.cli
import avin.toml

val parser = cli.Parser("srv", "Serve things.")
  .string("host", default = "localhost")
  .int("port", default = 80)
  .flag("verbose", short = 'v')
  .string("db.url", default = "postgres://localhost")

val file = "port = 8080\nverbose = true\n\n[db]\nurl = \"postgres://db\"\n"

main()
  val config = toml.parse(file).getOrElse(toml.Toml.table())
  val env = HashMap.empty[string, string]().updated("SRV_PORT", "9090")
  parser.parseWithConfig(["--host", "0.0.0.0"], config, "srv.toml", envPrefix = "SRV_", env = env) match
    Ok(a) ->
      println(s"${a.string("host")}:${a.int("port")} ${a.flag("verbose")} ${a.string("db.url")}")
      for name in ["host", "port", "verbose", "db.url"]
        println(s"$name from ${a.origin(name)}")
    Err(e) -> println(e.message)
  val typo = toml.parse("prot = 1\n").getOrElse(toml.Toml.table())
  println(parser.parseWithConfig([], typo, "srv.toml").err.map(e -> e.message))
  val wrong = HashMap.empty[string, string]().updated("SRV_PORT", "eighty")
  println(parser.parseWithConfig([], toml.Toml.table(), "srv.toml", "SRV_", wrong).err.map(e -> e.message))
```

```text output
0.0.0.0:9090 true postgres://db
host from the command line
port from the environment variable `SRV_PORT`
verbose from srv.toml
db.url from srv.toml
Some(`prot` in srv.toml is not an option of srv; did you mean `port`?)
Some(the environment variable `SRV_PORT` needs an int, not `eighty`)
```

| Method on `Parser` | Reads |
| --- | --- |
| `parseLayered(args, file = "", fileRequired = false, envPrefix = "", env = process.environment())` | The command line, the environment and the TOML file `file`. An empty `file` reads none; a file that does not exist is skipped unless `fileRequired`. |
| `parseWithConfig(args, config, source, envPrefix = "", env = process.environment())` | As `parseLayered`, with the file already read into the `Toml` table `config`, named `source` in the messages. |
| `parseLayeredOrExit(…)` | As `parseLayered`, ending the program on an error as `parseOrExit` does. |

**The file's keys are the options' names.** An option named with dots, `db.url`, is also the key
`url` of the table `[db]`, or the dotted key `db.url`. A flag's value is `true` or `false`, a
repeated option's an array of values or a single one, and every other value is of the option's
type: a string for a string or a choice, an integer that fits for an int, an integer or a finite
float for a real. **A key that is no option is an error**, naming the option it was most likely
meant to be, so a misspelt setting is never silently ignored.

**An option's environment variable is the prefix and its name in upper case**, each `-` and `.` an
`_`: `SRV_DB_URL` for `db.url`, `SRV_MAX_CONNECTIONS` for `max-connections`. With no prefix the
environment is not read, so `HOME` or `PATH` never set an option by chance. A flag's variable is
`true`, `false`, `1` or `0`; any other option's is its text, checked as the command line's is; a
repeated option takes one value from its variable. `env` is the environment to read, the process's
own unless a test gives another.

**A bad value is an `Invalid` error naming where it came from**, and `parseLayeredOrExit` writes it
as it writes a mistake on the command line:

| From | Message |
| --- | --- |
| The command line | `` `--port` needs an int, not `eighty` `` |
| The environment | `` the environment variable `SRV_PORT` needs an int, not `eighty` `` |
| The file's value | `` `port` in srv.toml needs an int, not a string ``; `` `port` in srv.toml needs an int, and 3000000000 does not fit one `` |
| The file's key | `` `prot` in srv.toml is not an option of srv; did you mean `port`? `` |
| The file's text | `srv.toml: '=' where a value was expected at line 2, column 8`, the TOML error with its line and column |
| The file itself | `cannot read srv.toml: permission denied`, and `no such file or directory` where it is required |

A flag the file or the environment sets to `false` stays unset even where a lower layer set it, and
`given` is true of it, since it was given. A required option is satisfied by any layer. **`--help`
and `--version` are answered whatever the file and the environment hold**, so a broken setting never
keeps a user from the help. Only the parser's own options are layered: a command's options, and every
positional argument, come from the command line alone.

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
  HostNotFound
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
| `HostNotFound` | A host's name stands for no address: the resolver found none, or the name could not be a host's (see [`lookup`](#ip-addresses-and-lookups)). | `no address found for the name` |
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
| `writeText(file, text, createParents = false)`, `writeBytes(file, bytes, createParents = false)` | With `createParents`, the directories above the file that are missing are made first (`createDirAll` of its parent). |
| `writeTextAtomic(file, text, createParents = false, mode = 0o666) -> Result[unit, IoError]`, `writeBytesAtomic(file, bytes, createParents = false, mode = 0o666)` | Writes to a new file beside `file`, syncs it to the device, renames it over `file` and syncs the directory, so the new name survives a crash: a reader sees the whole old contents or the whole new, and a failure before the rename leaves `file` as it was and removes the new file (a failed directory sync is an `Err` with the new contents in place). The new file's name is unique per call and per process. The result keeps the exact permission bits of the file it replaces, setuid and bits the umask would take included; where there was none it has `mode`, less the umask. Either way the new file never has more bits than it ends with. |
| `readLines(file) -> Result[Vector[string], IoError]` | Every line without its terminator. `\n` and `\r\n` both end a line; a last line with no terminator is kept; a final terminator does not start an empty line, so an empty file has no lines and `"\n"` has one empty line. Invalid UTF-8 becomes U+FFFD. |
| `lines(file) -> Result[Lines, IoError]` | The same lines, lazily: `Lines` is an `Iterator[Result[string, IoError]]` holding one line at a time. The file closes when the iterator gives its last item (a line, or the `Err` that ended it). To stop early, `close()` it (`defer reader.close()`); one forgotten is closed by the collector. |
| `writeLines(file, lines, createParents = false, mode = 0o666) -> Result[unit, IoError]` | Writes each line followed by `\n` to a made or emptied file, so the file ends in a newline; an empty collection leaves an empty file. A file it makes has `mode`, as `create` makes one. |
| `exists(file, followLinks = true) -> bool` | Whether anything, file or directory, is at the path. A dangling link answers `false`; with `followLinks = false` the link itself is asked about, so it answers `true`. |
| `isDir(file) -> bool` | Whether the path names a directory, one the program may not read included. |
| `isFile(file) -> bool` | Whether the path names a regular file: not a directory, and not a FIFO, a socket or a device either. |
| `isReadable(file) -> bool` / `isWritable(file) -> bool` | Whether the user who started the program may read, or write, what the path names, as its bits, owner and group say; `false` for a path that is not there. A link is followed. The superuser may read and write anything. Opening the file is the check that counts: the answer can be stale by then. |
| `isSameFile(a, b) -> Result[bool, IoError]` | Whether the two paths lead to one file: its inode and device, not its name or contents. Two hard links, and a symbolic link and its target, are the same; a copy is not. Both are followed and must exist: a missing one is `NotFound`. |
| `remove(file) -> Result[unit, IoError]` | Removes a file; never a directory, which is `IsADirectory`. A link is removed, not what it names. |
| `removeDir(dir) -> Result[unit, IoError]` | Removes an empty directory; `DirectoryNotEmpty` otherwise. |
| `removeDirAll(dir) -> Result[unit, IoError]` | Removes a directory and everything in it; a path already gone is success. |
| `removeAll(path) -> Result[unit, IoError]` | Removes a file, a link or a directory with everything in it. A link is removed and never followed (a link to a directory leaves the directory and its contents); a path that is not there is `NotFound`. |
| `removeIfExists(path) -> Result[bool, IoError]` | As `removeAll`, but a path that is not there is `Ok(false)`; `Ok(true)` when something was removed (a dangling link included). |
| `rename(from, to) -> Result[unit, IoError]` | Moves a file or directory in one step, replacing what is at `to`: a file replaces a file, a directory an empty directory (`DirectoryNotEmpty` if it holds entries); a file onto a directory is `IsADirectory`, a directory onto a file `NotADirectory`; a missing source or a missing directory above `to` is `NotFound`. The same path on both sides succeeds and changes nothing. Both names must be on one file system, or nothing moves. |
| `createDir(dir) -> Result[unit, IoError]` | Makes one directory, whose parent must exist; `AlreadyExists` if something is there. |
| `createDirAll(dir) -> Result[unit, IoError]` | Makes the directory and every missing one above it; a directory already there is success; a file where `dir` itself would go is `AlreadyExists`, and a file where one of the directories above it would go is `NotADirectory`. |
| `createTempDir(prefix) -> Result[string, IoError]` | Makes a new, empty directory in the platform's place for temporary files, its name starting with `prefix`, and answers its path. |
| `tempDir(prefix) -> Result[TempDir, IoError]` | Makes the directory as `createTempDir` does and holds it as an [`io.Close`](#close): `t.path` is its path, `t.close()` removes it and everything in it (closing again does nothing and answers `Ok`; one that failed may be closed again), and `t.isClosed` says whether it has been removed. One never closed stays on the disk. |
| `metadata(file) -> Result[Metadata, IoError]` | One look at a path, as a [`Metadata`](#file-metadata). A symbolic link is followed, so a dangling one is `NotFound`; a missing path is `NotFound`. |
| `linkMetadata(file) -> Result[Metadata, IoError]` | As `metadata`, but a symbolic link is described rather than followed: its `kind` is `Symlink`, its `size` the length of the path it holds, and a dangling one answers `Ok`. |
| `permissions(file) -> Result[int, IoError]` | The permission bits of the file or directory: the nine `rwx` bits with setuid `0o4000`, setgid `0o2000` and the sticky bit `0o1000`, never the file type. |
| `setPermissions(file, mode) -> Result[unit, IoError]` | Sets the permission bits to exactly `mode`, `0o0` to `0o7777`; not a mask added to what is there, and the umask plays no part. A mode outside that range is an `Err` of kind `Other` naming it, before the path is looked at, and changes nothing. |
| `setOwner(file, owner, group) -> Result[unit, IoError]` | Sets the owner and group to the numeric ids `owner` and `group` (`0` to `4294967295`; both are set, so pass the one to keep as `metadata` reports it). A program that is not the superuser may name its own ids and gets `PermissionDenied` for any other user; an id outside the range is an `Err` of kind `Other` naming it, before the path is looked at. A symbolic link is followed. |
| `ownerName(file) -> Result[string, IoError]` | The name the system's user database gives the file's owner (`process.userName(metadata(file).owner)`); an owner with no entry is `NotFound`. Group names are not offered. |
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
`m.isFile` is true for a regular file alone, so a socket or a FIFO another program left in the
directory is passed over rather than removed.

### Copying and moving

A copy is never seen half-written: each file is written under a new name beside its destination and
renamed onto it once whole, and a tree copied anew is built the same way and renamed into place in one
step. Publishing a release directory and keeping the previous one:

```text
import avin.fs
import avin.io.IoError

publish(build: string, live: string) -> Result[unit, IoError]
  val previous = live + ".previous"
  fs.removeIfExists(previous)?
  if fs.exists(live) then fs.move(live, previous)?
  fs.copyAll(build, live, attributes = true)
```

| Function | Meaning |
| --- | --- |
| `copy(from, to, replace = true, attributes = false, followLinks = true, createParents = false) -> Result[unit, IoError]` | Copies one file. A reader of `to` sees the old entry or the whole copy; a failure at any point leaves `to` as it was and removes the new file. `replace` puts the copy in place of a file or link at `to` (the entry is replaced: a hard link to the old file keeps the old bytes, a link at `to` is not written through); without it anything at `to` is `AlreadyExists`, checked and placed in one step. A directory at `to` or `from` is `IsADirectory`. The nine `rwx` bits of `from` always come along; `attributes` adds setuid, setgid, sticky and the modification time (the access time is set to the same instant); the owner never does. `followLinks = false` copies a link as a new link holding the same path. A FIFO, a socket or a device is an `Err` of kind `Other`, opening nothing. A file copied onto itself, by any name, changes nothing. |
| `copyAll(from, to, merge = false, replace = true, attributes = false, followLinks = false, createParents = false) -> Result[unit, IoError]` | Copies a directory with everything in it, `copy`'s options applying to each entry; anything else at `from` is copied as `copy` copies it. Links stay links unless `followLinks`, which refuses a link back to a directory above it (`Other("file system loop")`). Without `merge`, `to` is replaced as `rename` replaces it (absent, or an empty directory with `replace`; a full directory is `DirectoryNotEmpty`, a file `NotADirectory`), and holds the whole copy or is left as it was. With `merge`, the tree is copied into the directory at `to` (made if missing), directories merged by name, files replaced or (without `replace`) `AlreadyExists`; a failure stops at that entry, keeping those copied before it. Each directory the copy makes gets the source's `rwx` bits once its contents are in. A destination inside the source is an `Err`. Hard links inside become separate files. |
| `move(from, to, replace = true, createParents = false) -> Result[unit, IoError]` | `rename` within one file system. Across file systems, `copyAll` with `attributes` and links as links, then `removeAll(from)`: a failure while copying leaves `to` as it was and `from` whole; a failure while removing is an `Err` whose operation is `finish moving`, `to` being complete. Without `replace`, anything at `to` is `AlreadyExists`: in one step for a file, by a check just before the rename for a directory or a link. |

**On failure part way**, `copy`, `copyAll` without `merge`, and the copying half of a cross-device
`move` leave nothing behind and `to` untouched. `copyAll` with `merge` stops at the entry that failed,
named in the error (`cannot copy src/a/locked to out/a/locked: permission denied`), with every entry
before it copied whole and none after it started. A copy is not synced to the device; a file that
must survive a power loss is written with `writeBytesAtomic`.

### File metadata

A `Metadata` is everything one `stat` answered, so its fields describe one entry:

| Field | Meaning |
| --- | --- |
| `size: long` | The size in bytes. For a directory it is the file system's bookkeeping, not a count of entries; for a link read by `linkMetadata`, the length of the path it holds. |
| `modified: Instant` | When the contents last changed. |
| `accessed: Instant` | When the contents were last read. Whether a read moves it is the file system's choice: one that records reads moves it to now, and one that does not (macOS's) leaves it. `setModified` sets it with `modified`. |
| `changed: Instant` | When the entry itself last changed: a write, a change of permissions, and `setModified` move it to now, a read does not, and no ordinary call sets it back, which is what makes it the time a security check reads. |
| `isDir: bool`, `isFile: bool` | Whether it is a directory, and whether a regular file. |
| `kind: FileKind` | Which kind of entry it is: `Regular`, `Directory`, `Symlink`, `Fifo`, `Socket`, `CharDevice` (`/dev/null`), `BlockDevice` (a disk), or `UnknownKind(bits)` with the type bits for any other the platform has. `Symlink` comes only from `linkMetadata`. |
| `isLink: bool` | Whether `kind` is `Symlink`. |
| `permissions: int` | The twelve permission bits, as `permissions(file)` answers them. |
| `owner: long`, `group: long` | The numeric user and group ids. `process.userId()` is the program's own, and `process.userName(owner)` names the owner. |
| `links: long` | How many names lead to the file: more than one means a hard link. |
| `inode: long`, `device: long` | The file's number within its file system, and which file system: together, not either alone, they identify the file, which is what `isSameFile` compares. |

`FileKind`'s variants are written qualified, `m.kind == fs.Socket`, and compare as values. A walk that
must not leave a tree asks `linkMetadata(p).isLink` (or `isLink(p)`); a walk that follows links
remembers each directory's `(inode, device)` so a link back up is seen.

**Permissions** are written as octal literals, `fs.setPermissions(file, 0o600)`, and read back the
same way: `fs.permissions(file)` is `Ok(0o600)`, and `f"${p}%o"` prints it as `600`. A file the
library creates asks for `0o666`, which the process umask narrows; rewriting a file with
`writeText` or `writeBytes` keeps the mode it has, so a file set to `0o600` stays so.

**A file can be made with its mode from the start.** `create`, `createUpdate`, `writeLines`,
`writeTextAtomic` and `writeBytesAtomic` take `mode = 0o666`: a new file has those bits, less the
umask, from the moment it exists, so a secret written as `fs.writeTextAtomic(token, secret, mode =
0o600)` is never readable by others, not even between being made and being set. A file already
there keeps its mode, whatever `mode` asks (an atomic write keeps the replaced file's exact bits). A
mode outside `0o0` to `0o7777` is an `Err` of kind `Other` before anything is touched: no file is
made or emptied and no directory is made. `appendText`, `appendBytes`, `append`, `writeText` and
`writeBytes` take no mode yet. An open file's bits are set with `f.setPermissions(mode)`, which
changes the file the handle holds even after it is renamed. Where the
platforms differ the platform decides and the answer says so: macOS refuses the sticky bit on a
plain file to anyone but the superuser (an `Err`, the mode unchanged), and Linux keeps it.

**Ownership** is numeric. Giving a file away is privileged, so a program that is not the superuser
may set only its own ids, which changes nothing, and the platform's `PermissionDenied` answers any
other user:

```avin
import avin.fs
import avin.process

main()
  val file = fs.createTempDir("owner").getOrElse("") + "/token"
  _ = fs.writeText(file, "x")
  println(fs.setOwner(file, process.userId(), process.groupId()))
  println(fs.ownerName(file) == process.userName(process.userId()))
```

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

**A listing with paths and kinds.** `list(dir, pattern = None, skipInvalid = false)` answers a
`Vector[DirEntry]`, `DirEntry(name, path, kind)`, sorted by name as `listDir` sorts. `path` is the
directory joined with the name, and `kind` is the entry's own `FileKind` as `linkMetadata` reads it, so
a symbolic link is `Symlink` (a dangling one too) and `isDir` is false for a link to a directory;
`DirEntry` also has `isDir`, `isFile` and `isLink`. `pattern` keeps the entries whose name fits it, as
`matches` reads a pattern (`*.{c,h}`; a name starting with `.` needs a pattern starting with a literal
`.`). A name that is not UTF-8 is `InvalidUtf8` for the whole listing, as in `listDir`, unless
`skipInvalid` is true, which leaves such names out. An entry removed while the listing is made is left
out; another error reading an entry (a directory that can be read but not entered) is the listing's
`Err`. `hardLink(existing, fresh)` makes a second name for a file.

`entries(dir, pattern, skipInvalid) -> Result[Iterator[Result[DirEntry, IoError]], IoError]` is the
lazy form: the names are read at once, in one pass over the directory, and each entry's kind is asked
for when the iterator reaches it, so taking the first few entries of a huge directory asks the file
system about those few. An entry removed before the iterator reaches it is skipped, and an error
reading one is an `Err` element.

```text
import avin.fs

for entry in fs.list(dir, Some("*.log"))?
  if entry.isFile then println(entry.path)
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
| `root(p) -> Option[string]` | `Some("/")` for an absolute path, `None` for a relative one. |
| `isHidden(p) -> bool` | Whether the file name starts with a dot: `a/.git` is hidden, `a/b`, `.`, `..` and the root are not. |
| `withFileName(p, name) -> string` | `p` with its file name replaced by `name` as written: `/a/b.txt` and `c` give `/a/c`. A path with no file name (`/`, `""`, `.`, `..`, `a/..`) has `name` added after it; an empty `name` leaves the directory alone. |
| `withExtension(p, ext) -> string` | `p` with the extension of its file name replaced by `ext`, given without its dot: `a.tar.gz` and `xz` give `a.tar.xz`, `a` and `txt` give `a.txt`; an empty `ext` removes the extension; a dotfile such as `.profile` gains one (`.profile.bak`). A path with no file name is returned unchanged. |
| `sibling(p, name) -> string` | `join` of the parent of `p` and `name`: `/a/b` and `c` give `/a/c`, `b` and `c` give `c`. An absolute `name` is the answer; `name` may hold separators. A path with no file name has `name` joined after it. |
| `startsWith(p, prefix) -> bool` | Whether the components of `prefix` begin those of `p`, and both are absolute or neither is: `/a/b` starts with `/a` and `/`, not with `/ab` or `a`. Doubled and trailing `/` do not matter; `.` and `..` are not resolved. |
| `endsWith(p, suffix) -> bool` | Whether the components of `suffix` end those of `p`: `/a/b/c` ends with `c`, `b/c` and `/a/b/c`, not with `bc` or `/c`. An absolute `suffix` must be the whole of an absolute `p`. |

```text
import avin.fs

reportFor(source: string) -> string = fs.withExtension(fs.sibling(source, "out/" + fs.stem(source).getOrElse("x")), "html")
```

The questions below ask the file system.

| Function | Meaning |
| --- | --- |
| `absolute(p) -> Result[string, IoError]` | `p` from the root, normalized, a relative path read against the current directory. Links are not resolved. |
| `canonicalize(p) -> Result[string, IoError]` | The one absolute path the file system gives `p`, every link resolved; `NotFound` when nothing is there. |
| `isLink(p) -> bool` | Whether `p` is a symbolic link, dangling or not. |
| `symlink(target, link) -> Result[unit, IoError]` | Makes `link` a symbolic link holding `target` as written; a relative target is read from the link's directory. `AlreadyExists` when something is at `link`. |
| `readLink(link) -> Result[string, IoError]` | What a link holds, as written; a path that is not a link is an `Err`. |
| `walker(root, …) -> Iterator[Result[Entry, IoError]]` | The entries of the tree at `root`, read as the iterator advances, with the options described below. |
| `walk(root, …) -> Result[Vector[Entry], IoError]` | The same entries gathered, or the first `Err` the walk meets. |
| `find(root, p, …) -> Result[Vector[string], IoError]` | The paths of the entries below `root` that the predicate `p` is true for, or, when `p` is a pattern, whose path relative to `root` fits it. |
| `matches(pattern, p) -> bool` | Whether `p` fits the pattern, as described below. |
| `matcher(pattern) -> (string) -> bool` | `matches` for one pattern read once, to test many paths. |
| `glob(pattern) -> Vector[string]` | The paths that exist and fit the pattern. |

**`normalize` reads only the text, and the difference matters.** Where `a/lib` is a link to
`src/lib`, `normalize("a/lib/../main.c")` is `a/main.c`, while the file system climbs out of
`src/lib` into `src`: `canonicalize` of the same path names `src/main.c`. Whatever decides access to
a file is decided on `canonicalize`.

**A walk reads the tree as it goes, in a fixed order.** `walker` answers an `Entry(path, relative,
depth, isDir, isLink, error)` for each file, directory and link below the root: `relative` is the path
from the root and `path` the root joined with it; `depth` is 1 for the root's own entries. Each
directory is listed when the walk reaches it, so taking the first entries of a huge tree lists only
the directories on the way to them. The order is depth first and each directory's names in code-point
order, as `listDir` sorts them, so two walks of an unchanged tree agree. `walk` takes the same options
and gathers the entries into a vector; `find` gathers the paths of those a predicate or a pattern
picks. Every option is named and has a default:

| Option | Default | Meaning |
| --- | --- | --- |
| `maxDepth` | no limit | Entries deeper than this are left out; a directory at `maxDepth` is an entry but is not listed. `0` is the root alone; a negative one fails. |
| `includeRoot` | `false` | The root itself is an entry, with `relative` empty and `depth` 0. |
| `postOrder` | `false` | A directory comes after what it holds rather than before. |
| `followLinks` | `false` | A link that names a directory is gone into as the directory is. |
| `prune` | nothing | A predicate asked about each directory the walk is about to go into, the root excepted; one it answers `true` for is an entry, and nothing in it is. |
| `stopOnError` | `false` | A directory the walk cannot go into ends it with that `Err`, rather than being an entry that carries the reason. |

`find` takes every option but `includeRoot` and `postOrder`.

**A symbolic link is listed and, unless `followLinks` asks, never followed**: its `isLink` is true and
its `isDir` false, whatever it names, so a link back up the tree is one entry and not a loop. Followed,
a link naming a directory has `isLink` and `isDir` both true and is gone into, and one naming a
directory the walk is already inside, on its own way down from the root, is an entry whose `error`
says `file system loop` and is not gone into, so a link back up the tree ends the walk's descent there.
Two links to one directory from elsewhere are both gone into. The root itself is followed when it is
a link.

**A directory the walk cannot list is still an entry**, with `isDir` true and the reason in `error`
(for example `Some(PermissionDenied)`), and the walk goes on past it; with `stopOnError`, that reason
is an `Err` instead, the walk's last element, and `walk` and `find` answer it. A root that cannot be
listed is the walk's one `Err` either way: `NotFound`, `NotADirectory` or `PermissionDenied`. A root
that is not a directory is `NotADirectory`, unless `includeRoot` asks for it, when it is the one entry.

```text
import avin.fs
import avin.io.IoError

sourcesIn(dir: string) -> Result[Vector[string], IoError] =
  fs.find(dir, "**/*.avin", prune = e -> fs.fileName(e.path) == Some("build"))

firstLarge(dir: string) -> Option[string] =
  fs.walker(dir).flatMap(r -> r.toOption).find(e -> !e.isDir && fs.size(e.path).getOrElse(0) > 1000000).map(e -> e.path)
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
| `{a,b}` | any one of the comma-separated alternatives, which may hold `/`, wildcards and further braces: `src/{lib,test}/**/*.{c,h}`; `{,x}` fits the empty text too, and `{a}` is `a` |

**Alternation is written out before anything else is read**: `x{a,b}{1,2}` is the four patterns
`xa1`, `xa2`, `xb1` and `xb2`, and a path fits when it fits one of them. A brace escaped with `\` or
inside a class (`[{]`) is a literal character, and so is a `{` with no closing `}`; a `\,` or a comma
inside a class is a literal comma inside an alternative.

**A name starting with `.` is hidden**: only a component that starts with a literal `.` fits it, so
`*` does not fit `.git`, nor do `?git` and `[.]git`, while `.*` and `\.git` do; and `**` never passes
through a hidden name, so `**/*.c` stays out of `.git`. `*/x` does not fit `../x`, for the same reason.
Each alternative is its own pattern here, so `{.git,src}` fits `.git`. A walk itself lists hidden names
like any other; only a pattern leaves them out, so `find(dir, "**/*.c")` skips them where
`find(dir, e -> !e.isDir)` does not.
**Every pattern is valid**: a `[` with no closing `]` is a literal `[`, and a `\` at the end a literal
`\`. A `**` inside a component, as in `a**b`, is two `*`.

**`glob` answers what is there.** A relative pattern is read from the current directory and answers
relative paths; an absolute one answers absolute paths. The answer is in code-point order, each path
once, and every path in it fits the pattern by `matches`. A `**` goes through directories as `walk`
does, never through a link; a link a literal or wildcard component names is followed. A literal last
component answers a dangling link too. A directory that cannot be read contributes nothing: `glob`
has no `Err`. The empty pattern answers nothing, and `/` answers `/`. A pattern with alternatives is
globbed once for each and the answers merged, still sorted and each path once.

```text
for file in fs.glob("tests/**/*.avin")
  println(file)
```

### Watching

`watch(paths, …)` reads the trees at `paths` and reads them again every `interval`, sending what
changed between two readings to the watcher's `events` channel, which any thread receives from, alone
or as an arm of a [`select`](/spec/threads/#select). The readings run on a lightweight thread of the
watcher's own; a `Watcher` is sendable and an `io.Close`.

| Function or member | Meaning |
| --- | --- |
| `watch(paths, interval, recursive, maxEntries, capacity) -> Result[Watcher, IoError]` | Reads `paths` now, then every `interval` (1 s) after the last reading ends. |
| `watch(paths, ticks, recursive, maxEntries, capacity) -> Result[Watcher, IoError]` | The same, reading on each value received from `ticks`, a `Channel[Instant]`, rather than on a clock; a closed `ticks` ends the watch. |
| `w.events: Channel[WatchEvent]` | The changes, in order. `None` once the watch has ended and every event sent is received. |
| `w.check() -> Option[int]` | Reads now and answers once that reading's events are sent: how many. `None` once the watch has ended. |
| `w.close() -> Result[unit, IoError]` | Ends the watch and waits for its thread; always `Ok`, and closing again does nothing. |
| `w.isClosed -> bool` | Whether the watch has ended: closed, overflowed or out of ticks. |

A `WatchEvent` is `Created(path, isDir)`, `Modified(path, isDir)`, `Removed(path, isDir)`,
`Renamed(from, to, isDir)` or `Overflowed(limit)`, written qualified (`fs.Created`). `path` is the
watched path joined with the entry's path below it, as `walker` makes it.

- **What counts as a change.** A reading keeps a stamp of each entry: kind, size, modification and
  change times, permissions, owner, group, device and inode. A file is `Modified` when any of them
  differs, so a write that keeps the size, and a time set back, are both seen; a directory is
  `Modified` only when its permissions or owner change, since what it gains and loses is reported as
  those entries' own events. A path that changes what it is, a file that becomes a directory, is a
  `Removed` and then a `Created`. A file created and removed between two readings is never seen.
- **Renames, where they can be seen.** An entry gone from `from` and one new at `to` with the same
  device and inode (for a file, the same size and modification time too) is one `Renamed`. A
  directory renamed is a `Renamed` for it and for every entry below it. A file renamed onto a path
  that was already there replaces it: that path is `Modified`, and the old name, where a reading saw
  it, `Removed`.
- **Order.** A reading's events come in code-point order of their paths, `to` for a rename, so a
  directory comes before what it holds; for one path, `Removed` before `Created`.
- **What is watched.** Each path must exist when the watch starts, or `watch` answers its `Err`
  (`NotFound`, say); later it may go and come back, which is a `Removed` for it and everything below
  and a `Created` again. A directory is watched with every entry below it, or with `recursive =
  false` its own entries alone. Links below a watched path are watched as links and never followed;
  a watched path that is a link is followed. A directory that cannot be listed keeps what the last
  reading found in it, so a `chmod 000` is one `Modified` and not the loss of everything inside.
- **The first reading is made before `watch` answers**, so every change after it is reported, and
  `check` makes the reading a test or a "look now" needs: when it answers, that reading's events are
  in `events`.

**The cost is bounded and grows with the tree, not with time.** Each reading lists every watched
directory and reads every entry's metadata, a few system calls an entry: on a laptop's SSD a reading
of 20,000 entries takes about 0.15 s of one thread, so a tree that size read every second keeps a
sixth of a core busy; a longer `interval`, or `recursive = false`, is cheaper. Between readings the
watcher holds one stamp for each entry (nine numbers, the path and its slot in a map), and a reading
builds the next set beside it, leaving the old one and the walk's own values to the collector: the
same 20,000 entries grew the process by about 1.2 KB an entry over the first reading, garbage
included. `maxEntries` (100,000) bounds both sets. More entries than that is an `Err` from `watch`
and, later, an `Overflowed` event that ends the watch. The `events` channel holds at most `capacity` events (1,024; `0` hands
each over): while it is full the watcher waits and reads nothing, so a receiver that lags is never
handed a backlog bigger than that, and the next reading sees every change made meanwhile. An
`interval` of zero or less, a `maxEntries` below one and a negative `capacity` trap.

```text run
import avin.fs
import avin.threads.Channel
import avin.time.Instant

main()
  val dir = fs.createTempDir("watch-example").getOrElse(fail("no directory"))
  val w = fs.watch(Vector(dir), Channel[Instant]()).getOrElse(fail("cannot watch"))
  _ = fs.writeText(fs.join(dir, "app.conf"), "port = 8080")
  println(w.check())
  w.events.tryReceive() match
    Some(fs.Created(p, _)) -> println(s"created ${fs.fileName(p).getOrElse("")}")
    _ -> println("something else")
  _ = w.close()
  _ = fs.removeDirAll(dir)
```

```text output
Some(1)
created app.conf
```

A server reloading its configuration waits on the watcher beside its other work:

```text
serve(conf: string, jobs: Channel[Job], w: fs.Watcher) = select
  w.events.receive() as Some(e) -> reload(conf)
  jobs.receive() as Some(j) -> run(j)
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
| `create(file, mode = 0o666)` | For writing, made if it is not there and emptied if it is. A file it makes has the bits `mode`, less the umask, from the moment it exists; one already there keeps its mode. A mode outside `0o0` to `0o7777` is an `Err` that touches nothing. |
| `append(file)` | For writing, made if it is not there; every write goes to the end, whatever other handles append to the same file. |
| `openUpdate(file)` | The existing file, for reading and writing, keeping what it holds. |
| `createUpdate(file, mode = 0o666)` | For reading and writing, made if it is not there and emptied if it is; `mode` as for `create`. |
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
| `readAt(offset, max)` | Up to `max` bytes from `offset`, **without moving the position**: fewer only where the file ends first, empty at or past the end. What this handle has written is included. A negative `offset` or `max` is an `Err`. |
| `writeAt(offset, bytes)` | Writes all of `bytes` at `offset`, **without moving the position**; past the end the file grows, the gap reading as zeros. A handle opened to `append` may put the bytes at the end whatever the offset, as the platform decides. A negative `offset` is an `Err`. |
| `setLength(length)`, `truncate(length)` | Cuts the file to `length` bytes or extends it with zeros; the position does not move and may end up past the end. A negative length, or a file not open for writing, is an `Err`. |
| `sync()`, `syncData()`, `fullSync()` | Hands over what is written and asks the device to keep it. `syncData` skips details a reader of the contents does not need where the platform can; on macOS it is `sync`, which reaches the drive but not its write cache. `fullSync` also empties the drive's cache (`F_FULLFSYNC` on macOS) and is the slowest. A closed file is an `Err`; a file opened only to read may be synced. |
| `deleteOnClose()` | Marks the file so `close` removes its name after releasing it; a failed removal is the `Err` of `close`. A file the collector frees without a `close` is not removed. |
| `setPermissions(mode)` | Sets the permission bits of the file the handle holds to exactly `mode`, as `fs.setPermissions` does for a path, whatever name the file has now. A file open only to read may be changed by its owner; the handle keeps writing after its bits lose write permission. A mode outside `0o0` to `0o7777` or a closed file is an `Err` that changes nothing. |

Positioned calls on one file from several lightweight threads belong to separate handles, one per
thread, opened on the same path: each call is the platform's own positioned read or write, so
disjoint ranges written at once come out as written.

**One position serves reading and writing**, and lines, `read` and `position` agree on it: bytes the
file has buffered ahead are never lost to a `seek`, a `write` or a `read`. **A read at the end does
not end the file's life**: a file that grew meanwhile gives the new bytes to the next read, so no
reopening is needed to follow it. **A file closed by `close` answers every call with an `Err`**, as does
reading a file opened only to write and writing one opened only to read; none of them is an empty
answer that looks like the end of the file. A forgotten file is closed when the collector frees it.
**A file belongs to one thread at a time and moves as a connection does**: it is sendable, so it
goes to a thread or a future whose body reaches it, with a message to an actor, and on a channel,
whose receiver takes it by using it; and a file a `Mutex` keeps belongs to whichever thread holds
the mutex. Every call, on an open file or a closed one, first checks that the code running owns it,
so the code that handed it over traps (`` `File` belongs to another thread ``). Calls that read and write wait
as the calls of `avin.fs` above do, which is to say they do not park a lightweight thread.

```text run
import avin.fs
import avin.threads.{Channel, Mutex, scope, spawn}

main()
  val dir = fs.createTempDir("files").ok.get
  val log = Mutex(fs.append(dir + "/app.log").ok.get)
  scope(s ->
    for t in 0 ..< 4
      s.spawn(() ->
        _ = log.with(f -> f.writeText(s"thread $t\n").isOk)
        ()))
  _ = log.with(f -> f.flush().isOk)
  println(fs.readText(dir + "/app.log").ok.get.split("\n").filter(l -> !l.isEmpty).length)
  val files = Channel[fs.File]()
  val writer = spawn(() ->
    val f = files.receive().get
    f.writeText("received\n").isOk && f.close().isOk)
  files.send(fs.create(dir + "/sent.txt").ok.get)
  println(writer.join())
  print(fs.readText(dir + "/sent.txt").ok.get)
  _ = fs.removeDirAll(dir)
```

```text output
4
true
received
```

**A tail starts at a whole character.** When `readTail` begins in the middle of a UTF-8 sequence it
skips the rest of that sequence, so the text never starts with a stray byte; bytes that are not UTF-8
elsewhere become U+FFFD, since a log's last lines are read to be looked at and not to be refused.
Lines are decoded the same way, a `\r` before the `\n` going with it.

### Content types

| Call | Meaning |
|---|---|
| `probeContentType(file) -> Result[Option[string], IoError]` | The type of `file`: by its name's extension where `contentTypes` knows it, touching no disk; otherwise by the signature of its first `SniffBytes` (512) bytes. `Ok(None)` where neither knows, and for an empty file; `Err` where the name is unknown and the file cannot be opened or read. |
| `contentTypeOfName(name) -> Option[string]` | The type by extension alone: the text after the last `.` of the last segment, neither its first character nor its last, compared without regard to case. |
| `sniffContentType(bytes) -> Option[string]` | The type by signature: PNG, JPEG, GIF, WebP, WAV, PDF, ZIP, gzip, WebAssembly, ELF and Mach-O; else `text/plain; charset=utf-8` for UTF-8 text with no NUL and no control character but tab, line feed, form feed, carriage return and escape (a character cut off by the 512th byte does not spoil it); else `None`. |
| `contentTypes: HashMap[string, string]` | The table of extensions (lower case, no `.`) to types; every `text/` type names `charset=utf-8`. |

```avin
import avin.fs

main()
  println(fs.contentTypeOfName("site/app.JS").getOrElse("unknown"))
  println(fs.sniffContentType("hello\n".utf8).getOrElse("unknown"))
  println(fs.contentTypeOfName("README").getOrElse("unknown"))
```

```text
text/javascript; charset=utf-8
text/plain; charset=utf-8
unknown
```

### Memory-mapped files

A mapped file's bytes are read and written where they lie, a page at a time as they are touched,
rather than read into an array first: mapping a file of any size costs the same few calls, and a byte
never touched is never read from the device. It suits an index or a table read at scattered offsets,
a file larger than the memory a program wants to hold, and a record updated in place.

```avin run
import avin.fs
import avin.io.IoError

main() -> Result[unit, IoError]
  val dir = fs.createTempDir("mapped")?
  val table = dir + "/table"
  fs.writeText(table, "id=0042 state=new")?
  val m = fs.mapFileMutable(table)?
  println(s"${m.length} bytes, state ${string.fromUtf8Lossy(m.slice(14, 17))}")
  m.writeAt(14, Array(byte(int('o')), byte(int('l')), byte(int('d'))))
  m(6) = byte(int('3'))
  m.flush()?
  m.close()?
  println(fs.readText(table)?)
  val ro = fs.mapFile(table)?
  println(ro(3) == byte(int('0')))
  ro.close()?
  fs.removeDirAll(dir)
```

```text output
17 bytes, state new
id=0043 state=old
true
```

| Function | Meaning |
| --- | --- |
| `mapFile(file) -> Result[MappedBytes, IoError]` | The file mapped read-only. The file is closed before this answers: the mapping does not need it, and keeps the bytes it reads when the file is renamed over or removed. `IsADirectory` for a directory, `NotFound` for a missing file; an empty file is an empty mapping. |
| `mapFileMutable(file) -> Result[MutableMappedBytes, IoError]` | The file mapped shared and writable over its whole length: a write through the mapping is a write to the file. The file must exist and be readable and writable. |
| `File.map()`, `File.mapMutable()` | The same for a file already open, what was written through it handed over first. `mapMutable` needs a file open for reading and writing (`openUpdate`, `createUpdate`); any other, and a closed file, is an `Err`. The mapping outlives the file: closing the file takes nothing from it. |

| Method | Meaning |
| --- | --- |
| `length -> long`, `isEmpty` | How many bytes are mapped: the file's length when it was mapped. The mapping does not follow the file as it grows; `setLength` it first and map it again. |
| `m(i) -> byte` | The byte at offset `i`, a `long` or an `int`. An offset outside `0 ..< length` traps. |
| `m(i) = b` | Writes the byte at offset `i` (writable only), with the same check. |
| `slice(from, until) -> Array[byte]`, `toArray()` | A **new** array of the bytes from `from` up to `until`, or of them all. Offsets out of order or outside the mapping trap, and so does a range longer than an array holds. |
| `writeAt(offset, bytes)` | Writes every byte of `bytes` from `offset` on (writable only); a range running past the end traps before any byte is written. |
| `flush() -> Result[unit, IoError]` | Writes what was written through the mapping to the file and returns once it is there (writable only). `File.syncData` after it asks the device to keep it. |
| `close()` | Unmaps the bytes; a second `close` does nothing. A mapping is an [`io.Close`](#close). |

**No array shares a mapping's memory.** Bytes leave a mapping only by copy, through `m(i)`, `slice`
and `toArray`, and enter only by copy, so an array a program keeps never reads memory that has since
been unmapped. **Every offset is checked** against the length before the mapping is touched.

**A mapping is unmapped by `close` or, if the program forgot, when the collector frees it**, so a
program that maps a file in a loop and drops each mapping does not run out of address space. A call
on a closed mapping, other than `close`, traps:

```text
avin: trap: `MappedBytes` is closed at main.avin:9:11
```

**A mapping belongs to one thread at a time**, as every handle on a Sysl value does. It is `Sendable`
and moves to a thread whose body it is given to; the thread that gave it away traps if it uses it
again before it is handed back. Threads that each read a file map it for themselves.

**What a write promises.** A write through a writable mapping is in the file as far as every other
reader is concerned at once on the same machine; closing the mapping or ending the program loses
none of it. Only `flush` says it has reached the file, and `File.syncData` or `fullSync` after it
that the device keeps it through a power cut. A read-only mapping made before a write through
another mapping of the same file may or may not show it, as the platform decides; read the file
again, or map it again, to be sure.

**A file cut shorter while it is mapped** leaves pages of the mapping with nothing behind them. The
platform answers a touch of such a page of a writable mapping with a bus error rather than an error
value, so the program stops with a trap that says why, and never writes into memory nothing reads:

```text
avin: trap: bus error: touched a page with nothing behind it (a mapped file cut short under its mapping)
```

A read-only mapping is private to the program, and what touching such a page does is the platform's
choice: Linux answers the same bus error, while macOS may go on answering the bytes the file had.
Either way no memory of the program's is read or overwritten. A program that maps a file another
program may truncate, and must see the truncation, reads it with `readAt` instead.

**A mapping counts towards the next collection.** Each holds little of the heap but much address
space, and one of the system's map entries, until it is unmapped, so every thousand mappings made,
or every tebibyte mapped, since the last collection brings one on, and the mappings the program
dropped are unmapped by it.

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

Content digests and keyed digests: SHA-224, SHA-256, SHA-384, SHA-512, SHA-1 and MD5, HMAC over
SHA-1 and the SHA-2 family, PBKDF2 and HKDF, comparison of secrets in constant time, SASLprep for
passwords, and hexadecimal text for any digest; then [signatures and their keys](#signatures-and-keys)
and [authenticated encryption](#authenticated-encryption).

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
| `sha224(x)`, `sha384(x)` | The 28-byte and 48-byte digest of `x` (FIPS 180-4), the truncated members of the family. |
| `sha1(x)`, `md5(x)` | The 20-byte and 16-byte digest of `x`. **Broken for security** (below). |
| `hmacSha256(key, message)`, `hmacSha512(key, message)` | The 32-byte and 64-byte HMAC (RFC 2104) of `message` under `key`, both `Vector[byte]` or both `string`. Any key length is allowed, the empty key included; one longer than the algorithm's block is hashed first, as the RFC says. |
| `hmacSha224`, `hmacSha384`, `hmacSha1` | HMAC over SHA-224 (28 bytes), SHA-384 (48 bytes) and SHA-1 (20 bytes), taking what `hmacSha256` takes. HMAC-SHA-1 is what HOTP and TOTP (RFC 4226, RFC 6238) name. |
| `hkdfSha256(secret, salt, info, length)`, `hkdfSha384(...)`, `hkdfSha512(...)` | `length` bytes of key (RFC 5869 HKDF) from `secret`, with `salt` (empty is the standard's default) and `info`, all `Vector[byte]`. `length` below 1 or over 255 digests (8160, 12240, 16320 bytes) traps. |
| `hkdfExtractSha256(salt, secret)`, `hkdfExpandSha256(prk, info, length)` and the SHA-384 and SHA-512 pairs | HKDF's two halves (§ 2.2 and § 2.3), for a protocol that keeps the pseudorandom key, as TLS 1.3's key schedule does. Extract is the HMAC of `secret` under `salt`; Expand traps on a `prk` shorter than a digest, and on the lengths `hkdfSha256` traps on. |
| `pbkdf2HmacSha256(password, salt, iterations, length)`, `pbkdf2HmacSha512(...)` | `length` bytes of key (RFC 8018 PBKDF2 over HMAC-SHA-256 or -SHA-512) as a `Vector[byte]`; `password` and `salt` both `Vector[byte]` or both `string` (UTF-8), either empty. `iterations` below 1, `length` below 1, or `length` over 8160 (SHA-256) or 16320 (SHA-512) trap. |
| `constantTimeEquals(a, b) -> bool` | Whether `a` and `b`, both `Vector[byte]` or both `string` (compared as UTF-8 bytes), are the same, taking as long wherever they first differ. Different lengths answer `false` at once. |
| `toHex(bytes) -> string` | Lower-case hexadecimal, two digits for each byte: [`avin.encoding`](#avinencoding)'s, which this module offers too. |

**A hasher takes its input in pieces**, for data too large to hold whole, such as a file read in
chunks. `Sha224()`, `Sha256()`, `Sha384()`, `Sha512()`, `Sha1()` and `Md5()` each have `update(bytes)` and `update(text)`,
any number of times, and `finish()`, the digest of everything added. `finish()` answers the same
digest each time it is called, and `update` after `finish` traps. Hashing in any pieces gives the
one-shot digest. A hasher belongs to the thread that made it, as a handle does.

**`sha1` and `md5` are for formats that already use them**: a checksum in a listing, a cache key
nobody attacks. Two different inputs with the same digest can be made for both, so neither signs,
authenticates or names untrusted content; use `sha256` or `sha512` for those. HMAC does not rest on
collision resistance, so `hmacSha1` remains sound for the protocols that name it (TOTP above all);
a new design takes `hmacSha256`.

**HKDF turns a secret that is already unguessable into keys**: a Diffie-Hellman result, a random
master key. It adds no cost to guessing, so a password goes to `pbkdf2HmacSha256` instead. `info`
keeps the keys one secret gives apart, so each purpose names its own:

```text
val encryption = hkdfSha256(master, salt, "app v1 encryption".bytes, 32)
val signing = hkdfSha256(master, salt, "app v1 signing".bytes, 32)
```

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

### Signatures and keys

A `PrivateKey` signs and a `PublicKey` checks, in one of nine `SignatureScheme`s: `Ed25519` (RFC
8032), `EcdsaP256Sha256` and `EcdsaP384Sha384` (FIPS 186-5), and RSA (RFC 8017) with PKCS#1 v1.5
padding, `RsaPkcs1Sha256`, `RsaPkcs1Sha384` and `RsaPkcs1Sha512`, or PSS, `RsaPssSha256`,
`RsaPssSha384` and `RsaPssSha512`. They are the schemes TLS 1.3 and JOSE name (EdDSA, ES256, ES384,
RS256, PS256 and the rest). The signing is OpenSSL's libcrypto, which the program links only where
it signs, checks, reads a key or seals.

```text run
import avin.crypto.{generatePrivateKey, publicKeyFromPem, Ed25519Key, Ed25519}

main()
  val key = generatePrivateKey(Ed25519Key)
  val signature = key.sign(Ed25519, "the release notes").getOrElse(Vector())
  val public = publicKeyFromPem(key.publicKey.toPem()).getOrElse(key.publicKey)
  println(public.verify(Ed25519, "the release notes", signature))
  println(public.verify(Ed25519, "the release notes, changed", signature))
  println(signature.length)
  println(key)
```

```text output
Ok(true)
Ok(false)
64
PrivateKey(Ed25519)
```

| Function or method | Meaning |
| --- | --- |
| `privateKeyFromPem(text)`, `publicKeyFromPem(text)` | The key in the first `PRIVATE KEY` (PKCS#8), `RSA PRIVATE KEY` or `EC PRIVATE KEY` block, or `PUBLIC KEY` (SubjectPublicKeyInfo) block, among any other text; `Result[PrivateKey, CryptoError]` and `Result[PublicKey, CryptoError]`. |
| `privateKeyFromDer(bytes)`, `publicKeyFromDer(bytes)` | The same from DER, which must be the whole of `bytes`. |
| `generatePrivateKey(kind)` | A new key of `kind`, from the operating system's randomness. |
| `ed25519PrivateKey(seed)`, `ed25519PublicKey(raw)` | An Ed25519 key from its 32 bytes (RFC 8032's encoding, a JWK's `d` and `x`). |
| `key.sign(scheme, message) -> Result[Vector[byte], CryptoError]` | The signature over `message`, a `Vector[byte]` or a `string` (UTF-8). |
| `public.verify(scheme, message, signature) -> Result[bool, CryptoError]` | Whether `signature` is the key's over `message`. |
| `key.publicKey`, `key.kind`, `public.kind` | The public half, and what each key is: `Ed25519Key`, `EcP256Key`, `EcP384Key` or `RsaKey(bits)`. |
| `toPem()`, `toDer()` | The key written out: PKCS#8 for a private key, SubjectPublicKeyInfo for a public one, PEM in lines of 64 (RFC 7468). |
| `public.ed25519Bytes() -> Option[Vector[byte]]` | The 32 bytes of an Ed25519 key, `None` for another kind. |
| `scheme.fits(kind) -> bool` | Whether a key of `kind` signs with `scheme`. |
| `ecdsaFixedSignature(der, scheme)`, `ecdsaDerSignature(fixed, scheme)` | An ECDSA signature between DER and the fixed-width form, `Option[Vector[byte]]`. |

**The caller names the scheme, always.** Nothing reads it from the data being checked, so a token or
a message cannot choose how it is verified; that is how `alg: none` and the RSA-key-as-HMAC-secret
confusions work elsewhere. A scheme the key does not sign with is `Err(KeyMismatch(kind, scheme))`
from both `sign` and `verify`: a mistake in the program, told apart from a signature that does not
check.

**`verify` answers `Ok(false)` for every signature that does not check**: forged, made by another key
or over other bytes, of the wrong length, or not even well-formed, since the signature is bytes
somebody sent. An ECDSA signature is checked in strict DER, so BER and trailing bytes do not check;
Ed25519 refuses a non-canonical `S`, so a signature cannot be altered into a second valid one.

**An ECDSA signature is DER**, the form X.509, TLS and WebAuthn carry. JOSE (ES256), COSE and
WebCrypto carry the two integers side by side, each in the curve's width (64 bytes for P-256, 96 for
P-384): `ecdsaFixedSignature` turns `sign`'s answer into that, and `ecdsaDerSignature` turns one back
for `verify`. Either is `None` unless its input is exactly one signature of the scheme's width, in
canonical DER for the first. ECDSA and RSA-PSS signatures are randomised, so two signatures over one
message differ and both check; Ed25519 and PKCS#1 v1.5 give the same signature every time.

**PSS** uses MGF1 over the scheme's digest and a salt as long as the digest, in signing and in
checking (RFC 8017's recommendation, and what TLS 1.3 and JOSE fix); a PSS signature with another
salt length does not check.

**What is read is limited**: Ed25519 keys, elliptic-curve keys on P-256 and P-384, and RSA keys of
2048 to 16384 bits. Anything else is `Err(InvalidKey(reason))`: another curve (P-521, secp256k1),
Ed448, X25519, an RSA key under 2048 bits or restricted to PSS, bytes left after the key, and an
encrypted key, which is refused rather than asked a passphrase (`openssl pkey` decrypts one). A
traditional `RSA PRIVATE KEY` or `EC PRIVATE KEY` reads as the key it holds and writes as PKCS#8.
`generatePrivateKey(RsaKey(bits))` traps outside 2048 to 16384 bits; an RSA key takes a moment to
make, seconds from 4096 bits, and the collector carries on for the program's other threads meanwhile.

**A key is a value.** It holds its DER encoding and parses it again for each `sign` and `verify`, so a
key read once at startup is used from any thread at once, belongs to none, and costs nothing to
keep; the parse is small beside the signature. Two public keys are `==` when they are the same key.
A key's text is its kind, `PrivateKey(RSA 2048)`, and nothing of the key: `toPem` and `toDer` are
the only ways the key itself comes out. The DER the key holds is ordinary memory, which the runtime
does not wipe when the key is collected.

### Authenticated encryption

An `AeadKey` seals a message, encrypting it and authenticating it together with associated data that
travels in the clear (a header, a record's id), and opens it only if neither was changed. The
ciphers (`Aead`) are `Aes128Gcm` and `Aes256Gcm` (NIST SP 800-38D) and `ChaCha20Poly1305` (RFC
8439), each with a 12-byte nonce and a 16-byte tag; ChaCha20-Poly1305 is the fast one where the
processor has no AES instructions.

```text run
import avin.crypto.{Aes256Gcm, Unauthentic}

main()
  val key = Aes256Gcm.generateKey()
  val sealed = key.seal("attack at dawn", "msg-1".bytes)
  println(sealed.length)
  println(key.open(sealed, "msg-1".bytes).map(b -> string.fromUtf8Lossy(b.toArray())))
  println(key.open(sealed, "msg-2".bytes) is Err(Unauthentic))
  println(key)
```

```text output
42
Ok(attack at dawn)
true
AeadKey(AES-256-GCM)
```

| Function or method | Meaning |
| --- | --- |
| `aead.generateKey() -> AeadKey` | A new key from the operating system's randomness. |
| `aead.key(bytes) -> Result[AeadKey, CryptoError]` | The key whose bytes are `bytes`, `keyLength` long (16 for `Aes128Gcm`, 32 for the others), `InvalidKey` otherwise. |
| `key.seal(plaintext, aad = Vector())` | `plaintext` (a `Vector[byte]` or a `string`) sealed under a fresh random nonce: the nonce, the ciphertext and the tag, 28 bytes more than the message. |
| `key.open(sealed, aad = Vector()) -> Result[Vector[byte], CryptoError]` | What `seal` made, opened, or `Err(Unauthentic)`. |
| `key.sealWithNonce(nonce, plaintext, aad = Vector())`, `key.openWithNonce(nonce, sealed, aad = Vector())` | The same under a nonce the caller gives, which must be 12 bytes (anything else traps): the ciphertext and the tag, without the nonce. |
| `key.aead`, `key.toBytes()` | The cipher, and the key's bytes, the secret itself. |
| `aead.keyLength`, `aead.nonceLength`, `aead.tagLength` | 16 or 32, 12, and 16. |

**A message that does not open gives nothing back**: `Unauthentic` is the whole answer for a changed
ciphertext, tag, nonce or associated data, a message cut short or lengthened, and one sealed under
another key or cipher. Decryption writes into a buffer the program never sees until the tag has
checked, and wipes it when the tag does not; no part of an unauthenticated message is ever returned.

**A nonce is never used twice with one key**; one reuse gives away both messages and lets anyone
forge new ones. `seal` draws each nonce from `SecureRandom` and puts it in front, which is safe for
2^32 messages under one key; past that, change the key. `sealWithNonce` is for a protocol that fixes
its nonces (a record counter), which then answers for never repeating one. A key's text is its
cipher, `AeadKey(AES-256-GCM)`, and nothing of the key.

## `avin.uuid`

Universally unique identifiers (RFC 9562), 128 bits written as 32 hexadecimal digits in groups of
8-4-4-4-12.

```text
import avin.uuid.Uuid

main()
  val id = Uuid.random()
  println(id)
  assertEq(Uuid.parse(id.toString()), Some(id))
  val first = Uuid.v7()
  val second = Uuid.v7()
  assert(first < second)
  assert(first.timestamp().isDefined)
```

`Uuid` is a value: it compares and hashes by its bits, orders by them as sixteen unsigned bytes
(the first most significant), is a `Map` key as it stands, and is `Sendable`.

| Member | Meaning |
| --- | --- |
| `Uuid.random()` | A version 4 identifier from `SecureRandom`, with the version and variant bits set. |
| `Uuid.v7() -> Uuid` | A version 7 identifier: the wall clock's Unix milliseconds in the first 48 bits, a 12-bit counter, 62 random bits from `SecureRandom`. Identifiers made in one process sort strictly in the order they were made, from any thread, even when the clock stands still or steps back. No lock is taken. |
| `Uuid.v7At(instant) -> Uuid` | A version 7 identifier stamped `instant`, for tests and backfills. It has random bits where `v7()` has its counter, so it does not order against others of the same millisecond. An instant outside 1970 to the year 10889 reads as the nearest one inside. |
| `UuidGenerator()`, `next(instant) -> Uuid` | The sequence `v7()` draws from, made on its own: `next` gives an identifier above every earlier one of that generator whatever `instant` is. A test feeds it a clock it controls. |
| `timestamp() -> Option[Instant]` | The millisecond a version 7 identifier was stamped; `None` for any other version. |
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
padding, base64url, the URL- and filename-safe alphabet written without padding, and base32, written
padded, unpadded or in lower case.

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
| `toBase32(x) -> string` | `x` in base32: `A`–`Z` and `2`–`7`, padded with `=` to a multiple of eight characters. |
| `toBase32Unpadded(x) -> string` | `x` in base32 with no padding. |
| `toBase32Lower(x) -> string` | `x` in base32 in lower case (`a`–`z`, `2`–`7`) with no padding: sixteen bytes are 26 characters. |
| `fromBase32(text) -> Result[Vector[byte], DecodeError]` | The bytes base32 spells, in upper, lower or mixed case, padded to a multiple of eight or not padded at all. |
| `toQuotedPrintable(x) -> string` | `x` in quoted-printable (RFC 2045 § 6.7), for a mail body: a CR LF stays a line break; printable ASCII but `=` is itself, a space or tab too except at a line's end; every other byte, a lone CR or LF included, is `=XX` in capitals; a line is broken by a soft break, `=` CR LF, to at most 76 characters, never inside an `=XX`. |
| `fromQuotedPrintable(text) -> Result[Vector[byte], DecodeError]` | The bytes quoted-printable spells, as RFC 2045 says to read it: `=XX` in either case, `=` and optional spaces before a line break or the end as a soft break, spaces and tabs before a line break or the end dropped. An `=` followed by anything else is `BadCharacter` at the `=`, one with a single digit before the end `Truncated`. |

Encoding never fails, and the empty input encodes as the empty text and decodes back from it.

**Decoding is strict, so each byte sequence has exactly one spelling.** A decoder refuses whitespace
and line breaks, the other alphabet's characters, a missing, extra or misplaced `=`, any `=` in
base64url, and a last digit with bits set below the last whole byte, which an encoder always leaves
zero (`Zh==` is refused; `Zg==` is `f`). Two different texts therefore never decode to the same
bytes, which matters for a token or a signature carried as text. Text wrapped across lines has its
line breaks taken out before it is decoded. `fromBase32` is as strict, except that a digit's case
carries no bits, so either case is read, and the `=` padding is optional: a padded text must be padded
to a multiple of eight with the right number of `=`, and an unpadded one must end on a whole number of
bytes (2, 4, 5 or 7 digits past a multiple of eight, not 1, 3 or 6). Quoted-printable is the
exception: RFC 2045 has a reader drop the spaces a mail relay may add at a line's end, so its decoder
is as lenient as the RFC asks and no more.

**A refusal is a `DecodeError`**, an `Error` whose `at` is a byte offset in the text. The checks run
in order, the characters first, then the padding, the length and the unused bits, so the offset
named is of the first thing wrong.

| Case | Meaning |
| --- | --- |
| `BadCharacter(at)` | A character the encoding does not use. |
| `BadPadding(at)` | An `=` where padding cannot stand: before the last two characters, followed by a character that is not `=`, more than two of them, or any in base64url. |
| `Truncated(at)` | The text ends inside a unit: an odd number of hexadecimal digits, base64 whose length is not a multiple of four, base64url one character past one, base32 whose digits past a multiple of eight are 1, 3 or 6 or whose padding is not completed to eight. `at` is the text's length. |
| `UnusedBits(at)` | The last digit of the data sets bits no bytes would set. |

## `avin.html`

Text made safe to stand in HTML, and markup built from templates that escape every value they are
given. An `Html` is markup trusted as it stands; the `html"…"` interpolator builds one, reading its
template as a browser will and writing each hole escaped for the place it stands in.

```text
import avin.html.{Html, escapeHtml, escapeAttribute, jsString, safeUrl}

data Link(title: string, url: string)

item(link: Link) -> Html = html"<li><a href=\"${link.url}\" title=\"${link.title}\">${link.title}</a></li>"

page(user: string, links: Vector[Link]) -> Html =
  html"<h1>Hello, $user</h1><ul>${Html.join(links.map(item))}</ul><script>greet($user)</script>"

main()
  val links = [Link("Docs & <more>", "https://avin.dev/docs?a=1&b=2"), Link("Click", "javascript:alert(1)")]
  println(page("<b>Ann</b>", links))
  println(escapeHtml("a < b & \"c\""))
  println(escapeAttribute("x onmouseover=1"))
  println(jsString("</script>"))
  println(safeUrl(" JavaScript:alert(1)"))
```

prints the page with the user's name escaped as text in the heading and as a string literal in the
script, the first link's URL kept and the second's refused, and then:

```text
a &lt; b &amp; &quot;c&quot;
x&#x20;onmouseover&#x3D;1
"\x3C\x2Fscript\x3E"
about:invalid#unsafe-url
```

### Escaping

Each escaper answers text for one context, and the browser reads what it writes back as the text it
was given. Each keeps the empty text empty and every character it does not name as it is, Unicode
included, and answers its argument itself when nothing needs escaping.

| Function | Meaning |
| --- | --- |
| `escapeHtml(text) -> string` | `&`, `<`, `>`, `"` and `'` written `&amp;`, `&lt;`, `&gt;`, `&quot;` and `&#39;`. Safe as an element's text, `<textarea>`'s and `<title>`'s too, and as an attribute's value in double or single quotes. |
| `escapeAttribute(text) -> string` | Every ASCII character but a letter or digit written as a hexadecimal reference (`&#x20;` for a space). Safe as an attribute's value quoted or unquoted; an empty unquoted value takes the next attribute as its own, so an unquoted one still needs a character. |
| `escapeJs(text) -> string` | Every character but an ASCII letter or digit written as a script escape: `\xHH` within ASCII, `\uHHHH` beyond it, a character beyond U+FFFF as its two surrogate escapes. Safe inside a string literal in double quotes, single quotes or backticks, in a `<script>` element and in an event handler attribute: no character of it can end a string, a comment, a template or the element, U+2028 and U+2029 included. |
| `jsString(text) -> string` | `escapeJs(text)` in double quotes: a whole string literal. |
| `safeUrl(url) -> string` | `url` itself when it is relative or its scheme is `http`, `https`, `mailto` or `tel`; otherwise `about:invalid#unsafe-url`, which goes nowhere. Not escaped: write it into an attribute with `escapeHtml`. |

**`safeUrl` reads a scheme as a browser does.** It drops the spaces and control characters at the
start and every tab and line break within, and takes the text before the first `:` that comes before
any `/`, `?` or `#`, in either case. So ` JavaScript:`, `java\tscript:`, `data:` and `vbscript:` are
all refused, and `/a/b:c` and `?q=a:b` are relative. A scheme part holding anything else
(`java&script:`, `java script:`) is refused too, since no allowed scheme is spelled so. A character reference in
the URL stays text, since the URL is escaped after it is checked: `javascript&colon;x` and
`jav&#x09;ascript:x` (whose `#` begins a fragment) are kept, and written into an attribute as
`javascript&amp;colon;x` and `jav&amp;#x09;ascript:x`, relative URLs.

### `Html` and templates

`Html(text)` is markup trusted as it stands: making one says that `text` is safe, so it is written
only of text the program wrote itself or an escaper made. `Html.empty` is no markup,
`Html.escaped(text)` is `Html(escapeHtml(text))`, and `Html.join(items, separator = Html.empty)` joins
markup. An `Html` displays as its markup, so it prints and interpolates into `s"…"` as it is, and
`Response.html(status, page)` sends it as a response of the type `text/html; charset=utf-8`.

**`html"…"` writes each hole for the place it stands in**, in scope wherever `avin.html` or one of its
declarations is imported. A hole may be any `Display` value:

| Where the hole stands | What is written |
| --- | --- |
| An element's text, `<textarea>`'s and `<title>`'s included | An `Html` as it is; any other value escaped by `escapeHtml`. |
| An attribute's value in double or single quotes | The value, an `Html`'s markup included, escaped by `escapeHtml`. |
| The start of a URL attribute's value (`href`, `src`, `action`, `formaction`, `xlink:href` and the others whose value a browser follows) | The value passed through `safeUrl`, then escaped. Blanks written before it in the template are not text of the value. |
| A URL attribute's value after text of it, as in `href="/search?q=$q"` | The value percent-encoded, every byte of its UTF-8 but a letter, a digit, `-`, `.`, `_` and `~`, then escaped. |
| A `<script>` element, or an event handler attribute (`onclick`, any name beginning `on`) | The value as `jsString` writes it, a whole string literal, then, in an attribute, escaped. |

**A hole where no escaping makes it safe traps**, naming why: a hole in a tag's name or among its
attributes, an attribute's name, an unquoted attribute value (write it in quotes), a comment, a
declaration such as `<!doctype>`, a `style` or `srcdoc` attribute, the content of `<style>`,
`<noscript>`, `<iframe>`, `<xmp>`, `<noembed>`, `<noframes>` and everything after `<plaintext>`; a hole
in a script written just inside quotes (`f('$x')`: write `f($x)`, since the hole becomes the whole
literal); and an `Html` in a script, which is trusted as markup, not as script. A template whose
script holds `<!--`, which changes where the browser ends it, traps, and so does one that ends
anywhere but in an element's text, inside a tag, a comment or a script, so markup made by `html"…"`
can always be joined to more. `Html.render(parts, args)` is the interpolator as a function: the
markup, or the reason as an `Err` where `html"…"` traps.

**The template is read as the program runs**, each time it renders, as the HTML tokenizer reads it:
tag and attribute names in either case, the end of a script or a style only at its own end tag, a
`<` not followed by a letter, `/`, `!` or `?` as text. Its escapes are decoded first, as `s"…"`
decodes them. A mistake in a template is found the first time it renders, not as the program
compiles.

## `avin.toml`

TOML 1.0 documents, the configuration files a server reads: `parse` reads one into a `Toml` value,
with the line and column of anything wrong in it; `render` writes a value back as TOML text; and
`derives TomlEncoder, TomlDecoder` reads a document straight into a program's own type and writes
one from it.

```text run
import avin.toml
import avin.toml.{Toml, TomlDecoder, TomlEncoder, TomlKey}

data Database(host: string, port: int = 5432) derives TomlEncoder, TomlDecoder

data Config(
    name: string,
    @TomlKey("listen-port") port: int = 8080,
    tags: Vector[string] = [],
    db: Database = Database("localhost")
) derives TomlEncoder, TomlDecoder

val text = "name = \"api\"\nlisten-port = 9000\ntags = [\"a\", \"b\"]\n\n[db]\nhost = \"db.internal\"\n"

main()
  val config = toml.decode[Config](text)
  println(config)
  println(toml.decode[Config]("name = \"api\"\nlisten-port = \"x\"\n"))
  println(toml.parse("name = 1\nname = 2\n"))
  val doc = toml.parse(text).getOrElse(Toml.table())
  println(doc.at(["db", "host"]))
  print(toml.encode(Config("web", tags = ["ü"])).getOrElse(""))
```

```text output
Ok(Config(api, 9000, [a, b], Database(db.internal, 5432)))
Err(listen-port: expected an integer, found a string)
Err(the key `name` is defined twice at line 2, column 1)
Some("db.internal")
name = "web"
listen-port = 8080
tags = ["ü"]

[db]
host = "localhost"
port = 5432
```

**`Toml` is the value tree**, a `data` type with a case for each kind of TOML value. The four
date-time kinds are `avin.time`'s types: an offset date-time is a `Zoned` in its offset's fixed zone
(`Z` and `-00:00` read as UTC), a local date-time a `DateTime`, a local date a `Date` and a local time
a `Time`. A time keeps microseconds, so digits of a second's fraction past the sixth are cut off, as
TOML allows; a leap second, `:60`, is refused, since a `Time` cannot hold it.

| Case | Holds |
| --- | --- |
| `Str(value: string)` | Any of the four kinds of string. |
| `Integer(value: long)` | A decimal, hexadecimal, octal or binary integer; one outside 64 bits is an error. |
| `Float(value: real)` | A float, `inf`, `-inf` and `nan` included; one too large for a `real`, `1e309`, is an error. |
| `Bool(value: bool)` | `true` or `false`. |
| `OffsetDateTime(value: Zoned)`, `LocalDateTime(value: DateTime)`, `LocalDate(value: Date)`, `LocalTime(value: Time)` | The date-time kinds. |
| `Arr(items: Vector[Toml])` | An array, of values of any kinds, or an array of tables. |
| `Table(members: Vector[(string, Toml)])` | A table, its keys in the order the text first wrote them, each once. |

`Toml.table(…)`, `Toml.arr(…)`, `Toml.str(s)` and `Toml.num(n)` make values. The accessors answer an
`Option`: `asString`, `asLong`, `asInt` (where the integer fits), `asReal` (a float, or an integer
rounded), `asBool`, `asZoned`, `asDateTime`, `asDate`, `asTime`, `asVector`, `asMembers`; `get(key)`,
`get(index)`, `at(keys)` along a path of tables, and `has`, `keys`, `size`, `with(key, value)`;
`apply(key)` and `apply(index)` trap where there is no such value; `kind` names the value's kind for
a message, `an integer`.

| Function | Meaning |
| --- | --- |
| `parse(text, maxDepth = 128) -> Result[Toml, TomlError]` | The table `text` writes. Also for a document's UTF-8 bytes, an `Array[byte]`, whose bytes that are not UTF-8 are an error at the first of them. |
| `value.render -> string` | A table as a document, any other value as it is written after `=`. Also the value's `Display`. |
| `decode[A](text, maxDepth = 128) -> Result[A, TomlError]` | The value of type `A` the document holds; also from bytes, and from a `Toml` tree, a part of a larger one. |
| `encode[A](value) -> Result[string, TomlError]` | `value` as a document. |
| `toTree[A](value) -> Result[Toml, TomlError]` | The tree `encode` writes. |

**Every rule of the TOML 1.0 grammar is checked, and a breach is a `TomlError`**: its `message`,
`line` and `column` (counted in characters from 1), byte `offset`, and a `path` of keys and indexes
where a decoded value is at fault (`replicas[1].host`); one from a tree has no place, and line 0. A
key defined twice, a table defined twice by headers, a dotted key adding to a table a header or an
earlier section defined, an array of tables extending a static array, anything added to an inline
table, an unknown escape, a surrogate, a control character in a string or a comment, a leading zero,
an underscore not between two digits, a carriage return with no line feed after it: each is an error
naming the key or the text, at the place it starts. The whole TOML 1.0 suite of toml-test, 208 valid
and 509 invalid documents, is among the library's tests.

**Nesting is limited, 128 deep unless `maxDepth` says otherwise.** The document's own table is 0
deep, and each table or array inside another one deeper, counting every part of a dotted key or a
header, so neither `[[[[…` nor `a.a.a.…` of any length can exhaust the reader's stack. Strings,
arrays and tables of any length read in time linear in their size.

**`render` writes a table's keys that hold other values first**, one to a line, then each table it
holds under a `[header]` and each array of tables under one `[[header]]` for each element, with a
blank line before each header; arrays and tables inside an array are written inline. A key is bare
where it can be, else a basic string; strings are basic strings, every character but the control
characters, `"` and `\` written as it is; a float always has a point or an exponent, so it reads
back a float. A document read back from `render` therefore has the same values, a table's keys
perhaps in another order. A date outside the years 0 to 9999, which TOML cannot write, traps in
`render` and is an error from `encode`.

**The derivations are those of JSON, on a tree**: a value is a table of its fields, by name; a type
of several variants also carries its variant's name under the key `type`, written first. `@TomlKey(
"name")` renames a field or a variant, `@TomlTag("kind")` the discriminator, and `@TomlOmit` leaves a
field out both ways. A field absent from the input takes its default, or `None` for an `Option`, or
is an error naming it; a key no field has is skipped. TOML has no null, so a field holding `None` is
left out, and a `None` anywhere else, as an array's element, is an error. The built-in encoders and
decoders are `bool`, `int` (an integer that fits), `long`, `real` (from a float or an integer),
`string`, `Toml`, `Zoned`, `Instant` (written in UTC), `DateTime`, `Date`, `Time`, `Option`, `Vector`,
`List`, and `HashMap` and `TreeMap` keyed by a `TomlMapKey`: `string`, `int` or `long`.

## `avin.log`

Structured logging for programs that run for a long time: records of a level, a message and
key/value fields, written one line each as text for people or as JSON objects for machines. A
`Logger` is a [given](/spec/contextual-parameters/): the record functions take
`using logger: Logger`, and a program declares one given, at its module's top level or in the block
handling a request, rather than passing it down by hand.

```text
import avin.log
import avin.log.{Json, Level, Logger, info, warn}

given appLog: Logger = Logger(level = Level.Debug, fields = [("app", "shop")])

handle(order: Order)
  given reqLog: Logger = appLog.child([("order", order.id)])
  info("placing", [("items", order.items.length)])
  if order.total > 1000 then warn("large order", [("total", order.total)])

main()
  info("started", [("port", 8080)])
```

```text
2026-11-23T10:20:30.5Z INFO  started app=shop port=8080
2026-11-23T10:20:31.012Z INFO  placing app=shop order=a17 items=3
{"time":"2026-11-23T10:20:31.012Z","level":"info","message":"placing","app":"shop","order":"a17","items":3}
```

The first two are `Text`, the third the second record as `Json`. **A record below the logger's level
costs one comparison**: the message and the fields are [by-name](/spec/by-name/) arguments, so
`log.debug(s"state ${dump()}", [("size", measure())])` calls neither `dump` nor `measure` where
`Debug` is not written. `if logger.enabled(Level.Debug)` guards a costly block of several records.

**Where the logger comes from.** Each record function finds the nearest given `Logger` as any
contextual parameter does, and where there is none it writes to `DefaultLogger`: `Info` and above, as
text, to standard error. There is no logger to set at run time: a program configures its logging by
declaring a given at its top level, which every function of its module finds, and a request's
handler declares a child of it as a given of its own block.

**Fields.** A field is a `(string, Any)`. In a JSON line an `int`, `long`, `bool` and finite `real`
are written as JSON values, and every other value, a not finite `real` too, as a string of its
`Display`. In a text line each is `key=value` by its `Display`, quoted and escaped as a JSON string
where it is empty or holds a space, a control character, `"`, `=` or `\`; a key likewise. A value not
known is left out of the fields: `None` is written as its `Display`, `None`, like any other value.
Fields follow the message in order: the logger's own, each child's after its parent's, then the
record's. A key written twice is written twice. JSON escapes `"`, `\` and every control character
(`\b`, `\f`, `\n`, `\r`, `\t` short, the rest and DEL as `\u00XX`) and writes every other character,
Unicode included, as it is. A text line escapes a message's control characters the same way, so a
record is always one line and cannot move a terminal's cursor.

**Lines.** A text line is the time, the level's name in upper case padded to five columns, the
message and the fields, separated by spaces. A JSON line is an object of `time`, `level` (its name
in lower case), `message` and then the fields. The time is the logger's `clock`, `now` unless a test
gives another, in RFC 3339 form in UTC (`Instant`'s display: the fraction only where there is one).
A text logger paints the level's name (trace dim, debug cyan, info green, warn yellow, error bold
red) only where it was made with `colour = true` (the default) and its sink takes colour; the
standard sinks take it only where their stream is a terminal, `NO_COLOR` is not set and `TERM` is
not `dumb`. A JSON logger never paints.

**Sinks and threads.** A `Logger` is `Sendable`: its own fields are rendered when it is made, so a
child costs nothing per record and one logger serves every thread. Each line, with its `\n`, reaches
its sink in one call, and every standard sink writes it in one write or under one lock, so lines
from many threads at once never interleave. A line a sink cannot write is dropped: logging never
stops the program it reports on.

| Member | Meaning |
| --- | --- |
| `Level` | `Trace`, `Debug`, `Info`, `Warn`, `Error`, ordered so; `name` (`"warn"`), `label` (`"WARN "`), displayed by its name. |
| `parseLevel(text) -> Option[Level]` | A level's name in any case, `warning` for `Warn`; `None` for anything else. |
| `Format` | `Text` or `Json`. |
| `Field` | `(string, Any)`. |
| `Logger(level = Info, format = Text, sink = stderrSink(), colour = true, clock = now, fields = [])` | A logger writing the records at `level` and above. |
| `trace`, `debug`, `info`, `warn`, `error(message, fields = [])` | Methods of a `Logger`, and module functions taking `using logger: Logger = DefaultLogger`; `message` and `fields` by-name. |
| `log(at, message, fields = [])` | A record at a level chosen while running. |
| `enabled(at) -> bool` | Whether a record at `at` would be written. |
| `child(fields) -> Logger` | The same logger carrying `fields` after its own. |
| `atLevel(level) -> Logger` | The same logger with another threshold, its fields kept. |
| `level`, `format` | What the logger was made with. |
| `DefaultLogger` | `Logger()`: `Info` and above, as text, to standard error. |
| `Sink(write, colours = false, again = () -> ())` | A sink calling `write: @sendable (string) -> unit` once a line, the line ending in `\n`; `colours` says it takes colour; `again` is what `reopen` runs. |
| `sink.reopen()` | Makes the sink look at where it writes afresh: a file sink opens its file again at once, whatever its `recheck`, making the file where a rotator moved it away; any other sink does nothing. |
| `reopenOnSignal(sink, sig = signal.Hangup) -> Result[Reopener, IoError]` | Watches `sig` on a thread of its own and `reopen`s `sink` each time it arrives, which is what a log rotator asks for after moving the log. An `Err` for a signal that cannot be watched. While it runs the signal no longer ends the program. |
| `reopener.stop()` | Stops watching and waits for the thread to end; stopping again does nothing. |
| `stderrSink()`, `stdoutSink()` | The standard streams, each line in one write, taking colour on a colour terminal. |
| `fileSink(path, recheck = seconds(1)) -> Result[Sink, IoError]` | Appends to `path`, creating it; an `Err` where it cannot be opened. The file stays open behind a lock, and each line is one write, handed to the platform before the record's call returns. At most once every `recheck` (before every line where it is zero or less) the sink looks at what `path` names, and opens it again where a rotator moved the file away or put another in its place. |
| `writerSink(target: Mutex[W])` | Any `Writer`, each line written whole under its mutex. |

The access log of [`avin.net.http`](#http-server) writes through a logger as well: see
`http.accessLog(logger)` there.

## `avin.cache`

Bounded caches of computed values that one program's threads share: an `LruCache`, which drops the
entry used least recently to make room, and a `TtlCache`, which does the same and also lets each
entry expire a fixed time after it was stored. Both are `Sendable`, their keys and values sendable
too, so one cache serves every thread of a server.

```text
import avin.cache.{LruCache, TtlCache}
import avin.time.seconds

data User(id: long, name: string)

fetch(id: long) -> Result[User, string] =
  if id > 0 then Ok(User(id, s"user $id")) else Err(s"no user $id")

main()
  val users = LruCache[long, User](10000)
  println(users.getOrTry(7, fetch))
  println(users.getOrTry(7, fetch))
  println(users.getOrTry(-1, fetch))
  println(users.stats())
  val pages = TtlCache[string, string](seconds(30), 1048576, (k, v) -> long(k.byteLength + v.byteLength))
  pages.put("/", "<h1>home</h1>")
  println(pages.get("/"))
  println(pages.weight())
```

```text
Ok(User(7, user 7))
Ok(User(7, user 7))
Err(no user -1)
CacheStats(1, 2, 2, 1, 0, 0)
Some(<h1>home</h1>)
14
```

**Bounds.** A cache holds at most `capacity` worth of entries, each weighing what its `weigher`
says: one an entry unless told otherwise, so the capacity is a count of entries, or the bytes of a
key and its value where the weigher measures them. Storing an entry beyond the capacity drops the
least recently used ones until the weight is within it again, as many as a heavy entry needs. An
entry heavier than the whole capacity is not stored, and a put of it drops the key's old value all
the same; a capacity of zero stores nothing. A negative capacity, and a weigher giving a negative
weight, trap. `get` and the loaders make the entry they find the most recently used; `peek` reads it
without that and without counting.

**Loaders and one load a key.** `getOrLoad(key, load)` gives the cached value, or else runs
`load(key)` and stores what it gives. Threads that miss on one key at once share a single load: the
first runs the loader and the others wait for its value, so a burst of requests for a cold key costs
one query, not one each. `getOrTry(key, load)` takes a loader that may fail: its `Ok` value is stored,
and its `Err` is handed to every caller waiting on that load but **not stored**, so the next call
loads again. A cache that should remember failures too holds `Result`s and is filled with
`getOrLoad`. A loader runs on its caller's thread, outside the cache's lock: nothing of a caller's,
loader, weigher or clock, ever runs under the lock, so a loader may use the cache, its other keys
included. A loader asking for the very key it is loading waits for itself, as a thread joining
itself does. A `put`, `remove` or `clear` of a key while its load runs wins: the load's value is
handed to its waiters but not stored over the newer write. A load whose thread is
[cancelled](/spec/threads/#cancellation) ends with no value, and a thread waiting on it loads
again. Waiting on a load is waiting on a future, and so a cancellation point.

**Expiry.** A `TtlCache`'s entry expires `ttl` after it was stored, or after its load ended, by
`put(key, value, life)` a time of its own; it is live while the clock reads less than that and
expired from the reading on. The clock is the monotonic clock unless the cache is made with another,
`clock: @sendable () -> Duration`, which is how a test moves time by hand. An expired entry is never
given back. It stays held until a lookup finds it, room is made past it, or `purgeExpired()` drops
every expired entry, so `size()` may count expired entries that none of those has reached; the
capacity bounds them all the same. A time to live, or a life, that is not positive traps.

**Counts.** `stats()` gives a `CacheStats(hits, misses, loads, loadFailures, evictions,
expirations)`: lookups by `get` and the loaders that found a live value and that did not (a call
waiting on another's load among the misses, counted once however often it looks), loads run and
those that failed or were cancelled, entries dropped to make room, and entries dropped for having
expired. `requests` and `hitRate` are derived from them.

**Cost.** Every operation takes the cache's one lock for a few map operations, amortized constant
time. Measured at `-O2` on an Apple M-series machine with 4096 entries, one thread: a hit about
130 ns (the lock about 17 ns of it, the map's lookup and move to the newest the rest), a put
replacing a value about 230 ns, a `TtlCache` hit with the monotonic clock about 270 ns. Threads
contending for one cache wait for its lock in turn, each handed it by the one before: eight threads
hitting one cache at once reach about 0.26 million operations a second between them, against 7.7
million for one thread alone, so a cache that many threads hit at once costs microseconds an
operation; a bare `Mutex` contended the same way falls the same way.

| Member | Meaning |
| --- | --- |
| `LruCache[K, V](capacity: long, weigher = (_, _) -> 1)` | A cache of at most `capacity` by weight, dropping the least recently used. `K: Eq & Hash & Sendable`, `V: Sendable`; `weigher: @sendable (K, V) -> long`. |
| `TtlCache[K, V](ttl: Duration, capacity: long, weigher = (_, _) -> 1, clock = monotonic)` | The same, each entry expiring `ttl` after it was stored, by `clock: @sendable () -> Duration`. |
| `get(key) -> Option[V]` | The live value, made the most recently used and counted. |
| `peek(key) -> Option[V]` | The live value, neither made recent nor counted. |
| `put(key, value)` | Stores the value, making room for it; on a `TtlCache`, `put(key, value, life)` gives it a life of its own. |
| `getOrLoad(key, load: (K) -> V) -> V` | The value, or else `load(key)`'s, stored; one load for every thread missing on the key at once. |
| `getOrTry[E: Sendable](key, load: (K) -> Result[V, E]) -> Result[V, E]` | The same with a loader that may fail; an `Err` is handed to the load's waiters, not stored. |
| `remove(key) -> Option[V]` | Drops the entry, giving back its value where it was live. |
| `clear()` | Drops every entry; the counts stay. |
| `purgeExpired() -> int` | On a `TtlCache`: drops every expired entry, answering how many. |
| `size() -> int`, `weight() -> long` | The entries held and the sum of their weights. |
| `capacity`, `ttl` | What the cache was made with. |
| `stats() -> CacheStats` | The counts since the cache was made; `requests` and `hitRate` (0 with no request). |

## `avin.resilience`

Keeping a program working while what it depends on fails for a while. `retry` runs work again
after a growing, spread-out wait, inside the deadline the calling code runs under; a
`CircuitBreaker` stops calling what keeps failing, and lets a trial call through once it has had
time to recover; a `RateLimiter` holds any work to a rate. Work is a function answering a `Result`,
and an `Err` is the failure each of them reacts to.

```text run
import avin.resilience.{BreakerOpen, CallFailed, CircuitBreaker, RateLimiter, RetryPolicy, retry}
import avin.time.{millis, seconds}

main()
  var calls = 0
  val policy = RetryPolicy(maxAttempts = 5, initialDelay = millis(1))
  println(retry(policy, (e: string) -> e.startsWith("busy"), () ->
    calls += 1
    if calls < 3 then Err(s"busy $calls") else Ok(s"done after $calls")))

  val breaker = CircuitBreaker(failureThreshold = 2, openFor = seconds(30))
  for _ in 0 ..< 3
    val reply = breaker.call(() -> Err[int, string]("refused")) match
      Ok(n) -> s"answered $n"
      Err(BreakerOpen(_)) -> "open: not called"
      Err(CallFailed(e)) -> s"failed: $e"
    println(reply)
  println(breaker.state)

  val limiter = RateLimiter(2, seconds(1))
  println([limiter.admit("feed").isOk, limiter.admit("feed").isOk, limiter.admit("feed").isOk])
```

```text output
Ok(done after 3)
failed: refused
failed: refused
open: not called
Open
[true, true, false]
```

**Retrying.** `RetryPolicy(maxAttempts = 3, initialDelay = 100ms, maxDelay = 10s, multiplier =
2.0, jitter = 1.0, maxElapsed = None)` makes at most `maxAttempts` attempts, the first included.
Before retry `n` (counted from one) it waits the backoff `initialDelay × multiplierⁿ⁻¹`, at most
`maxDelay` (`policy.backoff(n)`), less a random part of it: `jitter` is the share that may be taken
off, so `1.0` spreads a wait anywhere from zero to the backoff, `0.5` over its upper half, and `0.0`
waits the backoff exactly (`policy.delay(n)` draws one). `retry(policy, body)` retries every `Err`;
`retry(policy, retryable, body)` only those `retryable` accepts, answering any other at once, so a
refused password is not tried again. Either answers the last result `body` gave, and
`policy.run(…)` is the same loop. The first attempt is always made. A retry is not made where its
wait would end more than `maxElapsed` after the first attempt began, the attempts' own time
counted, nor where the wait would reach the deadline the calling code runs under
([`withDeadline`](/spec/threads/#request-context)): a retry needs time left after its wait, and
the deadline cancels a wait it does reach. A `maxAttempts` below one, a negative `initialDelay` or
`maxElapsed`, a `maxDelay` below `initialDelay`, a `multiplier` below one or not finite, and a
`jitter` outside zero to one trap where the policy is made.

**Circuit breaking.** `CircuitBreaker(failureThreshold = 5, openFor = 30s, trialCalls = 1)` is
`Closed` to begin with and makes every call; `failureThreshold` failures in a row open it (a success
clears the count, `failures` reads it). `Open`, it makes no call for `openFor`, answering
`BreakerOpen(retryIn)` at once, `retryIn` the time until it will try again. Then it is `HalfOpen`
and lets `trialCalls` calls through at once, answering the rest `BreakerOpen(0s)`: one trial
failing opens it again for another `openFor`, and `trialCalls` succeeding close it.
`breaker.call(body)` counts every `Err` as a failure and answers `Result[T, BreakerError[E]]`,
`CallFailed(e)` wrapping the call's own error; `call(isFailure, body)` counts only the errors
`isFailure` accepts, an answer such as not-found showing the service is up. A call cancelled before
it answers counts neither way and frees its place as a trial, and an outcome arriving after the
breaker has changed state since the call began counts neither way either. `state` reads where it stands, `reset()` closes
it and `trip()` opens it by hand, and `onChange: (from, to) -> unit` is told of each change, after
the breaker's lock is free. A `failureThreshold` or `trialCalls` below one and a negative `openFor`
trap. `BreakerError[E]` is an `Error` where `E` is, a failed call's error its `cause`.

**Rate limiting.** `RateLimiter(rate, per, burst = rate, maxKeys = 10000)` is the token bucket the
[HTTP middleware](#http-server) uses, for any work: a bucket per key of `burst` tokens filled again at
`rate` every `per`. `admit(key)` takes a token or answers `Err` of the time until the next, never
waiting; `acquire(key)` waits for the token and answers `true`, or answers `false` at once where the
next token comes no sooner than the calling code's deadline. Both have a keyless form, the one bucket
`""`. Its bound on buckets and its traps are the HTTP limiter's.

**Time of one's own.** Each reads the time through a named parameter `clock: () -> Duration` (the
monotonic clock); the policy and the limiter wait through `pause: (Duration) -> unit`
(`threads.sleep`), and the policy draws through `random: () -> real` (the shared generator's
`nextReal`). A test gives its own, and drives a policy, a breaker or a limiter on a clock it moves
itself, with nothing sleeping. The deadline is still the real one, read from `Context.current()`.

## `avin.metrics`

Metrics a program keeps about itself for a scraper to read: counters, gauges and histograms, each a
**family** of **series** told apart by the values of its labels, held by one `Registry` and rendered
in the Prometheus text exposition format, version 0.0.4. Every type is `Sendable`: one registry
serves every thread.

```text
import avin.metrics.Registry
import avin.time.millis

main()
  val registry = Registry()
  val jobs = registry.counter("jobs_total", "Jobs finished, by queue.", ["queue"])
  val mail = jobs.labels("mail")
  mail.inc()
  mail.inc()
  jobs.labels("print").add(3.0)
  val depth = registry.gauge("queue_depth", "Jobs waiting.")
  depth.set(4.0)
  val took = registry.histogram("job_seconds", "Time a job took.", buckets = [0.1, 1.0])
  took.observe(millis(250))
  print(registry.render)
```

```text
# HELP jobs_total Jobs finished, by queue.
# TYPE jobs_total counter
jobs_total{queue="mail"} 2
jobs_total{queue="print"} 3
# HELP queue_depth Jobs waiting.
# TYPE queue_depth gauge
queue_depth 4
# HELP job_seconds Time a job took.
# TYPE job_seconds histogram
job_seconds_bucket{le="0.1"} 0
job_seconds_bucket{le="1"} 1
job_seconds_bucket{le="+Inf"} 1
job_seconds_sum 0.25
job_seconds_count 1
```

**The hot path.** `labels(values*)` finds a series by one value for each label, in order, making it
the first time; the series map is read without a lock and replaced whole, under the family's lock,
only when a series is made. A series counts with one atomic compare-and-set of its value, so a
program that keeps the series it counts often (`mail` above) pays that alone. Measured at `-O2` on
an Apple M-series machine, 20 million times each: `inc` on a kept series or on a family without
labels about 8 ns, a gauge's `inc` about 8 ns, a histogram's `observe` into 11 buckets about 12 ns,
and `labels("GET", "2xx").inc()`, finding the series by two labels each time, about 125 ns. Four
threads incrementing one series at once take about 100 ns an increment between them, the cost of
the value's cache line passing from core to core; series of their own each avoid it.

**The cardinality guard.** Each family holds at most `maxSeries` series (`defaultMaxSeries`, 10000,
unless given another). Past it, every new label set is the family's **overflow series**, each label
`OverflowValue` (`"_overflow"`), made the first time one is refused and rendered last, so a label
taken from what clients send cannot grow a program's memory and a scrape without bound, while the
totals stay whole. The series already held still count as before; `size` says how many a family
holds, the overflow series among them.

**The format.** A registry renders its families in the order they were made, each as its `# HELP`
line (a backslash and a line end escaped), its `# TYPE` line and its series ordered by their label
values. A label value escapes a backslash, a quote and a line end and keeps every other character,
Unicode included. A number is written whole without a fraction (`3`), otherwise in its shortest
decimal form (`0.25`), and infinities and `NaN` as `+Inf`, `-Inf` and `NaN`. A histogram writes each
bucket's count of the observations at or below its bound (`_bucket{le="…"}`), then `+Inf`, `_sum`
and `_count`; its count is the buckets' total, so `+Inf` and `_count` always agree, even while other
threads observe.

**Names.** A metric name is a letter, `_` or `:`, then letters, digits, `_` or `:`; a label name the
same without `:`, and not beginning `__`; a histogram's labels do not include `le`, which its buckets
are written with. A name is made once in a registry. A name or label name not allowed, a label named
twice, a name made again, or a `maxSeries` below one traps where the family is made.

| Member | Meaning |
| --- | --- |
| `Registry()` | The families of a program or a part of it. |
| `counter(name, help, labels = [], maxSeries = defaultMaxSeries) -> Counter` | A counter family. |
| `gauge(name, help, labels = [], maxSeries = defaultMaxSeries) -> Gauge` | A gauge family. |
| `histogram(name, help, labels = [], buckets = defaultBuckets, maxSeries = defaultMaxSeries) -> Histogram` | A histogram family; `buckets` are finite upper bounds in increasing order, else it traps. |
| `gaugeOf(name, help, read: @sendable () -> real)` | A gauge of one series without labels, `read` each time the registry renders. |
| `render -> string`, `names -> Vector[string]` | The registry in the format; its families' names in the order made. |
| `Counter`, `Gauge`, `Histogram` | A family: `name`, `help`, `labelNames`, `labels(values*)`, `size`, `render`; made alone, it renders alone. Without labels, its own methods are its one series'; with labels they trap. |
| `CounterSeries` | `inc()`, `add(amount)`, `value`: `amount` finite and at least zero, else it traps. |
| `GaugeSeries` | `set(v)`, `inc()`, `dec()`, `add(amount)`, `sub(amount)`, `value`. |
| `HistogramSeries` | `observe(v: real)`, `observe(took: Duration)` in seconds, `count`, `sum`, `cumulative` (each bucket's count with those below it, then the count). A `NaN` is counted past every bound and makes the sum `NaN`. |
| `defaultBuckets` | 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5 and 10 seconds. |
| `linearBuckets(start, width, count)`, `exponentialBuckets(start, factor, count)` | `count` bounds from `start`, `width` apart or each `factor` times the last; a `count` below one, a `width` not above zero, a `start` not above zero or a `factor` not above one (exponential) traps. |
| `number(v) -> string` | A number as the format writes it. |
| `TextContentType` | `text/plain; version=0.0.4; charset=utf-8`. |

[`avin.net.http`](#http-server) serves a registry (`metricsHandler`, `metricsRoute`) and keeps its
request metrics in one (`Metrics`, `ServerMetrics`).

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
| `TcpStream.connect(host: string, port: int, timeout: Duration = Duration.Zero, options: TcpOptions = TcpOptions()) -> Result[TcpStream, IoError]` | A connection to the first of the host's addresses that answers; where none does, the last one's error. The lookup and the connecting, to every address tried, must be done within `timeout`, else the error is `TimedOut`; zero leaves the waits to the system's own limits, and a negative `timeout` traps. Its socket is given `options` before it connects; see [socket options](#socket-options). |
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
| `s.localAddress() -> Result[string, IoError]`, `s.localPort() -> Result[int, IoError]` | This end's address, numerically without its port, and its port. |
| `TcpListener.bind(host: string, port: int, options: ListenOptions = ListenOptions()) -> Result[TcpListener, IoError]` | Listens on the host's first address (`127.0.0.1` for this machine alone, `0.0.0.0` for all of its addresses); port `0` lets the platform choose. It is given `options` before it binds. |
| `l.port() -> int` | The port it listens on. |
| `l.options -> ListenOptions`, `l.reusePort() -> Result[bool, IoError]` | The options it was bound with, and whether it shares its port, as the system says. |
| `l.accept() -> Result[TcpStream, IoError]` | Waits for the next connection, which is given `options.connections`. |
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

### Socket options

A connection's options are a `TcpOptions` given to `connect`, or to a listener for every connection
it accepts, and each has a method on the connection that changes it and one that reads it back from
the system, so what a read answers is what the socket has.

| Type or member | Meaning |
| --- | --- |
| `data TcpOptions(noDelay = false, keepAlive: Option[KeepAlive] = None, sendBuffer = 0, receiveBuffer = 0)` | A connection's options. A buffer size of zero leaves the system's own. |
| `data KeepAlive(idle = seconds(60), interval = seconds(10), count = 6)` | Probing an idle connection (`SO_KEEPALIVE`): the first probe after `idle` with nothing received, the next every `interval`, and `count` unanswered ones end the connection, its next read or write failing. |
| `data ListenOptions(backlog = 128, reuseAddress = true, reusePort = false, connections = TcpOptions())` | A listener's options. |
| `s.setNoDelay(on: bool)`, `s.noDelay() -> Result[bool, IoError]` | Sending each write at once (`TCP_NODELAY`) rather than holding a small one back to join the next. |
| `s.setKeepAlive(k: Option[KeepAlive])`, `s.keepAlive() -> Result[Option[KeepAlive], IoError]` | Probing while idle, or not for `None`; read back in whole seconds. |
| `s.setSendBuffer(bytes: int)`, `s.sendBuffer() -> Result[int, IoError]` | The send buffer asked for, and the size the system gave. |
| `s.setReceiveBuffer(bytes: int)`, `s.receiveBuffer() -> Result[int, IoError]` | The same for the receive buffer. |
| `s.configure(options: TcpOptions) -> Result[unit, IoError]` | Every option at once: `noDelay` and `keepAlive` as they say, each buffer size that is not zero. |

- **A new connection holds small writes back and is not probed**, the system's defaults; a
  listener's `connections` changes that for every connection it accepts, and `http.serve` turns
  both on (see [HTTP server](#http-server)).
- **Keep-alive times are whole seconds**, a part of one counting as one: `millis(1500)` is two.
  An `idle` or `interval` of zero or less, or a `count` less than one, traps where the options are
  used; a value past the platform's own limit is its `Err`.
- **The system may give a buffer of another size than asked**: Linux doubles it and caps it,
  macOS caps it. `connect` sets the buffer sizes before it connects and a listener before it
  listens, so a large receive buffer is in place when the connection's window is agreed. A
  negative size in options, or a size less than one asked of a connection, traps.
- **`reuseAddress`** (`SO_REUSEADDR`) lets a listener take a port a listener stopped a moment ago
  still holds, so a server restarts at once; it is on unless turned off. **`reusePort`**
  (`SO_REUSEPORT`) lets several listeners, in one program or several, bind the same address and
  port, each of them setting it, the system sharing the connections among them; one that does not
  set it is refused the port. **`backlog`** is how many connections the system queues before they
  are accepted, lowered to the platform's own limit where it is larger; less than one traps.
- Options no socket can be given trap before anything is looked up. A closed or split connection,
  and a closed listener, answers each option's read and change with an error.

```text
import avin.net.{KeepAlive, ListenOptions, TcpListener, TcpOptions, TcpStream}
import avin.time.seconds

main()
  val fast = TcpOptions(noDelay = true, keepAlive = Some(KeepAlive(idle = seconds(30))))
  val l = TcpListener.bind("127.0.0.1", 0, ListenOptions(reusePort = true, connections = fast)).ok.get
  val c = TcpStream.connect("127.0.0.1", l.port(), options = fast).ok.get
  println(c.noDelay(), c.keepAlive().map(k -> k.map(a -> a.idle)))
  _ = c.close()
  _ = l.close()
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
| `UnixListener.bind(path: string, backlog = 128) -> Result[UnixListener, IoError]` | Listens at `path`, which must name nothing yet: a path already holding anything is `AlreadyExists`. The system queues up to `backlog` connections, lowered to its own limit; less than one traps. |
| `UnixListener.bindReplacing(path: string, backlog = 128) -> Result[UnixListener, IoError]` | As `bind`, but a socket file left behind by a program that is gone (a connection to it is refused) is removed first. A live listener's path and anything that is not a socket file is `AlreadyExists` and untouched. |
| `s.peerCredentials() -> Result[PeerCredentials, IoError]` | Who is at the other end, as the system vouches for it: `data PeerCredentials(user: long, group: long, process: Option[long])`, the ids `process.userId()` and `process.groupId()` answer and the peer's process id, `None` where the system does not say. How a server learns which user of the machine talks to it, with no password; a closed or split connection answers an error. |
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
`withTimeout`'s deadline stops the thread at once, so a connect's time limit covers the lookup too;
`connect`'s own `timeout`, running out, abandons the lookup the same way and fails as `TimedOut`.
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

### IP addresses and lookups

An `IpAddress` is a value: version 4 or version 6, without a port, equal to another and hashing
alike where both are of one version with the same bytes and, for version 6, the same scope. It is
sendable, so it is a `Map` key or a `Set` member as it stands and crosses to any thread. It displays
in the numeric form RFC 5952 sets out (`127.0.0.1`, `::1`, `2001:db8::1`, `fe80::1%4`,
`::ffff:10.0.0.1`), and its `Debug` form is `IpAddress(::1)`.

| Function | Meaning |
| --- | --- |
| `lookup(host: string) -> Result[Vector[IpAddress], IoError]` | Every address `host` stands for, in the order the platform prefers them, each once. A numeric address is its own answer and asks nothing of the network. A name the resolver finds nothing for is `HostNotFound`, and so is one that could not be a host's, refused before anything is asked: empty, longer than `HostNameMost` (254) bytes, or holding white space or a control character. The lookup runs on one of the runtime's helper threads and is waited for as a socket is: a lightweight thread parks, and a cancellation or a `withTimeout` deadline ends the wait at once. |
| `IpAddress.parse(text: string) -> Option[IpAddress]` | The address `text` writes: dotted decimal, four parts of at most 255 with no leading zeros; or eight groups of one to four hexadecimal digits in either case, one run of zero groups written `::`, the last two groups perhaps an IPv4 address, then perhaps `%` and the number of the interface it is scoped to. `None` for anything else: brackets, a port, white space, a short form such as `127.1`, or an interface named rather than numbered. |
| `IpAddress.v4(a: int, b: int, c: int, d: int) -> IpAddress` | The address `a.b.c.d`; a part outside 0 to 255 traps. |
| `IpAddress.fromBytes(bytes: Vector[byte]) -> Option[IpAddress]` | The address of four bytes or sixteen, the most significant first; `None` for any other number. |
| `IpAddress.Localhost`, `LocalhostV6`, `Unspecified`, `UnspecifiedV6` | `127.0.0.1`, `::1`, `0.0.0.0` and `::`. |
| `a.isIpv4`, `a.isIpv6 -> bool` | Its version. |
| `a.bytes() -> Vector[byte]` | Its four or sixteen bytes, the most significant first. |
| `a.scopeId -> int` | The interface a version 6 address is scoped to, zero for none. |
| `a.isLoopback`, `a.isUnspecified`, `a.isMulticast -> bool` | Whether it is in `127.0.0.0/8` or is `::1`; is `0.0.0.0` or `::`; is in `224.0.0.0/4` or `ff00::/8`. |

### UDP

A `UdpSocket` sends and receives datagrams: each `sendTo` is one datagram and each `receiveFrom`
takes one whole, with who sent it. A datagram may be lost, two may arrive in either order, and one
longer than the room a receive gives loses its rest, which the `Datagram` says. A socket is sendable
and owned by one thread at a time, as a TCP connection is, and a receive, or a send waiting for room,
parks a lightweight thread on the runtime's poller, ended by a cancellation like any other wait.

| Function | Meaning |
| --- | --- |
| `UdpSocket.bind(host: string, port: int, reusePort: bool = false) -> Result[UdpSocket, IoError]` | A socket bound to `port` on the host's first address: `127.0.0.1` or `::1` for this machine alone, `0.0.0.0` or `::` for all of its addresses; port `0` lets the platform choose. With `reusePort`, several sockets that each ask for it share one port, the platform dividing the datagrams among them. |
| `UdpSocket.bind(address: IpAddress, port: int, reusePort: bool = false)` | The same, on an address. |
| `u.port() -> int`, `u.localAddress() -> IpAddress` | Where it is bound. |
| `u.sendTo(bytes: Array[byte], to: IpAddress, port: int) -> Result[unit, IoError]` | Sends `bytes` as one datagram, empty ones included. The address must be of the socket's own version. A datagram longer than the version carries, `DatagramMostIpv4` (65507) or `DatagramMostIpv6` (65527) bytes, fails without being sent. |
| `u.sendTo(bytes: Array[byte], host: string, port: int)` | The same, to the host's first address of the socket's version. |
| `u.receiveFrom(max: int) -> Result[Datagram, IoError]` | Waits for the next datagram and answers up to `max` of its bytes in a `Datagram(bytes, sender, senderPort, truncated)`; `truncated` says it was longer and the rest is lost. `max` is at least one (zero traps); a `max` of `DatagramMost` (65535) or more never truncates. |
| `u.connect(to: IpAddress, port: int)`, `u.connect(host: string, port: int)` `-> Result[unit, IoError]` | Points the socket at one peer: `send` goes there, and only its datagrams arrive. A port there that nothing listens on may make a later receive answer `ConnectionRefused`. |
| `u.connectedTo() -> Option[(IpAddress, int)]` | Where `connect` pointed it. |
| `u.send(bytes: Array[byte]) -> Result[unit, IoError]` | Sends one datagram to where it is connected; an `Err` where it is not. |
| `u.setTimeout(d: Duration)`, `u.timeout` | As a TCP connection's: a receive or a send that waits longer fails with `TimedOut`; zero or less takes the limit away. |
| `u.setReceiveBufferSize(bytes: int)`, `u.receiveBufferSize()`, `u.setSendBufferSize(bytes: int)`, `u.sendBufferSize()` | Asks the platform for buffers of a size, and says what it gave. A new socket's send buffer has room for the longest datagram. |
| `u.readable()`, `u.readinessDescriptor()`, `u.writable()`, `u.writabilityDescriptor()` | As a TCP connection's, for a [`select`](/spec/threads/#select)'s `u.readable()` and `u.writable()` arms. |
| `u.close() -> Result[unit, IoError]` | Closes it and answers `Ok`; closing again does nothing, and every other operation afterwards answers an `Err`. |

```text
import avin.io.IoError
import avin.net.{IpAddress, UdpSocket}
import avin.threads.spawn

main() -> Result[unit, IoError]
  val server = UdpSocket.bind("127.0.0.1", 0)?
  val port = server.port()
  val echo = spawn(() ->
    server.receiveFrom(1500).flatMap(d -> server.sendTo(d.bytes, d.sender, d.senderPort)))
  val client = UdpSocket.bind(IpAddress.Localhost, 0)?
  client.sendTo("ping".utf8, IpAddress.Localhost, port)?
  val reply = client.receiveFrom(1500)?
  println(s"${string.fromUtf8Lossy(reply.bytes)} from ${reply.sender}")
  echo.join()
```

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
| `TlsStream.connect(host, port, settings: TlsClient = TlsClient(), timeout = seconds(30)) -> Result[TlsStream, TlsError]` | A TCP connection as `TcpStream.connect` makes one, then TLS as the client, the certificate checked for `host`. The lookup, the connecting and the handshake together must be done within `timeout`, else the connection is closed and the error is `Transport` of kind `TimedOut`. The connection made has no limit on its waits until `setTimeout` gives it one. |
| `TlsStream.client(tcp: TcpStream, host, settings = TlsClient(), timeout = Duration.Zero) -> Result[TlsStream, TlsError]` | TLS as the client over a connection the caller made, each of its handshake's waits with `tcp`'s limit and the whole handshake within `timeout`, else `Transport` of kind `TimedOut`; `tcp`'s limit is put back afterwards. Where it fails, `tcp` is the caller's to close. |
| `TlsStream.server(tcp: TcpStream, settings: TlsServer, timeout = Duration.Zero) -> Result[TlsStream, TlsError]` | TLS as the server over an accepted connection, presenting the settings' certificate, its handshake bounded as `client`'s is. |

A handshake's waits are each bounded by the connection's own limit, but a peer sending a byte just
inside that limit, again and again, would hold a handshake open for ever; the `timeout` bounds the
whole of it, each wait cut to what is left. Zero takes the bound away, as it does for the low-level
`client` and `server` unless asked, since their caller owns the connection and its timing; a
negative `timeout` traps.
| `s.read`, `readLine`, `readAll`, `write`, `writeText`, `setTimeout` | As a `TcpStream`'s, of plaintext; the limit is the TCP connection's, for each of its waits. |
| `s.readable() -> bool`, `s.readinessDescriptor() -> int` | Whether a read of plaintext would answer now: plaintext is buffered or TLS holds some decrypted, a whole record of it has arrived, or the peer has closed TLS or the connection. It hands TLS the ciphertext already on the socket, so part of a record is not taken for plaintext, and never waits for more; a failure it meets is answered by the next read. The descriptor is the TCP connection's. |
| `s.shutdownOutput()` | Closes TLS (a `close_notify`) and the TCP connection's sending side; reading goes on. |
| `s.close() -> Result[unit, IoError]` | Closes TLS as far as the peer can be told without waiting, then the connection; the `Err` is a failure to send that notice, the connection being closed either way. Again does nothing and answers `Ok`. |
| `TlsClient(trusted = [])`, `c.trust(pem: string) -> TlsClient` | The machine's trusted authorities and those added: `trust` answers a client that also trusts the certificates in `pem`. |
| `TlsServer(certificate, key)`, `TlsServer.of(certificate, key) -> Result[TlsServer, TlsError]` | A server's certificate (a chain, leaf first) and private key as PEM text; `of` checks they can be used and go together, and one made directly is checked at its first connection. |
| `c.offer(names: Vector[string])`, `server.offer(names)` | The settings offering the application protocols `names` by ALPN, most preferred first, in place of those offered before; `[]` offers none, the default. A name empty or over 255 bytes long, or names over 65535 bytes in all, each with its length byte, trap. |
| `c.versions(min: TlsVersion, max: TlsVersion)`, `server.versions(min, max)` | The settings speaking the protocol versions from `min` to `max`, both included; `Tls12` to `Tls13` by default. A `min` newer than `max` traps. |
| `protocols`, `minVersion`, `maxVersion` | The settings' fields, also parameters of `TlsClient(…)` and `TlsServer(…)` after their own, defaulted; one made directly with a name ALPN cannot carry, or `minVersion` newer than `maxVersion`, traps at its first connection. |
| `s.protocol -> Option[string]`, `s.version -> TlsVersion` | The application protocol the two ends agreed on, `None` where one offered none or the server chose none, and the protocol version spoken; still known after `close` and `split`. |
| `TlsVersion`: `Tls12`, `Tls13`; `v.name` | A protocol version, its `name` and display `TLS 1.2` or `TLS 1.3`. Nothing older can be named. |
| `trait TlsSource`, `source.current -> TlsServer` | Where a server's settings come from, asked as each connection's handshake begins. A `TlsServer` is its own, the same every time. |
| `ReloadableTlsServer(initial: TlsServer)`, `r.reload(next: TlsServer)`, `r.reload(certificate, key) -> Result[unit, TlsError]` | Settings that may be replaced while a server runs, from any thread. Each connection keeps the settings it began with, so a reload changes the connections that follow and none already open. `reload(certificate, key)` keeps the protocols and versions; `Credentials`, the settings unchanged, where the two cannot be used together. |

**What a client checks, and what cannot be turned off.** The server's certificate must chain to an
authority the client trusts: the machine's store, read through OpenSSL's default paths (on macOS
the `cert.pem` of Homebrew's OpenSSL, not the Keychain), and the authorities `trust` adds. Its name
must cover the host connected to, a name or an address. The host's name is sent for the server to
choose a certificate by, and the oldest protocol spoken is TLS 1.2. `trust` only adds: there is no
way to accept a certificate these refuse, a private authority, a corporate one and a test's own
being the same case. The choices are [proposed](/spec/decisions/#proposed).

**Versions and application protocols.** Either end speaks TLS 1.2 and TLS 1.3 unless `versions`
narrows that, to TLS 1.3 alone for instance; nothing widens it below TLS 1.2, and two ends whose
ranges do not meet fail the handshake as `Handshake`. Each end may offer application protocols by
ALPN. Where both do, the server chooses the first of its own the client offered, so its order wins,
and where none is offered by both the handshake fails as `Handshake`, as RFC 7301 asks; where either
offers none, or the server chooses none, the connection's `protocol` is `None` and it goes on.
Names are compared byte for byte: `h2` is not `H2`, nor `h2c`.

**Reloading a certificate.** A `ReloadableTlsServer` holds a server's settings and may be
replaced while it serves, a renewed certificate for instance; `http.serve` takes one as its `tls`,
as it takes a `TlsServer`. Each connection takes the settings current as its handshake begins and
keeps them, so the connections accepted after a reload present the new certificate and every
connection already open goes on, unbroken. A certificate and key that do not go together are
refused and the old settings kept:

```text
import avin.fs
import avin.io.IoError
import avin.net.http
import avin.net.http.Response
import avin.net.tls.{ReloadableTlsServer, TlsError, TlsServer}
import avin.signal

main() -> Result[unit, TlsError | IoError]
  val identity = TlsServer.of(fs.readText("cert.pem")?, fs.readText("key.pem")?)?
  val tls = ReloadableTlsServer(identity.offer(["http/1.1"]))
  _ = http.serve("0.0.0.0", 8443, r -> Response.text(200, "hello"), tls = Some(tls))?
  // A hangup reads the certificate and key again; the connections open go on as they were.
  val hangup = signal.watch(signal.Hangup)?
  while true
    hangup.wait()
    tls.reload(fs.readText("cert.pem")?, fs.readText("key.pem")?) match
      Ok(_) -> println("reloaded")
      Err(e) -> println(s"kept the old certificate: ${e.message}")
  Ok(())
```

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
words. Each `TlsStream`, `TlsClient`, `TlsServer` and `ReloadableTlsServer` is sendable, a stream belonging to one thread
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
| `TcpServer.start(host, port, serve: @sendable (TcpStream, TcpServer) -> unit, maxConnections = 1024, timeout = Duration.Zero, failed: @sendable (TcpStream) -> unit = _ -> (), listen = ListenOptions()) -> Result[TcpServer, IoError]` | Listens as `TcpListener.bind` does with the [options](#socket-options) `listen`, and serves each connection by `serve`, given the connection and the server. |
| `TcpServer.startUnix(path, serve: @sendable (UnixStream, TcpServer) -> unit, maxConnections = 1024, timeout = Duration.Zero, failed: @sendable (UnixStream) -> unit = _ -> (), backlog = 128, replacing = false) -> Result[TcpServer, IoError]` | Listens at a Unix socket as `UnixListener.bind` does with `backlog`, or `bindReplacing` where `replacing`, and serves each connection as `start` does. The socket file stays once it stops. |
| `trait Listener[S: ByteStream]: Sendable, io.Close` | What a server accepts from, with `accept() -> Result[S, IoError]`: `TcpListener` is a `Listener[TcpStream]`, `UnixListener` a `Listener[UnixStream]`. |
| `s.port() -> int` | The port it listens on, the one the platform chose for port 0; zero for a Unix socket's. |
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
16384, maxBodyBytes = 1048576, tls: Option[TlsSource] = None, errors: ErrorPage = plainError,
date: bool = true, requestTimeout = seconds(60), handshakeTimeout = seconds(10), listen: ListenOptions
= http.ServerListen, streamsBody: @sendable (ServerRequest) -> bool = http.bufferedBodies,
headerTimeout = seconds(10), maxHeaders = 100, metrics: Option[ServerMetrics] = None) ->
Result[TcpServer, IoError]` listens as `TcpServer.start` does, with the
[socket options](#socket-options) `listen`; with `metrics` it counts every request it reads,
answers or refuses (**Metrics**, below). **`http.ServerListen`** is the listener's defaults with
each connection's `noDelay` on, since a response is written in pieces a client waits on whole, and
`keepAlive` `Some(KeepAlive())`, so a connection whose client vanished, a stream of events or a
WebSocket above all, which no idle timeout ends, is found out within two minutes. A server behind a
proxy on the same machine serves at a Unix socket with **`http.serveUnix(path, handler, …)`**, which
takes every parameter of `serve` but `host`, `port`, `tls`, `handshakeTimeout` and `listen`, and
`backlog = 128` and `replacing = false` as `TcpServer.startUnix` does; its requests' `remote` is
`None`. **Each
response carries a `Date` header** (RFC 9110 section 6.6.1), the time it is sent as an IMF-fixdate
(`Sun, 06 Nov 1994 08:49:37 GMT`, as `Instant.toHttpDate()` writes it), on every status: a
handler's, the server's own `400` or `503`, sized, chunked, closing and event-stream responses
alike. A handler that sets `Date` (in any case of the name) keeps its own value, and `date = false`
sends none; the `101` of an upgrade carries none. A `DateHeader(clock = now)` formats its text once
a second and hands it to every connection, with no lock, so sending the header costs a clock read
and a comparison; `DateHeader.value()` gives the text. Each
wait while a request is read may last `readTimeout`, each wait while a response is written
`writeTimeout`, and the wait for the next request on a connection kept alive `idleTimeout`; and the
whole of a request, its line, headers and body, must arrive within `requestTimeout` of its first
byte, so a client trickling bytes just faster than `readTimeout` cannot hold a connection, and its
slot among `maxConnections`, for ever. Each request on a kept-alive connection has a bound of its
own, and the idle wait before it is no part of it. A request's line and headers must arrive within
`headerTimeout`, counted for a connection's first request from its acceptance, so a client that
connects and never sends is closed then, and for a later one from its first byte. With `tls`, each
connection's TLS handshake, from the connection's acceptance, must be done within
`handshakeTimeout`, each of its waits also bounded by `readTimeout`; a connection whose handshake
runs out of time is closed unanswered, since no HTTP answer can reach it before TLS is set up, and
its slot freed. Zero takes any of these limits away. A `maxHeaderBytes` or `maxHeaders` less than
one, a `maxBodyBytes` less than zero or a negative `requestTimeout`, `headerTimeout` or
`handshakeTimeout` traps.
A server taking bodies larger than the default `maxBodyBytes` from slow clients raises
`requestTimeout` with it. The
wait for a connection's next request, its first among them, is made through `idle`, so a shutdown
closes a connection waiting there at once, unanswered; a request already being read or answered
finishes, its response saying `Connection: close`, and a request read once the server is stopping
is answered `503`.

**With `tls`, the server speaks HTTPS.** Each connection's actor begins with a TLS handshake
presenting the `TlsServer`'s certificate, each of its waits lasting at most `readTimeout`, and
serves its requests over the `TlsStream` exactly as over TCP, kept alive and pipelined alike. A
connection whose handshake fails, a client speaking plain HTTP among them, is closed, and the server
goes on. A handler that traps on a TLS connection has it closed without the `500`, since the answer
would have to be written through TLS the handler's turn held. `tls` is a `TlsSource`, a `TlsServer`
or a `ReloadableTlsServer`, whose settings each handshake takes as it begins, so a reload while the
server runs changes the connections that follow and leaves those open as they were. Settings that
offer no application protocol of their own offer `http/1.1` by ALPN, so a client offering `h2` and
`http/1.1` is answered `http/1.1`, and one offering `h2` alone is refused in the handshake.

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

**Cookies** (RFC 6265). `request.cookies()` reads every `Cookie` header into `Cookie(name, value)`s
in order, `request.cookie(name)` the first value of a name; `Cookie.parse(header)` reads one header.
Pairs are split at `;`, space around a pair, name and value is dropped, a value in double quotes is
read without them, a name seen twice gives two cookies, an empty value is a cookie and a `=` in a
value belongs to it. A pair that is not a cookie is skipped and the rest kept: empty, no `=`, a name
that is not a token, a value with an octet RFC 6265 does not allow (space, `"`, `,`, `;`, `\`,
non-ASCII). `SetCookie(name, value, expires, maxAge, domain, path, secure, httpOnly, sameSite,
partitioned)` is a cookie to set; `header` is its `Set-Cookie` value, attributes in that order, `Expires`
written by `Instant.toHttpDate()`, `Max-Age` in seconds (zero or less written `0`), `sameSite` an
`Option[SameSite]` of `Strict`, `Lax` or `Unrestricted` (written `SameSite=None`). A cookie is checked
where it is made and traps where it breaks a rule, nothing being escaped or cut: the name is a
token; the value holds only cookie octets; `Domain` is letters, digits, `.` and `-`; `Path` begins
with `/` and holds no `;` or control character; a `__Host-` name (any case) needs `Secure`,
`Path=/` and no `Domain`; a `__Secure-` name needs `Secure`; `SameSite=None` and `Partitioned` need
`Secure`. `response.setCookie(cookie)` adds one `Set-Cookie` header per cookie, never folded. On the
client side `response.cookies()` and `SetCookie.parse(text)` (a `Result[SetCookie, CookieError]`)
read `Set-Cookie` into the same type: attribute names are case-insensitive, the last of a repeated
attribute wins, and an unknown attribute, an `Expires` that is not an HTTP-date, a non-numeric
`Max-Age`, a `Path` not beginning with `/`, an empty `Domain` and an unknown `SameSite` are ignored;
a bad name or value, a broken prefix rule, or `SameSite=None` / `Partitioned` without `Secure` is an
`Err`, and `response.cookies()` skips it. A cookie jar is not part of this.

`Route(method, pattern, handler)`, or `Route.get`, `post`, `put`, `delete` and `patch`, routes a
method and a pattern; a method that is not a token or a pattern not beginning with `/` traps.
`Router(routes)` tries its routes in order. A pattern's segments match a request's decoded segments
one for one, a segment `:name` matching any one that is not empty and binding it, so Unicode paths
route whether escaped or not. A last segment `*name` matches the rest of the path, none of it or
several segments, binding them joined by `/` (`/files/*path` binds `a/b` for `/files/a/b`, and `""`
for `/files` and `/files/`); a `*name` before the last segment traps, and a bare `*` is a literal. A `GET` route answers `HEAD` too where no `HEAD` route matches. A path
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

**Forms.** `request.form()` reads an `application/x-www-form-urlencoded` body as a `Form`: its
`pairs` in order, a name given more than once kept each time, `value(name)` the first value of a
name and `values(name)` every one. The body is decoded by `Url.formDecode`, as UTF-8 whatever
`charset` the type names. `request.multipart(limits = MultipartLimits())` reads a
`multipart/form-data` body (RFC 7578) as a `Multipart`: its `parts` in order, `field(name)` the text
of the first part of that name that is not a file, `file(name)` the first that is, and
`named(name)` every part of that name. A `Part` has its `name`, its `filename` where it is a file
(`isFile`), its `contentType` (`text/plain` where it names none), its `headers` with `header(name)`,
its `body` and `text`. **A filename is the sender's text, untrusted**: it may hold `/`, `..`, a drive
letter or anything else, and is never a path to write to as given; the library writes nothing to
disk from it. `Multipart.parse(body, contentType, limits)` reads a body that did not come to a
server, such as a client's response.

Each answers `Result[_, FormError]`, and `FormError` is a `ToResponse`, so `?` ends a handler with it:

| Error | Status | When |
| --- | --- | --- |
| `NotAForm(expected, found)` | 415 | The `Content-Type` is not the form's type, or there is none. |
| `BadEncoding(UrlError)` | 400 | An urlencoded body with a malformed `%` escape or bytes that are not UTF-8. |
| `NoBoundary` | 400 | No `boundary` parameter, or one that is not 1 to 70 of RFC 2046's characters, not ending in a space. |
| `BadMultipart(reason)` | 400 | No delimiter, no closing delimiter, a part's headers unended, not UTF-8 or not `name: value`, or a part without `Content-Disposition: form-data` and a `name`. |
| `TooManyParts(limit)` | 413 | More parts than `limits.maxParts` (1000 by default). |
| `PartHeadersTooLarge(limit)` | 413 | A part's headers longer than `limits.maxHeaderBytes` (8192). |
| `FormTooLarge(limit)` | 413 | A body longer than `limits.maxBytes` (10 MiB); the server's own `maxBodyBytes` bounds it first. |

A multipart body is read as RFC 2046 writes one: text before the first delimiter and after the
closing one is skipped, every line ends in CRLF, a delimiter's line may end in spaces and tabs, and
bytes that resemble the boundary but are not followed by `--` or the end of the line stay in the
part. A name or filename is read as the WHATWG HTML standard writes one, quoted with `%22`, `%0D`
and `%0A` standing for `"`, CR and LF and no backslash escapes; a `filename*` (RFC 5987) in UTF-8 or
ISO-8859-1 is preferred to `filename`, and one in another charset or malformed is ignored. `parse`
and `multipart()` read a body whole; a large upload is read a part at a time by a `MultipartReader`
on a streamed body, under the same rules, limits and errors (see *Streamed request bodies* below).

```text
upload(r: ServerRequest) -> Result[Response, ToResponse]
  val form = r.multipart()?
  val title = form.field("title").getOrElse("untitled")
  val size = form.file("doc").map(p -> p.body.length).getOrElse(0)
  Ok(Response.text(200, s"$title: $size bytes\n"))
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

`http.accessLog(logger, clock = monotonic, ids = newRequestId)` is the same middleware writing
through an [`avin.log`](#avinlog) `Logger`: an `Info` record `request` for each request, its fields
`id`, `method`, `path`, `status`, `bytes`, `micros` and `remote` after the logger's own, a length or
an address not known left out, in the logger's format. A logger that leaves out `Info` records
makes none, though the request id is still found, passed on and answered.

```text
2026-11-23T10:20:30.5Z INFO  request svc=web id=4f1c…e2 method=GET path=/items/7 status=200 bytes=12 micros=1500 remote=127.0.0.1
```

**Metrics.** Request metrics are families of an [`avin.metrics`](#avinmetrics) `Registry`, so a
program's own counters are served beside them. `Metrics(buckets = defaultBuckets, clock = monotonic,
registry = Registry(), maxSeries = 10000)` counts the requests its `middleware` passes on:
`http_requests_total` by `method`, `route` (the route's pattern, `""` where none was chosen) and
`status` class (`2xx`); `http_request_duration_seconds`, a histogram of `buckets` (5 ms to 10 s by
default), a time equal to a bound counted in that bound's bucket, with its `_sum` and `_count`; and
the gauge `http_requests_in_flight`. A method HTTP defines (`GET`, `HEAD`, `POST`, `PUT`, `DELETE`,
`CONNECT`, `OPTIONS`, `TRACE`, `PATCH`) is its own label and any other is `_other`
(`methodLabel(method)`), so made-up methods make no new series; each family holds at most
`maxSeries` series, the rest counted in its overflow series. `render` is the registry in the
Prometheus text exposition format, every family in it; `handler` answers it as `200` of the type
`MetricsContentType` (`text/plain; version=0.0.4; charset=utf-8`), and `route(pattern = "/metrics")`
mounts it; `metricsHandler(registry)` and `metricsRoute(registry, pattern = "/metrics")` serve any
registry. `observe(method, route, status, took)` counts a request answered another way, and
`inFlight` is the gauge's value. Buckets not in increasing order, or one not above zero, trap, and so
does a second `Metrics` in one registry.

**The server's own metrics.** `ServerMetrics(buckets = defaultBuckets, clock = monotonic, registry =
Registry())`, given to `serve` as `metrics = Some(…)`, counts every request the server reads, by
method and status class, in `http_server_requests_total`; the time from the request read to its
response written, a streamed body included, in `http_server_request_duration_seconds` by method;
the requests being answered in `http_server_requests_in_flight`; and the requests refused before
any handler saw them (`400`, `408`, `413`, `414`, `431`) in `http_server_rejected_total` by status
code. A handler that traps is counted as answering `500`, its time and its place in flight ending
with its connection, over TCP and TLS alike, so this gauge is exact where a middleware's cannot be;
an upgrade is counted as its `101`.
`started(method) -> InFlight` and `rejected(status)` count a request served another way; an
`InFlight`'s first `finish(status)` counts it and answers `true`, any later one nothing.

```text
val registry = Registry()
val orders = registry.counter("shop_orders_total", "Orders placed, by payment.", ["payment"])
val metrics = Metrics(registry = registry)
val router = Router(Route.group("/api", routes, metrics.middleware) :+ metricsRoute(registry))
val server = http.serve("0.0.0.0", 8080, router.handle, metrics = Some(ServerMetrics(registry = registry)))
```

Wrapping a group or a route labels each request by its route; wrapping a router's `handle` counts the
`404`s and `405`s too, every request labelled `""`. A handler that traps is not counted by the
middleware, and leaves its gauge one higher, since a trap runs nothing after it; the server's own
metrics count it.

**Rate limiting.** `RateLimiter(rate, per, burst = rate, key = clientAddress, maxKeys = 10000, clock =
monotonic, errors = plainError)` gives each key a token bucket of `burst` tokens filled again at
`rate` every `per`. Its `middleware` takes a token for each request, keyed by `key` (the client's
address by default, `""` for a request with none), and answers one finding its bucket empty `429`,
rendered by `errors`, with `Retry-After` the seconds until the next token, rounded up and at least
one. `admit(key) -> Result[unit, Duration]` takes a token directly, `Err` saying how long until the
next. At most `maxKeys` buckets are held (`size` says how many): making one more forgets every bucket
full again, which a new one would equal, and where none is, the one nearest to full. A rate, burst
or `maxKeys` below one traps, and so does a `per` too short to give each token a whole microsecond.
The buckets are [`avin.resilience`](#avinresilience)'s `RateLimiter`, which limits work of any kind.
The three middlewares compose as any do: `h -> metrics.middleware(limiter.middleware(h))` counts the
`429`s the limiter answers.

**CORS.** `Cors(origins = ["*"], allowOrigin = _ -> false, methods = ["GET", "HEAD", "POST"], headers =
[], exposed = [], credentials = false, maxAge = None)` is a policy whose `middleware` lets a page from
another origin read responses. An origin is allowed if `origins` holds it (compared exactly), holds
`"*"`, or `allowOrigin` says so; `allows(origin)` asks. An allowed request's response gets
`Access-Control-Allow-Origin` (`*` where the policy is `*` without credentials, else the origin,
with `Vary: Origin` merged into any `Vary` the handler set, once, and left alone where it is `*`),
`Access-Control-Allow-Credentials: true` where `credentials`, and `Access-Control-Expose-Headers`
where `exposed` names some. A request with no `Origin`, or from one not allowed, is handled as it
is, with no CORS header. A preflight (`OPTIONS` with `Origin` and `Access-Control-Request-Method`)
is answered `204` and never reaches the handler; from an allowed origin asking for a method in
`methods` and headers in `headers` (compared without regard to case; any, where `headers` is empty,
echoed back) it carries `Access-Control-Allow-Methods`, `-Allow-Headers`, `-Max-Age` (whole
seconds) and the origin and credentials headers; asking for more, or from an origin not allowed, it
carries none. `*` together with `credentials`, and a negative `maxAge`, trap where the policy is
made.

**Trusted proxies.** `TrustedProxies(trusted = [], maxHops = 32)` is a middleware (`.middleware`)
that reads where a request came from behind a reverse proxy. `trusted` is a list of CIDR blocks
(`Cidr` texts: `10.0.0.0/8`, `2001:db8::/32`, or one address); a block that is not one traps where
the middleware is made, as does a `maxHops` below one. Forwarding headers are believed only from a
connection whose address is in `trusted`; on any other request (or one with no known address) they
are removed, so `Forwarded` and the `X-Forwarded-` headers are never a client's claim downstream.
From a trusted peer, `Forwarded` (RFC 7239: `for`, `proto`, `host`, quoted values, several
elements) is read where present, else `X-Forwarded-For`, `-Proto` and `-Host`; several header lines
are one list and empty entries are skipped. The client is found by walking the addresses from the
nearest hop towards the first, each trusted address being another proxy and the first untrusted
one the client, so entries a client prepended are never believed. An address is IPv4, IPv6 (in
brackets or not) with an optional port that is dropped, and is given in its shortest form, IPv4-mapped
as IPv4. The scheme (`http` or `https`) and the host (a name or IP literal with an optional port)
are those the outermost trusted proxy wrote, in lower case; an invalid one is not given. An entry
walked that is not an address (`unknown`, `_hidden`), more than `maxHops` entries, or malformed
`Forwarded` syntax makes the headers worthless: the client is the peer and no scheme or host is
given. The request handed on has `remote` set to the client, and `forwarding` (`Option[Forwarding]`:
`peer`, `address`, `scheme`, `host`), `scheme` and `host` (the forwarded one, else the `Host`
header) answer on every `ServerRequest`; `withRemote` and `withHeaderRemoved` make the changed
copies. `Cidr(text)` has `network`, `prefix` and `contains(address)`; an IPv4 address and its
`::ffff:` form are one address, the bits past the prefix are ignored, and a zone or a prefix out of
range traps. `trusted.isTrusted(address)` asks the list.

```avin
import avin.net.http.{Handler, Response, ServerRequest, TrustedProxies}

main()
  val proxies = TrustedProxies(["10.0.0.0/8"])
  val handler: Handler = r -> Response.text(200, s"${r.remote.getOrElse("-")} ${r.scheme.getOrElse("http")}")
  val app = proxies.middleware(handler)
  val headers = [("X-Forwarded-For", "6.6.6.6, 203.0.113.9"), ("X-Forwarded-Proto", "https")]
  println(app(ServerRequest("GET", "/", headers, remote = Some("10.0.0.5"))).text)
  println(app(ServerRequest("GET", "/", headers, remote = Some("198.51.100.7"))).text)
```

This prints `203.0.113.9 https`, then `198.51.100.7 http`: the second peer is not a proxy.

**Health and graceful drain.** `http.health(checks = [], live = [], ready = true, livePath =
"/livez", readyPath = "/readyz")` answers what a supervisor asks. `http.check(name, probe, timeout =
seconds(2))` is a named check: `probe` answers `Ok(())` or `Err(reason)` and runs on a thread of its
own; every check of a route is started at once, each is given its `timeout` counted from its start,
and one still running by then is reported as timed out and left to end by itself. A probe answers its
failures as `Err`: a trap in it stops the program, as any thread's does. A `name` that is empty or
holds a control character, a `timeout` of zero or less, and a path not beginning with `/` trap.
`health.live(request)` and `health.ready(request)` are handlers, and `health.routes()` the two `GET`
routes for a `Router`. Liveness runs the `live` checks (none by default: a process that answers is
alive), readiness the `checks`; each answers `200` where every check passes and `503` otherwise, with
`Cache-Control: no-store`. The body is plain text: `ok` or `failed`, and with `?verbose` one line a
check before it, `[+] db ok`, `[-] cache failed: connection refused` or `[-] slow timed out after
50ms`, a reason cut to one line of at most 200 characters. `health.setReady(false)` makes readiness
answer `503` at once without running its checks (`[-] ready failed: not ready` in the verbose form),
and `setReady(true)` undoes it; `ready = false` starts a program not ready; `isReady()` asks.

`http.drain(server, health = None, deadline = seconds(30), grace = Duration.Zero)` stops a server in
the order a load balancer needs: `health` stops being ready, the server waits `grace` still
accepting, so the balancer sees it, then stops accepting, and the requests in flight have `deadline`
to finish. It answers whether every connection had ended by then; where not, the stragglers go on
running and the caller decides whether to end the program. `http.serve` itself handles no signal.
`http.drainOnSignal(server, health = None, deadline, grace, signals = [Terminate, Interrupt])`
watches the signals, waits for one (or for the server to be stopped by other code), then drains,
answering `Result[bool, IoError]`. While it waits the signals no longer end the program.

```avin
import avin.net.http
import avin.net.http.{Response, Route, Router}
import avin.time.seconds

main()
  val db = http.check("db", () -> Ok(()))
  val health = http.health([db])
  val hello = Route.get("/", _ -> Response.text(200, "hello\n"))
  val router = Router(hello +: health.routes())
  http.serve("127.0.0.1", 8080, router.handle) match
    Ok(server) -> println(http.drainOnSignal(server, Some(health), grace = seconds(5)))
    Err(e) -> println(e.message)
```

**Reopening the log.** A file sink opened by `fileSink` follows a log rotator by itself, looking
again every `recheck`. A rotator that signals the program wants the file reopened at once:
`log.reopenOnSignal(sink)` does so on every `Hangup`, through `sink.reopen()`, until the `Reopener`
it answers is stopped.

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

**Static files.** `FileServer(root, index = Some("index.html"), listing = false, dotfiles = false,
linksOutside = false, cacheControl = None, types = MimeTypes.standard, chunkBytes = 65536, errors =
plainError)` serves the files under the directory `root`; `server.at(prefix)` is the `GET` route
`prefix/*path` serving them below `prefix` (`at("/")` serves every path), and `server.handle` is the
handler itself, which serves the whole request path. It answers `GET` and `HEAD`, any other method
`405` with `Allow: GET, HEAD`:

```text
import avin.net.http
import avin.net.http.{FileServer, Router}

main()
  val site = FileServer("public", cacheControl = Some("public, max-age=3600"))
  http.serve("0.0.0.0", 8080, Router([site.at("/static")]).handle).ok.get.join()
```

A file is answered `200` with its type, its length, `Accept-Ranges: bytes`, a weak `ETag`
(`W/"<size>-<modified>"`, the size in bytes and the modification time in microseconds, both in
hexadecimal), its `Last-Modified` as an HTTP-date, and `Cache-Control` where `cacheControl` gives
one. Its body is streamed from the file `chunkBytes` at a time with `Response.stream`, so memory does
not grow with the file; `HEAD` has the same headers and no body, and does not open the file.

Conditional requests follow RFC 9110 §13.2.2: an `If-Match` that does not match strongly, or with no
`If-Match` an `If-Unmodified-Since` the file has changed since, answers `412`; an `If-None-Match` that
matches weakly, or with no `If-None-Match` an `If-Modified-Since` the file has not changed since
(compared in whole seconds), answers `304` with the `ETag`, `Last-Modified` and `Cache-Control` and
no body. A date that is not an HTTP-date is ignored. Because the tag is weak, only `If-Match: *`
matches it strongly. A `Range` is honoured on `GET` after the conditions: one byte range (`bytes=0-0`,
`bytes=500-`, `bytes=-500`, a last position past the end held at the end, the unit compared without
regard to case) is answered `206` with `Content-Range: bytes first-last/length`; one starting at or
past the end, or a suffix of zero bytes, `416` with `Content-Range: bytes */length`. A `Range`
that is malformed, of another unit or of several ranges is ignored and the whole file is sent `200`.
An `If-Range` keeps the range only where it is the file's `Last-Modified` exactly, a date at least a
second old; an entity tag there never matches the weak tag, so the whole file is sent.

A path naming a directory without its final `/` is redirected there with `301`, the query kept; a
directory is answered with its `index` file, or, without one, with an HTML listing of its entries
where `listing`, otherwise `404`. The path is refused `400` where a decoded segment is `.` or `..` or
holds `/`, `\` or NUL (so `%2e%2e`, `%2F` and `%00` are refused as plainly written ones are); a name
beginning with `.` anywhere in the path is `404` unless `dotfiles`; a path whose links resolve
outside `root` is `403` unless `linksOutside`, a directory's index file included; a file that cannot
be read is `403`; a missing one, a file asked for with a final `/`, and anything that is neither file
nor directory are `404`. Empty segments (`//`) are passed over. The error responses are rendered by
`errors`.

`MimeTypes.standard` gives the types of the web by a file's extension, compared without regard to
case: pages, styles and scripts (`text/html`, `text/css`, `text/javascript`, each with `;
charset=utf-8`, as every `text/` type there is), data (`application/json`, `xml`, `yaml`, `wasm`,
...), images, fonts, audio, video and archives, and `application/octet-stream` for a name it does not
know or one with no extension (`.bashrc` has none). `types.typeOf(name)`, `types.get(extension)`,
`types.with(extension, contentType)` (an empty extension, one holding `.` or `/`, or a type holding a
line break traps) and `types.withFallback(contentType)` read and extend a table.

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
Empty lines before a request line are passed over. Every line ends in `\r\n`: a bare `\n`, a bare
`\r` or another control character than a tab is refused, so no two readers frame a request
differently (RFC 9112 sections 2.2 and 5). A body is read
by `Content-Length` or chunked (extensions and trailers passed over); an `Expect: 100-continue` is
answered `100 Continue` once the body is known to fit. The server writes `Content-Length` and
`Connection` itself, in place of any the handler gave, every response as `HTTP/1.1`, and no body for
`HEAD`, `204` and `304`; a streamed response's framing is its own, as above. A handler's status
outside 200 to 599, or a header the response cannot carry, traps.

**A request the server cannot take is answered and the connection closed**, gently: its side is shut
and what the client still sends read and dropped for half a second at most, so the answer is not
lost to a reset and a client that goes on trickling is closed all the same.

| Status | When |
| --- | --- |
| `400` | A request line that is not three words split by single spaces, a method that is not a token, a malformed version, a target that is not one (a fragment, an escape that is not UTF-8 among them), no `Host` in HTTP/1.1 or two, a header without a colon, with a name that is not a token or folded onto a second line, a line ended by a bare `\n` or holding a bare `\r`, a NUL, DEL or another control character than a tab (a tab in the request line too), a `Content-Length` that is not digits or two that differ, one beside `Transfer-Encoding`, a `Transfer-Encoding` in HTTP/1.0 or in two field lines, a coding list not ending in `chunked`, a malformed chunk |
| `408` | A wait while a request is read outlasts `readTimeout`, the request's line and headers are not read within `headerTimeout`, or the request is not read whole within `requestTimeout` of its first byte |
| `413` | A body longer than `maxBodyBytes`, by its length before it is read, or as its chunks arrive |
| `414` | A request line longer than `maxHeaderBytes` |
| `417` | An `Expect` other than `100-continue` |
| `431` | The request line and headers together, with their line ends, or a chunked body's trailers, longer than `maxHeaderBytes`, or more than `maxHeaders` header lines |
| `500` | The handler traps: it fails its connection alone, as a `TcpServer`'s `serve` does (over TLS, or once a streamed response's head is sent, the connection is closed unanswered) |
| `501` | A transfer coding besides `chunked` |
| `503` | A request read once the server is stopping |
| `505` | A version other than 1.0 and 1.1 |

**Streamed request bodies.** A route made with `route.streaming()` (kept through `wrap`, `under` and
`group`) takes its body as a stream, once the server is told so: `serve(…, streamsBody =
router.streamsBody)`, a `@sendable (ServerRequest) -> bool` asked of each request once its head is
read, answering for the route `handle` would choose (a `GET` one for `HEAD`). The handler is called
with the body still on the connection, `request.bodyStreamed` true and `request.body` empty, and
reads it through `request.bodyReader(maxBytes = 104857600)`, a `BodyReader`: an `io.Reader` whose
`read(max)` answers up to `max` bytes, waiting for at least one, and an empty array at the end, by
`Content-Length` or chunked; `length` is the `Content-Length` (zero without a body, `None` when
chunked) and `readAll()` the rest within the limit. The limit is the handler's, counted over the whole
body, in place of `maxBodyBytes`, which still bounds every route that did not ask; each wait lasts at
most `readTimeout`, and the whole request, the handler's time between reads included, `requestTimeout`
from its first byte. A `Content-Length` over the limit fails the first read before any of the body is
asked for, so an `Expect: 100-continue` is answered `100 Continue` at the first read and only where the
body fits: a handler that answers without reading never tells the client to send it. A read that
fails (over the limit, a malformed chunk, a wait too long, the client gone) fails every read after
it, and once the handler returns **the server answers the failure itself, `413`, `400`, `431` or
`408` as for a buffered body, in place of the handler's response**, or closes the connection
unanswered where the client went. A body left unread when the handler returns is read and dropped
where at most 64 KiB of it is left, so the connection carries the next request; one longer, one
whose client was never told to continue, or one under a streamed response is not, and the
connection is closed after the response, so no byte of a body is ever read as a request. A request
made without a server, or whose route did not ask, has a `bodyReader` over its buffered body, so a
handler written for a stream serves both.

`request.multipartReader(limits)` reads a streamed (or buffered) `multipart/form-data` body a part at
a time through `bodyReader(limits.maxBytes)`, and `MultipartReader.over(source, contentType, limits)`
reads any `io.Reader`. `next()` answers the next part's `PartHead` (`name`, `filename`,
`contentType`, `headers`, `header(name)`, `isFile`), passing over what was left of the part before,
and `None` past the closing delimiter; `read(max)` and `readAll()` read that part's body. It reads by
`Multipart.parse`'s rules and limits, counted as it goes: parts as each begins, a head's bytes as it
grows, and every byte taken from the source, the epilogue after the closing delimiter, never read,
not counted. A fault is named where it is read, so a body with two may be refused for another than
`parse` names; a source that fails is `FormError.BodyUnreadable(IoError)` (`400`); every read after a
fault fails alike; and a delimiter's line may carry at most 1024 spaces and tabs after its boundary.

```avin
import avin.net.http
import avin.net.http.{Response, Route, Router, ServerRequest, ToResponse}

// Counts an upload's file bytes as they arrive, holding at most 64 KiB of it at once.
upload(r: ServerRequest) -> Result[Response, ToResponse]
  val parts = r.multipartReader()?
  var total = 0
  loop
    val head = parts.next()? match
      Some(h) -> h
      None -> break
    if head.isFile
      loop
        val piece = parts.read(65536)?
        if piece.length == 0 then break
        total += piece.length
  Ok(Response.text(200, s"$total bytes\n"))

main()
  val router = Router([Route.post("/upload", upload).streaming()])
  http.serve("127.0.0.1", 0, router.handle, streamsBody = router.streamsBody) match
    Ok(server) ->
      println("serving")
      server.shutdown()
    Err(e) -> println(e.message)
```

A connection waiting for a first request waits `readTimeout` or `headerTimeout`, whichever is
shorter, and for a later one `idleTimeout`, then is closed without an answer. **Shutting down**, the
server answers the request in flight with
`Connection: close`, and one arriving on a kept-alive connection `503`; a connection waiting for its
next request ends when its idle limit runs out, and `join()` waits for that.

**Hardening.** Everything a client controls is bounded, by default, so a server started with no
options is safe to face the open network. Each limit is a parameter of `serve`:

| Limit | Default | What a hostile client meets |
| --- | --- | --- |
| `maxConnections` | 1024 | Connections past it wait unaccepted in the listener's backlog until one ends; none is accepted and then refused, so a flood costs the server no memory and no thread past the limit |
| `headerTimeout` | 10 s | A request line and headers trickled a byte at a time (slowloris) are `408` at the bound; a connection that never sends is closed then |
| `readTimeout` | 30 s | Each wait for a byte while a request is read; `408` |
| `requestTimeout` | 60 s | The whole of a request, body included, from its first byte; `408` |
| `idleTimeout` | 5 s | The wait for the next request on a kept-alive connection; closed unanswered |
| `writeTimeout` | 30 s | Each wait while a response is written: a client that pipelines and never reads is closed once one write cannot finish |
| `maxHeaderBytes` | 16 KiB | A request line longer is `414`; the line and headers together longer, with their line ends, `431` |
| `maxHeaders` | 100 | More header lines are `431` |
| `maxBodyBytes` | 1 MiB | A body longer is `413`, by its length before it is read |

Requests on one connection are read and answered one at a time, the server reading at most 8 KiB
ahead, so a client pipelining without end has one request in flight and its unread responses held
by the socket buffers, not by the server. A request refused is answered and its connection closed,
the drain after it half a second in all. A connection's memory is what its one request holds:
requests kept alive one after another leave none behind.

```avin
import avin.net.http
import avin.net.http.Response
import avin.time.{millis, seconds}

main()
  // A server for slow clients on a trusted network: a longer header bound, a shorter idle one.
  val server = http.serve(
    "127.0.0.1",
    0,
    _ -> Response.text(200, "ok"),
    idleTimeout = millis(500),
    headerTimeout = seconds(30),
    maxHeaders = 50
  )
  server match
    Ok(s) -> s.shutdown()
    Err(e) -> println(e.message)
```

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
in the handshake; `timeout` limits each wait of the handshake and each write after it, and the
lookup, the connecting and a `wss://` URL's TLS handshake together. A negative `timeout` traps.

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
| `Url.formDecode(text) -> Result[Vector[(string, string)], UrlError]` | `application/x-www-form-urlencoded` read as WHATWG URL reads it, and as `queryParams` reads a query: split at each `&` (an empty piece skipped) and each piece at its first `=`, a `+` a space, each decoded; a malformed escape is `BadPercent` and bytes not UTF-8 `InvalidUtf8`, at their offset. |
| `Url.formEncode(pairs) -> string` | The pairs written as WHATWG URL writes a form: `name=value` joined by `&`, a space as `+`, every byte but a letter, a digit, `*`, `-`, `.` and `_` as `%XX`; `formDecode` reads it back as the same pairs. |

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
| `http.send(request: Request) -> Result[Response, HttpError]` | Any method, headers, body, timeouts and TLS settings, built from `Request(method, url, headers)` with `withHeader`, `withHeaders`, `withQueryParam`, `withBody`, `withTimeout`, `withTls`, `withConnectTimeout` and `withTotalTimeout`. |
| `http.stream[R](request: Request, f: (StreamedResponse) -> R) -> Result[R, HttpError]` | Sends the request and answers what `f` makes of the response, its body read as it arrives. |
| `Client(...)` | A client that keeps its connections for the requests after the first, with the same verbs, `send` and `stream` as methods ([below](#client)). |

Every verb answers a `Result[Response, HttpError]`, and `headers`, a `Vector[(string, string)]`, are
sent after the ones the client writes, in order. `post`, `put` and `patch` send `Content-Type:
contentType` first, so a `Content-Type` among `headers` follows it. `withQueryParam(name, value)`
adds `name=value` to the request's URL, each encoded by `Url.encode`, after any query it has (with
`?` where it has none, `&` otherwise, nothing after a trailing `?` or `&`) and before its fragment.
`withForm(pairs)` sends the pairs as an urlencoded form, written by `Url.formEncode`, and
`withMultipart(form)` a `Multipart` of `Part.field(name, value)` and `Part.file(name, filename,
contentType, bytes)` parts as `multipart/form-data` between delimiters of a fresh
`Multipart.newBoundary()` (`avin-` and 32 hexadecimal digits from `SecureRandom`); each replaces the
request's `Content-Type` with its own. `withMultipart(form, boundary)`, `form.encoded(boundary)` and
`form.contentType(boundary)` take a boundary of the caller's, and trap on one RFC 2046 does not allow
or one a part's body holds. A part's name and filename are written quoted as the WHATWG HTML standard
writes them, `"`, CR and LF as `%22`, `%0D` and `%0A`, so the server's `multipart()` reads them back
exactly. `request.header(name)` looks a header up as a response's does.

A `Response` has its `status` and `reason`, its `headers` as a vector of name and value pairs,
`header(name)` looking a name up without regard to case, its `body` as an `Array[byte]` and `text`
reading that as UTF-8, a sequence that is not read as U+FFFD. **A status is an answer, not a failure**: `404` is an `Ok`. `HttpError` is `Io(IoError)`,
`BadUrl(UrlError)`, `Malformed(text)` for a reply that is not HTTP, `Unsupported(text)` for a scheme
other than `http` and `https`, `Tls(TlsError)` for TLS that could not be set up (a connection
that could not be made, broke or timed out being `Io`, for `https` as for `http`), and
`TooLarge(text)` for a reply over the client's limits. The module's functions speak HTTP/1.1 on a
connection of their own for each request, closed after its response, read bodies by
`Content-Length`, chunked or to the connection's end, and follow up to five redirects as a
[`Client`](#client) does; the sixth is the answer. They write `Host`, `Content-Length` (for a body,
and for a `POST`, a `PUT` and a `PATCH` even when theirs is empty) and `Connection: close` unless
the request gives them, then the request's own headers in order; a `Client` writes no `Connection`
of its own. `HEAD`, an interim
`1xx` (which is skipped for the answer after it), `204` and `304` have no body. A method is a token
and a header's name one too, and a header's value holds no line break; a request where one does not
traps as it is made, whether the header came to `Request`'s constructor, to a verb, or by
`withHeader` or `withHeaders`, so nothing is sent. A request's timeout bounds each read and write on
its connection. Its `connectTimeout`, 30 seconds unless `withConnectTimeout(d)` (or the constructor)
sets another, bounds looking the host up, connecting and an `https` URL's TLS handshake together, on
each connection the request makes, redirects included, failing with `Io` of kind `TimedOut`; zero
takes the bound away, and a negative one traps as the request is made. Its `totalTimeout`, none
unless `withTotalTimeout(d)` sets one, bounds the whole of it, from the call until the answer:
getting a connection, writing, every redirect and reading the response, its body included, or for
`stream`, the function given the response; once it runs out, the request is stopped where it is,
its connection closed, and it fails with `Io` of kind `TimedOut`.

**An `https` URL just works**: the client makes the TCP connection to the URL's port, 443 where it
gives none, sets the request's timeout on it, so the handshake's waits are bounded too, the whole
handshake within what is left of the request's `connectTimeout`, and speaks
the request over a [`TlsStream`](#tls), the certificate checked for the URL's host against the
machine's trusted authorities and those of the request's `TlsClient`, which `withTls(client)` sets
(`TlsClient()` unless it is set). A redirect between `http` and `https` is followed like any other,
keeping the request's TLS settings. TLS lives in the Sysl package `openssl`, which the library
names in `library/library.hocon`, its own `sysl-dependencies`; a program's shims depend on it only
where a binding the program compiles reaches it, which `avin.net.http` and `avin.net.tls` do and
`avin.net` alone does not ([proposed](/spec/decisions/#proposed)).

#### Client

**A `Client` keeps its connections.** It is made once and used for every request, by every thread
(it is sendable), and sends each request on a connection kept from an earlier one to the same
origin, its scheme, host and port (and for `https` its `TlsClient`'s settings, the authorities it
trusts, the protocols it offers and the versions it allows), so a connection, and its TLS
handshake, is paid for once:

```text run
import avin.net.http
import avin.net.http.{Client, Request, Response}

main()
  val server = http.serve("127.0.0.1", 0, r -> Response.text(200, "hello, world")).ok.get
  val base = s"http://127.0.0.1:${server.port()}"
  val client = Client()
  for _ in 1 .. 3
    println(client.get(s"$base/").ok.get.text)
  val length = client.stream(Request("GET", s"$base/"), r -> r.body.readAll().ok.get.length).ok.get
  println(s"$length bytes, streamed")
  println(client.stats())
  client.close()
  server.stop()
```

```text output
hello, world
hello, world
hello, world
12 bytes, streamed
ClientStats(1, 3, 1, 0)
```

`Client(maxPerHost = 16, idleTimeout = seconds(30), maxLifetime = minutes(10), maxRedirects = 10,
maxHeaderBytes = 65536, maxResponseBytes = 67108864, keepAlive = true)` has `get`, `head`,
`delete`, `post`, `put`, `patch` and `send` as the module has, and:

| Method | Meaning |
| --- | --- |
| `client.stream[R](request, f: (StreamedResponse) -> R) -> Result[R, HttpError]` | Hands `f` the response once its head has come, its `body` a `ResponseBodyReader` read as it arrives, and answers what `f` returns. |
| `client.stats() -> ClientStats` | `ClientStats(opened, reused, idle, active)`: connections made in all, requests sent on a connection an earlier one used, connections kept now and lent now. |
| `client.evictIdle() -> int` | Closes the kept connections past `idleTimeout` or `maxLifetime` now, answering how many. |
| `client.close()` | Closes the kept connections; one lent now closes when it comes back, and a request afterwards fails with `Io`, making no connection. |

**Limits.** An origin has at most `maxPerHost` connections, lent and kept together. A request
that finds them all lent waits for one, within its `connectTimeout`, which also bounds making a new
one, and fails with `Io` of kind `TimedOut` (`cannot wait for a connection to
http://host:port: timed out`) when it runs out. A connection kept longer than `idleTimeout`, or
older than `maxLifetime` (so a host whose address changes is looked up again), is closed instead of
used; zero takes either away. A response's status line and headers together, and its trailers,
may be at most `maxHeaderBytes` long, and a body `send` reads whole at most `maxResponseBytes`
(found from its `Content-Length` before any of it is read, where it has one); over either is
`TooLarge`. A `maxPerHost` or `maxHeaderBytes` below one, or a negative `maxRedirects`,
`maxResponseBytes`, `idleTimeout` or `maxLifetime`, traps as the client is made. `keepAlive =
false` keeps nothing: each request writes `Connection: close` and has its own connection, as the
module's functions do (they are a `Client(maxRedirects = 5, maxResponseBytes = long.MaxValue,
keepAlive = false)` made for each call).

**A connection is used again only after a clean exchange**: the request was written whole, the
response is `HTTP/1.1`, neither it nor the request says `Connection: close`, its body's end was
found by its `Content-Length` or its chunks (not both, and not by the connection's close) and the
body was read to that end, and nothing came after it. Anything else closes it: an error or a
timeout, a body left partly read or not read at all, a trap or a cancellation in the middle, a
`101`. A kept connection is checked before it is used: one with something to read, the server
having closed it or sent what nothing asked for, is closed and another found. Where a request on a
kept connection fails anyway, other than by a timeout, before any byte of its response comes (the
server's close having crossed it), a request whose method may be repeated (`GET`, `HEAD`,
`OPTIONS`, `TRACE`, `PUT`, `DELETE`) is sent once more on a new connection; any other fails.

**The response is read strictly.** A header line beginning with a space (an obsolete folded
line), a header name that is not a token (`Bad Name:`), a `Content-Length` that is not digits or
says two lengths (`5, 6`; `3, 3` is one), and a chunk size past `int.MaxValue` are `Malformed`. A
`Transfer-Encoding` whose last coding is `chunked` reads chunks, whatever `Content-Length` says, and
one with any other coding reads to the close.

**Redirects.** A `301`, `302`, `303`, `307` or `308` with a `Location` is followed, the location
resolved against the URL that answered, up to `maxRedirects` times; the redirect after them is the
answer, and with zero none is followed. A `303` asks again with `GET` (a `HEAD` stays a `HEAD`), and
`301` and `302` turn a `POST`, and only a `POST`, into a `GET`; a request turned into a `GET` loses
its body and its `Content-Type`, `Content-Length`, `Content-Encoding`, `Content-Language`,
`Content-Location` and `Transfer-Encoding`. `307` and `308`, and `301` and `302` of any other
method, send the same method and body again. A redirect to another origin drops `Authorization`,
`Proxy-Authorization`, `Cookie` and a `Host` the request gave; one to the same origin keeps them. A
`Location` that is not a URL is `BadUrl`, one to a scheme other than `http` and `https` is
`Unsupported`, and a redirect with no `Location` is the answer. A redirect's body, up to 64 KiB, is
read and dropped so its connection is kept.

**Streamed responses.** `StreamedResponse` has the `status`, `reason`, `headers` and `header(name)`
a `Response` has, and its `body`, a `ResponseBodyReader` (named apart from the server's
`BodyReader`, which reads a request's body), is a [`Reader`](#avinio): `read(max)` answers what has
come of the body, up to `max` bytes, and an empty array at its end, so `io.copy(r.body, file)`
saves it; `readAll()` reads the rest, `readAll(limit)` the rest or `TooLarge`, and `discard(limit)`
reads and drops it, stopping past `limit` bytes, answering whether its end was reached;
`isComplete` says whether it was read to its end. A body that breaks off, or whose chunks are not
HTTP's, answers an `Err`, as every read after it does. The response belongs to `f`: once `f`
returns, the body is not read again, and its connection is kept or closed as above.

### Redis

`avin.net.redis` is a client of a Redis server (and of the servers that speak its protocol), written
in Avin over a [`TcpStream`](#tcp) or a [`TlsStream`](#tls) ([settled](/spec/decisions/#settled)).
`Redis.connect` opens a connection and asks for RESP3 with `HELLO 3`, sending the credentials and
the client's name in it, and speaks RESP2 where the server has no `HELLO`; `command` sends any
command and answers its reply as a `Value`, and the typed methods send the common commands and
answer what their replies mean, the same over either protocol.

```text
import avin.net.redis.{Redis, RedisError, RedisOptions}
import avin.time.minutes

visit(page: string) -> Result[long, RedisError]
  val r = Redis.connect("127.0.0.1", 6379, RedisOptions(password = Some("s3cret")))?
  r.set(s"last:$page", "now", minutes(10))?
  val counts = r.pipeline(p ->
    p.command("INCR", s"views:$page")
    p.command("EXPIRE", s"views:$page", "86400"))?
  val views = counts(0)?
  _ = r.close()
  Ok(views.asLong.getOrElse(0))
```

| `RedisOptions(…)` | Meaning |
| --- | --- |
| `username`, `password` | Authenticate, in `HELLO` or by `AUTH`; a password with no username is the `default` user's. |
| `database = 0` | Chosen by `SELECT` once connected; zero sends nothing. |
| `tls`, `tlsHost` | A `TlsClient` makes the connection TLS, its certificate checked for `tlsHost`, or the host connected to. |
| `clientName` | The connection's name, as `CLIENT LIST` shows it. |
| `protocol = 3` | 3 asks for RESP3 and falls back to RESP2 where the server answers `NOPROTO` or has no `HELLO`; 2 never asks. |
| `connectTimeout = 10 s` | Bounds TCP, TLS and the handshake together. |
| `timeout = 30 s` | Bounds each wait for the server after that; zero or less is none. |
| `reconnectAttempts = 3`, `reconnectDelay = 100 ms`, `maxReconnectDelay = 5 s` | How the next command makes a broken connection again: so many tries, the wait doubling between them. |
| `limits = RespLimits()` | What the server may send (below). |

A protocol other than 2 or 3, a negative database or a negative number of attempts traps.

**A failure is a `RedisError`**: `ConnectionFailed(IoError)` (refused, broken, or `TimedOut`),
`ConnectionClosed(text)` (by the server or by `close`), `TlsFailed(TlsError)`,
`ProtocolFailed(RespError)`, `ServerError(code, text)` for an error reply, its code the first word
(`ERR`, `WRONGTYPE`, `WRONGPASS`, `EXECABORT`), and `UnexpectedReply(expected, got)` for a reply of
another kind than the method reads. **A broken connection is made again by the next command, never
by the one it broke under**: that command answers its failure, since it may or may not have run, and
nothing is sent twice. A server that refuses the credentials while reconnecting is answered at once.
An error reply, or one of the wrong kind, leaves the connection as it is.

| Typed commands | Answer |
| --- | --- |
| `get`, `getBytes`, `set(k, v)`, `set(k, bytes)`, `set(k, v, ttl)`, `setIfAbsent(k, v, ttl?)`, `mget`, `mset`, `append` | Text, bytes, `unit`, whether it was set; a value that is not UTF-8 is `UnexpectedReply` from `get` and read by `getBytes`. |
| `incr`, `incrBy`, `decr`, `decrBy`, `incrByFloat` | The new number. |
| `del`, `exists`, `expire(k, d)`, `ttl(k)`, `persist` | Counts, whether the key was there, and `Expiry`: `NoKey`, `NoExpiry` or `ExpiresIn(d)`. Times are whole milliseconds, a part of one counting as one. |
| `hset(k, pairs*)`, `hget`, `hmget`, `hgetAll`, `hdel`, `hexists`, `hincrBy`, `hlen` | `hgetAll` is a `Map[string, string]`. |
| `lpush`, `rpush`, `lpop`, `rpop`, `lrange`, `llen` | `lrange(k, 0, -1)` is the whole list. |
| `sadd`, `srem`, `smembers`, `sismember`, `scard` | Members in no order. |
| `zadd(k, (score, member)*)`, `zrange`, `zrangeWithScores`, `zscore`, `zincrBy`, `zrank`, `zrem`, `zcard` | Scores are `real`s, the infinities among them. |
| `scan(pattern, count)`, `hscan`, `sscan`, `zscan` | An `Iterator` of `Result`s following the cursor page by page; a failure is its last element. A key there throughout comes at least once. |
| `ping`, `select`, `flushDb`, `dbSize`, `publish`, `watch`, `unwatch` | As the commands answer. |

**`pipeline(p -> …)`** sends every command the callback adds before reading a reply, and answers
each reply in order as its own `Result`; an outer `Err` is a failure of the connection. It is written
a batch of at most 64 KiB at a time, the batch's replies read before the next, so a long pipeline
never fills the connection both ways. **`transaction(t -> …)`** sends `MULTI`, the commands and
`EXEC` as one pipeline and answers `Some` of each command's reply, `None` where a key `watch`
watched changed so nothing ran, and `Err(ServerError("EXECABORT", …))` where the server refused a
command as it was queued. Every reply is read whatever failed, so the connection stays in step.

**`Redis.pool(host, port, options, maxSize = 8, …)`** is an [`avin.threads.Pool`](#concurrency) of
connections, shared between threads and actors: `pool.use(r -> r.incr("hits"))` lends one for the
call, and a connection broken while lent is closed rather than lent again. Its other settings
(`minIdle`, `idleTimeout = 5 min`, `acquireTimeout = 10 s`, `maxLifetime`, `maxWaiting`) are the
pool's.

**Publish and subscribe** go through a `Subscriber`, a connection of its own:
`Subscriber.connect(host, port, options)`, then `subscribe(channels*)` and `psubscribe(patterns*)`,
each answering once the server has confirmed every name, a message that arrives meanwhile kept;
`unsubscribe` and `punsubscribe`, all of them where none is named. `receive()` waits for the next
`PubSubMessage(channel, payload, pattern)` (`text` reads its payload), `receive(timeout)` answers
`None` where none comes in time, and `messages()` is an `Iterator` of them that ends at a failure,
which `lastFailure` keeps. A broken connection is made again and every channel and pattern
subscribed again; what was published meanwhile is not received. RESP2's arrays and RESP3's pushes
are read alike, and a push arriving on a `Redis` connection, not a subscriber, is passed over.

**RESP itself** is public too. A `Value` is one of `SimpleString`, `BlobString` (an
`ArraySeq[byte]`), `VerbatimString(format, text)`, `SimpleError`, `BlobError`, `Number` (a
`long`), `Double` (a `real`, `inf`, `-inf` and `nan` among them), `BigNumber` (a `BigInt`),
`Boolean`, `Null` (RESP2's `$-1` and `*-1` too), `ArrayValue`, `MapValue` (pairs in order),
`SetValue`, `Push` and `Attributed(attributes, value)`, with `asText`, `asBytes`, `asLong`,
`asReal`, `asBool`, `asItems`, `asEntries` (a RESP2 flat array too), `isNull`, `isError` and
`plain` (without attributes). `encode(v)` writes RESP3, `encodeResp2(v)` RESP2,
`encodeCommand(args)` and `encodeCommandBytes(args)` a command; a simple string or error holding a
CR or LF traps, as does a verbatim format that is not three bytes. A `RespParser(limits)` reads
values out of bytes as they arrive: `feed(bytes)`, then `next()` answers `Ok(Some(v))`, `Ok(None)`
while the value is not whole, or a `RespError` (`Malformed`, `TooLong`, `TooDeep`, `TooMany`, each
with its offset in the stream), which is final. Streamed strings and aggregates are read whole.

| `RespLimits(…)` | Default | Bounds |
| --- | --- | --- |
| `maxBulkBytes` | 536870912 (512 MiB, a Redis string's most) | one blob string, blob error, verbatim string, or a streamed string's chunks together |
| `maxDepth` | 64 | how deep aggregates nest; a top-level array's items are at depth one |
| `maxAggregateLength` | 16777216 | the items of an array, set or push, or the pairs of a map or attributes |
| `maxLineBytes` | 1048576 (1 MiB) | a simple string's or error's line, a number's, a length's |

Each length is checked as its header is read, before the bytes it claims arrive; a limit below one
traps.

```text run
import avin.net.redis.{ArrayValue, Number, RespLimits, RespParser, blobOf, encode}

main()
  val bytes = encode(ArrayValue(Vector(blobOf("views"), Number(42))))
  println(string.fromUtf8Lossy(bytes).replace("\r\n", "|"))
  val p = RespParser()
  p.feed(bytes.slice(0, 9))
  println(p.next())
  p.feed(bytes.slice(9, bytes.length))
  println(p.next().map(v -> v.map(x -> x.asItems.get(1).asLong)))
  val strict = RespParser(RespLimits(maxDepth = 2))
  strict.feed("*1\r\n*1\r\n*1\r\n:1\r\n".utf8)
  println(strict.next().err.map(e -> e.message))
```

```text output
*2|$5|views|:42|
Ok(None)
Ok(Some(Some(42)))
Some(RESP aggregates nested deeper than 2 at byte 8)
```

### Mail messages

`avin.net.mail` writes a mail message as RFC 5322 and MIME (RFC 2045 to 2049) define it, ready for
[SMTP](#smtp) to send. A `Message` is data: who it is from and to, a subject, a plain-text body, an
HTML one or both, attachments and further headers; `render()` writes it, or says why it cannot.

```text run
import avin.net.mail.{Attachment, MailError, Mailbox, Message, decodeHeaderValue}

main() -> Result[unit, MailError]
  val m = Message(
    Mailbox("ada@example.com", "Ada Lovelace"),
    to = [Mailbox("charles@example.com", "Charles Babbage")],
    bcc = [Mailbox("archive@example.com")],
    subject = "Notes on the engine — draft",
    text = Some("The notes are attached.\n"),
    html = Some("<p>The notes are <b>attached</b>.</p>\n"),
    attachments = [Attachment("notes.txt", "text/plain; charset=utf-8", "Note G\n".bytes)]
  )
  val lines = m.render()?.split("\r\n")
  for line in lines
    if line.startsWith("From:") || line.startsWith("To:") || line.startsWith("Subject:") then println(line)
  println(decodeHeaderValue(lines.find(l -> l.startsWith("Subject:")).getOrElse("").drop(9)))
  println(s"recipients: ${m.recipients}, Bcc written: ${lines.exists(l -> l.startsWith("Bcc"))}")
  Ok(())
```

```text output
From: Ada Lovelace <ada@example.com>
To: Charles Babbage <charles@example.com>
Subject: =?UTF-8?B?Tm90ZXMgb24gdGhlIGVuZ2luZSDigJQgZHJhZnQ=?=
Notes on the engine — draft
recipients: [charles@example.com, archive@example.com], Bcc written: false
```

| Member | Meaning |
| --- | --- |
| `Mailbox(address, name = "")` | An address and the name a reader sees, written `Ada Lovelace <ada@example.com>`, or the address alone. |
| `Attachment(filename, contentType, content: Vector[byte], inline = false, contentId = "")` | A file; an inline one is shown in the body, which refers to it as `cid:<contentId>`. `contentType` is `type/subtype`, parameters allowed. |
| `Message(from, to = [], cc = [], bcc = [], replyTo = [], subject = "", text = None, html = None, attachments = [], headers = [], date = None, messageId = None, boundary = None)` | A message. `headers` are further `(name, value)` pairs, written after the message's own. `date` is `now()` where none is given, `messageId` (without angle brackets) a random one at the sender's domain, `boundary` (1 to 60 of `A–Z a–z 0–9 '()+_,-./:=?`) a random one. |
| `m.render() -> Result[string, MailError]` | The message as it goes over the wire, every line ending in CR LF and none longer than 998 characters. |
| `m.recipients -> Vector[string]` | Every address of `to`, `cc` and `bcc`, in that order, each once: the envelope's recipients. |
| `m.needsUtf8 -> bool` | Whether an address of the message is beyond ASCII, so the server must take SMTPUTF8. |
| `decodeHeaderValue(value) -> string` | A header's value as a reader sees it: folding removed and RFC 2047 encoded-words (`B` or `Q`; UTF-8, US-ASCII, ISO-8859-1) decoded, space between two words dropped; a word it cannot decode kept as it is. |
| `mailDate(t: Instant) -> string` | RFC 5322's date in UTC, `Thu, 01 Jan 1970 00:00:00 +0000`. |
| `checkedAddress(header, address) -> Result[string, MailError]` | The check every address meets: `local@domain` around the last `@`, a local part of at most 64 bytes, at most 254 in all, no space, control character or `<>()",;\`. An address beyond ASCII is kept for SMTPUTF8. |

**How it writes.** The headers come in the order `Date`, `From`, `Reply-To`, `To`, `Cc`, `Subject`,
`Message-ID`, `MIME-Version: 1.0`, then the caller's; `Bcc` is never written, its addresses being
recipients alone. A subject, a display name or a caller's value that is printable ASCII is written
as itself, folded at its spaces before 78 columns; anything else, and text a reader would take for
an encoded-word, is written as RFC 2047 encoded-words in UTF-8 and base64, each holding whole
characters and at most 64 characters long. A name of words is written bare, other plain text as a
quoted string. The body is `text/plain` or `text/html` in UTF-8, `multipart/alternative` of the two
where both are given (plain first), inline attachments beside it in `multipart/related` and the
others after it in `multipart/mixed`; with no body at all it is an empty `text/plain`. Line breaks
in a body (CR LF, CR or LF) are written CR LF; a body is `7bit` where it is ASCII with lines of at
most 998, and quoted-printable otherwise. An attachment is base64 in lines of 76, and a filename
that is not short printable ASCII is an RFC 2231 parameter, `filename*=UTF-8''r%C3%A9sum%C3%A9.pdf`,
in numbered sections where it is long. Each boundary is `=_`, the base and a number, which neither
quoted-printable nor base64 can hold.

**Nothing can be smuggled into a header.** A CR or LF in anything that would go into a header (a
subject, a name, an address, a filename, a content type or ID, a message ID, a caller's header name or
value) is an `Err`, never written: `MailError` is `LineBreak(header)`, `BadAddress(header, address,
why)` or `BadHeader(header, why)` (a name that is not a field name or is one of the message's own, a
value with a word too long for 998 columns, a bad content type, ID or boundary); `message` says it in
words.

Quoted-printable itself is in [`avin.encoding`](#avinencoding): `toQuotedPrintable(bytes)` and
`fromQuotedPrintable(text)`.

### SMTP

`avin.net.smtp` hands mail to a server by SMTP (RFC 5321), with STARTTLS (RFC 3207), `AUTH PLAIN` and
`LOGIN` (RFC 4954), `SIZE`, `8BITMIME` and `SMTPUTF8`. An `SmtpClient` says how to reach the server;
`send` opens a session, sends one message and quits, and `open` gives a session that sends several.

```text
import avin.net.mail.{Mailbox, Message}
import avin.net.smtp.{SmtpClient, SmtpError}
import avin.process

main() -> Result[unit, SmtpError]
  val client = SmtpClient("smtp.example.com", user = "ada", password = process.env("SMTP_PASSWORD").getOrElse(""))
  val m = Message(
    Mailbox("ada@example.com"),
    to = [Mailbox("charles@example.com")],
    subject = "Hello",
    text = Some("Hi.\n")
  )
  client.send(m)
```

| Member | Meaning |
| --- | --- |
| `SmtpClient(host, port = 0, security = StartTls, tls = TlsClient(), user = "", password = "", mechanism = None, authWithoutTls = false, helo = "localhost", connectTimeout = seconds(30), commandTimeout = minutes(5), dataTimeout = minutes(10), maxReplyLine = 1000, maxReplyLines = 100)` | How to reach the server. Port zero is the security's own: 587 for `StartTls`, 465 for `ImplicitTls`, 25 for `Unencrypted`. A negative timeout traps. |
| `client.open() -> Result[SmtpSession, SmtpError]` | Connects, reads the greeting, says `EHLO`, upgrades by STARTTLS (and says `EHLO` again) or speaks TLS from the start, and authenticates where `user` is given. |
| `client.send(message) -> Result[unit, SmtpError]` | `open`, `send` and `quit`. |
| `session.send(message) -> Result[unit, SmtpError]` | Renders the message and sends it from its sender to its `recipients`, Bcc included. |
| `session.sendData(from, recipients, data: Array[byte])` | Sends a message already written; an empty `from` is the null sender `<>`. |
| `session.reset()`, `session.quit()`, `session.close()` | `RSET`; `QUIT` and then the connection closed whatever the answer; the connection closed without a word. |
| `session.extensions`, `session.offers(keyword)`, `session.sizeLimit`, `session.encrypted` | What `EHLO` answered (keywords upper case), whether one is offered, `SIZE`'s limit, whether TLS is on. |

**Secure by default.** `StartTls` sends nothing but `EHLO` before upgrading, and a server not
offering STARTTLS is `Unsupported("STARTTLS")`, never a reason to go on in the clear; bytes the server
sends after its `220` to STARTTLS are a `Protocol` error, since they would be read as if they came
over TLS. The certificate is checked for `host` as every [`TlsStream`](#tls) checks it, against the
machine's authorities and the client's `tls`. Only `Unencrypted`, asked for by name, sends without
TLS, and a password is never sent without it unless `authWithoutTls` is `true` (`NotSecure`
otherwise). The mechanism is the one asked for, or `PLAIN` where offered and else `LOGIN`.

**What it sends.** `MAIL FROM` carries `SIZE=` where the server offers SIZE, `BODY=8BITMIME` for data
beyond ASCII and `SMTPUTF8` for an address beyond it; a message larger than the server's SIZE is
`TooLarge` before anything is sent, and one needing an extension the server lacks is `Unsupported`.
Every address is checked as `checkedAddress` checks it, so a CR, LF or `>` can never add a command.
The data is dot-stuffed (a `.` doubled at the start of a line) and every line break in it, a lone CR
or LF too, is sent CR LF, so nothing in a message can end its data early; a line longer than 998
bytes is `Invalid`. One recipient refused refuses the message: the transaction is reset with `RSET`
and the session can send the next.

**Every wait is bounded, and so is every reply.** Connecting (the lookup with it) must be done within
`connectTimeout`, and each wait of a TLS handshake too; each reply must have come whole within
`commandTimeout` of its command, so a server sending a byte at a time cannot hold a session; the
message's data is sent with each wait within `dataTimeout`, and its reply must come within it.
Zero takes a limit away. A reply line longer than `maxReplyLine` bytes or a reply of more than
`maxReplyLines` lines is a `Protocol` error, so a hostile server cannot make the client hold what it
sends.

`SmtpError` says what went wrong: `Transport(IoError)`, the connection failed, broke, closed or timed
out (`TimedOut`); `Tls(TlsError)`; `Rejected(command, reply)`, the server refused, with its
`Reply(code, lines)`, `reply.enhanced` its RFC 3463 status (`5.1.1`) where it gives one;
`Protocol(text)`; `Unsupported(text)`; `NotSecure`; `TooLarge(size, limit)`; `BadMessage(MailError)`;
`Invalid(text)`. `e.code` is a refusal's code, `e.transient` is true for a 4xx refusal and for a
failed connection (beneath TLS too), which may succeed later, and `e.permanent` for everything else, a 5xx above
all. After a failure of the connection or the protocol, and after a `421`, the session is closed and
every operation answers an error. A password is never part of an error.

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
