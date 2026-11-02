---
title: Equality, identity and memory
summary: Typed equality, identity only where it exists, Display and Debug, and the garbage-collected heap.
weight: 15
---

## Equality

`a == b` requires `a` and `b` to have a common type that implements `Eq`. Comparing an `int` with a
`string` is a compile error, not `false`. `!=` is the negation.

- Built-in types, tuples and `data` types are `Eq` when their components are, structurally.
- Collections are `Eq` when their elements are, by contents. This includes `Array` and `Buffer`.
- Classes are `Eq` only if they implement or derive it. There is no inherited default.

`Hash` must agree with `Eq`. Deriving gives both together, and a hand-written `Hash` requires an
`Eq` implementation. `Ord` is never derived implicitly; `derives Ord` orders lexicographically by
field, and by variant declaration order.

## Identity

`a === b` is reference identity. It is defined only on reference types: class instances, `object`s,
arrays, buffers and closures. It is a compile error on a value type, because a value type has no
identity to compare.

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
