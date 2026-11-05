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
| `i8`, `i16`, `int`, `long`, `byte`, `real`, `char`, `bool`, `unit` | By value; Sysl's `u8` is `byte`. An integer result, and an integer a result holds, takes [any Sysl integer](#integer-results); an integer parameter goes into [any](#integer-parameters), and so does a callback's integer answer [too](#callbacks) |
| `string` | Copied into a Sysl `string` in each direction |
| `Array[byte]` | Copied into a Sysl `[]u8` and, as a binding's own parameter, [copied back](#filled-arrays) after the call; out of a Sysl `[]u8` or `[]const u8` |
| `Array[T]`, `Vector[T]` of any other boundary type | **Copied**, element by element: into a Sysl slice, and out of a Sysl `[]T` or `Buf[T]` into a new array or vector. An array of numbers, booleans or characters as a binding's own parameter is [copied back](#filled-arrays); any other array goes in as a `[]const T` |
| `Option[T]`, `Result[T, E]` of boundary types | Converted variant by variant by the generated shim |
| `data` declared with `@sysl("…")` mirroring a Sysl struct or enum of boundary types | Converted field by field |
| `(T1, T2, …)`, a tuple of boundary types | A Sysl tuple of the same arity, converted part by part in each direction |
| `class` declared with `@sysl("…")` | An **opaque handle** on a Sysl struct, trait or instance of a generic type: an Avin object owning one counted Sysl reference, released by `close()` or, as a safety net, a finalizer. Its methods are bindings too. |

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

### Integer parameters

A binding's integer parameter goes into whatever integer the Sysl parameter is, of any width and
either sign, converted with a check, as a [callback's answer](#callbacks) is. Avin has no unsigned
integers, so a Sysl `u32`, `u64` or `usize` parameter takes an Avin `int` or `long`, and a `long`
reaches an `i32` or an `i8` too where its value fits. syslui takes a colour this way:

```text
@sysl("sh.sysl.ui.View")
class View
    background(color: int, radius: int = 0) -> View
```

A value the Sysl type cannot hold — a negative one for a `u32` or a `u64`, one past its largest
value — stops the program where the binding was called, naming the Sysl type by its sign and width.
A handle's method converts its parameters the same way, and a [call that waits](#calls-that-wait)
converts them while its thread is safe for the collector:

```text
avin: trap: value out of range for u32 at main.avin:11:13
```

A `byte` is not converted: it is passed as a Sysl `u8`. A Sysl parameter whose type is a type
parameter of a generic function, `same[T](x: T) -> T`, says no integer to convert into, and is
given the Avin type itself: `T` is then the binding's `int` or `long`. So is every integer parameter
of a binding to a generic function that infers a type parameter from one of them, as syslui's
`slider[T](value: &Signal[T], lo: T, hi: T, …)` does. An integer goes into an integer only:

```text
error: the Sysl binding `joinNumber` does not match `sysl.path.join`: parameter 2 is an integer, which goes into Sysl only as an integer, but the Sysl parameter is string
```

An integer a parameter holds — in an `Option`, a `Result`, a tuple or a collection — is passed as
the type it is declared and is not converted: an `Option[int]` reaches an `Option[i32]` parameter
only, and the diagnostic for any other says why:

```text
error: the Sysl binding `countOrZero` does not match `avintest.digits.count_or_zero`: 'n' of 'digits.avintest.digits.count_or_zero' is Option[i64], but Option[int] was given: an integer inside an `Option`, a `Result`, a tuple or a collection is passed as the type it is declared, and only a binding's own integer parameter is converted into the Sysl parameter's integer
```

A value narrower than 32 bits crosses widened as the C convention widens it, a signed integer
sign-extended and a byte or a bool zero-extended: as a parameter, and as a callback's answer.

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
- **A field may be a [handle](#handles)**, by itself — not in an `Option`, a collection or a tuple.
  Into Sysl it is passed as a handle parameter is, with the same checks: a closed handle, or one
  another thread owns, stops the program where the binding was called. Out of Sysl each such field
  becomes a handle of its own, as a binding's handle result does. Sysl's `sysl.process.Stdio` holds a
  `File` in `ToFile` and `FromFile`, so the ends of a pipe reach a child's streams:

  ```text
  @sysl("sysl.fs.File")
  class File
      closed() -> bool

  @sysl("sysl.process.Stdio")
  data Stdio
      Inherit
      ToFile(file: File)
      ToPath(path: string)
      FromFile(file: File)
      FromPath(path: string)
  ```

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
  returns one — directly, in an `Array`, a `Vector`, an `Option` or a `Result`, as a part of a
  tuple, or from a handle's own method — makes the object, holding one counted reference to the Sysl value it was given (a
  value is moved into a reference of its own; a counted reference is shared). A collection of
  handles comes from a Sysl slice or `Buf` of values or of counted references, and each element is
  a handle of its own, closed and collected on its own.
- **A handle goes back into Sysl by itself**: as the receiver of its own methods, or as a parameter
  of a binding or of another handle's method. A trait is always held by a counted reference, and its
  parameter shares the handle's; any other type is passed as the Sysl parameter takes it — by value,
  a copy; as a pointer to the handle's own value; or, at a `&T`, as the handle's own counted
  reference, so what the Sysl function sets through it the handle reads. A
  [mirror's field](#mirrors) that is a handle is passed the same way, and so is each element of a
  [collection of handles](#collections-of-handles) that is a parameter. A handle inside an
  `Option`, a tuple or any other collection does not go into Sysl, nor is it a mirror's field; a
  tuple holding a handle crosses out of Sysl as a result, the value of its `Option` or `Result`, or
  a part of a tuple that is one of these, as `accept` answers `Result[(Socket, Address), IoError]`,
  and never sits in a collection. Its Sysl value's fields are not the handle's members.

  ```avin
  @sysl("sysl.net.Address")
  class Address
      port() -> int

  @sysl("sysl.net.Socket")
  class Socket
      connect(to: Address) -> Result[unit, IoError]

  @sysl("sysl.net.resolve")
  resolve(host: string, port: int) -> Result[Vector[Address], IoError]

  @sysl("sysl.net.socket")
  socket(forAddress: Address) -> Result[Socket, IoError]
  ```

- **`close()` gives the value back.** Every handle has it. Closing again does nothing; a bound method
  called on a closed handle, or a closed handle passed to Sysl as a parameter or a mirror's field,
  stops the program at the call:

  ```text
  avin: trap: `Crc32` is closed at main.avin:18:13
  ```

- **A handle never closed is given back by the collector**, when it frees the object, through a
  finalizer the shim generates for the class. When that happens is not specified: a value that holds
  something scarce, such as a file, is closed.
- **A handle belongs to one owner at a time.** Its Sysl value is held by a counted reference whose
  count is not atomic, so a handle is never used by two threads at once. Each handle records its
  owner, the code that made it, and every use — a bound method, the handle passed as a parameter or
  a mirror's field, `close()` — compares it with the owner of the code running and stops the program at the call where
  they differ:

  ```text
  avin: trap: `File` belongs to another thread at main.avin:31:5
  ```

  The owner is not an operating system thread but whatever the runtime is running: a thread's body,
  a task on the futures' pool (whichever worker runs it), and, once there are actors, an actor, which
  keeps its handles from one message to the next whichever thread runs it. The check is one compare
  of two words, the handle's owner and the running code's, which the runtime sets when it starts
  running something else.
- **A trap names the handle by its class**, unless the class's `@sysl` gives it another name after
  the Sysl type, `named: "Name"`, a type's name ([proposed](/spec/decisions/#proposed)). A library
  value that keeps a handle only to carry its owner names the handle for itself, so the trap names
  the type its user holds: a started `Child` keeps one declared `@sysl("avin.rt.Token", named:
  "Child")`, and traps with "`Child` belongs to another thread". The name changes nothing else.

  **Ownership moves with what is handed to code that runs elsewhere.** The handles a thread's body or
  a future's body reaches — captured directly, or through any object or collection — that the
  calling code owns become the body's when it is started, and the caller's next use of one traps.
  When the body returns, what it was given and what it returns are **in transit**, as is a value a
  future or promise is completed with and what a callback given to `onComplete` or `map` reaches: a
  handle in transit is taken by the first code to use it. A value sent on a channel keeps its owner.
  How a program hands a handle over explicitly is [proposed](/spec/decisions/#proposed).

  The finalizer never checks the owner: a handle is finalized only once nothing can reach it, so no
  other code can be using it, and the collector, which runs finalizers on whichever thread collects
  while every other is stopped, gives its reference back.
- **A handle is compared by identity**, and has no `Display` of its own.

The bytes a handle's method answers may be a view of the handle's own value; they are copied out
before anything else runs, so the Avin array never reads the Sysl value's memory.

### Handles on traits

A handle may name a Sysl **trait**, and then holds a trait object: whatever value the binding that
made it answered, reached through the trait's method table. Its methods are the trait's, a default
one among them, and a method answering the trait is a handle of its own:

```text
@sysl("syslui.View")
class View
    padding(n: int) -> View

@sysl("syslui.text")
text(s: string) -> View
```

A trait handle is everything a struct handle is: made by a binding's result, alone or in an
`Option`, a `Result`, a tuple or a collection; passed back by itself, as a mirror's field, as a
callback's answer and in a collection; closed; given back by its finalizer; and owned by the code
that made it, its owner checked at every use. Its Sysl parameter takes it as a counted reference,
which the parameter shares, since a trait object is only ever reached through one.

A trait object is two words, its value and its method table, and a handle's object keeps its
reference in one: so a trait handle's reference is kept in a cell of its own, which that word points
at. Every handle's object is the same two words, the reference or the cell and the owner, and a
struct handle is reached as directly as it was; a trait handle's method reads one word more.

### Collections of handles

An `Array` or a `Vector` of handles that is a binding's parameter, or a handle's method's, goes into
Sysl as a slice of the handles' Sysl values, for the call, as `syslui`'s containers take their
children:

```text
@sysl("syslui.column")
column(children: Vector[View], spacing: int) -> View
```

- **Each element is checked as a handle passed by itself is**: one closed, or owned by other code,
  stops the program at the call, naming the handle's class, though the others are the caller's own.
- **Each element is shared for the call**: the slice holds a share of each handle's Sysl value,
  taken before the call, made the elements the Sysl parameter takes — the counted references, or,
  where it asks for them, the values or pointers to them. What Sysl keeps of the slice it
  keeps by its own shares, so a column outlives the vector it was made from, and a child closed
  afterwards. A call that [waits](#calls-that-wait) takes the shares before the thread is safe for the
  collector, and makes its slice from them, so it reads no Avin object.
- **A vector is a `[]T`, an array a `[]const T`**, as for every [collection](#collections-are-copies):
  an array of handles reaches only a Sysl parameter that may not write its elements. A Sysl parameter
  that takes a `Buf` is not reached, as for any collection.
- Only the collection itself is a parameter: a collection of collections or of options of handles, a
  `List` of them, an `Option` holding one and a callback answering one do not go into Sysl, and a
  mirror's field holds a handle only by itself.

A slice or a `Buf` of handles, of trait objects as of struct values, comes back out of Sysl as a
vector or an array of handles, each its own, as every collection does, so a tree built on either
side crosses both ways.

### Handles on generic instances

A handle may name an **instance** of a generic Sysl type, struct or trait, with its type arguments in
brackets after the type's name, each written as Avin writes it:

```text
@sysl("syslui.Signal[string]")
class StringSignal
    get() -> string
    set(v: string)

@sysl("syslui.signal")
stringSignal(v: string) -> StringSignal

@sysl("syslui.text_field")
textField(text: StringSignal, id: int) -> View
```

Its methods are bound at that instance, and it is a handle on that one Sysl type: a generic Sysl
function that answers the instance makes one, and a parameter that takes another instance is
refused when Sysl checks the shim. Each type argument is a boundary type that is one Sysl type —
an integer of 8, 16, 32 or 64 bits, `byte` (Sysl's `u8`), `real`, `char`, `bool`, `unit`, `string`,
or an `Option`, a `Result` or a tuple of these — so the instance is fixed by its name alone:

```text
error: `Vector[int]` cannot be a type argument of the Sysl type a handle is on: each is one Sysl type, an integer of 8, 16, 32 or 64 bits, `byte`, `real`, `char`, `bool`, `unit`, `string`, or an `Option`, a `Result` or a tuple of these
```

Each instance a program binds is its own class, and two classes may name the same instance; one Avin
class for every instance of a generic type is a [generic binding](#generics).

## Calls that wait

A Sysl function that may wait — for a connection, for data on a socket or a pipe, for a process to
end, for time to pass — is bound with **`blocking`** after its name. A handle's method bound by its
own name takes `@sysl(blocking)`:

```text
@sysl("sysl.net.accept", blocking)
private acceptRaw(listener: long) -> Result[long, SyslIoError]

@sysl("sysl.net.Socket")
class Socket
    @sysl(blocking)
    recv(max: long) -> Result[Array[byte], SyslIoError]
    @sysl("send_all", blocking)
    write(bytes: Array[byte]) -> Result[unit, SyslIoError]
```

While a blocking call waits, its thread is **safe for the collector**, as a thread waiting on a
lock, a channel or a join is ([threads](/spec/threads/)): every other thread goes on allocating, and
a collection runs without waiting for this one. A call that is not declared blocking holds up every
collection for as long as it runs, so a binding that can wait for something outside the program is
declared blocking. On a program's only thread, `blocking` costs two stores and two loads.

The shim keeps the Sysl call away from the Avin heap while the thread is safe, because the collector
may then free any object no frame still holds:

- **Every argument is converted before the call**, so the call reads Sysl copies, never an Avin
  object. Since a collection [crosses as a copy](#collections-are-copies), no argument is a
  pointer into the Avin heap.
- **A handle's method takes its own share** of the handle's Sysl value before it waits, so the value
  outlives the call even if the handle is collected, and its finalizer run, meanwhile. A handle
  passed as a parameter is held the same way: its share is taken before the call, and the copy or
  pointer the parameter asks for is made from that share.
- **The result is converted after the call**, once the thread is running again, since converting
  makes Avin objects. Bytes a handle's method answers, which may be a view of the handle's value,
  are copied into Sysl memory first.
- **A [filled array](#filled-arrays) is copied in before the call and back after it.** The Sysl
  function writes its own slice while it waits, the array stays held by the calling thread's stack
  throughout, and what the function left is copied into the array once the thread is running
  again, after the result is converted. A blocking `read(into: Array[byte])` is bound this way.

`blocking` is written alone and once, and only on a function or a handle's method: a type does not
wait.

```text
error: `blocking` is written alone, after the Sysl function's name: `@sysl("sysl.net.accept", blocking)`
```

## Generics

A binding may be generic when its type parameters are instantiated with boundary types. The compiler
generates one shim per instantiation the program uses. A handle on one instance of a generic Sysl
type is a class of its own, [above](#handles-on-generic-instances).

## Callbacks

A binding's parameter may take a **function**. Sysl is handed a closure the shim makes of it, so a
Sysl function taking a `&Fn(A) -> B`, a `&sync Fn(A) -> B` or a bare `A -> B` takes any Avin function
of the matching type: a lambda, a closure capturing locals or objects, a method value, a local or a
top-level function, a function value held in a variable.

```text
@sysl("ui.widgets.on_click")
onClick(button: Button, handler: (Point) -> unit)

@sysl("ui.app.run_every")
runEvery(ms: long, @threaded tick: () -> bool)
```

- **The callback's parameters cross out of Sysl and its result into Sysl**, as a binding's result
  and parameters do: any boundary type, a handle included. An integer Sysl calls it with in a wider
  type is [checked](#integer-results), stopping the program where the binding Sysl is running in
  was called. A function is not a callback's parameter or result, and a binding does not return
  one. A type that does not fit the closure Sysl takes is reported at the binding in Sysl's words:

  ```text
  error: the Sysl binding `wrongParameter` does not match `avintest.callbacks.apply`: the function's parameter 1 is string, but Sysl calls it with int
  ```

- **A callback's integer answer goes into whatever integer the closure answers**, converted with
  the same check. Avin has no unsigned integers, so a Sysl closure answering a `u32`, a `u64` or a
  `usize` takes an Avin function answering a `long` (or an `int`), and one answering an `i8` takes
  a `long` too where its values fit. syslui asks its window's colour this way:

  ```text
  @sysl("sh.sysl.ui_sdl.app")
  app(title: string, tree: () -> View, ground: () -> long) -> App
  ```

  An answer the Sysl type cannot hold — a negative one for a `u32`, one past its largest value —
  stops the program where the binding Sysl is running in was called, naming the Sysl type by its
  sign and width:

  ```text
  avin: trap: value out of range for u32 at main.avin:10:13
  ```

  A closure whose answer is a type parameter of a generic Sysl function, `answer_any[T](f: &Fn() ->
  T) -> T`, says no integer to convert into, and answers the Avin type itself: `T` is then the
  binding's `long` or `int`. An integer answer goes into an integer only:

  ```text
  error: the Sysl binding `integerForText` does not match `avintest.callbacks.text_op`: the function answers an integer, which goes into Sysl only as an integer, but the closure Sysl takes answers string
  ```

- **Sysl keeps the function alive for as long as it holds the closure.** The closure holds a root of
  the collector, made when the binding is called, so a callback Sysl stores and calls later runs
  even after the Avin code that gave it has let go of it. When Sysl drops the closure, its last
  counted reference, the root goes with it, and the function is collected like any other object.
  A closure Sysl only calls during the binding is dropped when the binding returns. Each call of a
  binding makes a closure of its own: the same function passed twice is held twice.
- **A call runs on the thread that makes it**, with that thread's context: the program's arguments
  and results never move between threads. Calls nest to any depth, Avin calling Sysl calling Avin,
  and a callback may call bindings, blocking ones included. A callback Sysl calls while its thread
  waits in a [blocking call](#calls-that-wait) makes the thread running for the call, so it may
  allocate and collect, and safe again after.
- **A callback belongs to the code that gave it**, as a [handle](#handles) does: one called by any
  other — another thread, a task on the futures' pool, a thread Sysl started — stops the program,
  before any of the function runs:

  ```text
  avin: trap: the callback given to `keep` belongs to another thread at main.avin:15:25
  ```

  The place is where the calling code entered Sysl; a thread Sysl started itself has no such place.
- **A parameter marked `@threaded` takes a function any thread may call.** A thread Sysl started,
  which the program never knew, is attached to the runtime for the call and detached after. The
  function is checked where the binding is called: a top-level function, which captures nothing,
  or a lambda, local function or method value whose every capture is a `val` or a parameter of a
  [sendable](/spec/actors/#what-may-be-sent) type or marked `@sendable`. A value of a
  [`@sendable` function type](/spec/types/#function-types), and a parameter marked
  `@sendable`, are taken as they are, since each was checked where it was made or passed to be what
  any thread may run. Any other function value, whose type does not say what it captures, is
  refused, as is a captured `var`, which the other thread would share; a binding with a `@threaded`
  parameter is called by name, never used as a value, so no call escapes the check.
  **A binding with a `@threaded` parameter is `blocking`** whether or not it says so: the Sysl
  call may wait for another thread to call back, and that thread's collections go on without the
  waiting one. A Sysl parameter of type `&sync Fn` needs the mark, and Sysl's refusal says so:

  ```text
  error: the function passed to `f` of `share`, which Sysl may call from any thread, captures `count`, a `var`, which that thread would share with the code here; capture a `val` holding its value
  error: the Sysl binding `shareUnmarked` does not match `avintest.callbacks.share`: Sysl may call the function it is given from another thread, so the parameter that takes it is marked `@threaded`
  ```

- **A trap inside a callback stops the program**, as any trap does, reported at its own place in
  the callback's code, however deep in Sysl it was called.
- `avin.rt.callbacks_held`, a function the runtime offers the library, answers how many functions
  Sysl holds now.

## Relationship to C

A C library is bound directly, by [C bindings](/spec/c-bindings/), which are calls from Avin's own
code rather than through a shim. Until they are built, and wherever Sysl already binds a library
well, a C library reaches Avin through Sysl: Sysl binds the C API with its own FFI, and Avin binds
the Sysl API.

## The runtime is different

The Avin **runtime** — program startup, the collector interface, traps, and the low-level string and
number primitives — is written in Sysl. It is reached through a private ABI that only the compiler
uses, not through bindings. Bindings are the public mechanism, and they are written in Avin. The one
exception is a plain top-level function the runtime offers the library, such as the platform's
sentence for a file-system error: the library binds it by its module and name, `avin.rt.<name>`, like
any Sysl function.
