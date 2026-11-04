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
  stack until the program ends.
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

## Synchronization

The library offers, all written in Avin over Sysl's atomics and futex-based locks:

- `Mutex[T]`, which owns the value it guards: `m.with(v -> …)` is the only way to reach it.
- `Condition`, waited on with a held `Mutex`.
- `Atomic[T]` for `int`, `long`, `bool` and references, with `load`, `store`, `swap`,
  `compareAndSet` and, for the integers, `add` and `sub`. Sequentially consistent; there are no
  weaker orderings in the library.
- `Channel[T]`, bounded or unbounded, with `send`, `receive` and `close`.
- `Once` and `lazy` module values, initialized by exactly one thread.

Every operation that can wait marks its thread safe for the collector while it waits, so a thread
blocked on a lock, a channel, a join or input never holds up a collection. A Sysl binding declared
as blocking does the same.

## The memory model

Starting a thread happens before its body; its end happens before `join` returns; releasing a
`Mutex` happens before the next thread takes it; a `send` happens before the `receive` that gets the
value; and an `Atomic` store happens before every load that reads it. A program whose shared
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

Actors (stage 12) are scheduled by the runtime onto a pool of these threads. An actor handles one
message at a time, so its own state needs no lock; its mailbox is a runtime queue on the shared
heap, and a message is a reference to a heap value. What an actor may send — any value, or only
values no other actor can mutate — is part of the open question on actors.
