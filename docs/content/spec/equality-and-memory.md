---
title: Equality, identity and memory
summary: Typed equality, identity only where it exists, Display and Debug, and the garbage-collected heap.
weight: 15
---

## Equality

Every value can be compared with `==` and hashed, so any value can be a key in a `Map` or an
element of a `Set`. `!=` is the negation.

- Built-in types, strings, tuples and `data` types compare and hash by value, structurally.
- Collections compare by contents when their elements do. This includes `Array` and `Buffer`: two
  arrays are equal when they have the same length and equal elements in order, and `===` compares
  their identities.
- Class instances, `object`s and closures compare and hash by identity, which always agree, unless
  the class implements or derives `Eq`. `class Point(x: int, y: int) derives Eq, Hash` compares and
  hashes by its fields.

`a == b` requires `a` and `b` to have a common type: comparing an `int` with a `string` is a compile
error, not `false`. Where the common type is `Any` or a trait, the comparison is decided at run time
by the values' own types, and values of different types are unequal, except that sequences are
equal when their elements are equal in order whatever their classes (`Any` holding `[1, 2]` equals a
`List(1, 2)`), with hashes that agree. An array compares as a sequence, by its contents, as
elsewhere in Avin.

`==` is `Eq`'s `equals(other: Self) -> bool` and the hash is `Hash`'s `hash() -> long`. Every type
conforms to both without declaring it, so `[K: Eq + Hash]` admits any key type, and `x.equals(y)`
and `x.hash()` may be called on any value. A type that implements `Eq` in an impl, or in a class's
header, compares by its own `equals` wherever it is compared: written directly, through a bound, as a
field of a data value or a part of a tuple, and as an `Any`.

Equal values must have equal hashes. `derives Eq, Hash` gives both from the same fields, so they
agree. A class that implements one of `==` and the hash by hand must implement the other, and keeping
the two consistent is then its author's responsibility. `Ord` is never derived implicitly;
`derives Ord` orders lexicographically by field, and by variant declaration order; its `compare` is
-1, 0 or 1.

