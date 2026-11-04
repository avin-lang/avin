---
title: Sysl bindings
summary: Avin calls Sysl through bindings written in Avin; the compiler generates the Sysl side and lets Sysl check it.
weight: 19
---

Avin's standard library is written in Avin. Where Sysl already provides something — files, processes,
time, hashing, text encoding, C libraries through Sysl's own FFI — Avin code reaches it through a
**binding**: an Avin declaration that names a Sysl function and states its signature in Avin types.

```text
module avin.fs

@sysl("sysl.fs.read_text")
private readTextRaw(path: string) -> Result[string, SyslIoError]

readText(path: string) -> Result[string, IoError] =
    readTextRaw(path).mapErr(IoError.fromSysl)
```

A binding is a top-level function declaration with no body. `@sysl` takes one string: the Sysl
function's module path and name. Bindings are usually private, with a public function giving the
Avin-shaped API: camelCase names, Avin error types, Avin collections. A binding is otherwise an
ordinary function: it is called, overloaded, given defaults and passed as a value like any other.

```text
@sysl("sysl.text.to_upper")
upper(s: string) -> string

@sysl("sysl.prints")
write(s: string)
```

A binding whose types do not fit the Sysl function is rejected with Sysl's own message, at the
binding:

```text
error: the Sysl binding `upper` does not match `sysl.text.to_upper`: 's' of 'sysl.text.to_upper' is string, but long was given
```

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
   binding changes. It is built together with the runtime, so the program holds one copy of Sysl's
   library, and what Sysl and Avin each write to standard output stays in order.

The Avin compiler does not need Sysl's compiler as a library. It needs only the `sysl` command at
build time; the produced executable does not.

## Boundary types

Data crosses the boundary in a fixed set of shapes:

| Avin type | Crossing |
| --- | --- |
| `i8`, `i16`, `int`, `long`, `byte`, `real`, `char`, `bool`, `unit` | By value; Sysl's `u8` is `byte` |
| `string` | Copied into a Sysl `string` in each direction |
| `Array[byte]` | Copied into a Sysl `[]u8` in each direction |
| `Option[T]`, `Result[T, E]` of boundary types | Converted variant by variant by the generated shim |
| `data` declared with `@sysl("…")` mirroring a Sysl struct or enum of boundary types | Converted field by field |
| `class` declared with `@sysl("…")` | An **opaque handle**: an Avin object owning one counted Sysl reference, released by `close()` or, as a safety net, a finalizer. Its methods are bindings too. |

These shapes nest: a `Result[Option[string], IoError]` crosses, and so does a mirror whose fields
are `Option`s or other mirrors. No other collection crosses: an `Array` of anything but `byte`, a
`Vector` or a `Map` is converted by the Avin function that wraps the binding.

```text
error: `Vector[string]` cannot cross to Sysl; a binding's parameters and result are integers of 8, 16, 32 or 64 bits, `byte`, `real`, `char`, `bool`, `unit`, `string`, `Array[byte]`, `Option` and `Result` of these, and data types declared with `@sysl` to mirror a Sysl enum or struct
```

**Avin heap references never enter Sysl memory.** A Sysl container cannot hold an Avin object,
because Sysl's reference counting cannot keep an Avin object alive and the collector cannot see Sysl
memory. Avin's own collections are written in Avin, so the standard library has no need for Sysl
containers of Avin values.

## Mirrors

A `data` type declared with `@sysl("module.Name")` **mirrors** a Sysl enum or struct: values of
either convert into the other wherever a binding's types hold one.

```text
@sysl("sysl.fs.IoError")
data IoError
    NotFound
    PermissionDenied
    AlreadyExists
    NotADirectory
    IsADirectory
    DirectoryNotEmpty
    TooManyOpenFiles
    NoSpaceLeft
    Interrupted
    NotOpen
    NotPermitted
    InvalidUtf8(offset: long)
    Other(code: int)

@sysl("sysl.fs.read_text")
readText(file: string) -> Result[string, IoError]
```

- **An enum** is mirrored variant by variant: each Avin variant is the Sysl variant of the same name,
  and its fields are the Sysl variant's payload, in order.
- **A struct** is mirrored by a data type with one variant named as the type, whose fields are the
  struct's fields of the same names: `data LocalDate` with the one variant `LocalDate(day: int)`
  mirrors `sysl.time.LocalDate`.
- **A field is a boundary type.** An integer field takes any Sysl integer, converted with a check in
  each direction: Avin has no unsigned integers, so Sysl's `usize` offset above is a `long`.
- A mirror has no type parameters, and none of its fields holds the mirror itself, directly or
  through an `Option`, a `Result` or another mirror.

Sysl checks a mirror against the type it names when it checks a shim that converts one, so a missing
variant, a field of the wrong type or a name Sysl does not declare is reported at the mirror, in
Sysl's words:

```text
error: the Sysl mirror `IoError` does not match `sysl.fs.IoError`: match on 'IoError' is not exhaustive; missing PermissionDenied, …
```

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
uses, not through bindings. Bindings are the public mechanism, and they are written in Avin. The one
exception is a plain top-level function the runtime offers the library, such as the platform's
sentence for a file-system error: the library binds it by its module and name, `avin.rt.<name>`, like
any Sysl function.
