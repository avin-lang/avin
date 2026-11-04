---
title: Equality, identity and memory
summary: Typed equality, identity only where it exists, Display and Debug, and the garbage-collected heap.
weight: 15
---

## Equality

Every value can be compared with `==` and hashed, so any value can be a key in a `Map` or an
element of a `Set`. `!=` is the negation.

- Built-in types, strings, tuples and `data` types compare and hash by value, structurally.
- Collections compare by contents when their elements do. This includes `Array` and `Buffer`.
- Class instances, `object`s and closures compare and hash by identity, which always agree, unless
  the class implements or derives `Eq`. `class Point(x: int, y: int) derives Eq, Hash` compares and
  hashes by its fields.

`a == b` requires `a` and `b` to have a common type: comparing an `int` with a `string` is a compile
error, not `false`. Where the common type is `Any` or a trait, the comparison is decided at run time
by the values' own types, and values of different types are unequal.

`==` is `Eq`'s `equals(other: Self) -> bool` and the hash is `Hash`'s `hash() -> long`. Every type
conforms to both without declaring it, so `[K: Eq + Hash]` admits any key type, and `x.equals(y)`
and `x.hash()` may be called on any value. A type that implements `Eq` in an impl, or in a class's
header, compares by its own `equals` wherever it is compared: written directly, through a bound, as a
field of a data value or a part of a tuple, and as an `Any`.

Equal values must have equal hashes. `derives Eq, Hash` gives both from the same fields, so they
agree. A class that implements one of `==` and the hash by hand must implement the other, and keeping
the two consistent is then its author's responsibility. `Ord` is never derived implicitly;
`derives Ord` orders lexicographically by field, and by variant declaration order.

## Identity

`a === b` is reference identity. It is defined only on reference types: class instances, `object`s,
arrays, buffers and closures. It is a compile error on a value type, because a value type has no
identity to compare, and on `Any`, which may hold one.

## Display and Debug

Text rendering comes from two traits, as in Sysl. `Display` is the human form, used by `print`,
interpolation and `toString()`. `Debug` is the diagnostic form, used by `debug(x)`, where strings are
quoted and escaped. Both render into a `Writer` with a `FormatSpec`, so formatting a value needs no
intermediate string. `data` types and tuples derive both; classes declare them. Nothing has a default
that prints an address.

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
