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
4. The shim library is cached by a hash of its source, the Sysl version and the
   [Sysl packages](#sysl-packages) it depends on, and rebuilt only when one of them changes. It is built together with the runtime, so the program holds one copy of Sysl's
   library, and what Sysl and Avin each write to standard output stays in order.

The Avin compiler does not need Sysl's compiler as a library. It needs only the `sysl` command at
build time; the produced executable does not.

## Sysl packages

A binding reaches Sysl's standard library, and any Sysl package the package's manifest names in
[`sysl-dependencies`](/spec/packages/#sysl-dependencies). The binding names the package's function
by the module the package declares it in, as Sysl code importing it would:

```hocon
sysl-dependencies {
  digits { path = "../digits" }
}
```

```text
@sysl("avintest.digits.digit_sum")
digitSum(n: int) -> int
```

The shim package depends on each Sysl package every package of the program names, so Sysl checks a
binding against the package's own declaration exactly as it checks one against the standard
library, and a library's bindings build in any program that uses the library. A binding to a module
no named package holds is reported at the binding:

```text
error: the Sysl binding `digitCount` does not match `avintest.digits.digit_count`: no module is called 'avintest.digits.digit_count', and nothing declares it: a binding reaches Sysl's standard library and the Sysl packages the manifest names in `sysl-dependencies`
```

A new release of a Sysl package, a different commit, or a change to the sources of one named by
directory builds the shims again; nothing else about it does.

## Boundary types

Data crosses the boundary in a fixed set of shapes:

| Avin type | Crossing |
| --- | --- |
| `i8`, `i16`, `int`, `long`, `byte`, `real`, `char`, `bool`, `unit` | By value; Sysl's `u8` is `byte`. An integer result, and an integer a result holds, takes [any Sysl integer](#integer-results) |
| `string` | Copied into a Sysl `string` in each direction |
| `Array[byte]` | Copied into a Sysl `[]u8` and, as a binding's own parameter, [copied back](#filled-arrays) after the call; out of a Sysl `[]u8` or `[]const u8` |
| `Array[T]`, `Vector[T]` of any other boundary type | **Copied**, element by element: into a Sysl slice, and out of a Sysl `[]T` or `Buf[T]` into a new array or vector. An array of numbers, booleans or characters as a binding's own parameter is [copied back](#filled-arrays); any other array goes in as a `[]const T` |
| `Option[T]`, `Result[T, E]` of boundary types | Converted variant by variant by the generated shim |
| `data` declared with `@sysl("…")` mirroring a Sysl struct or enum of boundary types | Converted field by field |
| `(T1, T2, …)`, a tuple of boundary types | A Sysl tuple of the same arity, converted part by part in each direction |
| `class` declared with `@sysl("…")` | An **opaque handle**: an Avin object owning one counted Sysl reference, released by `close()` or, as a safety net, a finalizer. Its methods are bindings too. |

These shapes nest: a `Result[Option[string], IoError]` crosses, and so do a
`Result[Vector[string], IoError]`, a `Vector[Vector[string]]`, a `Vector[Array[byte]]`, a
`Vector[(string, string)]`, an `Option[(string, (int, long))]` and a mirror whose fields are
`Option`s, collections, tuples or other mirrors. Sysl's `vars()`, a `Buf[(string, string)]`, is
bound as a `Vector[(string, string)]`, and a Sysl `(usize, u64)` as an `(int, long)`, each part
checked as an integer result is.

### Integer results

A binding's integer result takes any Sysl integer, of any width and either sign, converted with a
check into the integer type the binding declares. Avin has no unsigned integers, so a Sysl `usize`
or `u64` result is bound as a `long`, and a narrower type is fine where the values fit:

```text
@sysl("sysl.math.bigint.BigInt")
class BigInt
    limbs() -> long
```

A value the declared type cannot hold — a `u64` past the largest `long`, an `i64` result bound as
an `int` that does not fit — stops the program where the binding was called, as an Avin conversion
does:

```text
avin: trap: value out of range for long at main.avin:13:13
```

An integer a result holds converts the same way, wherever it is: the value of an `Option`, either
side of a `Result`, the elements of an `Array` or a `Vector`, the parts of a tuple, and these nested in one another. A
Sysl `Option[usize]` is bound as an `Option[long]`, a `Result[u32, E]` as a `Result[int, E]`, and a
slice of `u8` or a `Buf[u16]` as a `Vector[int]`. Each value is checked as it is converted, and the
first that does not fit stops the program at the binding's call. A `byte` is not converted: it is
made from a Sysl `u8` only, in a collection as anywhere.

A value of any other type is not converted to an integer, and the binding is rejected:

```text
error: the Sysl binding `upperCount` does not match `sysl.text.to_upper`: an integer is made from a Sysl integer, but the value is string
```

An integer parameter, and an integer a parameter holds, is passed as the type it is declared, which
Sysl accepts where its own parameter's type holds every value of it: an `i16` reaches an `i32`
parameter, and a `long` does not. An integer in an `Option`, a `Result` or a collection is not
widened: an `Option[int]` reaches an `Option[i32]` parameter only.

### Collections are copies

A collection crosses the boundary **as a copy**, so neither side ever holds the other's memory:

- **Into Sysl**, an `Array[T]` or a `Vector[T]` parameter becomes a fresh Sysl slice of its
  elements, each converted. A vector's slice is a `[]T`, which the Sysl function may take as a `[]T`
  or a `[]const T`: a vector never changes, so what Sysl writes into its copy is never missed. An
  array the Sysl function may fill is [copied back](#filled-arrays) after the call; any other array
  goes in as a `[]const T`, so a Sysl function that would write into it is refused.
- **Out of Sysl**, a slice or a `Buf` of a boundary type becomes a fresh Avin collection. A binding
  usually declares a **`Vector[T]`**, the immutable sequence the standard library's functions answer
  with; it may declare an **`Array[T]`** instead, a new array no one else holds. Which of `[]T` and
  `Buf[T]` the Sysl function returns does not matter: the shim takes either.

```text
@sysl("sysl.fs.entries")
private entries(dir: string) -> Result[Vector[string], SyslIoError]

@sysl("sysl.path.join_all")
private joinAll(parts: Vector[string]) -> string
```

A copy costs time in the length of the collection, which is the right trade for a boundary between
two memory models: a vector a binding answers is an ordinary Avin vector, collected like any other,
and the Sysl buffer it came from is released when the shim returns.

A Sysl function, or a mirrored struct's field, that takes a `Buf[T]` rather than a slice is not
reached through a collection: Sysl's own functions take slices. A collection whose elements do not
fit the Sysl function is reported at the binding, in each direction:

```text
error: the Sysl binding `splitInts` does not match `sysl.text.split`: `Vector[int]` is made from a Sysl `[]I` or `Buf[I]` of any integer type `I`, and the value is none of these
```

No other collection crosses: a `List`, a `Buffer`, a `Map` or a `Set` is converted by the Avin
function that wraps the binding, and an `Array` or a `Vector` crosses only when its elements do.

### Filled arrays

A Sysl function that fills a buffer its caller hands it, as a stream's `read` does, is bound with
an `Array` parameter. An `Array` of `byte`, of an integer type, of `real`, `bool` or `char` that is
the binding's **own parameter** is copied into a slice the Sysl function may write, `[]T`, and when
the function returns, what it left in the slice is copied back into the array, element by element.
The function's result says how much it wrote, and the rest of the array keeps what it held:

```text
@sysl("avintest.digits.fill_digits")
fillDigits(n: long, into: Array[byte]) -> long

main()
    val buffer = Array.fill(6, byte(9))
    println(s"${fillDigits(42, buffer)} $buffer")
```

```text
2 [4, 2, 9, 9, 9, 9]
```

A slice never grows, so the copy back covers the whole array, and an empty array is an empty
slice. A handle's method fills an array as a function does. The array is kept as a root of the
thread's stack from before the call until it is filled, so it lives through a collection that runs
while the call waits or while its result is made, even where nothing else holds it. A function that
takes its slice as a `[]const T` reads the copy, and the copy back leaves the array as it was.

An array anywhere else — of strings, mirrors or other collections, or held in an `Option`, a
`Result`, a collection or a mirror's field — has nothing to write back into, since Sysl's value and
the Avin one are not alike. It crosses as a `[]const T`, a slice Sysl may only read, so a Sysl type
that would let the function write into it is reported at the binding rather than handed a copy
whose writes are lost:

```text
error: the Sysl binding `names` does not match `avintest.digits.name_all`: Sysl may write the elements of its `[]string`, and an `Array` there crosses as a copy it may only read, so what it wrote would never reach the array: an `Array` is written back only as the binding's own parameter, of bytes, integers, reals, booleans or characters
```

The array is copied rather than handed to Sysl as a view of its own elements. The collector never
moves an object, so a view would be sound for the length of the call, but a Sysl slice of memory it
does not own carries no owner, and nothing stops the Sysl function from keeping it: in a structure,
a buffer, another thread. Once the call returns, the collector may free the array under that slice.
A copy keeps the rule below whole, and costs one pass over the array each way, which beside the
system call a buffer is usually filled by is nothing.

```text
error: `Vector[List[int]]` cannot cross to Sysl; a binding's parameters and result are integers of 8, 16, 32 or 64 bits, `byte`, `real`, `char`, `bool`, `unit`, `string`, data types declared with `@sysl` to mirror a Sysl enum or struct, and `Array`, `Vector`, `Option`, `Result` and tuples of these
```

**Avin heap references never enter Sysl memory.** A Sysl container cannot hold an Avin object,
because Sysl's reference counting cannot keep an Avin object alive and the collector cannot see Sysl
memory. That is why a collection crosses as a copy of its elements, never as the Avin array itself,
even an array Sysl [fills](#filled-arrays).
Avin's own collections are written in Avin, so the standard library has no need for Sysl
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
  struct's fields: `data LocalDate` with the one variant `LocalDate(day: int)` mirrors
  `sysl.time.LocalDate`. A field is written in Avin's lowerCamelCase, and the compiler matches it to
  the Sysl field of the same words in snake_case: each uppercase letter becomes an underscore and
  its lowercase, and digits stay, so `restArePositional` is Sysl's `rest_are_positional` and
  `utf8Offset` is `utf8_offset`. A field written with an underscore is an error that names the
  lowerCamelCase spelling, so a field has one spelling. Variants of an enum keep Sysl's names,
  which are PascalCase in both languages, and a variant's fields are matched by position.
- **A field is a boundary type.** An integer field takes any Sysl integer, converted with a check in
  each direction as an [integer result](#integer-results) is: Avin has no unsigned integers, so
  Sysl's `usize` offset above is a `long`, and a value the other side's type cannot hold stops the
  program where the binding was called. A collection field is copied as a binding's collection is.
- A mirror has no type parameters, and none of its fields holds the mirror itself, directly or
  through an `Option`, a `Result`, a collection or another mirror.

Sysl checks a mirror against the type it names when it checks a shim that converts one, so a missing
variant, a field of the wrong type or a name Sysl does not declare is reported at the mirror, in
Sysl's words:

```text
error: the Sysl mirror `IoError` does not match `sysl.fs.IoError`: match on 'IoError' is not exhaustive; missing PermissionDenied, …
```

## Handles

A Sysl value that Avin should hold without seeing inside — a checksum in progress, a mapped file, a
big integer — is reached through a **handle**: a `class` declared with `@sysl` naming the Sysl type.
Its body declares only methods. A method without a body is bound to the Sysl method of the same words
in snake_case (`isEmpty` is `is_empty`), or to the one its own `@sysl("name")` names; a method with a
body is an ordinary method, and may call the bound ones:

```avin
@sysl("sysl.checksum.Crc32")
class Crc32
    update(bytes: Array[byte])
    finish() -> long
    hex() -> string = s"${finish()}"

@sysl("sysl.checksum.new_crc32")
newCrc32() -> Crc32

@sysl("sysl.math.bigint.BigInt")
class BigInt
    sign() -> int
    neg() -> BigInt

@sysl("sysl.math.bigint.parse")
parseBigInt(s: string) -> Option[BigInt]
```

- **Only a binding makes a handle.** A handle has no parameters and no constructor; a binding that
  returns one — directly, in an `Option` or a `Result`, as a part of a tuple, or from a handle's own
  method — makes the
  object, holding one counted reference to the Sysl value it was given (a value is moved into a
  reference of its own; a counted reference is shared).
- **A handle goes back into Sysl only as the receiver of its own methods.** A binding taking a handle
  as a parameter, even inside an `Option`, is an error, and so is a collection of handles or a
  mirror holding one: a handle crosses out of Sysl only as a result, the value of its `Option` or
  `Result`, or a part of a tuple that is one of these, as `accept` answers
  `Result[(Socket, Address), IoError]`. Its Sysl value's fields are not the handle's members.
- **`close()` gives the value back.** Every handle has it. Closing again does nothing; a bound method
  called on a closed handle stops the program at the call:

  ```text
  avin: trap: `Crc32` is closed at main.avin:18:13
  ```

- **A handle never closed is given back by the collector**, when it frees the object, through a
  finalizer the shim generates for the class. When that happens is not specified: a value that holds
  something scarce, such as a file, is closed.
- **A handle is compared by identity**, and has no `Display` of its own.

The bytes a handle's method answers may be a view of the handle's own value; they are copied out
before anything else runs, so the Avin array never reads the Sysl value's memory.

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
