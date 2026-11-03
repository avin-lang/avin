---
title: Threads
summary: A proposal for starting and joining threads, what they share, how they synchronize, and what a racing read sees.
weight: 16
---

**This chapter is a proposal, awaiting the user's decision** (see [decisions](/spec/decisions/)).
The runtime machinery it rests on exists: contexts attached per thread, an allocation lock,
safepoints and stop-the-world collection ([the runtime](/compiler/#runtime)). No thread API is
exposed to programs yet.

## Starting and joining

Threads are library code in `avin.threads`, over two runtime entries that make and retire a thread's
context. A thread runs a closure and is joined for its result:

```avin
import avin.threads.spawn

main() -> int
    val worker = spawn(() -> sum(1..1000000))
    val mine = sum(1000001..2000000)
    worker.join() + mine
```

- `spawn[T](body: () -> T, stack: long = 8388608) -> Thread[T]` starts `body` on a new operating
  system thread with a stack of its own (8 MiB unless asked otherwise). `join() -> T` waits for it
  and gives back the result; joining a second time gives the same result again.
- **A trap in any thread stops the program**, as it does in a program with one thread: traps report
  bugs, and a bug in one thread leaves the state the others share in doubt.
- **The program ends when `main` returns**, whatever other threads are still running. A library
  scope makes the joins structural: `scope(s -> …)` returns only once every thread started through
  `s.spawn` has finished.

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
wrote. Every field is at most a word, since a reference is one word and so is every built-in
scalar, so the compiler gives each access to a heap field or a module-level variable LLVM's
`unordered` atomic ordering: it costs nothing on the machines Avin targets, and it is exactly the
promise that a word is never torn and a racing load is never undefined.

**An object is complete before any thread can see it.** A constructor ends with a release fence, so
a thread that reads a reference to an object, however it got it, sees the object's header and every
field its constructor set. Without it, a racing reader could see a reference field still zero,
which no Avin type allows. The fence is free on x86-64 and one store barrier on ARM, paid once per
object made.

## Collection

A collection stops the world. The collecting thread raises a flag, waits until every other thread is
parked at a safepoint or is in a safe region (waiting, or blocked in a call), collects alone, and
lets them go. Compiled code polls the flag at the entry of every function that makes calls and on
every back edge of its loops, one load and a branch that is not taken; allocation is itself a
safepoint. A program with one thread never stops anything.

## Actors

Actors (stage 12) are scheduled by the runtime onto a pool of these threads. An actor handles one
message at a time, so its own state needs no lock; its mailbox is a runtime queue on the shared
heap, and a message is a reference to a heap value. What an actor may send — any value, or only
values no other actor can mutate — is part of the open question on actors.
