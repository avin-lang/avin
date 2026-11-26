# `avin.reactive`: a design for reactive values in Avin's standard library

Research. This note and the module it describes (`library/avin/reactive/reactive.avin`, tests in
`library/tests/reactive*.avin`, `tests/run/reactive_trap_*.avin`, `tests/reject/reactive_not_sendable.avin`)
live on the branch `frp-research`, which goes into no train until the verdict below is approved. The
note is kept out of `docs/` so it does not appear on the site.

## Verdict

**Solid, with small gaps fixed first.** The synchronous core meets the bar as built and tested.

- **Glitches:** none, by construction (push to find effects, pull to compute in read order), and the
  randomized test compares every observation with a computation from scratch.
- **Ownership:** the compiler enforces the thread model. A graph is not sendable, so no other thread
  can reach it. Ports are the one way in, sendable and latest-wins. There are no data races and no
  module-level state.
- **Actors:** they own graphs naturally. Stopping and crashing mid-flush are defined, and a restart
  gives a fresh graph.
- **State machines:** they are driven through the graph with no lost or duplicated step.
- **Lifetimes:** they need no weak references, because only effects pin nodes and every effect has an
  owner. The collector confirms it, and a planted leak shows the measure fails.

What should be settled before this ships (none of it blocks the design):

- a supported way to read the live heap for lifetime tests (here, a runtime function added on this
  branch);
- a way for `stopping` to know the actor failed, so the trapped effect's cleanups can run;
- the actor field-initialization constraint on effects that capture `self`, which may only need
  documenting.

Async is deliberately left out. Its cancellation is a decision for the user (section 6).

## The model

A **`Graph`** holds three kinds of node, and one execution owns it.

- **`Atom[T]`**, a value the program writes: `g.atom(0)`, `a.set(v)`, `a.update(f)`, `a.get()`.
- **`Derived[T]`**, a value computed from other nodes: `g.derived(get -> get(a) + get(b))`, or
  `a.map(f)`. The computation is given a **`Get`**, and `get(node)` both reads a node and records it as
  a source, so the sources are exactly what the last computation read.
- **`Effect`**, code that runs once when made and again whenever a node it read changes:
  `g.effect(get -> println(get(area)))`. Its `Get` also registers cleanups (`get.onCleanup(f)`) and makes
  effects and scopes that the run owns (`get.effect`, `get.scope`).

Around them:

- **`Scope`**: an owner of effects, ports and other scopes, disposed together (`g.scope()`,
  `s.scope()`, `s.dispose()`, `s.onDispose(f)`).
- **`Port[T]`**: a sendable way for another thread to write to an atom.
- **`Machine[E, S]`**: a state machine driven by events through the graph.
- `g.batch(body)`, `g.drain()`, `g.awaitWrites(timeout)`, `g.dispose()`.

