---
title: Avin
heroTitle: Expressive, typed, compiled natively
heroHighlight: in clean, indented code
summary: A natively compiled, garbage-collected language with a small, expressive type system and a light, indentation-based syntax.
---

Avin is for writing applications: algebraic data, pattern matching, closures, classes and traits
with subtyping, immutable collections and type inference, compiled to native code. Its syntax is
light: blocks are marked by indentation, not braces, and most declarations fit on one line. Its generics are
reified and unboxed, absence is an explicit `Option`, equality is typed, `string` is a built-in
UTF-8 type, and failures are values.

```text
module demo

data Shape
    Circle(radius: real)
    Rect(width: real, height: real)

area(shape: Shape) -> real = shape match
    Circle(r) -> 3.14159 * r * r
    Rect(w, h) -> w * h

main()
    val shapes = [Circle(1.0), Rect(2.0, 3.0)]
    println(shapes.map(area).sum)
```

A list may be endless. Here every prime there is, in one lazy list: `#::` puts a prime in front of a
rest that is evaluated only when it is first read, so `sieve` takes one step for each prime the
program asks for, and keeps every prime it has found.

```text run
sieve(s: LazyList[int]) -> LazyList[int] =
    s.head #:: sieve(s.tail.filter(_ % s.head != 0))

main()
    val primes = sieve(LazyList.from(2))
    println(primes.take(10).toVector)
    println(primes(99))
```

```text output
[2, 3, 5, 7, 11, 13, 17, 19, 23, 29]
541
```

- [The AVIN philosophy](/spec/philosophy/): Abstraction, Versatility, Intuition and Naturalness
- [The specification](/spec/), starting with its [design choices](/spec/design/)
- [The compiler architecture](/compiler/) and [roadmap](/compiler/roadmap/)
- [Getting started](/getting-started/)

Avin is early: the specification is the design, and the compiler is being built to it.