A class derives from its constructor parameters, whether plain, `val` or `var`, in the order they are
written; fields its body declares take no part. It derives `Eq` and `Hash` together, and `Ord` only
beside an `Eq` by the same fields. A data type derives from each variant's fields, and already
compares and hashes by them, so `derives Eq, Hash` on it changes nothing. Each field's type must
implement what is derived from it: `class Ranked(p: Plain) derives Eq, Hash, Ord` is an error naming
`p` when `Plain` has no `Ord`. A generic type's derived `Ord`, `Display` and `Debug`, a data type's
or a class's, hold for the type arguments that implement the trait: `data Pair[A, B](a: A, b: B)
derives Display` displays a `Pair[int, string]`, and a `Pair[Plain, int]` is still made but does not
display.

All five derivations are [derive templates](../derivation/#the-built-in-derivations) in the core
library, written against the type's shape as any library's are; what is the language's own is only
when they apply.

## Identity

`a === b` is reference identity, and `!==` its negation. It is defined only where its answer is: at
least one side's type must be a reference type that cannot hold a value type, that is a class (an
`object`, an array and a buffer among them), a function type, or a type parameter bounded by a
class. The other side may then be:

- the same type, or a sub- or superclass of the class;
- a trait the class conforms to: `elements === self`, with `elements: Iterable[T]`, asks whether a
  buffer was given itself;
- an `Any`, or a type parameter with no class bound.

A value type on the other side, boxed as an `Any` or a trait's value, is never the object of a class,
so the answer is `false` whatever the compiler did with the value: `x === obj` with `x: Any` holding
`4` is `false`.

`===` is a compile error on a value type (a built-in type, a string, a tuple, a `data` type, an
`Option` among them), since a value type has no identity to compare. It is also an error when
neither side is such a reference, with both sides traits, `Any`s or type parameters that may hold a
value type: whether two equal values are one object is the compiler's choice, so `f(Tile(2),
Tile(2))` and `f(3, 3)` with `f(a: Shape, b: Shape) = a === b` would answer differently for no
reason the program can see. The message suggests `==`, or a class type for one side.

## Display and Debug

Text rendering comes from two traits. `Display` is the human form, used by `println`,
interpolation and `toString()`. `Debug` is the diagnostic form, used by `debug(x)`, where strings are
quoted and escaped. Both render into a `Writer` with a `FormatSpec`, so formatting a value needs no
intermediate string. `data` types and tuples derive both; classes declare them. Nothing has a default
that prints an address.

A tuple is also an `Ord` where each of its parts is, at every arity: part by part, the first part
that differs deciding, so later parts break ties. `(1, "b") < (2, "a")`, and `(1, 2) < (1, 3)`.
`bool` and `unit` have no order, so a tuple holding one has none.

A `data` type has both without asking, wherever its fields have them: `println(Some(3))` writes
`Some(3)`, and `assertEq(Some(3), Some(3))` needs nothing written. `derives Display, Debug` on a data
type is allowed and means the same. An impl of the type's own replaces the automatic one: `impl
Display for Money` gives `Money` its own `Display` while its `Debug` stays the derived one, and `List`,
`Map` and `Set` keep their literal forms this way. A field without the trait leaves the type without
it rather than being an error: `data Wrapper(p: Plain)` is made, compared and matched, and only a use
that needs its `Display` is rejected, naming the field. A program that never shows a value compiles
none of this code; it makes only each type's shape, before `main`.

A derived `Display` writes the type's or variant's name and its fields in parentheses, separated by
`, `, each in its own `Display` form: `Point(1, 2)`, `Named(ada, Point(1, 2))`. A variant without
fields is its bare name, `Empty`, and so is a class without constructor parameters. A derived `Debug` has the same shape with each field's `Debug`
form, so strings and chars inside are quoted and escaped: `Named("ada", Point(1, 2))`. A field width
in the `FormatSpec` pads the whole rendering; the fields themselves are written without one. Derived
on a generic type, either holds for the type arguments that implement the trait: `data Box[T](item:
T)` displays a `Box[int]`, and a `Box` of a type without `Display` does not display.

A collection displays as its elements in brackets, separated by `, `, each in its own `Display` form:
an `Array[int]` or a `List[int]` as `[1, 2, 3]`, an empty one as `[]`, nested ones as `[[1], [2,
3]]`. Its `Debug` has the same shape with each element's `Debug` form, `["a", "b"]`. A field width
pads the whole. A collection displays only when its elements do.

## Values and references

| Kind | Examples | Identity | Representation |
| --- | --- | --- | --- |
| Value | `int`, `real`, `char`, `bool`, `string`, `unit`, tuples, `data` | No | Chosen by the compiler: in registers or inline, or on the heap for recursive or large `data`. The program cannot tell which. |
| Reference | class instances, `object`s, `Array`, `Buffer`, closures, boxes | Yes | A pointer to a heap object whose header names its exact type |

Because `data` values are immutable and have no identity, sharing, copying, inlining and boxing them
all give the same observable behaviour, so the compiler is free to choose among them.

## The heap

Reference types, and value types the compiler places on the heap, are managed by a precise, nonmoving
tracing collector: Sysl's `sh.sysl.gc`, shared with Slate. There is no manual freeing and no
reference counting in Avin code.

- The compiler emits precise roots for every live reference, including temporaries and captured
  variables, through a shadow stack. A conservative scan is never needed.
- Collection happens at allocation when the heap is under pressure. The heap grows within a
  configured limit.
- Garbage collection is never the only way a scarce resource is released. Files, sockets and Sysl
  handles have an explicit `close()`, and a finalizer is only a safety net.

## Closures

A lambda or local function that captures variables is a heap object holding its environment.
Immutable captures are copied in; a captured `var` is shared, so the closure and the enclosing scope
see the same variable. A closure that does not escape may be allocated on the stack. That is an
optimization with no visible difference.
