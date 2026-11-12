---
title: User interfaces
summary: The ui package — syslui's views, controls and signals, and syslui-sdl's window and frame loop, bound for Avin; drawing and tapping a tree with no window.
weight: 28
---

A program with a window reaches it through the **`ui` package**, which binds two Sysl packages:
**syslui**, which lays a tree of views out, paints it, measures its text and routes its events, and
**syslui-sdl**, which opens the window, runs the frame loop and turns the platform's events into
syslui's. The package is a thin binding: every view, control and signal in it is syslui's own,
reached through [handles](/spec/sysl-bindings/#handles), and its functions are syslui's under Avin's
names. On it stands a layer written in Avin, [components and state](#components-and-state): an
interface built by functions from state the program keeps, and built again when the state changes.

The package lives in this repository at `packages/ui`, its module is `ui`, and a program names it as
a dependency:

```hocon
dependencies {
  ui { path = "../../packages/ui" }
}
```

It names `syslui-sdl` 0.2.26 in its own `sysl-dependencies`, which brings syslui 0.2.1 and SDL3,
so a program using it names no Sysl package itself, and is linked with SDL3.

```text
import ui.{View, app, button, column, invalidate, run, text}

var count = 0

add()
    count += 1
    invalidate()

screen() -> View = column([text(s"$count"), button("Add one", add)], 16)

main() -> int = run(app("Counter", screen, () -> 0x1E1E2E)) match
    Ok(_) -> 0
    Err(_) -> 1
```

## Views

`View` is a handle on syslui's `View` trait: whatever view a binding answered, a leaf, a container
or a modifier wrapping another. Each modifier answers a new view wrapping its receiver, so a chain
reads from the innermost view out: `text("hi").padding(8).panel()` is a padded label on a surface.

| Function | View |
| --- | --- |
| `text(s)` | One run of text; Unicode is measured and drawn whole |
| `spacer()` | A gap taking one share of what its column or row has left over |
| `column(children, spacing = 0)`, `row(children, spacing = 0)` | Children top to bottom, or left to right, `spacing` apart; `children` is a `Vector[View]` |
| `grid(children)` | Children on a grid of columns, each spanning what its `cell` says, the whole width without one |
| `scroll(child, offset, height)` | `child` in a window `height` tall, scrolled down by an `IntSignal` |
| `divider(thickness = 1)` | A line across its container |

| Modifier | Effect |
| --- | --- |
| `padding(n)`, `pad(top, right, bottom, left)` (each defaulting to 0), `padded()` | Inset on every side, on the sides named, or by the theme's padding |
| `panel()` | On a surface the theme draws |
| `background(color, radius = 0)` | The rectangle behind it filled with a [colour](#colours), its corners rounded by `radius` |
| `border(color, width = 1, radius = 0, dash = 0)` | Outlined `width` thick inside the rectangle it was given, so nothing moves; dashes `dash` long where `dash` is not 0 |
| `foreground(color)` | The text inside drawn in `color`; the innermost one in force wins |
| `tinted(color)` | The controls inside in `color` where they name no `Role`: the theme's primary colour |
| `textStyle(style)`, `fontSize(pt)` | The text inside at one of the theme's sizes (`Display`, `Title`, `Body`, `Caption`), or at `pt` points |
| `frame(w, h)` | Exactly `w` by `h` |
| `flex(n = 1)` | `n` shares of a column's leftover height or a row's leftover width; the last link of a chain |
| `cell(span, offset = 0)` | `span` of a grid's columns after skipping `offset`; the last link of a chain |
| `onTap(f)` | Calls `f` when tapped |
| `named(name)` | A name a snapshot reports and a tap by key finds; nothing is drawn differently |

`weight()` answers the shares a view asks of its container's leftover: 0 for a leaf and any modifier
but `flex`, 1 for a spacer and `n` for `flex(n)`.

A view's children cross into syslui as [a collection of handles](/spec/sysl-bindings/#collections-of-handles):
each is shared for the call, so one view may be the child of several containers, and a container
outlives the vector it was made from. A view closed with `close()` and then given to a container
stops the program at the call:

```text
avin: trap: `View` is closed at main.avin:11:13
```

### Colours

A colour is an `int` holding `0xRRGGBB`, as syslui draws it, so a hex literal is one: there is no
alpha, since nothing in syslui composites. syslui takes it as a `u32`, which an Avin `int` reaches as
any [integer parameter](/spec/sysl-bindings/#integer-parameters) does, converted with a check: a
negative colour stops the program where the modifier was called.

```text
avin: trap: value out of range for u32 at main.avin:11:12
```

| Function | Colour |
| --- | --- |
| `rgb(r, g, b)` | Of the channels `r`, `g` and `b`, each from 0 to 255; a channel outside that range stops the program |
| `mix(a, b, pct)` | `a` moved toward `b` by `pct` percent, channel by channel; a percentage outside 0 to 100 is taken as the nearer end |
| `lighten(c, pct)`, `darken(c, pct)` | `c` moved toward white, or black, by `pct` percent |

```text
text("Saved").foreground(0xFFFFFF).padding(8).background(darken(0x3E9B5F, 20), 6)
```

## Controls

| Function | Control |
| --- | --- |
| `button(label, onTap, role = Primary, variant = Filled)` | Calls `onTap` when tapped |
| `switch(on, role)`, `checkbox(on, label, role)` | Flip a `BoolSignal` |
| `radio(chosen, at, label, role)`, `radioGroup(labels, chosen, role, spacing = 6)` | Make an `IntSignal` hold an option's index |
| `slider(value, lo, hi, width, step, role, id)` | Over an `IntSignal` and `int` bounds, or a `RealSignal` and `real` bounds; a step of zero moves by one, or continuously |
| `progress(fraction, width, role)`, `meter(value, lo, hi, width, band, target, role)` | Show a value; take no input |
| `textField(text, id, width, hint, onEnter, onEscape)`, `textArea(text, id, width, rows, hint, onEscape)` | Edit a `StringSignal`, focused by a nonzero `id` |
| `tabs(labels, chosen, body)` | Headers over an `IntSignal`, and the view `body` answers for the chosen index |

`Role` (`Primary`, `Secondary`, `Accent`, `Success`, `Warning`, `Error`) names which of the theme's
colours a control takes, and `Variant` (`Filled`, `Outline`, `Dashed`, `Soft`) how it wears it; both
are [mirrors](/spec/sysl-bindings/#mirrors) of syslui's enums. Every callback a control is given runs
on the thread that runs the frame loop, the thread that called `run`, so none is `@threaded`.

## Signals

A control that holds a value reads and writes a **signal**: a handle on one instance of syslui's
`Signal[T]`, made by `intSignal`, `realSignal`, `boolSignal` or `stringSignal` and read and written
with `read()` and `set(v)`. The four classes are `IntSignal`, `RealSignal`, `BoolSignal` and
`StringSignal`, one for each instance a control takes.

A control writes the application's own signal, the one the program made and holds, never a copy:
a tap's write is read by the program at once, the next build of the tree shows it, and every other
control over the same signal shows it too. A signal outlives the views made over it, so a tree
built afresh each frame hands its controls the same signals each time:

```text
val done = boolSignal(false)
val tree = () -> column([switch(done), checkbox(done, "Done"), text(s"done=${done.read()}")])
// a tap on the switch: done.read() is true at once, and the next tree() shows the checkbox checked
```

Work [handed to the main thread](#threads) and a control may both write one signal in a frame: the
work runs at the frame's start and the control's write comes after it, each is one write, and the
later is the value the build shows.

Every `set` is one write, whatever it writes, and `invalidate()` is one more; `writes()` answers how
many there have been. The frame loop builds the tree again on the frame after the count has moved,
so a program keeping its state in Avin variables changes them and calls `invalidate()`.

## The application

| Function | |
| --- | --- |
| `app(title, tree, ground, width = 900, height = 660)` | An `App`: the window's title, the function building its tree, the function answering its colour, `0xRRGGBB`, and its size in points. Nothing is opened |
| `run(a)` | Opens the window and runs the frame loop until the window is closed or `stop()` is called; `Err` holds SDL's sentence where the window could not be made |
| `stop()` | Ends the loop after the frame it is in |
| `setTitle(title)` | Retitles the running window; `false` with none |
| `saveFrame(file)` | Writes the next finished frame to a PNG file |

`run` is `blocking`: while the loop waits for events the thread is safe for the collector, and it is
running for each call of `tree` and `ground` and every control's callback. A window runs on the
[main thread](/spec/threads/#lightweight-threads), the operating system thread `main` was called
on, which is never a lightweight thread; at the start of every frame the loop runs the
[work other threads have handed it](#threads), and when the loop ends `run` runs what is still
waiting before it returns. `ground` answers a
`long`, which Sysl takes as the `u32` syslui asks for, [checked](/spec/sysl-bindings/#callbacks): an
answer below 0 or past `0xFFFFFFFF` stops the program.

Only the account at the screen can open a window: `run` from a process with no window session waits
for one for ever. A test draws with no window instead.

## Drawing with no window

syslui's **recording canvas** lays a tree out against a fixed font, eight points a byte and sixteen
a line by default, and writes down every operation instead of drawing it:

| Function | |
| --- | --- |
| `recorder(charW = 8, lineH = 16, size = 16.0)` | A new recording canvas, which is also the pointer a test presses |
| `render(v, c, at)` | Lays `v` out in the `Rect` `at` and draws it on `c` |
| `c.dump()` | What was drawn, one operation a line |
| `c.point(x, y, down)`, `c.release(x, y)` | Moves the pointer, held or not, and lets it go; a press and a release in one control is a tap |
| `snapshot(v, c, at)` | A `Snapshot` of what `v` puts on screen: `len()`, `dump()`, `toJson()`, and `clickPoint(key)`, where to tap a control's name or its reference `@n`, or a `Missed` saying why there is nowhere (`Unknown`, `Duplicate`, `Covered`) |
| `tap(v, key, at)` | Draws `v` in `at` on a new canvas and taps the control `key` names, at the centre of what of it is shown, drawing a frame while the pointer is held so a slider moves to it |

A key no control on screen answers to, and a name two controls share, are `Err(Missed(…))`:

```text
Unknown: nothing on screen is called 'nothing'
```

## Components and state

An interface is a function of the program's state, built again whenever that state changes. The
state is held in **`State[T]`**, an Avin object holding a value of any type, made by `state(v)`:

| Member | |
| --- | --- |
| `get()` | The value |
| `set(v)` | Replaces it; one write, which the frame loop builds again after |
| `update(f)` | `set(f(get()))` |

A set is one write whatever it writes, the same value included, so nothing else is called after
one. A state of a reference holds the reference: changing the object in place is no write.

```text
import ui.{View, button, column, component, root, text}

counter(step: int) -> View = component c ->
    val count = c.state(0)
    column([text(s"${count.get()}"), button(s"Add $step", () -> count.update(n -> n + step))])
```

### Components

A **component** is a function building a view, whose body is written `component c -> …`: the
body is given the component's **`Scope`**, which keeps values from one build to the next. Its props
are its function's parameters, read afresh on every build.

| Scope member | |
| --- | --- |
| `c.state(init)` | A `State` holding `init` on the component's first build, the same state on every build after; `init` is by-name, evaluated only then |
| `c.remember(make)` | Any value, made by the by-name `make` on the first build and kept |
| `c.derived(f)` | The value of a [derived](#derived-values) `f`, kept across builds |

A scope answers by order: the component's *n*th request on every build is the same value. A request
for another type than the last build's in that place, or a request made after the build, from a
callback the build left behind, stops the program:

```text
avin: trap: a component asked for the values it keeps in a different order from its last build
avin: trap: a component's scope was asked for a value outside the component's build
```

**`root(body)`** answers the function a frame loop calls for its tree, `app(title, root(screen),
ground)`: each call is one **build**, and every component built during it is in the root's tree. A
component is found again on the next build by its **place** among the components its parent builds
without a key, the first, the second, and so on; **`keyed(key) c -> …`** finds it by its key, a
`string` or an `int` (an `int` key and its decimal string are one key), wherever among its parent's
components it is built. A list reordered keeps its keyed components' state; unkeyed, the state stays
in its place, and a component that disappears from before another gives that one its state.
Components nest: each keeps its own inside its parent's, and separate roots keep separate trees.

**A component the build of its parent leaves out is gone**, with everything it kept and every
component inside it: a component shown again after a build without it starts afresh. A component
built in a tab's body is built during the build, and is one place whichever tab shows it.

Two components built under one parent with one key, a component built while no root is building,
and a root built inside its own build each stop the program:

```text
avin: trap: two components built under one parent have the key 'x'
avin: trap: a component was built outside the build of a root
avin: trap: a root was built again inside its own build
```

A root built inside a component's build is a tree of its own, and is fine.

### Controls over state

Every control that holds a value has an overload taking a `State` in place of a signal:
`switch`, `checkbox`, `radio`, `radioGroup`, `slider` over a `State[int]` or a `State[real]`,
`textField`, `textArea`, `tabs` and `scroll`. It reads and writes the state's own signal,
`State.signal(s)`, which is made from the state's value on the first call and is from then on
where the value lives: a control's write is the state's value at once, with no build in between,
and a set is the signal's write. There is one value, so no write echoes back, and either way a
change is one write.

```text
val done = state(false)
checkbox(done, "Done")              // a tap flips `done`
done.set(true)                      // the next build shows it checked
```

### Derived values

`derived(f)` is a `Derived[T]` whose `get()` computes `f` when first asked, and again only after
something has been written since: a state's set, a signal's write, an `invalidate()`. Otherwise it
answers the value it kept. Derived values may read other derived values.

### Threads

The interface runs on the thread that called `run`, and its state belongs to that thread. A
`State`, a `Derived` and a `Scope` are mutable, so none is [sendable](/spec/threads/#what-is-shared),
and a closure for another thread cannot capture one:

```text
error: what is passed for `body` of `spawn` may use only sendable values from around it, but it uses `count`, a `State[int]`, whose field `held` is a `var`; make it a `val`
```

Another thread changes the interface by handing the main thread something to do, and the main
thread does it at the start of a frame. An **inbox** takes messages: `inbox(handle)`, made on the
main thread, is an `Inbox[T, R]` whose handler `handle: (T) -> R` runs on the main thread with the
interface's state in reach, and an inbox is sendable, so threads are handed it:

| Function | |
| --- | --- |
| `inbox(handle)` | An `Inbox[T, R]` whose messages `handle` handles on the main thread; `T` and `R` are sendable |
| `box.send(m)` | Queues `m` and answers at once a `Future[R]` of the handler's answer |
| `box.ask(m)` | Queues `m` and waits for the handler's answer |
| `post(work)` | Queues the `@sendable () -> unit` `work` to run on the main thread, and returns at once |
| `handlePosted()` | Runs every message and piece of work waiting, on the main thread, and answers how many |

```text
import avin.threads.{sleep, spawn}
import avin.time.millis
import ui.{inbox, state}

val ticks = state(0)
val tick = inbox((n: int) -> ticks.set(n))
spawn(() ->
    for n in 1..10
        sleep(millis(100))
        tick.ask(n))
```

- **Everything runs on the main thread, in the order it was sent.** Messages to every inbox and
  posted work wait in one queue, so what one thread sends and posts runs in the order it did them,
  whichever inboxes they went to, and a send happens before its handling. Among many senders each
  thread's order is kept.
- **The frame loop runs what is waiting at the start of every frame**, before it decides whether to
  build the tree again, so a state the work sets is built in that same frame. `run` runs what is still waiting
  once more when the loop ends. A program with no window, a test among them, calls
  `handlePosted()` itself.
- **What the work sends or posts waits for the next frame.** `handlePosted` runs only what was
  waiting when it was called, so work that posts itself again runs once a frame rather than holding
  the frame for ever.
- **An ask parks.** A lightweight thread waiting in `ask`, or in `await` on what `send` answered,
  frees its worker until the main thread has handled the message. An ask on the main thread, which
  would wait for itself, stops the program:

  ```text
  avin: trap: an inbox was asked from the main thread, which would wait for itself to handle the message
  ```

  A send or a post from the main thread, from a handler too, is queued like any other.
- **Only the main thread makes an inbox, handles what is waiting and runs a window.** Each reaches
  the main thread's own state, so code for another thread that does one of them is refused where it
  is compiled, as reaching any module-level mutable state is:

  ```text
  error: what is passed for `body` of `spawn` may run on another thread, which shares the program's module-level state, but through `handlePosted` → `handleOne` it reaches `handlers`, a module-level value holding a `Map[int, () -> unit]`, which is mutable; an immutable copy (`toMap`) is sendable
  ```

- **A message, an answer and posted work cross threads, so each is sendable**; a handler is not,
  since it stays on the main thread, and may capture a `State`. An inbox lasts as long as the
  program. When `main` returns the program ends, as it always does: work still waiting is never
  run, and a thread parked on an ask does not hold the program open.

## What is not bound

- Overlays (`popover`, `modal`, `menu_bar`), tables, images, selection, the theme and its
  `restyle`, keyboard input to a recording canvas, `on_event`, `on_frame` and the control socket;
  each is bound when something needs it.
