---
title: Language specification
summary: What Avin is — a natively compiled, garbage-collected application language with Sysl's syntax.
weight: 10
---

Avin is a compiled, garbage-collected application language: expression-oriented code, algebraic
data, pattern matching, first-class functions, rich immutable collections, classes and traits with
real subtyping, and type inference, with a deliberately small type system and Sysl's
indentation-based syntax.

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
5. **Library, not compiler.** `print`, collections, formatting and the rest are ordinary Avin code in
   the standard library. The compiler knows only the built-in types and a declared set of intrinsics.
6. **Sysl is the system layer.** Avin reaches Sysl — and through Sysl, C — with
   [bindings written in Avin](/spec/sysl-bindings/). The standard library is written in Avin.

## Chapters

| Chapter | Contents |
| --- | --- |
| [Design choices](/spec/design/) | The choices that shape the type system, runtime and library |
| [Types](/spec/types/) | Built-in types, numbers, strings, tuples, functions, `Any`/`Nothing` |
| [Declarations](/spec/declarations/) | `data`, `class`, `trait`, `object`, `impl`, members, modules, initialization |
| [Generics and inference](/spec/generics/) | Reification, bounds, variance, type tests, local inference |
| [Equality, identity and memory](/spec/equality-and-memory/) | `==`, `===`, hashing, display, values vs references, GC |
| [Errors](/spec/errors/) | `Result`, `Option`, `?`, traps |
| [Collections](/spec/collections/) | The Iterable hierarchy, arrays, literals, ranges, `for` |
| [Sysl bindings](/spec/sysl-bindings/) | Calling Sysl from Avin |
| [Operators and control flow](/spec/operators-and-control-flow/) | Operators, blocks, `if`, `while`, `match` |
| [Call-by-name](/spec/by-name/) | `value: -> T` parameters |
| [Strings and formatting](/spec/strings-and-formatting/) | Interpolation and format specifiers |
| [Settled decisions](/spec/decisions/) | Every decision made so far, in one list |

Syntax follows Sysl wherever Sysl has a form: indentation blocks, `name(params) -> T` functions,
postfix `match` with `->` arms, `[T: Bound]` generics, `impl Trait for Type`. Where Avin needs a form
Sysl lacks (classes, objects, variance), this specification defines it in the same style.
