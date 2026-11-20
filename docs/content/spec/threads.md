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
  val worker = spawn(() -> sum(1 .. 1000000))
  val mine = sum(1000001 .. 2000000)
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
  status, taking the other threads with it. A `main` answering `Err` ends it the same way, its
  error line written first and with status 1 (see [`main`](/spec/declarations/#main)).
- A library scope makes the joins structural: `scope(s -> …)` returns its body's result only once
  every thread started through `s.spawn` has finished. `s.spawn` takes the same arguments as
  `spawn` and is called by the thread that runs the scope's body, while the scope is open; a call
  from another thread, or after the scope has returned, traps. A `Scope` is not sendable, so a
  thread the scope started cannot reach it; one that starts more uses a scope of its own.

```avin
import avin.threads.{Atomic, scope}

main() -> int
  val parts = (0 ..< 4).map(_ -> Atomic(0)).toVector
  scope(s ->
    for i in 0 ..< 4
      s.spawn(() -> parts(i).store((i + 1) * 10)))
  parts.map(p -> p.load()).sum
```

### Cancellation

A scope can end its threads early. `s.cancel()` cancels every thread the scope has started, and every
one it starts from then on; the scope still returns only once all of them have ended, and its body
goes on running after the call. What a program can rely on:

- **A cancelled thread stops at its next cancellation point** and ends with no result: nothing of its
  body runs after that point but its [deferred statements](/spec/operators-and-control-flow/#defer),
  which run there, innermost first, as its frames are dropped. A deferred statement runs to its end
  once begun: its own waits are not cancellation points while it runs, and a cancellation that comes
  meanwhile stops the thread at its next cancellation point after. Its `result() -> Option[T]` answers `None`, and its `join()` traps,
  since there is no value to give back; a thread that finished before its cancellation reached it
  keeps its result, and `join` answers it as before. A thread started in a cancelled scope runs
  nothing.
- **Every wait is a cancellation point**: a `sleep`, a join, a channel's `receive` or a full channel's
  `send`, timed or not, a `select`, a `withTimeout`'s wait for its body, a future's `await`, a
  promise's future, a condition's `wait` (plain or timed), a wait group's `wait` (plain or timed), a
  semaphore's `acquire` (plain, timed or in `withPermit`), a read-write lock's `read` and `write`
  waiting to go in, a barrier's `wait`, a [pool](#pools)'s `use` or `lease` waiting for a resource, a socket's
  `accept`, `connect`, read or write that waits for its connection, and the lookup of a host's name
  that a `connect` or a `bind` makes, however long the resolver takes to answer. A thread
  parked in one is woken at once, however long the wait. **So is every safepoint poll**, which
  compiled code makes on every back edge of a loop and before every call that may recur, so a thread
  that computes without waiting stops too, within a loop's turn.
- **A thread is never stopped where others may be waiting on what it is doing**: while it holds a
  lock (inside a `Mutex`'s `with`, for one), while it writes a `RwLock`, while it runs a barrier's
  action, while it initializes a `lazy val`, while it runs what
  waits for a future it completed. It goes on to the end of that region and stops at its first
  cancellation point after, so a cancelled thread never leaves a mutex held or a value half made. Waiting
  for a lock is not a cancellation point; waiting on a condition is, and a thread stopped there takes
  its mutex again, so the deferred statements of the `with` body run under it, and `with` lets it go
  after them. A wait that left a mark on what it waits for takes it back as it stops:
  a writer or a reader waiting for a `RwLock` no longer holds back those behind it, and a party
  waiting at a `Barrier` withdraws its arrival. A thread stopped inside a `RwLock`'s read body or a
  semaphore's `withPermit` body gives its share of the lock, or its permit, back as it stops, as one
  inside a pool's `use` or `lease` body gives its resource back; what
  such a body was doing is its own, as any cancelled code's is.
- **Cancellation reaches down through scopes.** A cancelled thread that is running a scope of its own,
  in that scope's body or waiting for its threads, stops first at that scope: the scope body's
  deferred statements run, the inner scope cancels its threads and waits for them to end, each
  running its own, and only then does the thread go on stopping, running those of the functions
  around the scope. So every
  thread below a cancelled scope has ended once that scope returns. Cancelling an inner scope cancels
  only its own threads. A scope opened while its thread holds a lock is not cancelled with that
  thread, since nothing stops the thread there: its threads finish, and the thread stops once it has
  let the lock go.
- **Actors are not a scope's threads.** An actor started by a scope's thread is a top-level actor,
  and its turns are threads of their own, so cancelling the scope does not reach it: it runs on and
  ends as any actor ends, having handled what the cancelled thread told it, in the order told. A
  thread parked in an actor's `ask` is parked in a future's `await`, so it stops at once; the
  question stays in the actor's mailbox and is handled in its place, and its reply, going to an ask
  nobody awaits, is dropped, the ask's timer with it. An actor's turn is never cancelled by a scope,
  but the scopes a turn runs are cancelled as any thread's are.
- `s.isCancelled()` answers whether the scope has been cancelled, by `cancel` or by a cancellation of
  the thread running it. `cancel`, like `spawn`, is called by the thread that runs the scope's body
  while the scope is open, and traps otherwise; cancelling twice is cancelling once.
- **A failure is a value.** A trap still stops the whole program, so a scope's threads report a
  failure by returning or sending a `Result`, and the body that sees it cancels the scope and returns
  it. A cancelled thread runs no code of its own as it stops: what it must leave tidy is either under a
  lock, which it finishes, or its own, which nothing else reaches. The Sysl handles its body reached,
  a connection for one, are handed back once the scope has waited for it, as a body that returns
  hands them back itself, so the code after the scope may use or close them.

```avin
import avin.threads.{Channel, scope}

fetchAll(urls: Vector[string]) -> Result[Vector[string], string]
  val pages = Channel[Result[string, string]](urls.length)
  scope(s ->
    for u in urls do s.spawn(() -> pages.send(fetch(u)))
    var got: Vector[string] = []
    var failure: Option[string] = None
    while got.length < urls.length && failure.isEmpty
      pages.receive() match
        Some(Ok(page)) -> got = got :+ page
        Some(Err(e)) ->
          failure = Some(e)
          s.cancel()
        None -> ()
    failure match
      Some(e) -> Err(e)
      None -> Ok(got))
```

## Lightweight threads

Every thread a program starts is a lightweight thread: the runtime runs it on one of a few operating
system threads of its own, its **workers**, about one for each processor, started as threads become
ready. What a program can rely on:

- **Threads are cheap.** A thread's stack is reserved at its full size and the system commits its
  pages only as the thread first touches them, so a thread that has not recursed deeply costs a page
  or so of memory and a small record. A program may start a hundred thousand threads and join them
  all, and a deep recursion still fits in a thread as it does in `main`.
- **A parked thread holds only the stack it is using.** A page of a stack stays committed once
  touched, so the runtime gives pages back: a thread that has stayed parked for about a millisecond
  (a tick of the runtime's monitor, with no run between) returns every page below where it parked
  but the 16 KiB just under it, and the stack of an ended thread kept for the next returns all but
  its top 16 KiB. A
  thread that recursed a hundred thousand calls deep and then waits costs, while it waits, what a
  shallow one does; woken, it recurses as deep again on pages committed afresh. A thread parked
  only briefly, as one handing values back and forth parks, has run again before its tick and is
  not trimmed, so a park costs nothing more.
- **A wait parks only its thread.** A thread that waits for a lock, a condition, a channel, a join, a
  future or a promise is set aside, and its worker runs another ready thread meanwhile; the thread
  that ends the wait makes it ready again. Ready threads run in the order they became ready, and a
  thread may resume on a different worker from the one it waited on. A timed wait parks the same way
  and is made ready when its time runs out, and so does a `sleep`: ten thousand threads sleeping at
  once hold no worker between them and all wake in about one sleep. Threads whose times run out
  together are made ready soonest first, two due at the same moment in the order they began to wait.
  A timed wait that its event ends first answers once, as woken, and its timer goes with it.
  A thread waiting on a [connection](/spec/system-library/#avin-net), to accept, connect, read or
  write, parks the same way on the runtime's poller, one kernel readiness set for the whole program,
  and is made ready when the socket is; a connection's timeout parks it on a timer as well, and
  whichever comes first ends the wait. So
  waiting code is written as plain straight-line code:
  a server may hold one thread per connection, thousands of them waiting at once on a few workers,
  and a chain of futures each awaiting the one before it resolves however long it is.
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
  and its own waits block that thread while the workers run the rest. It stays free for work that
  has to run there, a window above all, which the `ui` package runs on it while other threads
  [hand it work](/spec/ui/#threads). `isMainThread()` answers whether the calling code runs on it:
  `true` in `main` and what it calls, `false` on every lightweight thread, a pool's workers among
  them.

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
A function type carries no marks either, so a function, a constructor or a method whose parameter
is marked is a value only where the value's type gives that parameter a sendable type, a `@sendable`
function type for one that takes a function: `spawn` is a `(@sendable () -> T, long) -> Thread[T]`,
and as a `(() -> unit, long) -> Thread[unit]` it is refused, as `Mutex` as an `(Array[int]) ->
Mutex[Array[int]]` is. Where no function type is expected, the value takes the `@sendable` form of
each marked parameter that takes a function, so `val sp = spawn[int]` holds it and what is passed
through `sp` is checked where it is written. A by-name parameter so marked has no such type, so
`isolate` is not a value. A class or a data type that is a function has an `apply` that marks a
parameter only where its function type makes that parameter sendable likewise.

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
reach, every closure made on the way, an extractor a pattern calls, and the initializer of every
`lazy val`, module-level or a field, that it reads, since its first read runs the initializer on
whichever thread makes it. Which implementations a call may reach follows the types its receiver may
have: a data type's own, a class's and its subclasses', those of the types conforming to a trait or
to a type parameter's bounds, and, in a generic function called at known type arguments, those of
the types it is called at. So `Map[int, string]` in a body never reaches a program's own `hash`, and
`[Money(1), Money(2)].sum` reaches `Money`'s `+`. A module-level `val` of an
`Atomic`, a `Mutex`, a `Channel` or immutable data is the way to share state between threads; a
`lazy val` of a sendable type is initialized once whichever threads read it. A local `lazy val` a
body captures brings its initializer along, so what that initializer captures and reaches is held to
these rules too. The error names the
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
program's, and each call's output goes out whole: a line one thread prints is never split by
another's (system-library.md § Standard input). Reading standard input is legal from any thread
too: threads read it one at a time, and a line goes whole to exactly one of them. What is passed for an `@isolating` parameter is held to it too, though it runs at once on
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
- **`Channel[T](capacity = unbounded)`**, a queue between threads: unbounded with no capacity
  (`unbounded` is the largest `int`), bounded with a positive one, where `send` waits while the
  channel is full, and a [rendezvous](#rendezvous) with a capacity of zero; a negative capacity
  traps. `receive()` waits for a value and gives `Some` of it, or `None` once the channel is closed
  and every value sent before is received. `close()` wakes every waiting thread, and a send to a
  closed channel traps; closing twice does nothing. `trySend(x) -> bool` and
  `tryReceive() -> Option[T]` never wait, and `isClosed()` asks. A channel is `Iterable`:
  `for x in ch` receives until it is closed and drained. The waits have
  [timed forms](#timers-and-deadlines), `receive(timeout)` and `send(x, timeout)`.
- **`Once`**, whose `run(body)` runs its body exactly once, whichever threads call it and however
  many at once: every other call waits until that body has returned, and returns without running its
  own. `isDone()` says whether it has run. A body that runs its own `Once` again traps, as a mutex
  taken twice does.
- **`WaitGroup()`**, a count of work under way. `add(n)` counts `n` more pieces (or `-n` finished,
  where `n` is negative), `done()` counts one finished, and `wait()` sleeps until the count is zero,
  returning at once where it is. `waitFor(timeoutMillis: long) -> bool` and
  `waitFor(timeout: Duration) -> bool` wait at most that long and answer whether the count reached
  zero; a time that is not positive only asks. `pending()` reads the count. It counts work, not
  threads, so the pieces may be finished by threads started anywhere (a scope, an executor, an
  actor) or by no new thread at all; threads a body starts and waits for itself are a
  [scope](#starting-and-joining)'s to count, so a wait group has no `spawn` of its own. A count taken
  below zero traps, since a piece was finished twice or never added. A wait group is sendable.
- **`Semaphore(permits: int, capacity: int = permits)`**, a count of permits. `acquire()` takes one,
  sleeping while there is none; `tryAcquire() -> bool` takes one only where one is free now;
  `acquireFor(timeoutMillis: long) -> bool` and `acquireFor(timeout: Duration) -> bool` wait at most
  that long; `release()` gives one back, waking a waiter. `withPermit(body)` takes a permit for its
  body and gives it back when the body returns, giving back what it returned; a cancellation that
  stops the thread inside the body gives the permit back too, and a trap inside it stops the
  program even in an [actor](/spec/actors/), since the permit would never come back. A semaphore holds at most `capacity`
  permits, `permits` of them at first, so `Semaphore(0, capacity = 1)` starts empty and signals. A
  release that would hold more than the capacity traps, since it gives back a permit nobody took, as
  does a semaphore made with negative permits, a capacity below one, or more permits than its
  capacity. Permits belong to no thread: one thread may take a permit another gives back. Waiters
  take no place in a queue, so a thread that asks just as a permit comes back may take it before one
  that has waited longer. A semaphore is sendable.
- **`RwLock[T](value)`**, a value many threads may read at once and one at a time may write. It owns
  its value as a `Mutex` does, made in the argument, and its bodies are `@isolating` and give back a
  sendable value: `read[R: Sendable](@isolating body: (T) -> R) -> R` runs alongside any other
  readers, `write[R: Sendable](@isolating body: (T) -> R) -> R` alone. A read body only reads; one
  that changes a value other readers are reading races with them, which nothing checks. For a
  sendable value `get()` reads, `set(v)` writes and `update(f)` replaces the value with `f` of it,
  giving back the new value. **Writers come first**: once a writer waits, a thread that asks to read
  waits behind it, so a stream of overlapping readers never keeps a writer out; and when a write
  ends, every reader that was waiting goes in before the next writer, so a stream of writers never
  keeps those readers out. A write body runs to its end once begun, as a mutex's does; a read body is
  stopped by a cancellation like any other code, and its share of the lock is given back as it stops.
  A trap inside either body stops the program, even in an actor, as one under a mutex does.
  It is not reentrant: reading or writing inside one's own write traps, and writing inside one's own
  read, or reading inside it while a writer waits, waits for ever. It is sendable whatever `T` is.
- **`Barrier(parties: int, action: () -> unit = () -> ())`**, a meeting point for `parties` threads.
  `wait() -> bool` sleeps until that many have called it, then all go on and the barrier is ready for
  the next round; the last to arrive runs the `@sendable` `action` first, once a round, with the
  others still waiting, and its `wait` answers `true`, every other `false`. The action runs to its
  end once begun, and one that waits on its own barrier traps. A party whose thread is cancelled while
  it waits takes its arrival back, so the round still waits for `parties` threads. `parties` reads the
  number and `waiting()` how many wait now. A barrier has at least one party, and is sendable.
- **`lazy val`s**, module-level, fields and locals alike ([Initialization](declarations.md#initialization)),
  initialized by exactly one thread, as a `Once` is run: a thread that reads one while another runs
  its initializer waits, safe for the collector, and the stored value is published with a release
  store that every read pairs with an acquire load, so no thread sees it half made. A read by the
  initializing thread itself traps, as a `Once` run again from its body does, and a trap in the
  initializer stops the program; a read that the initializer's own code reaches, not through a
  function it hands on, is a compile error instead.

```avin
import avin.threads.{Channel, scope}

main() -> int
  val jobs = Channel[int](16)
  val results = Channel[int]()
  scope(s ->
    for w in 0 ..< 4
      s.spawn(() ->
        for n in jobs do results.send(n * n))
    for n in 1 .. 100 do jobs.send(n)
    jobs.close())
  results.close()
  var total = 0
  for r in results do total += r
  total % 256
```

Every operation that can wait marks its thread safe for the collector while it waits, so a thread
parked on a lock, a condition, a channel or a join, blocked on standard input, or writing to standard
output or standard error where they lead to a full pipe, its own trap report included, never holds up
a collection. A lightweight thread waiting for standard input from a pipe, a socket or a terminal
parks and leaves its worker to the others; that wait is not a cancellation point.
A Sysl binding [declared `blocking`](/spec/sysl-bindings/#calls-that-wait) does the same. A thread
waiting for a mutex takes it in turn: the mutex goes to the thread that has waited longest. A thread
taking a lock it already holds traps at once. A program whose threads, `main` among them, all wait for
each other stops with a [deadlock report](#deadlock); threads that wait for each other while another
still runs sleep for ever.

### Rendezvous

A channel made with a capacity of zero holds no value at all: each send hands its value straight to a
receiver, so the two threads meet.

```avin
import avin.threads.{Channel, spawn}

main() -> int
  val handoff = Channel[string](0)
  val worker = spawn(() ->
    var n = 0
    for job in handoff do n += job.charCount
    n)
  for job in ["parse", "check", "emit"] do handoff.send(job)
  handoff.close()
  worker.join()
```

What a program can rely on:

- **A send returns only once a receiver has the value.** `send(x)` waits until a receiver takes `x`,
  and a receive waits until a sender hands it one. Everything the receiving thread did before its
  receive happens before the send returns, as everything the sender did before its send happens
  before the receive returns, so each side knows the other has reached the meeting point.
- **The `try` forms need a partner waiting now.** `trySend(x)` hands `x` over only where a receiver is
  already waiting, a plain `receive` or a `select` waiting on a receive arm, and answers whether it
  did; `tryReceive()` takes a value only from a sender already waiting. Neither waits. A thread that
  has not yet begun to wait is not waiting.
- **The timed forms give up cleanly.** `send(x, timeout)` answers `false` where no receiver took `x`
  within the time, and then `x` is received by nobody; `receive(timeout)` answers `None` where no
  sender came. A timeout of zero or less is the `try` form.
- **A `select` arm hands a value over only when it is paired.** A send arm runs once a receiver has
  taken its value, a receive arm once a sender has handed it one, and the partner may be a plain
  `send` or `receive` or another select's arm, waiting first or coming while the select waits. Every
  arm that does not run hands nothing over, so a value meant for a send arm that lost is never
  received. A select never pairs with itself: a send and a receive arm on one channel wait for other
  threads. With `else`, a select pairs only with a partner already waiting.
- **A close ends every hand-off.** A sender waiting when the channel is closed traps, as any send to a
  closed channel does, its value handed to nobody; a receiver waiting gets `None`, and so does every
  receive after, since a closed rendezvous has nothing to drain. A select's send arm traps and its
  `None` arm runs, as on any closed channel.
- **Its waits are waits like any other.** A thread waiting to hand a value over, or to take one, parks
  only itself, is safe for the collector, and is a cancellation point: cancelled, it leaves nothing
  on the channel, and the next sender's value goes to the next receiver. A cancellation that comes
  after a value was handed over stops a receiver that has it. The wait counts for the
  [deadlock check](#deadlock), whose report names a sender as *waiting to hand a value to a
  receiver*. Which of several waiting receivers gets a value is not specified.
- **Sendable as any channel.** A channel of capacity zero is sendable where its values are, as every
  channel is.

### Pools

A **`Pool[T, E]`** holds resources that are costly to make, connections to a database or a server
for one, and lends them again and again. `use(f)` lends one for the call of `f`, takes it back when
`f` returns, and answers `Ok` of what `f` returned, or the `PoolError[E]` that kept the call from
getting one:

```avin
class Pool[T, E](
    create: @sendable () -> Result[T, E],
    maxSize: int,
    minIdle: int = 0,
    validate: Option[@sendable (T) -> bool] = None,
    destroy: Option[@sendable (T) -> unit] = None,
    idleTimeout: Option[Duration] = None,
    acquireTimeout: Option[Duration] = None,
    maxLifetime: Option[Duration] = None,
    jitter: Option[Duration] = None,
    maxWaiting: Option[int] = None
): Sendable

data PoolError[+E]
  AcquireTimedOut
  QueueFull
  PoolClosed
  CreateFailed(error: E)

data PoolStats(size: int, idle: int, lent: int, waiting: int)

data PoolCounters(
    acquires: long,
    waited: long,
    waitTime: Duration,
    timeouts: long,
    rejected: long,
    created: long,
    destroyed: long
)
```

- **A resource is made when it is wanted.** A call that finds a resource idle takes the one given
  back last; one that finds none, with fewer than `maxSize` made, makes one with `create`; one that
  finds all `maxSize` lent waits for one to come back. A `create` that fails makes the call fail with
  `CreateFailed`, carrying the error, and gives up its place, so the pool holds nothing more than
  before and the next call tries again. `warm()` makes resources until the pool holds `minIdle`, each
  left idle, answering the first failure of `create`.
- **Waiters are served in the order they came.** A resource given back goes straight to the call that
  has waited longest, and so does the place a destroyed resource gives up, where that call then makes
  a resource of its own; a call that comes while others wait queues behind them.
- **A call waits for at most its timeout.** `use(timeout, f)` waits at most `timeout`, `use(f)` the
  pool's `acquireTimeout`, or for ever where there is none, and a call whose time runs out fails
  with `AcquireTimedOut`, leaving the queue as if it had never come. A timeout of zero or less takes
  a resource only where one is idle, or can be made, now.
- **The queue may be bounded.** Where `maxWaiting` calls already wait, a call that would join them
  fails at once with `QueueFull`, so a server can turn a request away instead of letting it queue;
  a call that finds a resource idle, or a place to make one, is never turned away, and a zero
  `maxWaiting` means no call ever waits. With no `maxWaiting` the queue has no bound. A negative
  `maxWaiting` traps where the pool is made.
- **A broken resource is destroyed, not lent again.** `lease(f)` and `lease(timeout, f)` lend the
  resource as a `Lease[T]`, whose `resource` is the resource and whose `markBroken()` says it is no
  good any more: the pool destroys it, with `destroy` where one is given, when the body returns.
  `validate` checks an idle resource before it is lent again, never one just made: one it answers
  `false` for is destroyed and the call looks again.
- **Idle resources expire.** A resource idle for longer than `idleTimeout` is destroyed the next time
  a call looks for one, oldest first, keeping at least `minIdle`; `evictIdle()` does the same at once
  and answers how many it destroyed. With no `idleTimeout` nothing expires.
- **Old resources retire.** A resource older than `maxLifetime` is never lent again: one found idle
  is destroyed instead of being lent, and the call looks again; one given back is destroyed instead
  of being kept; and `evictIdle()` destroys the idle ones however few that leaves, `warm()` making
  up `minIdle` again. `jitter` lengthens each resource's `maxLifetime` and `idleTimeout` by its own
  random part of `jitter`, from none up to all of it, drawn when the resource is made, so resources
  made together do not all expire together; it never shortens either limit.
- **The resource always comes back.** It is taken back when `f` returns and when the thread running
  `f` is cancelled inside it. A cancellation anywhere else in the call puts back what the call held:
  its place among the waiters, a resource handed to it there, the place it held while `create` ran,
  a resource `validate` was checking, which goes back as one given back does (destroyed only where
  the pool is closed by then or the resource has outlived `maxLifetime`) and is checked again before it is next lent, so a deadline passing
  during a check never destroys a good resource. A trap inside `f`, `create`, `validate` or
  `destroy` stops the program, even in an [actor](/spec/actors/), as one inside a semaphore's
  `withPermit` does, since what the pool counts would otherwise never come right.
- **`close()` ends the pool.** Every call waiting fails with `PoolClosed`, as does every call after,
  the idle resources are destroyed, and `close` waits for the lent ones to come back, destroying each
  as it does; a resource whose make finishes after the close is destroyed too. Closing a closed pool
  waits for the same, and a thread that closes a pool while a resource is lent to it waits for ever.
  `isClosed()` says whether it was closed.
- **`stats()`** answers what the pool holds now: `size` resources in all, of which `idle` wait to be
  lent and `lent` are lent, and `waiting` calls waiting for one; the rest are being made or checked.
  `maxSize` below one, and `minIdle` below zero or above `maxSize`, trap where the pool is made.
  **`counters()`** answers running totals since the pool was made: `acquires` calls lent a resource,
  `waited` of them after waiting in the queue, and `waitTime` all of them took from the call to the
  lending, making or checking a resource included; `timeouts` calls whose time ran out; `rejected`
  calls turned away with `QueueFull`; and `created` resources made and `destroyed` let go, for
  whatever reason. A `create` that fails makes nothing.
- **A resource belongs to one call at a time**, so a pool of any type is sendable, as a `Mutex` is,
  and its resources may be mutable connections. So that nothing else reaches one, `create`,
  `validate` and `destroy` are `@sendable`, and a body is `@isolating` and gives back only a sendable
  value: it may not keep the resource, or its lease, or hand it to the body of another pool's call.
  Each resource given back has its [Sysl handles](/spec/sysl-bindings/#handles) put in transit, so
  the thread or actor it is lent to next uses them as its own.

```avin run
import avin.threads.{Atomic, Pool, scope}

class Conn(val id: int): Sendable

main()
  val made = Atomic(0)
  val db = Pool[Conn, string](() -> Ok(Conn(made.add(1))), maxSize = 2)
  val total = Atomic(0)
  scope(s ->
    for n in 1 .. 10 do s.spawn(() -> _ = db.use(c -> total.add(n))))
  println(s"total ${total.load()}, made at most 2: ${made.load() <= 2}")
  val st = db.stats()
  println(s"lent ${st.lent}, all idle: ${st.idle == st.size}")
  db.close()
  println(db.use(c -> c.id))
```

```text output
total 55, made at most 2: true
lent 0, all idle: true
Err(the pool is closed)
```

## select

A `select` block waits on several channels at once, and on connections, and runs exactly one of its
arms: the first whose channel operation can complete or whose connection has bytes to read.
`select` ending its line opens the block of its arms, written as a match's are; elsewhere `select`
is an ordinary name. As with a match, inside brackets whose commas separate items a comma after a
one-line arm's body ends the arm, the `select` and the item, so in `f(select` …
`ch.receive() as v -> v, "a")` the `select` is the first argument and `"a"` the second; a tuple
body there is written in parentheses.

```avin
import avin.threads.Channel
import avin.time.millis

step(jobs: Channel[int], results: Channel[int], r: int) -> string = select
  jobs.receive() as Some(j) -> s"job $j"
  jobs.receive() as None -> "no more jobs"
  results.send(r) -> "sent"
  after 100.millis -> "idle"
```

What a program can rely on:

- **The arms.** `ch.receive()` receives, with an optional `as` pattern: `Some(p)` runs for a value
  and binds it by `p`, any pattern a `val` takes; `None` runs once the channel is closed and every
  value sent before is received; a name runs for either and binds the `Option` received; `_`, or no
  pattern, runs for either and binds nothing. `ch.send(x)` sends, and binds nothing.
  `stream.readable()` runs once a read of `stream` would answer without waiting, and binds nothing:
  its body reads. `sink.writable()` runs once a write to `sink` would send something without
  waiting, and binds nothing: its body writes, with `writeSome` to send only what goes at once.
  `after d` runs
  where no other arm became ready within the [`Duration`](/spec/time/) `d`; `else` runs where no
  arm is ready now, and is written as a match's `else` is, with no arrow. A select has at most one
  `after` and one `else`, and not both, since a select with `else` never waits. Any other arm, an
  `as` pattern on a `readable()` or `writable()` arm among them, is an error.
- **Guards.** Any arm but `else` may end in an `if` guard before its arrow, as a match arm does:
  `local.writable() if pending.length > 0 -> …`, `ch.receive() as Some(x) if open -> …`, `after d
  if waiting -> …`. Where the guard is `false` the arm is absent from this select: its channel or
  stream is not evaluated, its value is not evaluated or sent, nothing of it is watched, and it never
  runs, as if it were not written. A guard is evaluated before anything is received, so unlike a
  match guard it cannot name what its arm's pattern binds. An absent `after` leaves a select that
  waits until an arm runs.
  `else` takes no guard: what follows it is its body, so `else if c then a else b` is an `if`. A
  select whose every arm is guarded off, with no `after` or `else` left, traps when it is reached,
  since nothing but a cancellation could end its wait; with an `else` it runs the `else`, and with
  an `after` it waits its time.
- **Evaluation.** The arms are evaluated once, in the order they are written, before any arm is
  tried: an arm's guard first, then, where it is `true` or there is none, its channel or stream, its
  value sent and the `after`'s duration. Only the arm that runs
  receives or sends: every other channel keeps its values, and a value meant for an arm that does not
  run is never sent. A `readable()` arm reads nothing and a `writable()` arm writes nothing,
  whether it runs or not.
- **Streams.** A `readable()` arm's stream is any `Readiness` (`avin.threads`), which every
  [connection](/spec/system-library/#avin-net), a `ByteStream`, is: TCP, Unix and TLS. It is ready
  where the stream's `readable()` answers `true` when the arm is tried: bytes already buffered, those
  a `readLine` read beyond its line or plaintext TLS has decrypted and not yet handed over, or bytes
  arrived on the socket, and also the other end having finished sending or reset the connection, or
  the connection closed, each of which its read then answers. A TLS stream counts only whole records
  of plaintext, so bytes of a record still arriving do not make it ready. Where it is not ready, a
  select that waits watches the stream's descriptor on the runtime's poller under the same one wait
  as its channel arms, and whichever comes first, bytes, a channel's value, the `after`'s time or a
  cancellation, ends it. The arm uses its stream as a read does, so only the thread a connection
  belongs to may name it in a select; any other traps. This is how a thread that owns a connection
  waits both for its peer and for work other threads send it: it does all of the connection's reads
  and writes, which only it may. A connection [split](/spec/system-library/#tcp) into a reading and
  a writing half gives each half to a thread of its own, the reader's select naming its half's
  `readable()` and the writer's its half's `writable()`.
  [Standard input](/spec/system-library/#standard-streams-as-bytes), `io.stdin()`, is a
  `Readiness` too: ready where bytes are in the buffer every reader of it shares, those a
  `readLine` read past its line among them, or have arrived, or the input has ended or failed, or it
  is a file. It belongs to no thread, so any thread may name it, and several at once; a select
  waits on it while another thread reads it, and bytes that read leaves in the buffer end the wait
  as arriving bytes do.
- **Sinks.** A `writable()` arm's sink is any `Writability` (`avin.threads`): every connection, and
  the writing half of a split one. It is ready where the sink's `writable()` answers `true` when the
  arm is tried: the socket has room, or the connection has failed or is closed, which its write then
  answers. A TLS sink is ready once the record an earlier `writeSome` began has gone, `writable()`
  sending what of it there is room for; the writing half of a split TLS connection sends each
  record whole before its `writeSome` answers, so the loop below leaves nothing behind when it ends. Where it is not ready, a select that waits watches the
  descriptor for room on the poller as a `readable()` arm watches it for bytes, and a reading and a
  writing half of one connection may each wait on it at once, each woken by its own direction.
- **Choosing.** Of several arms ready at once, each is as likely to run as any other: the arms are
  tried in an order drawn at random each time. Where none is ready, a select with `else` runs it, and
  one without waits, parking only its thread, until an arm can complete and runs it, or until its
  `after`'s time runs out. An `after` of zero or less tries the arms once, as `else` does. A select of
  `after` alone waits its duration; one of `else` alone runs it.
- **Closed channels behave as `receive` and `send` do.** The values sent before a close are received
  first; then a `None` arm, a name and `_` run, every time after. `Some(p)` is never ready on a
  closed and drained channel, so a select whose only arms want values from closed channels waits for
  its `after`, runs its `else`, or waits for ever. A send arm on a closed channel traps when it is
  tried, as `send` does.
- **Its value.** A select is an expression whose value is the arm that ran, the arms' types unified as
  a match's are. Its bodies are code of the function it is in: `return`, `break` and `continue` leave
  an arm as they leave a match's, and a select may be nested in another's arm. `end select` may close
  one.
- **Waiting.** A select that waits is a wait like a channel's own: safe for the collector, and a
  cancellation point, so a scope's `cancel` stops a thread parked in it at once. Nothing of a
  select that has ended, by an arm, its time or a cancellation, is left on the channels or the
  poller it waited on, and a notification a select was woken by and could not use is passed on to the next thread
  waiting on that channel, so no other waiter, a plain `receive` or another select, misses it.

```avin
import avin.threads.Channel

// Workers take jobs until told to stop; each result is sent where there is room for it.
work(jobs: Channel[int], stop: Channel[unit], results: Channel[int])
  var going = true
  while going
    select
      jobs.receive() as Some(j) -> results.send(j * j)
      jobs.receive() as None -> going = false
      stop.receive() as None -> going = false
```

```avin
import avin.io.IoError
import avin.net.TcpStream
import avin.threads.Channel

// The thread that owns a connection echoes each line its peer sends and forwards each message other
// threads put on `pushed`, until the peer finishes sending.
serve(conn: TcpStream, pushed: Channel[string]) -> Result[unit, IoError]
  var open = true
  while open
    select
      conn.readable() -> conn.readLine()? match
        Some(line) -> conn.writeText(s"echo $line\n")?
        None -> open = false
      pushed.receive() as Some(m) -> conn.writeText(s"push $m\n")?
  conn.close()
```

```avin
import avin.io.IoError
import avin.net.StreamWriter
import avin.threads.Channel

// The writing half of a split connection sends `data` a part at a time, as room comes, and stops
// early where `stop` is closed. Answers how much went.
send(w: StreamWriter, data: Array[byte], stop: Channel[unit]) -> Result[int, IoError]
  var at = 0
  while at < data.length
    select
      w.writable() -> at += w.writeSome(data.slice(at, data.length))?
      stop.receive() as None -> return Ok(at)
  Ok(at)
```

```avin
import avin.io.IoError
import avin.net.TcpStream

// Relays bytes both ways between two connections on one thread. Each side is read only while
// nothing it gave is still waiting to be written, and watched for room only while something is.
relayPair(a: TcpStream, b: TcpStream) -> Result[unit, IoError]
  var toA: Array[byte] = Array.empty()
  var toB: Array[byte] = Array.empty()
  var aOpen = true
  var bOpen = true
  while aOpen || bOpen || toA.length > 0 || toB.length > 0
    select
      a.readable() if aOpen && toB.length == 0 ->
        toB = a.read(4096)?
        if toB.length == 0
          aOpen = false
          b.shutdownOutput()?
      b.readable() if bOpen && toA.length == 0 ->
        toA = b.read(4096)?
        if toA.length == 0
          bOpen = false
          a.shutdownOutput()?
      a.writable() if toA.length > 0 -> toA = toA.slice(a.writeSome(toA)?, toA.length)
      b.writable() if toB.length > 0 -> toB = toB.slice(b.writeSome(toB)?, toB.length)
  Ok(())
```

### Selectors

A `Selector[K]` (`avin.threads`) is a select whose arms are made at run time, for a set of sources
whose size is known only then: N connections, N tunnels, N workers' channels. Each entry is added
under a key of the program's choosing, and a wait answers the key of the entry that ran.

```avin
import avin.io.IoError
import avin.net.TcpStream
import avin.threads.Selector

// One direction of a relayed connection pair, and what it has read and not yet written.
class Leg(val from: TcpStream, val to: TcpStream)
  var pending: Array[byte] = Array.empty()
  var open = true

// Relays bytes both ways between each pair of connections, all on one thread, until every pair has
// finished: each leg's reading side is watched while it has nothing pending, and its writing side
// while it has.
relay(pairs: Vector[(TcpStream, TcpStream)]) -> Result[unit, IoError]
  val legs = Buffer[Leg]()
  for (a, b) in pairs
    legs.append(Leg(a, b))
    legs.append(Leg(b, a))
  val sel = Selector[int]()
  while legs.exists(l -> l.open || l.pending.length > 0)
    sel.clear()
    for i in 0 ..< legs.length
      val leg = legs(i)
      if leg.pending.length > 0 then sel.writable(leg.to, i)
      elif leg.open then sel.readable(leg.from, i)
    val leg = legs(sel.wait())
    if leg.pending.length > 0
      leg.pending = leg.pending.slice(leg.to.writeSome(leg.pending)?, leg.pending.length)
    else
      val bytes = leg.from.read(4096)?
      if bytes.length == 0
        leg.open = false
        leg.to.shutdownOutput()?
      else leg.pending = bytes
  Ok(())
```

What a program can rely on:

- **Entries.** `receive(channel, key) -> SelectReceive[T]` adds a receive, ready for a value or the
  channel closed and drained, and answers the entry, whose `received()` is the `Option` received
  once a wait has answered its key. `send(channel, x, key)` adds a send of `x`, which a wait sends
  only where it answers the key; the entry is then taken out, so a value goes once.
  `readable(source, key)` and `writable(sink, key)` add a stream or a sink, as a select's
  `readable()` and `writable()` arms take them. Several entries may share a key.
- **The set.** The entries stay from one wait to the next, so a loop may keep them and change only
  what changed, or `clear()` them and add afresh. `remove(key)` and `removeWhere(p)` take out every
  entry under a key, or whose key satisfies `p`, and answer how many; `size` and `isEmpty` count
  them. The set is changed only between waits, by the thread that waits on it.
- **Waiting.** `wait() -> K` waits until an entry can complete, does its one operation and answers
  its key. `within(timeout) -> Option[K]` waits at most `timeout`, `None` where the time runs out
  first; `until(deadline)` waits until an [`Instant`](/spec/time/); `poll() -> Option[K]` tries the
  entries once, as `else` does, and a timeout of zero or less does the same. A `wait` on an empty
  selector traps, as a select with every arm guarded off does; `within` and `until` on one wait out
  their time, and `poll` answers `None`.
- **The same wait as `select`.** A selector's wait is a select's own: entries tried in an order
  drawn at random each time, so of several ready at once each is as likely to run, and none is
  starved; one pass and one park on the runtime's poller however many entries there are; at most one
  operation done, no other entry receiving, sending, reading or writing; a closed channel, a send to
  a closed channel, a stream used by a thread it does not belong to, cancellation and collection
  all as a select's arms meet them, above.

## Timers and deadlines

Time reaches threads as channels and as limits on their waits, all in `avin.threads`, each taking a
[`Duration`](/spec/time/). Every one of them waits as the other waits do: parking its lightweight
thread on the runtime's timers, holding no worker, safe for the collector, and a cancellation point.

```avin
import avin.threads.{Channel, Ticker, after}
import avin.time.seconds

// Reports progress every second, and gives up on a result after five.
watch(results: Channel[int]) -> Option[int]
  val progress = Ticker(seconds(1))
  val deadline = after(seconds(5))
  var got: Option[int] = None
  var waiting = true
  while waiting
    select
      results.receive() as r ->
        got = r
        waiting = false
      progress.ticks.receive() -> println("still waiting")
      deadline.receive() -> waiting = false
  progress.stop()
  got
```

What a program can rely on:

- **`after(d) -> Channel[Instant]`** gives a channel that delivers one value once `d` has passed from
  the call, at once for a duration of zero or less: the [`Instant`](/spec/time/) at which the time
  fell due, however late it is received. It is never delivered sooner. Only that one value ever
  comes; a receive after it waits for ever, as on any channel nobody sends to, unless the channel is
  closed, which ends it: nothing is delivered after a close. Of several threads receiving from one,
  one gets the value. It is an ordinary channel to receive from, alone, with a timeout, or as a
  `select` arm (`after(d).receive() -> …`, beside the `after d` arm, which it does not replace).
- **An `after` nobody receives from costs nothing but itself.** The channel holds the time its value
  falls due, a number, and no timer, thread or registration anywhere: the value is delivered by the
  first receive or select that looks once the time has come, and a thread waiting on the channel waits
  on its condition until that time at the most, its timer going with its wait. So an `after` that is
  dropped, lost in a select to another arm, or waited on by a thread that is cancelled leaves nothing
  behind and is collected like any object once nothing reaches it, however long its duration.
- **`Ticker(period)`** delivers a tick on its channel `ticks` every `period`, the first a period after
  it is made, each tick the instant it fell due, so each later than the one before. The channel holds
  one tick at the most: **a tick that falls due while one is waiting to be received is dropped**, so a
  slow receiver gets the oldest tick it has not received and then the ticks from its next period on,
  never a backlog to catch up on. `stop()` ends the ticks: none is received after it, not even one
  that fell due before and was waiting; stopping twice is stopping once, and a thread waiting on
  `ticks` goes on waiting. A ticker, like `after`, holds no timer or thread, so one dropped without
  `stop` is collected as any object is. A period of zero or less traps. A `Ticker` is sendable.
- **Nothing sends to these channels but their clocks.** A `send`, `trySend`, timed send or `select`
  send arm on a channel `after` or a `Ticker` made traps.
- **`receive(timeout: Duration) -> Option[T]`** waits at most `timeout` for a value: `Some` of one, as
  soon as it comes, or `None` where the time ran out first or once the channel is closed and every
  value sent before is received; `isClosed()` tells the two apart where it matters. **`send(x,
  timeout: Duration) -> bool`** waits at most `timeout` for room in a bounded channel, or for a
  receiver to take `x` from a [rendezvous](#rendezvous), and answers whether it sent `x`, at once on
  an unbounded one; a value whose time ran out is not sent. A send to
  a closed channel traps, as `send` does, rather than answering `false`. A timeout of zero or less is
  the `try` form, `tryReceive` or `trySend`. Neither waits less than its timeout where nothing comes,
  a part of a millisecond waited for as a whole one.
- **`withTimeout(timeout, s -> …) -> Option[T]`** runs its body with a new [scope](#starting-and-joining)
  for at most `timeout`: `Some` of what the body returns where it returns in time, `None` where the time
  runs out first. Then the body is cancelled where it is, waiting or computing, with every thread its
  scope started and everything below them, as a scope's [`cancel`](#cancellation) cancels them, and
  `withTimeout` answers once they have all ended. A body that has returned keeps its result, however
  close to the time it did. A timeout of zero or less runs nothing and answers `None`. Timeouts nest:
  an inner one ends at its own time or when an outer one's runs out, whichever is sooner, and a
  `withTimeout` on a thread a scope cancels stops with that thread, its body first.
- **The body of a `withTimeout` runs on a lightweight thread of its own**, so that it can be stopped
  wherever it is, even when `withTimeout` is called from `main`, whose own thread no cancellation
  reaches. The calling thread waits for it, so the body reads and assigns the caller's variables, uses
  the Sysl handles the caller owns and those it makes are the caller's after it, as a closure the
  caller ran itself would. What tells the threads apart still does: `isMainThread()` answers `false` in
  the body, and a lock the caller holds is not the body's, so a body that takes it waits for ever, and
  one that waits on a condition of it traps.

## Request context

A piece of work, a request a server handles above all, carries a deadline and values that every call
it makes may need: the time left for a query, the request's id for a log line. They live in the
running thread's **request context**, in `avin.threads`, which code reads where it is rather than
being handed it through every signature on the way. A scope sets it for the code it runs:

```avin
import avin.threads.{Context, Key, withDeadline, withValue}
import avin.time.{now, seconds}

val requestId = Key[string]("request id")

handle(id: string, request: Request) -> Reply
  withValue(requestId, id, () ->
    withDeadline(now() + seconds(5), s -> respond(request)) match
      Ok(reply) -> reply
      Err(e) -> Reply.unavailable(e.message))

// Far below, with no parameter for either.
log(line: string) = println(s"[${Context.current().value(requestId).getOrElse("-")}] $line")
```

- **`Context.current() -> Context`** is the calling thread's context: what the scopes around the
  running code set, or one with no deadline and no values. A `Context` is an immutable snapshot, and
  sendable: the scopes the code opens after reading it change the thread's context, not the snapshot.
  It answers **`deadline -> Option[Instant]`**, the instant the work must be done by, and
  **`remaining() -> Option[Duration]`**, the time left on the monotonic clock, never less than zero;
  both `None` where no scope set a deadline. `main` and every thread begin with an empty context.
- **`Key[T](name)`** names a value of type `T`. **Keys are told apart by identity**, not by name, so
  two keys of one name are two keys: a module makes each of its keys once, as a module-level value, and
  what it sets is reached by that key alone. `key.name` is for showing.
- **`withValue[K: Sendable, T](key: Key[K], value: K, body: () -> T) -> T`** runs `body` on the
  calling thread with `value` under `key`, and puts the context back however the body ends, by
  returning or by a cancellation. Inside, **`value[T](key: Key[T]) -> Option[T]`** answers `Some(value)`,
  in the body, everything it calls and the threads it starts, until a scope inside sets the key again,
  which shadows it for its own body. A value of another type is not set under a key, and a lookup
  answers the key's own type, so nothing is cast; the value is sendable, since other threads read it.
- **`withDeadline[T](deadline: Instant, body: (Scope) -> T) -> Result[T, DeadlineExceeded]`** is
  [`withTimeout`](#timers-and-deadlines) at an instant: `Ok` of what the body returns before
  `deadline`, and `Err(DeadlineExceeded(deadline))` where the deadline passes first. Then the body is
  cancelled where it is, with every thread its scope started and everything below them, and
  `withDeadline` answers once they have all ended. A deadline already past runs nothing. The wall clock
  is read once, as the scope opens; from then on the deadline runs on the monotonic clock, so setting
  the wall clock moves nothing. **`DeadlineExceeded(deadline: Instant)`** is an
  [`Error`](/spec/errors/#the-error-trait) whose message is `deadline exceeded`.
- **`withTimeout` sets the deadline too**, `timeout` from the call, so code below either reads the
  time it has left the same way.
- **A nested deadline tightens and never loosens.** The context keeps the sooner of a scope's deadline
  and the one around it, so the code inside a later deadline still reads the earlier one, and the
  outer scope's cancellation ends the inner one with its body: no scope can buy its body more time than
  the scope around it has.
- **Blocking calls observe the deadline through cancellation.** Since a deadline that passes cancels
  its body, every [cancellation point](#cancellation) below it ends at once: a socket's `accept`,
  `connect`, read or write, a [pool](#pools)'s `use` or `lease` waiting for a resource, a channel's
  `receive` or `send`, a `select`, a `sleep`, a join, a future's `await`, and the safepoint poll of
  code that computes without waiting. Nothing inside sees an error value: the body stops, its deferred
  statements run, and the error is `withDeadline`'s answer. Code that must tidy up when it is stopped,
  a database driver telling its server to drop a query, does it in a `defer`, whose waits run to their
  end.
- **What a new thread takes follows what can cancel it.** A thread a scope starts, through `s.spawn`
  or as the body of a `withTimeout` or `withDeadline`, takes the starter's whole context, its deadline
  with its values, since a cancellation of the scope reaches it. A plain `spawn`, the code of a future
  (`Future(…)`, `executor.future`, `map`, `flatMap`, `onComplete`) and an executor's `execute` task take
  the values with no deadline, since nothing cancels them when it passes; a future's callback takes the
  context of the code that attached it, not of the thread that completed the future. **An actor's turn
  takes nothing**, neither from the code that told the actor nor from the worker it runs on: an actor
  outlives any one request, and its turns are not a scope's threads. A value that should cross a
  `tell` goes in the message.
- **Siblings share nothing.** A value a thread sets is its own and its children's: the thread that
  started it, and the threads beside it, do not see it.

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
    words.length
  val both = for p in page, n in counted yield s"${p.charCount} characters, $n words"
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
  val workers = (0 ..< 64).map(i -> Future(blocking(jobs.receive().getOrElse(0)))).toVector
  Future
    for n in 1 .. 64 do jobs.send(n)
  println(Future.all(workers).await().sum)
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
  val sizes = (1 .. 100).map(n -> io.future(n * n)).toVector
  val total = Future.all(sizes).await().sum
  io.shutdown()
  println(total)
  0
```

## The memory model

Starting a thread happens before its body; its end happens before `join` returns; releasing a
`Mutex` happens before the next thread takes it; a `send` happens before the `receive` that gets the
value; an `Atomic` store happens before every load that reads it; and completing a future happens
before every `await` that gives its value and before every combinator's function runs with it.
In the rest of the kit: a wait group's `add` and `done` happen before every `wait` they let return;
a semaphore's `release` happens before the acquire that takes the permit it gave back; the end of a
`RwLock`'s write happens before every read and write after it, and the end of a read before the
write after it; and every party's arrival at a barrier happens before that round's action, which
happens before every party's `wait` of the round returns. A program whose shared
mutable state is reached only under those edges sees sequentially consistent behaviour.

**A data race on a plain field or `var` is not undefined behaviour.** A racing read returns some
value that was written to that location, never a mixture of two writes and never a value nothing
wrote. The compiler gives each access to a heap field, an array element or a module-level variable
LLVM's `unordered` atomic ordering: it costs nothing on the machines Avin targets, and it is exactly
the promise that a word is never torn and a racing load is never undefined. A reference is one word,
and so is every built-in scalar.

**An integer wider than 64 bits and a union with a scalar member are the exceptions.** An `i128` or
an `i257` is several words, and so is a union such as `int | string` (its tag, its bits and its
reference); a field or variable of one is read and written as untorn 64-bit words, so a racing read
of it may combine words of different writes. Each word is still one some write stored, a union read
so is still a value of one of its members (never a reference no write stored), and a program that
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

## Diagnostics

The runtime says what a program's threads are doing in two cases: when every one of them waits for
something nothing can end, and when the program is asked for a thread dump.

### Deadlock

A program whose threads all wait for each other stops, as a trap does, with a list of its threads
and what each waits on:

```
avin: trap: deadlock: all threads are waiting
  main: waiting for a thread to end
  thread 1: waiting for a lock
  thread 2: waiting for a lock
```

The program is deadlocked when `main` waits without a limit (for a lock, a condition, a join, a
channel, a future, a [`select`](#select) with no `after`, or anything built on them) and at that
moment

- no lightweight thread is running or ready, so every one waits too, and
- nothing outside the threads can end a wait: no timer is set (a sleep, a wait with a limit, an
  `ask`'s limit, an actor's timer, a signal watch's `wait`), no thread waits on a socket, no thread
  the program did not start is attached, and no callback that such a thread may call
  ([`@threaded`](/spec/sysl-bindings/#callbacks)) is held.

A thread in a blocking call (a [`blocking`](/spec/sysl-bindings/#calls-that-wait) binding, reading
input, waiting for a command to end) is running, so a program with one is never deadlocked, and
neither is a `main` that runs its own work, a window's frame loop among it. An `ask` that nobody
answers is not a deadlock either: its limit ends it with `TimedOut`. An actor waiting for mail
nobody can send has no thread at all, so `main` awaiting its end is deadlocked once everything else
waits.

The report lists `main` first, then each lightweight thread still there in the order the threads
started, numbered from 1 by that order; a thread that has ended is not listed. A pool's idle workers,
which wait for work by design and are as many as the machine has processors, are left out. What a
thread waits on is one of: a lock (a mutex's), a condition, a thread to end (a join), receiving from
a channel, sending to a full channel, a future, or a select, `waiting in a select on 2 channels`,
which counts its arms that name a channel. A select with an `after` is a wait with a limit, whose
timer can always end it, so it is never part of a deadlock. The report goes to standard error after what the
program had printed is flushed, the exit hooks run after it, and the program ends as a trap ends it.

**Only a deadlock of the whole program is found.** Threads that wait for each other while another
thread still runs, or while `main` computes or sleeps, are not reported: the program goes on, and
ends when `main` returns.

### Thread dump

The quit signal (Ctrl-\ at a terminal, `kill -QUIT`) writes every thread's state to standard error,
and the program goes on as it was:

```
avin: thread dump
  main: running
  thread 1: waiting to receive from a channel, 250 ms
  thread 2: sleeping, 1200 ms
  thread 3: waiting on a condition with a time limit, 3 ms
  thread 4: running
  thread 5: idle, waiting for work, 40 ms
```

Each thread is `running` (`running, in a blocking call` while it is in one), `ready` for a worker,
`ending`, or waiting, with what it waits on as in the deadlock report, `sleeping`, `waiting on a
socket`, or, for a pool's worker with no work, `idle, waiting for work`; a wait with a limit says so,
and every wait says how long it has lasted, in milliseconds. Idle workers are listed here. A dump
names no functions: a thread's frames do not record which function they belong to.

A program that [watches](/spec/system-library/#avinsignal) the quit signal hears of it instead, and no
dump is written while the watch lasts. A program started with the signal ignored keeps it ignored.
