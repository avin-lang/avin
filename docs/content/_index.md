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
    print(shapes.map(area).sum())
```

- [The AVIN philosophy](/spec/philosophy/): Abstraction, Versatility, Intuition and Naturalness
- [The specification](/spec/), starting with its [design choices](/spec/design/)
- [The compiler architecture](/compiler/) and [roadmap](/compiler/roadmap/)
- [Getting started](/getting-started/)

Avin is early: the specification is the design, and the compiler is being built to it.
