---
title: Sysl bindings
summary: Avin calls Sysl through bindings written in Avin; the compiler generates the Sysl side and lets Sysl check it.
weight: 18
---

Avin's standard library is written in Avin. Where Sysl already provides something — files, processes,
time, hashing, text encoding, C libraries through Sysl's own FFI — Avin code reaches it through a
**binding**: an Avin declaration that names a Sysl function and states its signature in Avin types.

```text
module avin.fs

@sysl("sysl.fs.read_text")
private readTextRaw(path: string) -> Result[string, SyslError]

readText(path: string) -> Result[string, IoError] =
    readTextRaw(path).mapErr(IoError.fromSysl)
```

A binding is a function declaration with no body. Bindings are usually private, with a public
function giving the Avin-shaped API: camelCase names, Avin error types, Avin collections.

## How a binding is compiled

1. The Avin compiler collects every binding the program reaches.
2. It generates a Sysl **shim** module: one exported function per binding, with a fixed boundary
   signature. Each shim converts its arguments, calls the named Sysl function, and converts the
   result back.
3. It builds the shim with `sysl build-c`. **Sysl type-checks each shim against the real
   declaration**, so a wrong binding or a Sysl signature change fails the build with a Sysl
   diagnostic that the Avin compiler maps back to the binding. Avin never guesses a mangled name or a
   Sysl memory layout.
4. The shim library is cached by a hash of its source and the Sysl version, and rebuilt only when a
   binding changes.

The Avin compiler does not need Sysl's compiler as a library. It needs only the `sysl` command at
build time; the produced executable does not.

## Boundary types

Data crosses the boundary in a fixed set of shapes:

| Avin type | Crossing |
| --- | --- |
| `iN`, `real`, `char`, `bool`, `unit` | By value |
| `string` | Copied into a Sysl `string` in each direction |
| `Array[byte]` | Copied |
| `Option[T]`, `Result[T, E]` of boundary types | Converted variant by variant by the generated shim |
| `data` declared with `@sysl("…")` mirroring a Sysl struct or enum of boundary types | Converted field by field |
| `class` declared with `@sysl("…")` | An **opaque handle**: an Avin object owning one counted Sysl reference, released by `close()` or, as a safety net, a finalizer. Its methods are bindings too. |

**Avin heap references never enter Sysl memory.** A Sysl container cannot hold an Avin object,
because Sysl's reference counting cannot keep an Avin object alive and the collector cannot see Sysl
memory. Avin's own collections are written in Avin, so the standard library has no need for Sysl
containers of Avin values.

## Generics

A binding may be generic when its type parameters are instantiated with boundary types. The compiler
generates one shim per instantiation the program uses.

## Callbacks

Passing an Avin function to Sysl is deferred. It needs a rooted handle and a generated trampoline,
and will be specified when a library needs it.

## Relationship to C

Public C interoperability is deferred. In the meantime, a C library reaches Avin through Sysl: Sysl
binds the C API with its own FFI, and Avin binds the Sysl API.

## The runtime is different

The Avin **runtime** — program startup, the collector interface, traps, and the low-level string and
number primitives — is written in Sysl. It is reached through a private ABI that only the compiler
uses, not through bindings. Bindings are the public mechanism, and they are written in Avin.
