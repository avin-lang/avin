---
title: Language specification
summary: What Avin is — a natively compiled, garbage-collected application language with a small type system and a light, indentation-based syntax.
weight: 10
---

Avin is a compiled, garbage-collected application language: expression-oriented code, algebraic
data, pattern matching, first-class functions, rich immutable collections, classes and traits with
real subtyping, built-in actors and state machines, and type inference, with a deliberately small
type system. Its syntax is light and quiet: indentation marks blocks, functions are often a single `name(params) = expression` line, and
`match` reads top to bottom as a list of cases.

Avin owns its whole runtime, so its generics are reified, values stay unboxed in containers,
absence is an explicit `Option`, equality is typed, `string` is a built-in type, and failures are
values. The [design choices chapter](/spec/design/) lists each choice and what it means.

## Principles

1. **Native and fast.** Whole-program compilation through LLVM; generics are monomorphized; values
   that need no identity are not boxed. A feature whose cost is invisible at the call site needs a
   reason.
2. **Reified types.** Every type that exists at compile time exists at run time; a type test never
   lies.
3. **Nominal.** A type conforms to a trait or class because it says so, never because its member
   names happen to match.
4. **Two failure channels.** A failure a correct caller can meet is a value (`Result`, `Option`); a
   bug is a trap. There is no `null` and there are no exceptions.
5. **Library, not compiler.** `println`, collections, formatting and the rest are ordinary Avin code in
   the standard library. The compiler knows only the built-in types and a declared set of intrinsics.
6. **Sysl is the system layer.** Avin reaches Sysl — and through Sysl, C — with
   [bindings written in Avin](/spec/sysl-bindings/). The standard library is written in Avin.

## Chapters

| Chapter | Contents |
| --- | --- |
| [The AVIN philosophy](/spec/philosophy/) | Abstraction, Versatility, Intuition and Naturalness: the principles behind the design |
| [Design choices](/spec/design/) | The choices that shape the type system, runtime and library |
| [Types](/spec/types/) | Built-in types, numbers, strings, tuples, functions, `Any`/`Nothing` |
| [Declarations](/spec/declarations/) | `data`, `class`, `trait`, `object`, `impl`, members, modules, initialization |
| [Generics and inference](/spec/generics/) | Reification, bounds, variance, type tests, local inference |
| [Equality, identity and memory](/spec/equality-and-memory/) | `==`, `===`, hashing, display, values vs references, GC |
| [Threads](/spec/threads/) | Starting and joining, sharing, synchronization, the memory model |
| [Actors and state machines](/spec/actors/) | `actor` and `machine` declarations, typed messages and `Sendable`, `tell` and `ask`, supervision, lifecycle, scheduling, states and transitions |
| [Errors](/spec/errors/) | `Result`, `Option`, `?`, traps |
| [Collections](/spec/collections/) | The Iterable hierarchy, arrays, literals, ranges, `for` |
| [Sysl bindings](/spec/sysl-bindings/) | Calling Sysl from Avin |
| [Operators and control flow](/spec/operators-and-control-flow/) | Operators, blocks, `if`, `while`, `match` |
| [Call-by-name](/spec/by-name/) | `value: -> T` parameters |
| [Strings and formatting](/spec/strings-and-formatting/) | Interpolation and format specifiers |
| [System library](/spec/system-library/) | `avin.process` — arguments, environment, exit; `avin.io` — `IoError`, standard input; `avin.fs` — files and directories; `avin.time`; `avin.math` — the elementary functions on `real`; `avin.random` — seeded and platform-seeded pseudo-random numbers; the environment, commands and pipelines, `Source`, `avin.net` (TCP, URLs, HTTP) and futures' additions |
| [Regular expressions](/spec/regex/) | `avin.regex`: the pattern language, leftmost-first backtracking, `Regex`, `Match` and `MatchIterator`, replacing and splitting |
| [Packages](/spec/packages/) | `avin.hocon`, dependencies, version selection, `avin.sum`, workspaces, `avin new` and `avin tidy`, features, members of a repository, vendoring and the standard library's modules |
| [Derivation](/spec/derivation/) | Library-defined `derives`: shapes of data types and classes, `derive` templates, annotations, a JSON library written with them |
| [User interfaces](/spec/ui/) | The `ui` package: syslui's views, controls and signals and syslui-sdl's window and frame loop, bound for Avin; drawing and tapping a tree with no window |
| [Settled decisions](/spec/decisions/) | Every decision made so far, in one list |

The syntax is light and regular: blocks by indentation, functions as `name(params) -> T`, `match`
written after the value with `->` arms, generics as `[T: Bound]`, conformance as `impl Trait for
Type`, and classes, objects and variance in the same style.
