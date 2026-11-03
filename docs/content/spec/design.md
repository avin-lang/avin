---
title: Design choices
summary: The choices that shape Avin's type system, runtime and library, and what each one means for a program.
weight: 11
---

Avin compiles whole programs to native code and owns its runtime, so its types, values and library
are designed together. These are the choices that shape it, each with what it means for a program.

| Choice | What it means |
| --- | --- |
| **Types are real at run time** | Every generic is monomorphized, and every heap object's header names its exact type, type arguments included. A type pattern on a generic type is checked and sound, and generic code creates an `Array[T]` as easily as any other value. |
| **Values stay unboxed** | `List[int]` stores `int`s directly. A value is boxed only where it becomes a trait or `Any` value, at that one site. |
| **Absence is explicit** | A value that may be missing has type `Option[T]`. Every reference refers to an object, and every field is initialized before the object can be observed. |
| **Equality is typed** | `==` compares two values of one type with an `Eq` implementation. `data` types derive structural `Eq` and `Hash`; classes declare the equality they mean. Identity is the separate `===`, for types that have identity. |
| **Printing is a trait** | A type is printable because it implements `Display` and `Debug`; `data` derives both. Output always shows the value. |
| **`string` is built in** | `string` is a type of the language: UTF-8, immutable, and named simply `string`. Its methods live in Avin's own core library. |
| **`char` is a Unicode scalar value** | One `char` is one character of text, whatever its code point. |
| **`Array[T]` is an ordinary collection** | Invariant, contiguous, fixed length and mutable; elements are stored unboxed; `==` compares contents, and printing shows them. |
| **Integers keep their width, and overflow traps** | Fixed-width integers keep their width through arithmetic (`i8 + i8` is `i8`), and a result that does not fit stops the program. Wrapping and saturating arithmetic are written with their own operators. |
| **Binary data is `byte`** | Integers are signed and are for counting; `byte` is the octet that files, sockets and encodings are made of. |
| **Failures are values; bugs are traps** | A failure a correct caller can meet is a `Result` or `Option`, propagated with `?`. A bug stops the program with a trap. |
| **Overloading is resolved statically** | Functions and methods may share a name when their parameter types differ. The compiler chooses the most specific overload from the arguments at compile time, and an ambiguous call is an error rather than a guess. |
| **`object` holds module-wide state** | An `object` is an ordinary value, and the one place for state and functions that belong to a type rather than an instance. |
| **Initialization is checked** | A constructor assigns every field before `self` is used. Traits carry behaviour, not state. Module-level values and `object`s initialize before `main`, in dependency order, and the compiler rejects a cycle. |
| **Objects are just data** | An object's header holds its type and what the collector needs, nothing more: there is no lock in every object. Threads share the heap; synchronization is library code, and actors are built into the language. |
| **Tuples and functions of any arity** | A tuple or function type may have as many elements or parameters as the program needs. |
| **`unit` is an ordinary type** | It has one value, `()`, and works as a type argument at no cost. |
| **One top type, `Any`** | Every type conforms to `Any`; value types reach it by boxing at that one site. |
| **`data` is pure value** | A `data` value has no identity, so the compiler chooses to store it inline or on the heap, and the program sees the same behaviour either way. |
| **Conversions are written** | `+` on strings joins two strings; numeric conversions are explicit (`long(x)`); interpolation formats any value. |
| **Generic programming at compile time** | Derivation (`derives`) and reified type tests give generic code what it needs, checked by the compiler. |
| **Whole-program compilation** | The program, its libraries and the standard library are compiled together. That is what makes reification, monomorphized generic methods and devirtualization possible. |
