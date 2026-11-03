---
title: Roadmap
summary: The stages that build the Avin compiler, runtime and Avin-written standard library to the specification.
weight: 41
---

## Approach

The compiler is built to the [architecture](/compiler/) from the first stage: every program, from
`main() = 42` on, goes through the real pipeline — resolve, check, lower to MIR, emit — and is linked
against the real runtime. Early stages make that pipeline thin and end-to-end; later stages widen
it. No stage builds a temporary path that a later stage has to tear out.

An earlier one-day prototype is archived as `edadma/old-avin`. Its lexer and parser, its
pattern-coverage algorithm, its capture analysis and its execution-test programs are carried over,
each reviewed against the specification on the way in. Nothing else is.

## Stages

Each stage is one or more bounded items. Each ends green on the full gate and is merged to `dev`
with `--ff-only`.

| # | Stage | Delivers |
| --- | --- | --- |
| 0 | **Specification** | This specification, the architecture, this roadmap, the project reset |
| 1 | **Skeleton and test harness** | The `compiler/`, `runtime/` and `library/` packages; the lexer and parser carried over; `avin parse`, `check` and `build`; a thin end-to-end pipeline (`main() = 42` and `print` of a literal) through resolve, check, MIR and the LLVM emitter, linked with a minimal runtime; the Sysl-written execution-test driver in `sysl test` (`.avin` cases with expected output, run at `-O0` and `-O2`) |
| 2 | **Type core and scalar programs** | The `Type` enum over symbols; scopes and symbol tables; local bidirectional inference with subtype constraints and `Nothing`; integers with trapping overflow, `byte`, `real`, `char`, `bool`, `unit`; functions, local functions, `if`/`match`/loops, guards, scalar patterns and coverage; unbounded generic functions; by-name parameters. The old execution corpus, revised to the specification, joins the suite here |
| 3 | **Modules and packages** | `avin.hocon` loading, multi-file modules, imports and visibility, the prelude as an Avin file, `@intrinsic` declarations |
| 4 | **Runtime and heap values** | The collector over `sh.sysl.gc` with shadow-stack roots; type descriptors; traps with messages and locations; `string` as a runtime value; `data` layouts, tuples, lambdas and closures with environments; by-name parameters |
| 5 | **Object model** | `class`, `trait`, `object`, `impl`, methods, `override`, `open`/`abstract`, constructors with checked initialization, companions, member access, type patterns; overloading, default and named arguments |
| 6 | **Monomorphization and dispatch** | Bounded generics, closed-world instantiation, dispatch for generic trait methods, reified headers, variance checking, devirtualization |
| 7 | **Sysl bindings** | `@sysl` declarations, shim generation, the cached shim build, boundary conversions, opaque handles, diagnostics mapped back to bindings |
| 8 | **Core library in Avin** | `Eq`, `Hash`, `Ord`, `Display`, `Debug`, `Writer`, `Option`, `Result`, `?`, string methods, `print`, `debug`, `StringContext` and interpolators, interpolation and formatting |
| 9 | **Collections in Avin** | `Iterable`/`Iterator` (Iterator is Iterable), `Seq`, `List`, `Vector`, `Array`, `Buffer`, `Deque`, `Set`, `Map`, `Range`, builders, views, literals built through them, `::`, list and sequence patterns, `for` comprehensions |
| 10 | **Test runner and tooling** | `avin test`, `avin run`, project and package commands |
| 11 | **System library** | `avin.io`, `avin.fs`, `avin.process`, `avin.time` and the rest over Sysl bindings |

Until stage 8, `print` in stage 1 is an `@intrinsic` over a runtime entry point, declared in Avin
source like every other intrinsic; stage 8 replaces it with the library's `print` over `Display`.

## Status

| Stage | State |
| --- | --- |
| 0 | Done |
| 1 | Done |
| 2 | In progress: local bindings and assignment; integer arithmetic with trapping, wrapping and saturating operators; bitwise operators and shifts; integer literals of any width; `byte`, `real` and `char` with their operators, comparisons and conversions; comparison chains; `&&`, `||`, `!`; `if`, `while`, `break`, `continue` and `return`, with `Nothing` for code that never finishes; `match` and `is` with scalar patterns (literals, ranges, alternatives, bindings, `@`, guards) and exhaustiveness over a pattern matrix; local functions that capture enclosing locals by reference; unbounded generic functions, monomorphized, with type arguments written (or a leading prefix of them) or inferred from the arguments and the expected type, each call's unknowns settled within that call. By-name parameters moved to stage 4, with closures; overloading and default and named arguments to stage 5, with methods; function clauses were built with stage 4's `data` types. Still to come: settling an unknown by a later use in the body, needed by `None` and `[]`, and the old corpus's programs, ported as their features land |
| 3 | Not started |
| 4 | In progress: the collector over `sh.sysl.gc`, with exact roots on a shadow stack, collection when allocation since the last collection reaches what survived it, and a heap limit; type descriptors for `data` variants; `data` types, generic and recursive, with constructors, field access on one-variant types, variant patterns nested to any depth and exhaustiveness over variants; a nested call's unknown settled by the call around it; functions defined by clauses, with inline guards and guard blocks; comparison patterns and `n + k`; exhaustiveness over integers, bytes and chars by the values their literals, ranges, comparisons and `n + k` patterns leave out; tuples, made on the heap like a one-variant `data` value, with parts read by position, tuple patterns, tuples without parentheses where they fill a line, destructuring bindings and multiple assignment. Still to come: `string` as a runtime value, lambdas and closures, by-name parameters, traps with messages and locations |
| 5–11 | Not started |
