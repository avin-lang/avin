---
title: Actors and state machines
summary: "`actor` and `machine` declarations: typed messages, what may be sent and how the compiler checks it, `tell` and `ask`, supervision of a trap, lifecycle and watching, scheduling on a pool, and states with transitions."
weight: 27
---

Avin's way to thread safety is **no shared data where it can be avoided**. An actor owns its state
outright, and the only way to reach that state is to send the actor a message. Messages carry
immutable values, or hand over the ownership of something mutable, and never a reference that two
actors could both change. The compiler checks every message type, so a program that compiles never
shares mutable data through a message.

The pieces fit together without special cases: an actor is declared like a class, its messages are a
`data` type, receiving one is a `match`, a reply arrives as a [`Future`](/spec/threads/#futures), and
an actor's behaviour, where it has distinct phases, is written as a state machine in the same body.
Everything runs on the threads, executors and memory model of [`avin.threads`](/spec/threads/); the
library is `avin.actors`.

## Declaring an actor

```text
import avin.actors.{ActorRef, spawn}

data CounterMsg
    Increment(by: int)
    Get(replyTo: ActorRef[int])
    Reset

actor Counter(start: int) receives CounterMsg
    var count = start

    receive
        Increment(by) -> count += by
        Get(replyTo) -> replyTo.tell(count)
        Reset -> count = 0
```

- **`actor Name(params) receives M`** declares an actor whose messages are of type `M`, usually a
  `data` type whose variants are the messages. An actor may be generic, `actor Cache[K: Hash +
  Sendable, V: Sendable](limit: int) receives CacheMsg[K, V]`, and its parameters follow the rules
  for a class's, with defaults and named arguments.
- **The body is the actor's start.** Its fields and statements run once, on the actor's own
  execution, before it handles any message: that is where its state is made. The parameters are its
  immutable fields, as a class's plain parameters are, and each one's type must be `Sendable`, which
  is checked where the actor is declared. Since an actor runs on any of an executor's workers,
  nothing its body, its arms or its methods reach may be a module-level `var` or a module-level
  value that is not sendable.
- **`receive` holds the arms** that handle a message: exactly a `match`'s arms over `M`, with
  patterns, guards and block bodies, and checked for exhaustiveness and for unreachable arms in the same way. A message the
  actor wants to ignore is handled by an arm that does nothing, `_ -> ()`, so ignoring one is always
  written down. Each arm's value is discarded.
- **Methods** in the body are the actor's own helpers, with the implicit receiver a class's methods
  have. They are private: an actor has no members anyone else can call, since only its messages
  reach it.
- **`self` is the actor's reference**, an `ActorRef[M]`, which it hands out so others can reply to
  it or send it more work. The object holding the actor's fields is never a value the program sees,
  so no reference to an actor's state can escape the actor.

`Counter(0)` does not start anything. It makes a **`Behaviour[CounterMsg]`**: an immutable value
holding the declaration and its arguments, which `spawn` starts. Keeping the two apart lets the
program say how a new actor is to be supervised and where it runs before it starts, and lets the
runtime start the actor again from the same arguments after a failure ([Supervision](#supervision)).

A behaviour is also made without a declaration, from a name and a start:
**`Behaviour[M](name: string, start: @sendable (ActorRef[M]) -> (M) -> unit)`**. The start runs
once, on the actor's first turn, is given the actor's own reference, makes the actor's state in its
locals and gives back the handler of each message. It is a [`@sendable`
function](/spec/types/#function-types), so it captures only sendable values and no `var`, and
reaches no module-level mutable state. An `actor` declaration is this, written as a class is:

```avin
import avin.actors.{Behaviour, spawn}
import avin.threads.Promise

main()
    val total = Promise[int]()
    val counter = spawn(Behaviour[int]("Counter", me ->
        var count = 0
        n ->
            count += n
            if n == 0 then total.complete(count)))
    counter.tell(5)
    counter tell 2
    counter.tell(0)
    println(total.future.await())
```

## Starting, telling and asking

```text
import avin.actors.{spawn, AskError}
import avin.time

main() -> int
    val counter = spawn(Counter(10))
    counter.tell(Increment(5))
    counter tell Increment(2)
    val answer = counter.ask(r -> Get(r), timeout = time.seconds(1)).await()
    answer match
        Ok(n) -> n
        Err(_) -> 1
```

- **`spawn[M](behaviour: Behaviour[M], onFailure: Supervision = Escalate, executor: Executor =
  Executor.shared()) -> ActorRef[M]`** starts the actor and gives back its reference at once; the
  body runs on the executor's threads, and messages sent before it has finished queue behind it.
  Called by code an actor is running, `spawn` starts a **child** of that actor; called anywhere else
  (`main`, a thread, a future's body) it starts a **top-level** actor.
- **`tell(message: M)`** puts the message in the actor's mailbox and returns at once; it never waits
  and never fails. `!` is a reserved spelling in Avin, so there is no operator for it, and `tell` is
  written infix where that reads better (`counter tell Reset`).
- **`ask[R: Sendable](make: ActorRef[R] -> M, timeout: Duration) -> Future[Result[R, AskError]]`**
  makes a reference that accepts one reply, builds the message around it with `make`, sends it, and
  gives back the future of the reply. The first message to the reply reference completes the future
  with `Ok`; any later one is dropped. `AskError` is `TimedOut` where no reply arrived within
  `timeout`, and `Ended` where the asked actor had already ended, so the message could not be
  delivered. The timeout is required: an ask always ends.
- **A reply address is an ordinary `ActorRef[R]`.** An actor that asks another passes the reply
  reference `ask` makes, or its own `self` where its message type is `R`, or an **adapter**:
  `adapter[R: Sendable](f: R -> M) -> ActorRef[R]`, called in an actor, makes a reference whose
  messages arrive in this actor's mailbox as `f` of them, so an actor talks to one with another
  protocol without widening its own message type. `f` runs where the message is sent, so it is
  `@sendable`; the adapter refers to this actor, equal to its `self` and ending with it. The
  reference an ask makes is shown as `Reply#1`, the replies numbered apart from the actors, and
  ends once its ask is answered or has timed out.
- **`ActorRef[-M]`** is contravariant: a reference that accepts every `Shape` is also one that
  accepts `Circle`s. It compares by the actor it refers to (`==` and `hash`), is `Sendable` and is shown
  as the declaration's name and a number, `Counter#3`, the actors of a program being numbered from
  one in the order they are spawned. It holds no state, so holding one keeps
  nothing but the reference alive.

Any thread may `tell` and `ask`. Inside an actor, waiting for an ask's future with `await` holds the
actor: it handles no other message until the reply comes, which is sometimes what is wanted. The
usual way is **`pipeToSelf[T](future: Future[T], f: T -> M)`**, which sends the actor `f` of the
value once the future completes, so the actor goes on handling messages meanwhile; a future complete
already sends at once, before anything the actor sends itself after the call. `f` runs in the
actor's own turn, so a trap in it fails this actor, not whatever completed the future. An actor
that awaits an ask of itself waits until the timeout.

An adapter's `f` runs where the message is sent, so a trap in it is the sender's: it fails the
actor that sent, or stops the program where `main` or a thread sent.

### Mailboxes and order

A mailbox is an unbounded queue. **Two messages one sender sends to one actor arrive in the order
they were sent**; messages from different senders interleave in any order. A message sent to an
actor that has ended is dropped, since an actor ending while messages are on their way to it is an
ordinary race and not a bug; a program that needs to know when an actor ends [watches](#lifecycle)
it, and an ask of one completes with `Ended`. Mailboxes are not bounded: a producer that must not
outrun its consumer asks rather than tells, and waits for each answer, or for every *n*th.

## What may be sent

**A message type, an actor's parameters and every type `ask` and `adapter` carry must be
`Sendable`.** `Sendable` is a trait the compiler decides for each type from its structure; it is
written as a bound like any trait (`[T: Sendable]`), and `ActorRef[M]`'s parameter is bounded by it,
so the check is made once, where the type is named, and never at a send:

| Sendable | Why |
| --- | --- |
| `int` and the other integers, `byte`, `real`, `char`, `bool`, `unit`, `string` | immutable values |
| tuples, `data` types (`Option`, `Result` and `Either` among them) whose fields are all sendable | immutable values |
| `List`, `Vector`, `ArraySeq`, `Range`, the immutable `Map` and `Set`, of sendable elements | immutable, persistent collections |
| a class with no `var` field, whose fields are all sendable, and which is not `open` or `abstract`; an `object` likewise | deeply immutable: no code can change it, and a subclass cannot add a `var` |
| a trait declared `: Sendable` | every type implementing it is checked to be sendable, at its `impl` or declaration |
| `ActorRef[M]`, `Behaviour[M]`, `Future[T]` and `Promise[T]` of a sendable `T` | made to be handed between threads |
| `Channel[T]` and `Atomic[T]` of a sendable `T`, `Mutex[T]` of any `T`, a mutex's `Condition`, `Once` | synchronization: every access is synchronized, and a mutex's value is reached only under its lock, so a mutex is the one deliberate door to shared mutable state |
| `Thread[T]` of a sendable `T`, an `Executor` | joining a thread and giving an executor work are synchronized |
| a [Sysl handle](/spec/sysl-bindings/#handles) | sent by handing over its ownership, checked when it is used (below) |
| `Isolated[T]` | an object graph nothing else reaches (below) |
| a [`@sendable` function type](/spec/types/#function-types), `@sendable (A) -> B` | checked where each value is made: it captures only sendable values and no `var`, and reaches no module-level mutable state |

**Not sendable**: `Array`, `Buffer`, `Deque`, the mutable maps and sets, a class with a `var` field
or a field that is not sendable, an `open` or `abstract` class, a [machine](#state-machines), a
thread `Scope`, a value of a plain function type, and `Any`. A plain function value is not sendable
because its type does not say what it captures, and a captured `var` is shared with the scope that
made it; a closure given to another thread is checked instead by what it captures, where it is
written, and one made as a `@sendable` function is checked where it is made ([what is
shared](/spec/threads/#what-is-shared)). The error names the part that is not sendable:

```text
error: `JobMsg` cannot be sent between actors: its variant `Fill` holds a `Buffer[byte]`, which is mutable; send an immutable copy (`toVector`), or the buffer as an `Isolated[Buffer[byte]]`
```

A declaration may also **promise** `Sendable`: a trait or a class after `:` in its header, a data
type in an impl (`impl Sendable for Reading`). The promise is checked against the structure, and
the error says what breaks it (`` `Blob` promises to be `Sendable`, but its field `size` is a `var` ``).
An `open` or `abstract` class that promises it is sendable, each subclass being checked in turn, as
each type implementing a trait that promises it is. A generic trait that promises `Sendable` is
sendable at sendable type arguments, and a type implementing it has the parameters it passes to the
trait taken as sendable when it is checked. A type that implements one through a generic trait,
`HashMap[K, V]` through `Map[K, V]`, is sendable at sendable arguments for those parameters. The
standard library promises `Sendable` for the types whose structure cannot show it, the synchronized
ones and `Vector`, and the immutable `Map` and `Set` traits promise it. `Sendable` is only a bound
and a promise, never the type of a value.

### Handing over ownership

Some mutable things have to move: a socket accepted by a listener goes to the actor that serves the
connection, and a large buffer filled by one actor goes to the next.

- **A Sysl handle in a message is handed over.** A handle has an owner, the execution that may use
  it ([Sysl bindings](/spec/sysl-bindings/#handles)), and an actor is an owner. Sending a message
  to an actor hands every handle it holds that the sender owns to that actor, as the message is put
  in its mailbox, so the receiver uses it and the sender's later use of it traps (`` `TcpStream`
  belongs to another thread ``), whether or not the receiver has used it yet. A handle the sender
  does not own stays as it is, so one sent twice goes with the first message, and the second
  receiver's use traps; a receiver that sends one on hands it over in turn. A message dropped
  because its actor has ended leaves its handles the sender's. A reply to an `ask` completes a
  future, which puts its handles in transit for the code that waits for it. The compiler knows
  which types can hold a handle, so a send walks the message only for those: a message of numbers
  and strings, of immutable collections of them or of actors' references is never walked. The walk
  follows what the message holds — its fields, a collection's elements, what a function value
  captures, an `Isolated`'s objects — and stops at what is shared between owners, an actor's
  reference, a future, a promise, a channel, a mutex, an atomic, a thread or an executor, whose
  handles keep their owners.
- **A mutable object graph moves inside an `Isolated[T]`.** `isolate(body)` runs its body, a by-name
  argument written in place or as a trailing block, and wraps what it returns:

  ```text
  val chunk = isolate
      val b = Buffer[byte]()
      for i in 0..<size do b.append(byte.wrap(i))
      b
  worker tell Fill(chunk)
  ```

  The compiler checks that **the body reads nothing from around it that is not sendable**, and that
  **nothing it calls, however indirectly, reaches module-level mutable state** (a module-level
  `var`, or a module-level value or `object` whose type is not sendable, a `lazy val` among them).
  Avin compiles the whole program, so the second is known for every function, in every module,
  through every trait method a call may reach and every function value made inside the body. Every
  mutable object the body leaves reachable from its result was therefore made inside it, and
  nothing outside holds one. The body may read a `var` of the code around it whose type is
  sendable, since it runs at once, and module-level values that are immutable and sendable; an
  actor's own state, its parameters among them, is read through a `val` copied first. The receiver
  gets the objects out with **`take() -> T`**, after which they are ordinary objects it owns; a
  second `take`, by anyone, traps with `` `Isolated` was already taken ``, one atomic
  compare-and-set deciding which take wins. **`with[R: Sendable](f: T -> R) -> R`** works on the
  objects while leaving them isolated, `f` being checked by the body's rule, so a pipeline of actors
  can fill, pass, change and pass one buffer without a copy. `with` holds the objects while `f`
  runs, so a `take` or a `with` of the same `Isolated` meanwhile, from another thread or from inside
  `f`, traps with `` `Isolated` is in use by `with` ``, and a `with` after the objects are taken
  traps as a second `take` does.

  The rule is a parameter's mark, **`@isolating`**, which a program's own functions may write too
  ([what is shared](/spec/threads/#what-is-shared)): `isolate[T](@isolating body: -> T) ->
  Isolated[T]`, and `with`'s `f`. `Isolated` and `isolate` are imported from `avin.actors`.

The cheapest way across stays the plainest: build with a `Buffer` or a builder, turn the result into
an immutable collection, and send that.

## Supervision

A trap reports a bug. In a thread it stops the program, because the state the threads share is in
doubt. **An actor shares no mutable state**, so a trap in one leaves only that actor's own state in
doubt, and the runtime can contain it:

- **A trap in an actor fails the actor**: in its body, an arm, a timer's message, or an `enter` or
  `leave` block. The runtime unwinds the actor's frames back to the worker running it, drops the
  message being handled, and reports the trap as it reports any, naming the actor:

  ```text
  avin: trap in Counter#3: integer overflow at counter.avin:12:31
  ```

  A trap inside the core library is reported where the actor's code called it, as any library trap
  is. The actor's `Failed` reason is the trap's reason, without its place: `Failed(integer
  overflow)`.
- **Where the trap leaves shared state in doubt, the program stops instead**, as it would for a
  thread: a trap while the actor holds a `Mutex`, runs a `Once` body or initializes a `lazy val`
  (each of which other threads may be waiting on), or inside a Sysl call: an Avin function Sysl
  called back, or a value a binding cannot take. The report still names the actor. A stack that
  runs out stops the program too, with the same `avin: trap: stack overflow` a thread's does, since
  it may run out anywhere, inside the runtime or the C library among the places.
- **The supervision given to `spawn` decides what follows**, a value of the `data` type
  `Supervision`:
  - **`Escalate`**, the default: the parent fails as if it had trapped, and its own supervision
    decides. A top-level actor that escalates stops the program, with the trap already reported. So a
    program that supervises nothing behaves as it would with threads: a bug stops it. The actor
    that escalated handles nothing more and waits for its parent's failure to stop it, so a failure
    escalated to the top stops the program with no `stopping` block run; it ends `Failed`.
  - **`Stop`**: the actor ends, its children first, and its watchers are told it failed.
  - **`Restart(limit: int, within: Duration)`**: the actor's children are stopped and waited for,
    its timers and watches cancelled, its `stopping` block runs, and its body runs again from the
    same `Behaviour`, so it starts again with fresh state; the messages still in its mailbox stay
    there, and its reference, number and watchers are unchanged. A restart is reported on standard
    error after the trap, as `avin: restarting Counter#3`. A failure that would make more than
    `limit` restarts within the last `within` escalates instead, so `Restart(0, …)` never restarts.
    A limit less than zero, or a window not more than zero, traps in `spawn`.
- **There is no resuming with the old state.** The trap stopped the actor part-way through changing
  its state, so the state is not kept.
- **What was asked of a failed actor.** The question whose message trapped is dropped with it, so
  its ask times out; questions still in the mailbox are dropped and time out where the actor ends,
  and are answered by the fresh state where it restarts; a question sent once it has ended answers
  `Ended`. A failure escalated to a parent that is already ending or restarting changes nothing,
  since the parent is stopping its children already.

## Lifecycle

- **An actor runs until it stops.** `stop()` in an actor ends it once the current message is
  handled; what is left in its mailbox is dropped. `stop(child)` ends one of its own children, and
  stopping any other actor is an error: an actor that should end on request is sent a message
  saying so.
- **Children end first.** An ending actor stops each child, and waits for their ends, before it ends
  itself; an actor's tree of children is torn down from the leaves.
- **`stopping`** is a block in the actor's body that runs whenever the actor ends or restarts, the
  place to close handles and tell others. It runs after a failure too, on state the trap may have
  left half changed, so it does only what is safe on any state. A trap inside it is reported and the
  actor ends without running it again: `Failed` with that trap's reason, or with the first one where
  a trap had failed it already, and with no supervision asked, so a block that traps during a
  restart ends the actor instead. Of several `onStopping` functions, those after one that traps do
  not run.
- **Watching.** `watch(ref, message: M)` asks for `message` in this actor's own mailbox when the
  actor `ref` ends, for whatever reason, and `watch(ref, f: Ending -> M)` for `f` of how it ended;
  `unwatch(ref)` stops asking. Watching an actor that has already ended delivers at once. `Ending` is
  `Stopped` or `Failed(reason: string)`; a restart is not an ending.
- **From outside any actor**, `ref.ended() -> Future[Ending]` completes when the actor ends, so `main`
  waits for the top-level actor that runs the program with `spawn(App()).ended().await()`.
- **An actor is not collected because nothing refers to it**: an actor nobody can send to still runs
  its timers and its children. It ends by stopping, by its parent ending, or with the program.
- **The program ends when `main` returns**, as it does with threads, with every actor still running.

An actor also has **`children() -> Vector[ActorRef[Nothing]]`**, its own reference `self` and its
parent's in **`parent() -> Option[ActorRef[Nothing]]`**, an `ActorRef[Nothing]` being one that may be
watched and compared but not sent to.

These are the actor's own methods, those of **`ActorContext[M]`** in `avin.actors`, which the object
holding an actor's state extends; a field or method of the actor's own with the same name and
parameters is an error. A behaviour written as a library value makes one in its start from the
reference it is given, `ActorContext(me)`, and `ctx.onStopping(f)` gives it what a `stopping` block
is. Made, or called, by any code other than the actor's own, an `ActorContext` traps. The details:

- `children()` lists the children that have not ended, in the order they started; `parent()` is
  `None` for a top-level actor, and for one a future's body started, which is not an actor's code.
- `stop(self)` is `stop()`. Stopping an actor that has ended does nothing, whoever started it.
- An actor started while its parent is ending, from its `stopping` block, is stopped at once, and the
  parent does not wait for it.
- Watching an actor again replaces the earlier watch, so its end is told once. An actor that watches
  itself is told nothing, since its mailbox ends with it. The `f` of `watch(ref, f)` runs in this
  actor's own turn, when it takes the message, so a trap in it fails this actor and never the one
  watched; it is `@sendable`, since it is carried to the mailbox from wherever the end is known.
  `unwatch`, and cancelling a timer, also drop
  the message already queued and not yet handled.
- A `stopping` block runs once the body has run: the body gives it to the context last. An actor has
  one.
- The state of an actor is never captured by a `@sendable` function, whatever its fields, since its
  methods act for the actor that is running; a body that needs a field copies it into a `val`.

### Timers

`after(delay: Duration, message: M) -> Timer` sends the actor `message` once `delay` has passed, and
`every(interval: Duration, message: M) -> Timer` each time `interval` passes. `cancel()` on the
`Timer` stops it; an actor's timers are all cancelled when it ends or restarts. A timer's message is
queued like any other, so it never interrupts an arm.

Timers fire in the order they fall due, those due at the same time in the order they were set; a
delay of zero or less is due at once. A repeating timer is set again an interval after it fires, and
an interval of zero or less traps. A `Timer` is sendable, and `cancel()` works from any thread. One
thread the runtime starts with the first timer keeps every actor's timers.

## Scheduling

**Actors are many; threads are few.** An actor's messages are handled in **turns**, and each turn
is a [lightweight thread](/spec/threads/#lightweight-threads) of its own, run by the same workers,
about one per processor, as every other thread. An actor given an executor,
`spawn(b, executor = Executor.fixed(1))`, runs its turns as tasks on that executor's workers
instead, which bounds how many of them work at once:

- **An actor with messages has one turn queued.** A send to an actor with an empty mailbox queues a
  turn, which handles its messages one at a time. **An actor never runs two turns at once**, not
  even while a turn is parked and new messages arrive meanwhile, and the handling of each message
  happens before the handling of the next, wherever each runs, so an actor's fields need no lock
  and no `Atomic`.
- **Fairness.** A turn handles at most 16 messages before the actor's next turn goes to the back of
  the queue, so a busy actor cannot hold a worker from the others. Within one message, a turn is
  time-sliced as any thread is: one that computes for a second shares its worker with the other
  ready threads at its safepoints, though the actor itself handles nothing else meanwhile. On an
  executor of its own, a task is not set aside, so an arm that computes for a second holds that
  executor's place for that second.
  The body runs in the actor's first turn and is not one of the 16; a turn ends early when the
  mailbox is empty. An actor's executor being shut down does not end it: its turns still run there,
  on a worker started for them where none is left, so `tell` never fails.
- **An actor may move between workers**, between messages and, where a turn parks, within one. A
  turn sets its context's owner word to the actor before handling its messages, so the handles an
  actor owns are its own on any worker and never trap. `spawn` gives the actor the handles its behaviour reaches that the
  spawning code owns, as a thread's `spawn` does with its body.
- **Waiting.** A turn that waits, in `await`, an ask, a `sleep`, a channel or a lock, parks: its
  worker runs other actors and threads meanwhile, and the actor handles no other message until the
  turn goes on. A thousand actors each sleeping in a turn hold no worker between them. A
  [blocking binding](/spec/sysl-bindings/#calls-that-wait) holds its worker, and the runtime runs
  another worker in its place while it does. On an executor of its own, a task that waits in `await`
  or `threads.blocking(body)` leaves its place to another worker of that executor, and one that
  waits otherwise keeps it.
- **Memory model.** A `tell` happens before the handling of the message it sent, and a reply's
  `tell` happens before the `await` or `pipeToSelf` that delivers it, joining the edges of
  [the memory model](/spec/threads/#the-memory-model).

### Threads, channels and futures

Actors are built on `avin.threads` and live beside it. Any thread may start, tell and ask actors;
an ask is a future, and a future's value reaches an actor by `pipeToSelf`. A `Channel` is the tool
for a fixed pipeline of threads, and an actor that receives from a channel parks its turn and
handles nothing else while it waits, so between actors, messages are the channel. Locks remain for the state that genuinely
must be shared; a `Mutex` is sendable for that reason.

## State machines

**`machine`** declares a type whose objects move between declared **states**, driven by **events**:

```text
data DoorEvent
    Open
    Close
    Lock(code: int)
    Unlock(code: int)

machine Door receives DoorEvent
    var opened = 0

    state Closed
        Open ->
            opened += 1
            Opened
        Lock(code) -> Locked(code)

    state Opened
        enter
            println("open")
        Close -> Closed

    state Locked(code: int)
        Unlock(c) if c == code -> Closed

    receive
        _ -> stay
```

- **Each `state` declares a state**, optionally with parameters, the data that state holds; its
  arms, `enter` and `leave` blocks read them by name. The machine's own fields and parameters are
  the data every state shares. A machine is declared like a class, generic and with parameters, and
  its other methods are ordinary methods.
- **An arm's value is the next state**: a state written as a constructor (`Locked(code)`), or
  **`stay`**, which keeps the current state and its data. There is no separate transition statement:
  what an arm yields is where the machine goes.
- **The first `state` is where the machine starts**, and takes no parameters.
- **`enter` runs when the machine moves into the state, and `leave` when it moves out**, `leave`
  before `enter`. An arm yielding a state, even the current one, is a transition and runs both;
  `stay` runs neither. The first state's `enter` runs when the machine is made, after the body.
  The arm runs first; in `leave`, `state()` still answers the state being left, and in `enter` the
  new one.
- **`receive` holds the arms every state shares**, tried after the current state's own. **Every
  state must handle every event**: the compiler checks, state by state, that its own arms and the
  shared ones together cover the event type, as a `match` is checked, guarded arms not counting.
  A state's own arm that its arms above it already cover, and a shared arm the shared arms above
  it already cover, is never reached and is an error; a shared arm that a state's own arms cover is
  not, since a state overrides a shared arm by handling the event itself.
  Without its shared `receive`, `Door` would be refused:

  ```text
  error: state `Locked` of `Door` does not handle `Open`, `Close`, `Lock(_)` or `Unlock(_)` where the guard is false
  ```

- **Using a machine.** `Door()` makes one in its first state. `handle(event) -> Door.State` applies
  one event and answers the new state, and `state() -> Door.State` reads the current one, where
  `Door.State` is a `data` type the declaration makes, with a variant for each state and the state's
  parameters as its fields, so a program matches on it:
  `door.state() is Locked(_)`. Its variants are names of the machine's module, as every variant
  is, and may share a name with another machine's state or an event's variant, as [variants of
  different types may](/spec/declarations/#data): an arm's result is its own machine's state,
  an event passed to `handle` is the event type's, and `door.state() is Idle` matches against
  `Door.State`. A machine is a mutable object, so it is not sendable; handling an
  event from inside one of its own arms, `enter` or `leave` blocks traps, as taking a held mutex does.

### An actor's states

An actor's body takes the same `state` blocks, so an actor with phases is written as a machine
driven by its mailbox. Each message is an event, the arms yield the next state, and `enter`,
`leave`, `stay` and the check that every state handles every message work as they do in a machine:

```text
data ConnMsg
    Connect(host: string)
    Connected(socket: Socket)
    Send(bytes: Vector[byte])
    Closed

actor Connection receives ConnMsg
    var pending = Vector[Vector[byte]]()

    state Idle
        Connect(host) ->
            pipeToSelf(openSocket(host), s -> Connected(s))
            Connecting

    state Connecting
        Connected(socket) ->
            for b in pending do socket.write(b)
            pending = Vector()
            Ready(socket)
        Send(bytes) ->
            pending = pending :+ bytes
            stay

    state Ready(socket: Socket)
        leave
            socket.close()
        Send(bytes) ->
            socket.write(bytes)
            stay
        Closed -> Idle

    receive
        _ -> stay
```

An actor with states needs no `receive` block; its shared arms, where it has them, yield states as
a machine's do. Its first state's `enter` runs after the body, when the actor starts. A message that
arrives, a timer that falls due or a reply piped to itself is handled by the state current when it
is handled, not the one that asked for it. Only a transition runs `leave`: an actor that stops or
fails runs its `stopping` block, where `state()` answers the state it ended in. A trap in an
`enter` or `leave` fails the actor as one in an arm does. A restarted actor starts again in its
first state. The actor's `State` type, its states and `state()` are its own, private to its module
as the actor's state is. Its states share names as a machine's do, with a message's variant, another
actor's state or a machine's: an arm's result is the actor's own state, an arm's pattern the message
type's, and a bare name nothing decides is refused, naming the qualified forms (`Ev.Idle`,
`Connection.State.Idle`). An actor without `state` blocks is one whose arms all `stay`, which is why
its arms yield nothing.

## Keywords

`actor` and `machine` are keywords only where a declaration starts, before the declaration's name;
`receives` only after an actor's or machine's header; and `state`, `receive`, `enter`, `leave`,
`stopping` and `stay` only inside an actor's or a machine's body, at the start of a line (`stay` as
an arm's value). Everywhere else they are ordinary names, so a variable called `state` stays legal.
