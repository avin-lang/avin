# Avin

A natively compiled, garbage-collected application language with a light, indentation-based
syntax.

```text
data Shape
    Circle(radius: real)
    Rect(width: real, height: real)

area(shape: Shape) -> real = shape match
    Circle(r) -> 3.14159 * r * r
    Rect(w, h) -> w * h

main() = println([Circle(1.0), Rect(2.0, 3.0)].map(area).sum())
```

Algebraic data, pattern matching, closures, classes and traits, immutable collections and type
inference, with reified, unboxed generics, typed equality, explicit `Option`s, and failures as
values. The compiler is written in Sysl; the standard library is written in Avin.

## The AVIN philosophy

**Abstraction, Versatility, Intuition and Naturalness** guide the design:

- **Abstraction**: express complex ideas simply. Powerful abstractions should simplify code, not
  complicate it.
- **Versatility**: one language, many possibilities. The language should adapt to the problem, not
  the other way around.
- **Intuition**: code should behave as you expect. The obvious way to express something should
  usually be the correct way.
- **Naturalness**: code that flows naturally. Writing code should feel like expressing an idea, not
  negotiating with a compiler.

The whole of it is in [The AVIN philosophy](https://avin-lang.github.io/avin/spec/philosophy/).

## Status

Avin is early. The [specification](https://avin-lang.github.io/avin/spec/) is the design, and the
[roadmap](https://avin-lang.github.io/avin/compiler/roadmap/) tracks the compiler being built to it.
The [examples](examples/) are programs the compiler builds today, and the test suite runs each one.
[Getting started](https://avin-lang.github.io/avin/getting-started/) shows how to build and test
it, and [editor support](https://avin-lang.github.io/avin/getting-started/editors/) covers VS Code
and the language server. The documentation's sources are in [docs/](docs/).
