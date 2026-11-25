---
title: Data
summary: "Your own types with `data`, taking them apart with patterns, `Option`, and failures as values with `Result`."
weight: 5
---

The built-in types are numbers, text and truth values. A program about shapes, cards or orders wants
types of its own, and `data` declares one.

```text run
data Point(x: int, y: int)

main()
  val p = Point(3, 4)
  println(p)
  println(p.x + p.y)
  println(p == Point(3, 4))
```

```text output
Point(3, 4)
7
true
```

`data Point(x: int, y: int)` declares a type `Point` whose values each hold two fields. `Point(3, 4)`
makes one, `p.x` reads a field, and two points are equal when their fields are. A data value prints
as it would be written. The [data](/spec/declarations/#data) section of the specification has the
whole story.

## Values never change

A data value is **immutable**: its fields keep the values it was made with. To get a point with a
different `y`, make a new one; `with` copies a value with the fields you name replaced:

```text run
data Point(x: int, y: int)

data Player(name: string, at: Point, score: int)

main()
  val start = Player("Ada", Point(0, 0), 0)
  val moved = start with {at.x = 5, score = 10}
  println(start)
  println(moved)
```

```text output
Player(Ada, Point(0, 0), 0)
Player(Ada, Point(5, 0), 10)
```

`at.x` reaches a field inside a field. `start` is just as it was: nothing that holds it can see it
change, which is what makes immutable values safe to share.

## One type, several shapes

A data type can have several **variants**, each with its own fields. A shape is a circle or a
rectangle or a triangle:

```text run
import avin.math

data Shape
  Circle(radius: real)
  Rect(width: real, height: real)
  Triangle(a: real, b: real, c: real)

area(s: Shape) -> real
area(Circle(r)) = math.Pi * r * r
area(Rect(w, h)) = w * h
area(Triangle(a, b, c)) =
  val p = (a + b + c) / 2.0
  math.sqrt(p * (p - a) * (p - b) * (p - c))

main()
  val shapes = [Circle(1.0), Rect(2.0, 3.0), Triangle(3.0, 4.0, 5.0)]
  for s in shapes
    println(f"$s has area ${area(s)}%.3f")
```

```text output
Circle(1) has area 3.142
Rect(2, 3) has area 6.000
Triangle(3, 4, 5) has area 6.000
```

The patterns `Circle(r)` and `Rect(w, h)` test which variant a shape is and name its fields at the
same time. A `match` takes data apart the same way. The compiler knows every variant of `Shape`, so
it checks that every one is handled: add a `Square` variant, and each function that forgets it is an
error until it learns how.

A variant may hold values of its own type, which makes a **recursive** type. A binary search tree is
either empty or a node with a value and two smaller trees, every value on its left less than its own
and every one on its right greater:

```text run
data Tree
  Leaf
  Node(left: Tree, value: int, right: Tree)

insert(t: Tree, v: int) -> Tree = t match
  Leaf -> Node(Leaf, v, Leaf)
  Node(l, x, r) if v < x -> Node(insert(l, v), x, r)
  Node(l, x, r) if v > x -> Node(l, x, insert(r, v))
  _ -> t

contains(t: Tree, v: int) -> bool = t match
  Leaf -> false
  Node(l, x, r) -> if v < x then contains(l, v) elif v > x then contains(r, v) else true

inOrder(t: Tree) -> Vector[int] = t match
  Leaf -> []
  Node(l, x, r) -> inOrder(l) ++ [x] ++ inOrder(r)

depth(t: Tree) -> int = t match
  Leaf -> 0
  Node(l, _, r) -> 1 + max(depth(l), depth(r))

main()
  var tree = Leaf
  for v in [50, 30, 70, 20, 40, 60, 80, 30]
    tree = insert(tree, v)
  println(inOrder(tree))
  println(contains(tree, 60))
  println(contains(tree, 65))
  println(depth(tree))
```

```text output
[20, 30, 40, 50, 60, 70, 80]
true
false
3
```

`Leaf` has no fields, so it is a value by itself. Inserting never changes a tree: it builds a new one
that shares every branch it did not touch with the old.

## `Option`: a value that may be missing

Avin has no null. A value that might not be there has the type `Option[T]`: either `Some(value)` or
`None`. The type says so, so a caller cannot forget to ask.

```text run
safeDivide(a: int, b: int) -> Option[int] = if b == 0 then None else Some(a / b)

main()
  println(safeDivide(10, 2))
  println(safeDivide(1, 0))
  println(safeDivide(1, 0).getOrElse(-1))
  println(safeDivide(9, 3).map(n -> n * 10))
  safeDivide(7, 0) match
    Some(n) -> println(s"the answer is $n")
    None -> println("there is no answer")
```

```text output
Some(5)
None
-1
Some(30)
there is no answer
```

`getOrElse` gives a default for `None`, `map` changes a value that is there and leaves `None` alone,
and a `match` handles both cases by hand. `Option` is an ordinary data type of the library, declared
just as `Shape` is.

## `Result`: failures are values

Avin has no exceptions. A function that can fail says so in its type, `Result[T, E]`: either `Ok` of
the answer or `Err` of what went wrong. Here a failure is a `ParseError`, a data type with a line
number and a reason. `impl Display for ParseError` says how one is written as text; chapter 8 explains
`impl`.

```text run
data ParseError(line: int, reason: string)

impl Display for ParseError
  display(out: Writer, spec: FormatSpec) = out.write(s"line $line: $reason")

parseAge(line: int, text: string) -> Result[int, ParseError] = text.toIntOption() match
  Some(n) if 0 <= n <= 150 -> Ok(n)
  Some(n) -> Err(ParseError(line, s"$n is not an age"))
  None -> Err(ParseError(line, s"'$text' is not a number"))

main()
  println(parseAge(1, "36"))
  println(parseAge(2, "abc"))
  parseAge(3, "999") match
    Ok(age) -> println(s"age $age")
    Err(e) -> println(s"error: $e")
```

```text output
Ok(36)
Err(line 2: 'abc' is not a number)
error: line 3: 999 is not an age
```

Checking every `Result` by hand would be tiring, so `?` does it: `expr?` is the value inside an `Ok`,
and on an `Err` it returns that `Err` from the function at once. A sum of ages either adds them all up
or stops at the first that fails:

```text run
data ParseError(line: int, reason: string)

impl Display for ParseError
  display(out: Writer, spec: FormatSpec) = out.write(s"line $line: $reason")

parseAge(line: int, text: string) -> Result[int, ParseError] = text.toIntOption() match
  Some(n) if 0 <= n <= 150 -> Ok(n)
  Some(n) -> Err(ParseError(line, s"$n is not an age"))
  None -> Err(ParseError(line, s"'$text' is not a number"))

totalAge(texts: Vector[string]) -> Result[int, ParseError]
  var total = 0
  for (text, i) in texts.zipWithIndex
    total += parseAge(i + 1, text)?
  Ok(total)

main()
  println(totalAge(["30", "41", "12"]))
  println(totalAge(["30", "old", "500"]))
```

```text output
Ok(83)
Err(line 2: 'old' is not a number)
```

`zipWithIndex` pairs each element with its position, counting from 0. A `Result` cannot be ignored:
calling a function that returns one and dropping the answer is an error, so a failure never passes
unseen. `main` itself may return a `Result`, so `?` works there too; an `Err` that reaches the end of
`main` is written to standard error, and the program exits with status 1.

```text run
main() -> Result[unit, string]
  val n = "12".toIntOption().okOr("not a number")?
  println(n * 2)
  Ok(())
```

```text output
24
```

`okOr` turns an `Option` into a `Result`, with the error to use for `None`. The
[errors](/spec/errors/) chapter of the specification explains `?` and `Result` in full, and when a
bug should stop a program instead.

## Exercises

1. Add a `Square(side: real)` variant to `Shape` and see what the compiler says about `area` before
   you teach it.
2. Write `minimum(t: Tree) -> Option[int]`, the smallest value in a tree, or `None` for an empty one.
3. Write `parsePoint(text: string) -> Result[Point, string]` that reads text like `"3,4"`, with a
   helpful error for anything else.
