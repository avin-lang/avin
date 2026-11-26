---
title: Closures and higher-order functions
summary: Functions as values, functions that take and make functions, and closures that remember.
weight: 7
---

In Avin a function is a value, like a number or a string. It can be given a name, passed to another
function, returned from one, and kept in a collection.

```text run
twice(f: (int) -> int, x: int) -> int = f(f(x))

addThree(n: int) = n + 3

main()
  println(twice(addThree, 10))
  println(twice(n -> n * n, 3))
```

```text output
16
81
```

`(int) -> int` is the type of a function that takes an `int` and yields an `int`. `twice` takes one
and calls it two times. It can be given a function declared with a name, `addThree`, or a **lambda**,
`n -> n * n`, written where it is needed. A function that takes or returns functions is called
**higher-order**; `map` and `filter` from the last chapter are higher-order functions.

## Lambdas

A lambda is its parameters, `->` and its body. Its parameter types are usually inferred from where it
is used; where they cannot be, they are written as in a function. Several parameters go in
parentheses, and a body of several lines is an indented block.

```text run
main()
  val double = (x: int) -> x * 2
  val add = (a: int, b: int) -> a + b
  val describe = (name: string, n: int) ->
    val plural = if n == 1 then "" else "s"
    s"$n $name$plural"
  println(double(21))
  println(add(2, 3))
  println(describe("cat", 1))
  println(describe("dog", 3))
```

```text output
42
5
1 cat
3 dogs
```

A lambda that uses each parameter once, in order, can use `_` for them: `_ + 3` is `x -> x + 3` and
`_ * _` is `(a, b) -> a * b`. The [function types](/spec/types/#function-types) section of the
specification has the rules, and the [placeholder](/spec/types/#placeholder-lambdas) form.

```text run
main()
  val xs = [4, 1, 3]
  println(xs.map(_ + 3))
  println(xs.foldLeft(1, _ * _))
  println(xs.sortBy(-_))
```

```text output
[7, 4, 6]
12
[4, 3, 1]
```

## Functions that make functions

A function can return a function. `adder(5)` returns a function that adds 5 to whatever it is given;
the returned function remembers the `n` it was made with.

```text run
adder(n: int) -> (int) -> int = x -> x + n

compose[A, B, C](f: (A) -> B, g: (B) -> C) -> (A) -> C = x -> g(f(x))

main()
  val addFive = adder(5)
  val addTen = adder(10)
  println(addFive(1))
  println(addTen(1))
  val shout = compose((s: string) -> s.toUpper, s -> s + "!")
  println(shout("hello"))
  val pipeline = ((x: int) -> x * 2).andThen(addFive).andThen(_.toString() + " apples")
  println(pipeline(10))
```

```text output
6
11
HELLO!
25 apples
```

`compose` is **generic**: `[A, B, C]` are type parameters, so it joins functions of any types where
the result of the first is what the second takes. Each call works out `A`, `B` and `C` from its
arguments. `f.andThen(g)` is the library's own way of writing `compose(f, g)`.

## Closures remember

A lambda that uses a name from around it is a **closure**: it captures the name itself, not a copy of
its value. When the name is a `var`, the closure can change it, and the change outlives the call that
made the closure. Each call of `counter` makes a fresh `count`, so each counter counts on its own:

```text run
counter() -> () -> int
  var count = 0
  () ->
    count += 1
    count

main()
  val a = counter()
  val b = counter()
  a()
  a()
  println(a())
  println(b())
```

```text output
3
1
```

`() -> int` is the type of a function that takes nothing and yields an `int`. Closures are how a
small piece of state is kept private: nothing but the function `counter` returned can reach its
`count`.

## Generic higher-order functions

A higher-order function with type parameters works for every type. `iterate` applies a function `n`
times to a starting value, whatever the type of that value:

```text run
iterate[T](n: int, f: (T) -> T, start: T) -> T
  var value = start
  for _ in 1 .. n do value = f(value)
  value

main()
  println(iterate(10, (s: string) -> s + "*", ""))
  println(iterate(5, (x: long) -> x * 3, 1))
  println(iterate(3, (v: Vector[int]) -> v ++ v, [1, 2]))
```

```text output
**********
243
[1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2]
```

## Functions in collections

Since functions are values, a collection can hold them. Here a table of named operations drives a
tiny calculator:

```text run
main()
  val operations: Vector[(string, (int, int) -> int)] = [
    ("plus", (a, b) -> a + b),
    ("minus", (a, b) -> a - b),
    ("times", (a, b) -> a * b),
    ("max", (a, b) -> max(a, b))
  ]
  for (name, op) in operations
    println(s"6 $name 4 = ${op(6, 4)}")
```

```text output
6 plus 4 = 10
6 minus 4 = 2
6 times 4 = 24
6 max 4 = 6
```

## Exercises

1. Write `applyAll(fs: Vector[(int) -> int], x: int) -> Vector[int]` that calls each function on
   `x`.
2. Write `memoize(f: (int) -> int) -> (int) -> int` that remembers the answers it has computed in a
   `Map` held by a `var` the closure captures.
3. Use `compose` to build a function that trims a string, makes it lower case, and counts its
   characters.