The tracking is **explicit**. No ambient "current computation" lives in a module-level variable. This
is forced by Avin and is also the better design: an actor may reach no module-level mutable state
([actors](/spec/actors/#declaring-an-actor)), so an ambient tracker would make the module unusable
from actors. A `Get` parameter is the tracker, it cannot be confused between two graphs or two
threads, and it costs one parameter in every lambda (`get -> …`).

Nodes are **objects, not keys**. An atom's value lives in the atom, so a node is collected like any
other object once nothing refers to it (see Leaks). The alternative, sendable atom definitions used as
keys into a per-owner store, lost for the reasons given under *Alternatives*.

## 1. Glitch-freedom

**The algorithm: push to find the effects, pull to compute.**

1. A write that changes an atom advances the graph's clock and the atom's version. It walks the
   atom's *observers* (only nodes an effect observes, transitively), marking each derived node once
   per write and queuing each effect it reaches once.
2. When the write is complete (at once, at the end of the outermost `batch`, or after the running
   effect returns), the graph **flushes**: it takes the queued effects in order. For each one it checks
   whether a source has changed since the effect last ran, and runs it only if so.
3. Checking or reading a derived value **pulls**. If the clock has not moved since it was last checked,
   the cached value stands. Otherwise its sources are brought up to date *in the order the last
   computation read them*, and it recomputes as soon as one of them has a new version, never reading
   the rest. A recomputation whose result is `==` to the cached value keeps the old version, so its
   readers are left alone (the "equality cutoff").

**Why no observer sees a mix.** A derived value is only ever computed inside a pull, and a pull
happens after its sources have been brought up to date. A derived computation may not write
(`writable()` traps), so no write can happen between reading two sources of one computation. Effects
run only after a write, or a batch of writes, is complete. Every value an effect reads is therefore a
function of one state of the atoms: the state at the moment of the read.

- **Diamonds.** `d = (b, c)`, where `b` and `c` both read `a`. When `d` is pulled, it brings `b` and
  `c` up to date first, so it sees both new or (before the write) both old. Each is computed once per
  change. Tested in `rxDiamond`, which counts every computation.
- **Deep chains.** A 1000-link chain is brought up to date by one pull, each link computed once
  (`rxDeepChain`). The pull is recursive, so the depth is bounded by the stack. A thread's stack is
  reserved at full size and committed lazily, and 1000 levels fits easily. An iterative pull is a
  possible later refinement.
- **Dynamic dependencies.** Sources are rechecked in read order, and the walk stops at the first one
  that changed. So a computation that switched branches never brings a branch it will no longer read
  up to date. `rxDynamicGuard` proves this: a branch that would divide by zero is never computed. When
  an observed derived value recomputes, it subscribes to the new sources *before* unsubscribing from
  the dropped ones, so a source in both is never briefly unobserved (`rxDynamic` checks the observer
  counts).
- **Batching.** `g.batch(body)` holds effects back until the outermost batch returns, then runs each
  effect at most once. Batches nest. `drain` applies all ports in one batch. A machine applies each
  event's state and step writes in one batch.
- **A write from inside an effect is *queued*, not refused.** The write takes effect at once (an
  effect that writes and then reads sees its own write), and the effects it reaches run *after* the
  writing effect returns, in the same flush. An effect whose run moved the clock is checked again at
  the end of the flush, and runs again only if something it read changed. An effect that writes what
  it reads never settles. After `RerunLimit` (100) runs in one flush it traps with "an effect ran 100
  times in one flush: it writes what it reads, directly or through other effects"
  (`tests/run/reactive_trap_effect_loop.avin`).
- **A write from inside a derived computation is refused**, with a trap
  (`reactive_trap_write_in_derived`). So is a derived value that reads itself (`reactive_trap_self_read`),
  and a `Get` kept and used after its computation returned (`reactive_trap_get_escapes`).

**Property test.** `rxRandomized` builds 12 random graphs, each with 5 atoms and 30 derived nodes.
Each derived node is either a sum of up to three earlier nodes or a parity-chosen pick between two
(so dependencies switch as values change). Eight effects (re-made every 20 rounds) watch random
nodes. Each graph runs 300 random batches of 1 to 3 writes. Every value every effect sees is compared
against the whole graph computed from scratch at that moment. The test also asserts that:

- no effect runs twice in one batch;
- no effect runs with an unchanged value;
- after each batch, every effect has seen its node's current value (no lost update);
- a randomly chosen node pulled outside any effect equals the scratch value;
- once every effect is disposed, every node has zero observers.

**Alternatives weighed.**

- *Push-only with topological heights*, recomputing in height order. Glitch-free, but it computes
  values nobody reads, needs a height maintained under dynamic dependencies, and recomputes
  unobserved nodes eagerly. It lost to pull, which computes only what is read.
- *Pull-only*, re-checking every effect on every flush. Correct and simpler, but O(effects) per
  write. The push phase is cheap (it marks only observed nodes) and finds exactly the effects that
  must be checked.
- *Effects run synchronously inside `set`*. This gives glitches the moment an effect writes, and
  makes re-entrancy undefined. Lost.
- *Writes in effects refused*. It is safe, but a real program needs writes there (copying a reloaded
  config into derived state, forwarding a port). Queued writes plus the rerun limit keep it safe.

Cost noted for later: after any write, a pull of an unrelated derived value re-walks its sources once
(the cache is keyed on the global clock). A per-node "nothing pushed through me" flag for observed
nodes would skip that walk. It is not needed for correctness, and the prototype keeps the simpler rule.

## 2. Threads

**The ownership model: one graph, one owner, enforced by the compiler.**

- A `Graph`, its atoms, its derived values and its effects are classes with `var` fields, so none of
  them is `Sendable` ([what is shared](/spec/threads/#what-is-shared)). A closure given to `spawn`, a
  `Future`, a `@sendable` function, a `@threaded` callback, or an actor's message or parameters cannot
  carry one. `tests/reject/reactive_not_sendable.avin` pins those diagnostics. The owner is the
  execution that made the graph:
  - the main thread (an event loop or a tui);
  - one lightweight thread;
  - one actor, which keeps the graph in its state.
- **Read or write from another lightweight thread or OS thread: impossible to write.** No such code
  compiles, because it would need a reference the compiler refuses to share. A lightweight thread may
  move between workers, and an actor between turns, but it is one execution at a time. The runtime's
  park and resume, and the actor's "handling of each message happens before the next", give the
  happens-before edges, so the graph's plain fields are race-free without locks.
- **A cross-thread write goes through a `Port`.** `g.port(atom)` or `scope.port(atom)` gives a
  `Port[T]` for a sendable `T`. Its fields are a `Mutex[Buffer[T]]` slot of at most one value, an
  `Atomic[bool]` "closed" flag, and the graph's wake `Channel[unit](1)`. It is sendable by its structure, with no
  `@trusted_sendable`. `port.set(v)` stores the latest value and offers a wake. The owner applies every
  port's latest value in one batch with `g.drain()`, or `g.awaitWrites(timeout)` to wait first. The
  wake channel holds at most one token, and it is offered *after* the slot is written and taken
  *before* draining, so no write is ever left unseen. A port is **latest-wins**: a writer never waits
  and never grows the owner's memory, and the owner sees each atom's newest value. For state that is
  the right meaning. A stream of events is a `Channel` or an actor's mailbox, not an atom.
  `rxPortsUnderStress` runs 8 threads writing 3000 values each while the owner drains, checks every
  observation's consistency, and checks that every writer's last value lands.
- **`@sync`, `&sync` and `blocking`.**
  - Sysl's `&sync Fn` parameter is bound with `@threaded` in Avin
    ([callbacks](/spec/sysl-bindings/#callbacks)). Such a function may capture only sendable values,
    so it can hold a `Port` and never a graph. A C or Sysl thread that calls back writes through the
    port.
  - A `blocking` binding called from an effect blocks the owner inside the flush. That is legal, and
    nothing else touches the graph meanwhile because nothing else can reach it. Ports keep accepting
    writes, to be drained later.
  - Avin has no `@sync` mark of its own on functions. The analogues are `@sendable` and `@isolating`,
    and a graph meets neither.
- **Data races: none.** The graph's state is reached by one execution, and a port's state only
  through a `Mutex`, an `Atomic` and a `Channel`. The module declares no module-level `var` (its only
  module-level value is the `int` `RerunLimit`), so actors may use it.
- **A graph inside a `Mutex`.** `Mutex(Graph())` is accepted (a mutex's value is `@isolating`). Each
  thread then uses the graph only while holding the lock, so it is still one execution at a time, and
  effects run on whichever thread wrote, inside the lock. That is legal but discouraged: an effect that
  waits for another thread holding that lock deadlocks, as with any mutex. Ports are the
  recommended path.

Alternative weighed: a run-time owner check on every operation, built on the owner word handles use
(`owner_current`). It lost. The compile-time rule already excludes every shared access. A stored owner
word would also mis-fire after a legitimate hand-over of ownership
([handing over ownership](/spec/actors/#handing-over-ownership)), unless the graph took part in the
runtime's hand-over protocol the way handles do. Whether to add it, for defence in depth, is listed
under the user's decisions.

## 3. Actors

- **An actor owns a graph** by making it in its body: it is the actor's state. **Messages turn into
  writes** in an arm (`RxReading(v) -> value.set(v)`), and each message is its own flush.
- **A subscription sends to another actor** with an effect: `g.effect(get -> sink.tell(RxValue(get(d))))`.
  `tell` never waits. An effect runs only on a change (the equality cutoff), so a burst of writes in
  one batch sends one message.
- **Backpressure.**
  - *Outbound*: the receiver chooses with its mailbox. `Bounded(n, DropOldest)` gives latest-wins,
    which is exactly a signal's meaning. An effect may call `send` (waiting), which parks the actor's
    turn in the middle of the flush and holds the actor, a choice the program makes knowingly. `offer`
    answers `Err(Full)` and never waits.
  - *Inbound*: the actor's own mailbox bounds the writes. Each message is one write and one flush. A
    way for an actor to take several waiting messages in one batch would coalesce bursts, but that is
    a library addition to actors, not something this module needs.
  - *From a thread*: ports, which are bounded by construction (one slot per atom).
- **Stopping mid-propagation.** `stop()` called from an effect ends the actor *once the current
  message is handled*, so the flush under way completes: every effect already queued runs. The
  actor's `stopping` block then calls `g.dispose()`, which runs every cleanup. After that the actor
  tells nobody anything (`rxActorStops`: readings `1, 2, 5, 7, 3`, stop at 10, and the collector gets
  exactly `[0, 2, 4, 10]`). Without a `dispose` in `stopping`, the graph simply becomes unreachable and
  is collected (`rxNoLeakAbandoned`), but its cleanups do not run.
- **A crash mid-propagation.**
  - A trap in an effect unwinds the actor's frames, and the rest of that flush never runs. The
    effects after the one that trapped do not send, which is correct: the state is in doubt.
    `stopping` runs `g.dispose()` on the half-done graph. `dispose` is written to be safe on any state:
    it disposes the tree, clears the queue, and never asserts. The one loss is that the trapped
    effect's own last-run cleanups do not run, because the graph cannot tell "its body is still on the
    stack" from "its body was unwound". A runtime way to know, in `stopping`, that a failure
    happened would fix that.
  - Under `Restart`, the body runs again, so a **fresh graph** is made from the same behaviour, and the
    messages still in the mailbox write to it (`rxActorRestarts`: the collector sees
    `[0, 2, 0, 8]`, with the failed flush's `26` never sent).
- **Supervision.** The graph is the actor's state and follows the actor's rules: no resuming with
  the old state, and a restart is not an ending. Subscribers in other actors see the restarted actor
  start again from its initial values. A subscriber that must know about restarts asks for a message
  saying so. Watching only reports endings.
- **Glitch-freedom is per graph.** Between actors there is no common clock. An actor combining values
  from two others may see one's newer value with the other's older one, as any message-passing system
  may. The documentation must say so plainly.

## 4. State machines

What Avin means by one ([state machines](/spec/actors/#state-machines)):

- A `machine` declaration has `state`s (with data), events (a `data` type), and arms whose value is
  the next state, or `stay`.
- `enter` and `leave` run on every transition, including one into the current state, and `stay` runs
  neither.
- `handle(event) -> State` and `state()` drive and read it.
- Handling an event from inside its own arms, `enter` or `leave` traps.
- An actor's body may take the same `state` blocks, its mailbox being the events.

**Exposed as atoms.** `g.machine(door.handle, door.state())` gives a `Machine[E, S]` with:

- `state()`, a `Readable[S]`;
- `lastStep()`, a `Readable[Option[Step[E, S]]]` whose `number` advances on every event, so a `stay`
  or a move into the current state is still seen;
- `steps()`, a count;
- `onStep(f)`, a listener;
- `fire(event)`.

**No lost or duplicated transition:**

- every event fired is appended to a FIFO;
- one fired while another is being handled (from an `enter`, a listener or an effect) waits its turn
  instead of trapping;
- each event is handled exactly once, with its state and step written in one batch, so effects see
  the two agree.

Effects see the state after a batch (conflated, as every signal is). `onStep` listeners see **every**
step, synchronously and in order, because a transition is an event and an event is not a value. Tested
by `rxMachine`, an eight-event sequence with stays, a self-transition, a failed guard, numbering,
from/to, and the effect log. `rxMachineReentrant` covers firing from a listener and from an effect.

**Driven by atoms.** An effect that reads atoms and fires events (`g.effect(get -> if get(alarm) then
m.fire(Lock(code)))`) fires on each *change*, never on an unchanged value. In an actor with states,
an effect tells `self` a message, and the mailbox's order makes each one handled once, in the
actor's own turn.

A small open point: from outside a machine, `handle` cannot tell a `stay` from a transition into the
current state. Both answer the same state, so `Step` records `from` and `to` but not which one it was.
Only the machine's own `enter` and `leave` know. If that matters, `handle` could answer it.

## 5. Leaks and lifetime

- **Who keeps what.**
  - A derived value refers to its sources, because it must pull them.
  - A source refers to a derived value or an effect (its `targets`) **only while an effect observes
    that node**, directly or through other derived values ("mounted"). When the last observer leaves,
    the node unsubscribes from its own sources, recursively.
  - So a derived value nobody observes is referred to by nothing in the graph, and the collector
    takes it once the program drops it. No weak references are needed, which matters because Avin has
    none (see Gaps).
- **Every effect has an owner**:
  - the graph's root;
  - a `Scope`;
  - the run of an effect that made it, which disposes it before running again.
  An effect the program forgets lives as long as its owner, by design. Disposing an owner disposes
  what it owns, newest first, and runs its cleanups newest first. Disposing removes the effect's
  edges both ways, which can unmount a chain of derived values.
- **Ports** belong to a scope and are closed when it is disposed. A `Port` held by another thread
  refers only to its mutex, flag and channel, **never to the graph**, so a writer thread never keeps a
  graph alive.
- **A graph nobody refers to is collected whole**, with live effects and all. The module keeps no
  registry and no module-level state.
- **Disposal during propagation is defined:**
  - an effect disposed while it waits in the queue is skipped (`rxDisposeWhilePropagating`);
  - one disposed inside its own body finishes that run, then cleans up once and never runs again
    (`rxSelfDispose`);
  - `g.dispose()` from inside an effect runs none of the effects still waiting (`rxGraphDispose`).
- **Against the collector** (`library/tests/reactive_heap.avin`). These tests measure the heap's live
  bytes after a full collection, through a runtime function added on this branch
  (`avin.rt.live_bytes_after_collection`, see Gaps). Each runs a warm-up first.
  - 20,000 scopes are made and disposed. Each holds two effects (one making an inner effect per run)
    over a three-node derived chain, plus a write. The live heap must return to within 64 KiB.
  - 20,000 derived values are read and dropped, never observed.
  - 5,000 graphs with live effects are let go of without `dispose`.
  - **The measure can fail**: the same churn keeping its scopes must grow the heap by more than 1.28
    MB.
  - **Planted leak, checked on purpose.** I ran a copy of the library whose `Effect.dispose` left the
    effect observing its sources (one line removed), with `reactive_heap.avin` at -O2 against it. The
    disposal test failed with `assertion failed: the live heap grew by 143625216 bytes`, and the control
    test's observer check failed (`expected 0, found 20200`). With the real library, all four pass at
    -O0 and -O2, within 64 KiB.

## 6. Async: left for later, deliberately

Nothing in sections 1 to 5 depends on async, and async needs decisions that belong with the user, not
in a prototype. The shape that fits this design:

- `g.async(start: (Get) -> Future[T])` is a derived `Loadable[T]` (`Loading`, `Ready(v)`, or
  `Failed(e)` when the future yields a `Result`). Each recomputation is numbered.
- A completion is delivered to the owner through a port-like slot tagged with its number, and drained
  in a batch. **A result whose number is not the latest is dropped**, so a late answer for an old input
  never wins.
- Errors are values. Avin's futures carry no failure of their own, and a computation that can fail
  yields a `Result`.
- Cancellation is the open part. `Future` has no cancel. A thread in a `scope` does have one (a
  cancellation point at every wait). So either an async atom runs its work as a scope's thread and
  cancels it when superseded or disposed, or the futures library grows cancellation. That is the
  user's choice.

## 7. API and ergonomics

The API holds to "as flexible as possible for any user" and conveniences that add no complexity:

- one `Get` type for every read;
- `map` on any readable;
- `update(f)`;
- `batch` returning the body's value;
- scopes nesting freely;
- effects owning effects;
- any `T` (equality is `==`, which every Avin value has);
- `Port[T]` for any sendable `T`.

A tui-style view, one effect per visible row, owned by the list's effect so rows vanish with it:

```text
val g = Graph()
val items = g.atom(Vector[string]())
val filter = g.atom("")
val shown = g.derived(get -> get(items).filter(s -> s.contains(get(filter))))
val selected = g.atom(0)
val view = g.effect(get ->
  val rows = get(shown)
  screen.clear()
  for i in 0 ..< rows.length do get.effect(get2 ->
    screen.row(i, rows(i), highlighted = get2(selected) == i)))
loop
  val key = terminal.readKey()
  g.batch(() -> handleKey(key, items, filter, selected))
```

A server: a config reloaded from a watcher thread, and a health aggregate logged only on change
(`rxServerShape` runs this shape):

```text
val g = Graph()
val config = g.atom(loadConfig())
val reload = g.port(config)
spawn(() -> for change in watch(configPath) do _ = reload.set(loadConfig()))
val up = services.map(s -> g.atom(true))
val health = g.derived(get ->
  val down = services.indices.filter(i -> !get(up(i)))
  if down.isEmpty then Up else Degraded(down.map(i -> services(i).name)))
_ = g.effect(get -> log.info(s"health ${get(health)}"))
val listen = config.map(c -> c.port)
_ = g.effect(get -> rebind(get(listen)))
loop
  _ = g.awaitWrites(time.seconds(1))
```

## How it was tested

Every run went through marshal, with the installed `avin` 0.0.38 and `AVIN_ROOT` set to the
worktree.

- **Job 10885** checked the current code (`avin fmt` only rewrote whitespace after it):
  - `library/tests/reactive.avin`: 21 passed at -O0 and -O2, and 21 passed under
    `AVIN_GC_STRESS=1` at both levels;
  - `library/tests/reactive_actors.avin`: 3 passed at -O0 and -O2, and under GC stress at both;
  - the four `tests/run/reactive_trap_*` programs at -O0 and -O2, standard output and error
    compared byte for byte with their `.out` and `.err`;
  - `tests/reject/reactive_not_sendable.avin`, which gives the three diagnostics in its header;
  - `avin doc --std --check`, clean.
- **Job 10902:**
  - `avin fmt --check`, clean;
  - `reactive_heap.avin`: 4 passed at -O0 and -O2;
  - the planted leak, which failed as described in section 5.
- **Job 10782** was the first round, whose failures (a port bug, now fixed and covered by `rxPorts`
  and `rxServerShape`, and test-side mistakes) led to the fixes above.

The full gate was not run. The branch goes into no train.

## Gaps found

1. **No way for Avin code to read the live heap.** Only `avin.rt.allocated_bytes` (allocation, not
   liveness) is bindable, and nothing collects on request. A leak test needs both. Closed on this branch
   by `live_bytes_after_collection() -> long` in `runtime/avin/rt/heap.sysl`, which collects with
   every other thread stopped and answers `gc.live_size`. The test binds it with
   `@sysl("avin.rt.live_bytes_after_collection")`. Whether it becomes a public library facility is
   decision 8.
2. **An actor's effect that captures `self` cannot be held in a field declared before another
   field.** Shortest input, in an actor body:
   `val stopper = g.effect(get -> if get(doubled) >= stopAt then stop())` followed by
   `val teller = g.effect(…)`. The diagnostic:
   `error: a lambda that captures `self` is made before `teller` is initialized; a class body uses `self` for more than reading and assigning its fields only once every field is`.
   The rule is the language's definite-initialization rule working as designed. The consequence is
   that effects in an actor are made as statements after the fields (`_ = g.effect(…)`), or kept in a
   `var` assigned after them. That reads naturally and is what the tests do. It is recorded because a
   reader will meet it in the first actor they write.
3. **`stopping` cannot tell whether the actor failed**, so `Graph.dispose` cannot tell an effect whose
   body was unwound by a trap from one still on the stack, and that effect's last-run cleanups do not
   run (section 3). A `failed()` on `ActorContext`, or an `Ending` passed to `stopping`, would close
   it.
4. **`Mutex` has no take or swap.** `update` answers the new value, so a port's slot is a one-element
   `Buffer` inside the mutex and is taken with `with`. This is a normal use of `with`, not a
   workaround. A `Mutex.swap` would read better.
5. **From outside a machine, a `stay` and a move into the current state look the same** (section 4).
6. **No weak references.** This is not a gap for this design, which needs none (section 5). It *is*
   one for the rejected key-and-store design (decision 2).

## Decisions that are the user's

1. **The module's name.** `avin.signal` is taken (Unix signals), so the proposal is `avin.reactive`.
   The alternatives are `avin.atoms`, `avin.cells` and `avin.state`.
2. **Nodes as objects (built) or atoms as sendable keys into per-owner stores.** Keys would let one
   module-level atom definition serve every actor's store. They need a weak map so that a store does
   not keep every key it ever saw, and atoms' initial values would have to be sendable. Objects need
   neither, and the compiler already keeps them with their owner.
3. **Port semantics.** Latest-wins (built), or a queued port that delivers every write with
   `send`-style backpressure. The second is a stream, and arguably belongs in a `Channel`.
4. **Writes inside effects.** Queued (built) or refused.
5. **`RerunLimit`** (100 runs of one effect in one flush) and whether it should be a parameter of
   `Graph`.
6. **Async** (section 6), and in particular cancellation: a scope's thread, or cancellation added to
   futures.
7. **A run-time owner check** on top of the compile-time one (section 2).
8. **A public way to read the live heap.** On this branch it is a test-only binding of
   `avin.rt.live_bytes_after_collection`. A public `avin.runtime` facility would serve every library
   test of lifetimes.
9. **Custom equality** per atom or derived value, beyond `==`.
10. Whether `stopping` should be able to learn that the actor failed, so a graph's `dispose` can run
    the cleanups of an effect that trapped.
