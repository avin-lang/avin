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

- `spawn[T](body: () -> T, stack: long = 8388608) -> Thread[T]` starts `body` on a new operating
  system thread with a stack of its own (8 MiB unless asked otherwise). `join() -> T` waits for it
  and gives back the result; joining a second time gives the same result again. Any thread may
  join, several at once: one waits for the thread and the others wait for that one. A `Thread` is
  collected like any other object once nothing reaches it; a thread that is never joined keeps its
  stack until the program ends. A thread that overflows its own stack traps with `stack overflow`
  like any other thread's trap.
- **A trap in any thread stops the program**, as it does in a program with one thread: traps report
  bugs, and a bug in one thread leaves the state the others share in doubt. Only the first thread to
  stop the program reports: what was written is flushed once and one reason is printed, and a
  thread that traps after it, or calls `exit`, waits for the end.
- **The program ends when `main` returns**, whatever other threads are still running: standard
  output is flushed and the process exits with `main`'s status, taking the other threads with it.
- A library scope makes the joins structural: `scope(s -> …)` returns its body's result only once
  every thread started through `s.spawn` has finished. `s.spawn` takes the same arguments as
  `spawn` and is called by the thread that runs the scope's body, while the scope is open; a call
  from another thread, or after the scope has returned, traps. A thread the scope started that
  starts more uses a scope of its own.

```avin
import avin.threads.scope

main() -> int
    val parts = Array.fill(4, 0)
    scope(s ->
        for i in 0..<4
            s.spawn(() -> parts(i) = (i + 1) * 10))
    parts(0) + parts(1) + parts(2) + parts(3)
```

## What is shared

The heap is shared: every object is reachable from every thread that is handed a reference to it.
Module-level `val`s, `var`s and `object`s are shared by all threads. A closure's captured `var`
lives in a box on the heap, so a closure run on another thread shares it with the scope that made
it. Nothing is copied when a thread starts; the closure and what it captures are simply reachable
from the new thread too.

A [Sysl handle](/spec/sysl-bindings/#handles) is the exception: it belongs to one owner at a time,
and a use by any other code traps. The handles a thread's body or a future's body reaches move to
it when it starts, and are in transit, for the next code to use them, once it returns; an actor
will own its handles across every message, whichever thread runs it.

## Synchronization

`avin.threads` offers, all written in Avin over Sysl's atomics and futex-based locks:

- **`Mutex[T](value)`**, which owns the value it guards. `m.with(v -> …)` takes the lock, runs the
  body with the value and lets the lock go when the body returns, giving back what it returned; it
  is the way to a mutable value, such as a `Buffer`, which the body changes in place. For an
  immutable value, `get()` reads it, `set(v)` replaces it and `update(f)` replaces it with `f` of it
  under the lock, giving back the new value. A mutex is not reentrant: a thread that takes one it
  already holds, from inside a body, would wait for ever, so it traps instead.
- **`Condition`**, made by a mutex's `condition()` and waited for inside that mutex's `with`.
  `wait()` lets the mutex go while it sleeps and holds it again before it returns; `notify()` wakes
  one waiting thread and `notifyAll()` every one. A wait may end with nothing having changed, so the
  state is checked again in a loop. `waitFor(timeoutMillis: long) -> bool` waits at most that many
  milliseconds and answers `false` where the time ran out first; a time that is not positive
  answers `false` at once. Waiting from a thread that does not hold the mutex traps.
- **`Atomic[T](value)`**, with `load`, `store`, `swap`, `compareAndSet(expected, desired) -> bool`
  and `update(f)`, and for the integers `add(n)` and `sub(n)`, each giving back the new value and
  trapping on overflow, as `+` and `-` do, before anything is stored. Every operation is
  sequentially consistent; there are no weaker orderings in the library. `int`, `long`, `bool` and
  references are the types it is for, and it holds any type: a value of one word is accessed
  lock-free, an integer wider than 64 bits under a lock the runtime keeps for them. A
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
blocked on a lock, a condition, a channel, a join or input never holds up a collection. A Sysl
binding [declared `blocking`](/spec/sysl-bindings/#calls-that-wait) does the same. **A deadlock is not detected**, apart from a thread
taking a lock it already holds: threads that each wait for another sleep for ever.

## Futures

A `Future[T]` is a value some thread is computing. `Future(body)` runs `body` on a shared pool of
worker threads and gives back the future of its result at once; the body is a by-name argument, so
it is written in place or as a trailing block:

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
  without waiting. `isCompleted()` asks and `value()` gives `Some` of a value already there; neither
  waits. Times are milliseconds as a `long`, the unit every timed wait in the library takes.
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
- **A `Promise[T]()`** is completed by another thread: its `future` field is the future it completes,
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
least `d` (from [`avin.time`](/spec/system-library/#avin-time)), safe for the collector while it
sleeps; a duration of zero or less returns at once. A worker that sleeps keeps its place, and
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
pool `Future(body)` uses; `Future.successful`, `Future.all`, `Future.first`, `Future.traverse` and a
promise's future belong to it.

`ex.shutdown()` lets the workers end once the queued work is done, and `isShutdown()` asks. A new
body given to an executor after its shutdown traps, but the functions of futures already made on it
still run there, on a worker started for them where none is left, so no future made before is
stranded. The shared executor is never shut down: `isShutdown()` answers `false`, and
`shutdown()` on it traps.

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
lets them go. Compiled code polls the flag on every back edge of its loops and once per level of
every recursion, one load and a branch that is not taken; allocation is itself a
safepoint. A program with one thread never stops anything.

## Actors

[Actors](/spec/actors/) are scheduled by the runtime onto a pool of these threads. An actor handles
one message at a time, so its own state needs no lock; its mailbox is a runtime queue on the shared
heap, and a message is a reference to a heap value. A message holds only values no other actor can
change, or hands over the ownership of what it carries, and the compiler checks every message type
for it ([what may be sent](/spec/actors/#what-may-be-sent)).
