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
names. A layer of components and signals written in Avin is to stand on it.

The package lives in this repository at `packages/ui`, its module is `ui`, and a program names it as
a dependency:

```hocon
dependencies {
  ui { path = "../../packages/ui" }
}
```

It names `syslui-sdl` 0.2.22 in its own `sysl-dependencies`, which brings syslui 0.1.17 and SDL3,
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
running for each call of `tree` and `ground` and every control's callback. `ground` answers a
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
| `tap(v, key, at)` | Draws `v` in `at` on a new canvas and taps the control `key` names, at the centre of what of it is shown |

A key no control on screen answers to, and a name two controls share, are `Err(Missed(…))`:

```text
Unknown: nothing on screen is called 'nothing'
```

## What is not bound

- **A control writing the program's signal.** Sysl 0.9.0 gives a `&T` parameter a copy of a handle's
  value where the type has no destructor, so a switch, checkbox, radio, slider, text field or tab
  given a signal writes a copy; reading and setting a signal from Avin is unaffected. The next Sysl
  release passes the handle's own value.
- Overlays (`popover`, `modal`, `menu_bar`), tables, images, selection, the theme and its
  `restyle`, keyboard input to a recording canvas, `on_event`, `on_frame` and the control socket;
  each is bound when something needs it.
