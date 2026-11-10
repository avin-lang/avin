---
title: Threads
summary: Starting and joining threads, what they share, how they synchronize, and what a racing read sees.
weight: 16
---

Avin programs run threads in parallel over one shared, collected heap.

## Starting and joining

Threads are library code in `avin.threads`, over runtime entries that start a thread with a context
of its own and wait for it to finish. A thread runs a closure and is joined for its result:

```avin
import avin.threads.spawn

main() -> int
    val worker = spawn(() -> sum(1..1000000))
    val mine = sum(1000001..2000000)
    worker.join() + mine
```

- `spawn[T: Sendable](@sendable body: () -> T, stack: long = 8388608) -> Thread[T]` starts `body`
  on a new [lightweight thread](#lightweight-threads) with a stack of its own (8 MiB unless asked
  otherwise, 64 KiB at the least). The body
  captures only sendable values, and its result is sendable ([what is shared](#what-is-shared)).
  `join() -> T` waits for it
  and gives back the result; joining a second time gives the same result again. Any thread may
  join, several at once: one waits for the thread and the others wait for that one, and a `Thread`
  is itself sendable. A `Thread` is
  collected like any other object once nothing reaches it; a thread that is never joined still gives
  its stack back when its body returns. Once `join` answers, the ended thread keeps nothing alive: its
  body and what the body captured are collected at the next collection unless something else still
  reaches them. A thread that overflows its own stack traps with
  `stack overflow` like any other thread's trap, whatever its stack's size: one under the least a
  thread is given, the default, and a stack kept from an ended thread that is larger than the new
  one asked for, which overflows at its own end.
- **A trap in any thread stops the program**, as it does in a program with one thread: traps report
  bugs, and a bug in one thread leaves the state the others share in doubt. Only the first thread to
  stop the program reports: what was written is flushed once and one reason is printed, and a
  thread that traps after it, or calls `exit`, waits for the end. The stopping thread runs the
  program's [exit hooks](/spec/system-library/#exit-hooks) after its report, the others still
  running.
- **The program ends when `main` returns**, whatever other threads are still running: standard
  output is flushed, the main thread runs the exit hooks, and the process exits with `main`'s
  status, taking the other threads with it.
- A library scope makes the joins structural: `scope(s -> …)` returns its body's result only once
  every thread started through `s.spawn` has finished. `s.spawn` takes the same arguments as
  `spawn` and is called by the thread that runs the scope's body, while the scope is open; a call
  from another thread, or after the scope has returned, traps. A `Scope` is not sendable, so a
  thread the scope started cannot reach it; one that starts more uses a scope of its own.

```avin
import avin.threads.{Atomic, scope}

main() -> int
    val parts = (0..<4).map(_ -> Atomic(0)).toVector()
    scope(s ->
        for i in 0..<4
            s.spawn(() -> parts(i).store((i + 1) * 10)))
    parts.map(p -> p.load()).sum()
```

## Lightweight threads

Every thread a program starts is a lightweight thread: the runtime runs it on one of a few operating
system threads of its own, its **workers**, about one for each processor, started as threads become
ready. What a program can rely on:

- **Threads are cheap.** A thread's stack is reserved at its full size and the system commits its
  pages only as the thread first touches them, so a thread that has not recursed deeply costs a page
  or so of memory and a small record. A program may start a hundred thousand threads and join them
  all, and a deep recursion still fits in a thread as it does in `main`.
- **A wait parks only its thread.** A thread that waits for a lock, a condition, a channel, a join, a
  future or a promise is set aside, and its worker runs another ready thread meanwhile; the thread
  that ends the wait makes it ready again. Ready threads run in the order they became ready, and a
  thread may resume on a different worker from the one it waited on. A timed wait parks the same way
  and is made ready when its time runs out, and so does a `sleep`: ten thousand threads sleeping at
  once hold no worker between them and all wake in about one sleep. Threads whose times run out
  together are made ready soonest first, two due at the same moment in the order they began to wait.
  A timed wait that its event ends first answers once, as woken, and its timer goes with it. So
  waiting code is written as plain straight-line code:
  a server may hold one thread per connection, and a chain of futures each awaiting the one before
  it resolves however long it is.
- **Threads are time-sliced.** A thread that has run for 10 ms while another thread is ready yields
  its worker at its next safepoint poll, which compiled code makes on every back edge of a loop and
  before every call that may recur, and goes to the back of the ready queue. A thread that computes
  without ever waiting therefore shares its processor rather than holding it, even in a tight loop
  that calls nothing, and a thread that does not overrun its slice pays nothing for it.
- **A call that blocks its operating system thread does not starve the others.** A Sysl binding
  [declared `blocking`](/spec/sysl-bindings/#calls-that-wait) or a wait for a `lazy val` being
  initialized elsewhere holds the worker it runs on, but once such a call has held its worker
  for a millisecond or two while other threads are ready, the runtime runs another worker in its
  place. A worker freed again rejoins the others, or rests until it is needed.
- **The main thread is not a worker.** `main` runs on the program's first operating system thread,
  and its own waits block that thread while the workers run the rest.

## What is shared

The heap is shared: every object is reachable from every thread that is handed a reference to it.
Nothing is copied when a thread starts; the closure and what it captures are simply reachable from
the new thread too. **So what a thread is given is checked**: a closure that runs on another thread
captures only [sendable](/spec/actors/#what-may-be-sent) values and no `var`, and the value it gives
back is sendable, and the code it runs reaches no module-level mutable state (below). The threads
then share nothing mutable but through the synchronized types, a `Mutex`, an `Atomic`, a
`Channel`, a `Once` and a `Condition`. The error names what is captured and why it cannot be:

```text
error: what is passed for `body` of `spawn` may use only sendable values from around it, but it uses `parts`, an `Array[int]`, which is mutable; an immutable copy (`toVector`) is sendable
```

A captured `var` lives in a box on the heap that the closure would share with the code that made
it, so it is never captured for another thread, whatever its type: the closure copies it into a
`val` first, or the two share an `Atomic`. A closure is checked by what it captures, the closures
it makes and the local functions it calls included; a method value captures its object, so
`spawn(counter.tick)` needs a sendable `counter`; and a by-name body, as `Future(body)` takes, is a
closure too. A function value held in a `val` of a plain function type is not sendable, since its
type does not say what it captures: the closure is written where it is passed, or the `val` is of a
`@sendable` function type.

**A `@sendable` function type says it.** `@sendable (A) -> B` ([function
types](/spec/types/#function-types)) is the type of a function that may run on another thread, and
it is `Sendable`. A lambda, a function reference or a method value written where one is expected is
checked where it is made exactly as what is passed for a `@sendable` parameter is: what it captures,
and the module-level state it reaches (below); the error names the type. A value already of a
`@sendable` type is passed, stored, captured and sent as it is, so a data type or a class whose
fields are `@sendable` functions is sendable by its structure, and grammars, handlers and tasks held
in module-level values may be used by every thread:

```avin
import avin.threads.spawn

data Rule[T](run: @sendable (string) -> Option[T])

val digits: Rule[int] = Rule((s) -> s.toIntOption())

main() = println(spawn(() -> digits.run("42")).join())
```

```text
error: a `@sendable (int) -> int` may use only sendable values from around it, but it uses `a`, an `Array[int]`, which is mutable; an immutable copy (`toVector`) is sendable
```

**The checks are three marks on a parameter, which any function may use.** What is passed for an
**`@isolated`** parameter may use only sendable values from around it: a closure captures only
sendable values (a `var` among them, where its type is sendable), and any other expression reads
only sendable locals and parameters; what it makes itself is its own. Nothing outside therefore
reaches a mutable value it makes or is given. An **`@isolating`** parameter asks that, and that
nothing it runs, however indirectly, reaches module-level mutable state (below), so no mutable
object it makes or is given can be stored where code outside reaches it: it is the mark of
[`isolate`'s body and `with`'s function](/spec/actors/#handing-over-ownership), and of a
[mutex's value and its body](#synchronization). A **`@sendable`**
parameter asks both, and that a closure captures no `var`, since it may run on another thread while
the code that made it goes on.
Inside the function, a parameter so marked that holds a function may be passed on for a parameter
with the same mark or a weaker one; an unmarked one may not, and the error says to mark it. A
method that overrides or implements another marks no parameter more strongly than the method it
overrides does, since a call through that method would not check what is passed.

**A parameter marked `@sendable` is sendable inside its function, whatever its type**, since what
is passed for it was checked where it was passed: a `@sendable` lambda captures it, and it is passed
on for another `@sendable` parameter as it is. So what is passed for one that does not take a
function is itself sendable: a [`match` function](/spec/operators-and-control-flow/#match-functions)
written in place, whose arms are checked as a lambda's body is, by what they capture and the
module-level state they reach; a value of a sendable type; a parameter so marked; or `f.orElse(g)`
of two of these, chains of it included, which holds only the two. A class's `val` parameter so
marked is a field whose value is sendable, so it leaves the class sendable by its structure; a `var`
one does not, since it may be assigned anything later. The mark belongs to the parameter, not to its
type: a marked value copied into a local, a combination held in one, or one stored in a data field
of the trait's type is a value of that type again. `orElse` is sendable of sendable parts by the
standard library's promise, `@sendable_result`, which says a function's result holds nothing but its
receiver and its arguments; nothing checks the promise against the body, so only the standard
library makes it, and a function of the program's own that passes its marked arguments back is not
taken as one.

```avin
data Rule[T](run: @sendable (char) -> Option[T])

pick[U](@sendable f: PartialFunction[char, U]) -> Rule[U] = Rule(c -> if f.isDefinedAt(c) then Some(f(c)) else None)

pickAgain[U](@sendable f: PartialFunction[char, U]) -> Rule[U] = pick(f)

val digit: Rule[int] = pickAgain(match c if '0' <= c <= '9' -> int(c) - int('0'))

either[U](@sendable f: PartialFunction[char, U], @sendable g: PartialFunction[char, U]) -> Rule[U] = pick(f.orElse(g))

val sign: Rule[int] = either(match '+' -> 1, match '-' -> -1)
```

A lambda or a `match` function passed for a type argument bounded by `Sendable`, as
`take[U: Sendable](value: U)` has, is made as a `@sendable` function and checked as one where it is
made; a plain function type the expected type gives that argument is taken as its `@sendable` form.
A `match` function written where a `PartialFunction` is expected is made of two `@sendable`
functions there, one testing its patterns and guards and one running its arms, checked as one
function whose fault is reported once. `PartialFunction` itself does not promise `Sendable`, so the
bound holds at that call only where every value the call gives of the type argument is such a
`match` function: an argument of exactly that type for each parameter whose type could give the
callee a value of it (one naming it as what a function passed takes gives none). A written type
argument is held to the bound as written.

```avin
take[U: Sendable](value: U) -> U = value

val vowel: PartialFunction[char, int] = take(match
    'a' -> 1
    'e' -> 2)
```

```avin
import avin.threads.spawn

background[T: Sendable](@sendable body: () -> T) -> T = spawn(body).join()
```

`spawn` and `s.spawn` take their body `@sendable`, as do `Future(body)`, `ex.future(body)`, a
future's `map`, `flatMap` and `onComplete` and the `orElse` of a future of a `Result`, whose
functions run on a pool thread. A mutex's value and the body of its `with` are `@isolating`
([synchronization](#synchronization)).

**Module-level state is shared by every thread**, and a body reaches it by name, not by capture.
So what is passed for a `@sendable` parameter, directly or through every function it calls, may
read and write no module-level `var`, and may read no module-level `val`, `lazy val` or `object`
whose type is not sendable. Avin compiles the whole program, so every function a body can reach is
known: the one a call names, every implementation a call through a trait method or an operator may
reach, every closure made on the way, and an extractor a pattern calls. A module-level `val` of an
`Atomic`, a `Mutex`, a `Channel` or immutable data is the way to share state between threads; a
`lazy val` of a sendable type is initialized once whichever threads read it. The error names the
module-level item and the functions it is reached through:

```text
var hits = 0

record() = hits += 1

main()
    spawn(() -> record()).join()
```

```text
error: what is passed for `body` of `spawn` may run on another thread, which shares the program's module-level state, but through `record` it reaches `hits`, a module-level `var`, which every thread would share; share it through an `Atomic` or a `Mutex` held in a `val`
```

With `val hits = Atomic(0)` and `record() = hits.add(1)` the program is legal. Printing is legal from any
thread: `print`, `println` and `eprintln` reach the runtime's streams, which keep no state of the
program's. What is passed for an `@isolating` parameter is held to it too, though it runs at once on
the calling thread: a module-level value is where it could leave what it isolates for code that
does not take the lock. What is passed for an `@isolated` one is not.

A [Sysl handle](/spec/sysl-bindings/#handles) is the exception: it belongs to one owner at a time,
and a use by any other code traps. The handles a thread's body or a future's body reaches move to
it when it starts, and are in transit, for the next code to use them, once it returns, before a
future's value is published, so its awaiter may use them at once; an actor
will own its handles across every message, whichever thread runs it.

## Synchronization

`avin.threads` offers, all written in Avin over Sysl's atomics and futex-based locks:

- **`Mutex[T](value)`**, which owns the value it guards, and is sendable whatever `T` is.
  `m.with(v -> …)` takes the lock, runs the
  body with the value and lets the lock go when the body returns, giving back what it returned; it
  is the way to a mutable value, such as a `Buffer`, which the body changes in place. So that
  nothing reaches the value but under the lock, `value` is `@isolating`, made in the argument
  (`Mutex(Buffer[int]())`, not a buffer the caller keeps), and so is the body, whose result is
  sendable: `with[R: Sendable](@isolating body: (T) -> R) -> R`. A body may read and assign the
  caller's `var`s of sendable types, since it runs at once on the calling thread, and may read
  module-level `val`s of sendable types (another mutex among them), but nothing it runs may reach a
  module-level `var` or a module-level value of a type that is not sendable: storing the value
  there, in a module-level collection or in an object's field would let the program change it later
  without the lock. A body reads such a `var` through a local read before it. For a sendable
  value, `get()` reads it, `set(v)` replaces it and `update(f)` replaces it with `f` of it
  under the lock, giving back the new value. A mutex is not reentrant: a thread that takes one it
  already holds, from inside a body, would wait for ever, so it traps instead.
- **`Condition`**, made by a mutex's `condition()` and waited for inside that mutex's `with`; like
  its mutex, it is sendable.
  `wait()` lets the mutex go while it sleeps and holds it again before it returns; `notify()` wakes
  one waiting thread and `notifyAll()` every one. A wait may end with nothing having changed, so the
  state is checked again in a loop. `waitFor(timeoutMillis: long) -> bool` waits at most that many
  milliseconds and answers `false` where the time ran out first; a time that is not positive
  answers `false` at once. `waitFor(timeout: Duration) -> bool` is the same wait for a
  [`Duration`](/spec/time/), a part of a millisecond waited for as a whole one. Waiting from a thread
  that does not hold the mutex traps.
- **`Atomic[T](value)`**, with `load`, `store`, `swap`, `compareAndSet(expected, desired) -> bool`
  and `update(f)`, and for the integers `add(n)` and `sub(n)`, each giving back the new value and
  trapping on overflow, as `+` and `-` do, before anything is stored. Every operation is
  sequentially consistent; there are no weaker orderings in the library. `int`, `long`, `bool` and
  references are the types it is for, and it holds any type: a value of one word is accessed
  lock-free, an integer wider than 64 bits under a lock the runtime keeps for them; an atomic
  reaches another thread only where its value is sendable, as a `Channel` does. A
  compare-and-set of a reference to an object of a class compares identity, as `===` does (even for
  a class that derives `Eq`), and of a function value likewise, so a reference held in an `Atomic`
  is also kept alive by it. A compare-and-set of a scalar compares the value (a `real`, its bits).
  A compare-and-set of a value without identity (a data value, an `Option` among them, a tuple, a
  string or an `Any`) compares with `==`: it succeeds where the current value is `==` to `expected`,
  however the two were made. It loads the value, compares, and compares-and-sets the word it
  loaded, trying again where another thread changed it in between. The kind is known for each
  `T`, so a scalar or a class reference costs one hardware compare-and-set. `update(f)` retries by
  the same rule.
- **`Channel[T](capacity = 0)`**, a queue between threads, unbounded when the capacity is zero and
  bounded otherwise, where `send` waits while the channel is full. `receive()` waits for a value and
  gives `Some` of it, or `None` once the channel is closed and every value sent before is received.
  `close()` wakes every waiting thread, and a send to a closed channel traps; closing twice does
  nothing. `trySend(x) -> bool` and `tryReceive() -> Option[T]` never wait, and `isClosed()` asks.
  A channel is `Iterable`: `for x in ch` receives until it is closed and drained.
- **`Once`**, whose `run(body)` runs its body exactly once, whichever threads call it and however
  many at once: every other call waits until that body has returned, and returns without running its
  own. `isDone()` says whether it has run. A body that runs its own `Once` again traps, as a mutex
  taken twice does.
- **`lazy val`s**, module-level and fields alike ([Initialization](declarations.md#initialization)),
  initialized by exactly one thread, as a `Once` is run: a thread that reads one while another runs
  its initializer waits, safe for the collector, and the stored value is published with a release
  store that every read pairs with an acquire load, so no thread sees it half made. A read by the
  initializing thread itself traps, as a `Once` run again from its body does, and a trap in the
  initializer stops the program.

```avin
import avin.threads.{Channel, scope}

main() -> int
    val jobs = Channel[int](16)
    val results = Channel[int]()
    scope(s ->
        for w in 0..<4
            s.spawn(() ->
                for n in jobs do results.send(n * n))
        for n in 1..100 do jobs.send(n)
        jobs.close())
    results.close()
    var total = 0
    for r in results do total += r
    total % 256
```

Every operation that can wait marks its thread safe for the collector while it waits, so a thread
parked on a lock, a condition, a channel or a join, or blocked on input, never holds up a collection.
A Sysl binding [declared `blocking`](/spec/sysl-bindings/#calls-that-wait) does the same. A thread
waiting for a mutex takes it in turn: the mutex goes to the thread that has waited longest. **A deadlock is not detected**, apart from a thread
taking a lock it already holds: threads that each wait for another sleep for ever.

## Futures

A `Future[T]` is a value some thread is computing. `Future(body)` runs `body` on a shared pool of
worker threads and gives back the future of its result at once; the body is a by-name argument, so
it is written in place or as a trailing block. Its value is sendable, as a thread's result is, and
the body, like the functions of `map`, `flatMap` and `onComplete`, is `@sendable`:
`apply[T: Sendable](@sendable body: -> T) -> Future[T]`.

```avin
import avin.threads.Future

main() -> int
    val page = Future(fetch(url))
    val counted = Future
        val words = load(path).split(" ")
        words.length()
    val both = for p in page, n in counted yield s"${p.charCount()} characters, $n words"
    println(both.await())
    0
```

- **`await() -> T`** waits for the value and gives it back, the same value at every call, safe for
  the collector while it waits. **`await(timeoutMillis: long) -> Option[T]`** waits at most that many
  milliseconds and gives `None` where the time ran out first; a time that is not positive asks
  without waiting; **`await(timeout: Duration) -> Option[T]`** is the same wait for a
  [`Duration`](/spec/time/), a part of a millisecond waited for as a whole one. `isCompleted()` asks
  and `value()` gives `Some` of a value already there; neither waits. Every timed wait takes a count
  of milliseconds as a `long` or a `Duration`.
- **The combinators never wait.** `map(f)` and `flatMap(f)` give the future of `f` of the value,
  `f` running on a pool thread once the value is there, so a `for … yield` over futures is a future
  too; `zip(other)` pairs two values, and `onComplete(f)` runs `f` with the value on a pool thread,
  at once where it is already there. `Future.successful(x)` is complete from the start.
  `Future.all(fs)` is the future of every value of an `Iterable` of futures, as a `Vector` in the
  order given, and `Future.first(fs)` of the value of whichever completes first; the first of no
  futures traps. `Future.traverse(xs, f)` is `Future.all` of `f` of each element: `f` is called for
  the elements in turn on the calling thread, so their futures start in that order, and the values
  come back in it however the futures finish.
- **A future carries no failure.** A computation that can fail returns `Future[Result[T, E]]`, and
  such a future also has `mapOk(f)` and `flatMapOk(f)`, which go on from an `Ok` and pass an `Err`
  through unchanged, `mapErr(f)`, which turns one failure into another, and `orElse(other)`, which
  goes on to the future `other` gives only where this one ended in `Err`: `other` is by-name, so it
  is not even evaluated after an `Ok`, and its value may widen to a supertype as `Result.orElse`'s
  does. A trap inside a body stops the program, as it does in any thread.
- **A `Promise[T]()`**, of a sendable `T`, is completed by another thread: its `future` field is the future it completes,
  `complete(x)` completes it and traps where it is already complete, and `tryComplete(x) -> bool`
  completes it only where nothing has yet, answering whether it did, for threads that race to give
  the value. Every thread waiting on the future wakes with the value.

**The pool** is started on first use, one worker at a time as work arrives with no idle worker, up
to one for each processor the machine offers. Its workers are never joined, so the program ends when
`main` returns, whatever futures are pending. **A body may wait for another future.** A worker that
waits in `await` for a value not yet there counts as not working, and where work is queued with no
worker free, the pool starts another in its place; that one stays afterwards and is idle while there
is nothing to do. So waits nested in bodies never leave the pool without a thread to run the future
they wait for, however deep they go, and the pool grows only by as many workers as wait at once.

**`blocking(body)`** says a body waits on something other than a future: a channel, a lock, a
blocking binding, a sleep. On a worker of a pool, the pool counts it as waiting while `body` runs,
exactly as it counts one in `await`, so a full pool whose workers all wait on a channel still runs
the task that fills it. A `blocking` inside another, or an `await` inside one, is counted once; on a
thread of no pool, `body` simply runs. **`sleep(d: Duration)`** pauses the calling thread for at
least `d` (from [`avin.time`](/spec/time/)), safe for the collector while it
sleeps; a duration of zero or less returns at once, and one longer than about thirty years sleeps
about thirty years. A thread that sleeps parks, freeing the operating system thread it ran on, and
`main` blocks its own. A worker of a pool that sleeps keeps its place in the pool, and
`blocking(sleep(d))` gives it up.

```avin
import avin.threads.{blocking, Channel, Future}

main() -> int
    val jobs = Channel[int]()
    val workers = (0..<64).map(i -> Future(blocking(jobs.receive().getOrElse(0)))).toVector()
    Future
        for n in 1..64 do jobs.send(n)
    println(Future.all(workers).await().sum())
    0
```

### Executors

**An `Executor`** is a pool of threads of one's own, chosen explicitly. `Executor.fixed(n)` keeps at
most `n` tasks working at once, `n` at least one, and starts its workers as work comes, as the shared
pool does; a worker waiting in `await` or `blocking` leaves its place to another there too.
`ex.future(body)` runs `body` on it, and **a future runs its `map`, `flatMap` and `onComplete` on the
executor it was made on**, so a chain begun on an executor stays there. `Executor.shared()` is the
pool `Future(body)` uses, the same executor every time it is asked (`Executor.shared() ===
Executor.shared()`); `Future.successful`, `Future.all`, `Future.first`, `Future.traverse` and a
promise's future belong to it.

An executor is sendable, so a body on one thread may give work to an executor another made.
`ex.shutdown()` lets the workers end once the queued work is done, and `isShutdown()` asks. A new
body given to an executor after its shutdown traps, but the functions of futures already made on it
still run there, on a worker started for them where none is left, so no future made before is
stranded. The shared executor is never shut down: `isShutdown()` answers `false`, and
`shutdown()` on it traps.

**`ex.execute(task)`** runs a `@sendable` task on the executor's workers with no future of it, the
way to give a pool work that queues itself again and again, as an [actor's](/spec/actors/#scheduling)
turns do. The task runs as an owner of its own and nothing is handed to it: a Sysl handle it reaches
that the giving code owns traps there, and one in transit is taken by its first use. Work given
after `shutdown` still runs, as the functions of futures do.

```avin
import avin.threads.{Executor, Future}

main() -> int
    val io = Executor.fixed(4)
    val sizes = (1..100).map(n -> io.future(n * n)).toVector()
    val total = Future.all(sizes).await().sum()
    io.shutdown()
    println(total)
    0
```

## The memory model

Starting a thread happens before its body; its end happens before `join` returns; releasing a
`Mutex` happens before the next thread takes it; a `send` happens before the `receive` that gets the
value; an `Atomic` store happens before every load that reads it; and completing a future happens
before every `await` that gives its value and before every combinator's function runs with it. A
program whose shared
mutable state is reached only under those edges sees sequentially consistent behaviour.

**A data race on a plain field or `var` is not undefined behaviour.** A racing read returns some
value that was written to that location, never a mixture of two writes and never a value nothing
wrote. The compiler gives each access to a heap field, an array element or a module-level variable
LLVM's `unordered` atomic ordering: it costs nothing on the machines Avin targets, and it is exactly
the promise that a word is never torn and a racing load is never undefined. A reference is one word,
and so is every built-in scalar.

**An integer wider than 64 bits is the one exception.** An `i128` or an `i257` is several words,
and a field or variable of one is read and written as untorn 64-bit words, so a racing read of it
may combine words of different writes. Each word is still one some write stored, and a program that
shares such a value under the edges above, or through an `Atomic` or a `Mutex`, never sees a mixture.

**An object is complete before any thread can see it.** A constructor ends with a release fence, so
a thread that reads a reference to an object, however it got it, sees the object's header and every
field its constructor set. Without it, a racing reader could see a reference field still zero,
which no Avin type allows. The fence is no instruction at all on x86-64 and one `dmb ish` barrier
on arm64, paid once per object made.

## Collection

A collection stops the world. The collecting thread raises a flag, waits until every other thread is
parked at a safepoint or is in a safe region (waiting, or blocked in a call), collects alone, and
lets them go. A lightweight thread that is not running is always safe, and its frames stay roots of
every collection however long it waits. Compiled code polls the flag on every back edge of its loops and once per level of
every recursion, one load and a branch that is not taken; allocation is itself a
safepoint. A program with one thread never stops anything.

## Actors

[Actors](/spec/actors/) handle their messages in turns, each turn a lightweight thread of its own
([scheduling](/spec/actors/#scheduling)), so a turn that waits parks as any thread does. An actor
handles one message at a time, so its own state needs no lock; its mailbox is a runtime queue on the shared
heap, and a message is a reference to a heap value. A message holds only values no other actor can
change, or hands over the ownership of what it carries, and the compiler checks every message type
for it ([what may be sent](/spec/actors/#what-may-be-sent)).
