---
title: Compiler architecture
summary: The target pipeline of the Sysl-written Avin compiler, its runtime, and how Sysl bindings are built.
weight: 40
---

The compiler is written in Sysl, as the package in `compiler/` with modules under
`dev.avinlang.compiler`. It compiles a whole program — the application, its Avin dependencies and
the standard library — in one run.

## Pipeline

| Stage | Input → output | Responsibility |
| --- | --- | --- |
| Lex and parse | source → AST | Indentation layout and grammar, on `sh.sysl.parsing`. Keeps spans; does no name lookup. |
| Load | `avin.hocon` → module graph | Packages, modules and files; the reserved `avin` root; the prelude, which is an Avin file. |
| Resolve | AST → resolved AST | Scopes and symbol tables. Every name is bound to a declaration symbol, and imports, visibility and shadowing are decided here, once. |
| Check | resolved AST → typed HIR | A `Type` algebra over declaration symbols; local bidirectional inference with subtype constraints; bounds, variance and `impl` coherence; initialization; pattern exhaustiveness. |
| Bindings | `@sysl` declarations → shim | Generates the Sysl shim source and its boundary signatures. |
| Monomorphize | typed HIR → mono HIR | Instantiates functions, types and generic trait methods for every use, and computes the dispatch for trait-typed calls over the closed world. |
| Lower | mono HIR → MIR | Explicit control flow, object layouts and headers, type descriptors, vtables, closures and environments, allocation, GC root slots, traps. |
| Emit | MIR → LLVM IR | Spelling only: no typing, layout or semantic decisions. |
| Link | `.ll` + runtime + shim → executable | clang; the runtime library; the cached shim library. |

Each stage has its own data structures and its own tests. No stage reaches back into an earlier stage's
input. **Types are an enum over symbols, never strings**: built-in types, nominal types, functions,
tuples, type parameters, inference variables, `Any`, `Nothing` and an error type.

## Runtime

`runtime/` is a Sysl package, built into a library archive when its sources change rather than once
per program, and linked into every executable. It contains:

- program entry, arguments and exit status;
- the collector interface over `sh.sysl.gc` — heap setup and growth, collect-on-pressure, a
  shadow-stack root walk, and finalizers;
- type descriptors: size, trace map, type identity and dispatch tables;
- traps with messages and source locations;
- the string, number-formatting and text primitives the core library builds on.

Its ABI is private between the compiler and the runtime, and not visible to Avin code.

The heap is one block reserved when the program starts: 8 GiB, or `AVIN_HEAP_LIMIT` bytes. The
operating system commits its pages as they are first used, so the heap grows within the reservation
and costs nothing until then. A program whose live data outgrows the limit stops with a message
naming it. Every object's first word is its descriptor; constant objects (a variant with no fields)
live outside the heap, and the collector leaves them alone. `AVIN_GC_STRESS=1` makes every
allocation collect first, and the execution tests run that way, so a reference the compiler failed
to root is found at the first allocation that could lose it.

Programs are multi-threaded, so the runtime keeps no per-thread state in globals. Each thread has a
**context**, and every compiled function, closure entry and runtime entry point takes the running
thread's context as a hidden first argument. A function that holds references links its frame of
roots onto its thread's shadow stack by writing the context's first word directly, on entry and on
every return, without calling the runtime. A collection walks the frames of every attached
context; the main thread's is attached before the program's entry function runs.

## Standard library

`library/` holds Avin source under the module root `avin`, starting with the prelude every program
sees. The compiler knows a small, declared set of
**intrinsics**: arithmetic on built-in types, allocation, field access and the runtime entry points.
Each is declared in Avin source with `@intrinsic("…")` and has no body. Everything else, including
`print`, is ordinary library code.

## Tests

`sysl test compiler`, run from the repository root, runs every test.

- **Compiler unit tests** use Sysl's runner, one file per stage or feature. Parser tests compare a
  compact rendering of the tree: `1 + 2 * 3` shows as `(+ 1 (* 2 3))`.
- **Runtime tests** are the runtime package's own Sysl tests, which the compiler's tests run.
- **Execution tests** are Avin programs in `tests/run/`, compiled and run at `-O0` and `-O2`. A
  program's leading `// expect: exit N` or `// expect: trap` says how it ends (exit 0 when it says
  neither), and `NAME.out` and `NAME.err` beside it hold the output it must write.
- **Rejection tests** are Avin programs in `tests/reject/` that must fail to compile; each
  `// error: TEXT` line names text the diagnostics must contain.
- **Library tests** are Avin `@test` functions run by `avin test`, once that runner exists.
- **Semantic references**: where Avin defines arithmetic as Sysl does, a test compares against an
  independently compiled Sysl program.
