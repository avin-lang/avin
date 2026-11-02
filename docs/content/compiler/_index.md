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

`runtime/` is a Sysl package built once per toolchain version, not once per program. It contains:

- program entry, arguments and exit status;
- the collector interface over `sh.sysl.gc` — heap setup and growth, collect-on-pressure, a
  shadow-stack root walk, and finalizers;
- type descriptors: size, trace map, type identity and dispatch tables;
- traps with messages and source locations;
- the string, number-formatting and text primitives the core library builds on.

Its ABI is private between the compiler and the runtime, and not visible to Avin code.

## Standard library

`stdlib/` holds Avin source under the module root `avin`. The compiler knows a small, declared set of
**intrinsics**: arithmetic on built-in types, allocation, field access and the runtime entry points.
Each is declared in Avin source with `@intrinsic("…")` and has no body. Everything else, including
`print`, is ordinary library code.

## Tests

- **Compiler unit tests** use Sysl's runner (`sysl test compiler`), one file per stage or feature.
- **Execution tests** are Avin programs with expected output, compiled and run at `-O0` and `-O2` by
  a Sysl-written test driver inside `sysl test`, so one command runs every test.
- **Library tests** are Avin `@test` functions run by `avin test`, once that runner exists.
- **Semantic references**: where Avin defines arithmetic as Sysl does, a test compares against an
  independently compiled Sysl program.
