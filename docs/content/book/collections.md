---
title: Collections
summary: Vectors, maps and sets, `map`, `filter` and `foldLeft`, ranges, comprehensions, and a list that never ends.
weight: 6
---

Most programs work on many values at once. Avin's collections hold them, and their methods do
something with every element in one call.

```text run
main()
  val scores = [72, 95, 88, 61]
  println(scores)
  println(scores.length)
  println(scores(1))
  println(scores.sum)
  println(scores.max)
```

```text output
[72, 95, 88, 61]
4
95
316
95
```

## Vectors

`[72, 95, 88, 61]` is a `Vector[int]`, a sequence of `int`s. `scores(1)` reads the element at index
1, counting from 0. Like data values, a vector never changes: methods that seem to change one return a
new vector, and the old one is still there, sharing most of its insides with the new.

```text run
main()
  val xs = [5, 3, 8, 1]
  println(xs :+ 9)
  println(0 +: xs)
  println(xs ++ [10, 20])
  println(xs.sorted)
  println(xs.reverse)
  println(xs.take(2))
  println(xs.drop(2))
  println(xs.contains(8))
  println(xs.indexOf(8))
  println(xs)
```

```text output
[5, 3, 8, 1, 9]
[0, 5, 3, 8, 1]
[5, 3, 8, 1, 10, 20]
[1, 3, 5, 8]
[1, 8, 3, 5]
[5, 3]
[8, 1]
true
2
[5, 3, 8, 1]
```

`:+` adds at the end and `+:` at the front; the colon is on the side of the collection. A vector may
be empty, but then its element type has to be written, since there is nothing to infer it from:
`Vector.empty[string]()`. `headOption` reads the first element as an `Option`, so it works on an empty
vector too:

```text run
main()
  val names = ["Ada", "Bo"]
  val none = Vector.empty[string]()
  println(names.headOption)
  println(none.headOption)
  println(none.isEmpty)
```

```text output
Some(Ada)
None
true
```

## `map`, `filter` and `foldLeft`

Three methods do most of the work on a collection. `map` makes a new collection by calling a function
on each element. `filter` keeps the elements a test is true for. `foldLeft` combines them all into
one value, starting from a value you give and folding in each element in turn.

```text run
main()
  val xs = [1, 2, 3, 4, 5, 6]
  println(xs.map(x -> x * x))
  println(xs.filter(x -> x % 2 == 0))
  println(xs.foldLeft(0, (total, x) -> total + x))
  println(xs.foldLeft("", (text, x) -> text + x.toString()))
  println(xs.filter(_ % 2 == 1).map(_ * 10))
```

```text output
[1, 4, 9, 16, 25, 36]
[2, 4, 6]
21
123456
[10, 30, 50]
```

`x -> x * x` is a **lambda**, a function written where it is used; [closures](/book/closures/) has
more about them. A lambda that uses its parameter once can be written with `_` in its place:
`_ % 2 == 1` is `x -> x % 2 == 1`.

Many common folds have names of their own:

```text run
main()
  val words = ["pear", "fig", "banana", "kiwi"]
  println(words.count(_.charCount == 4))
  println(words.exists(_.startsWith("b")))
  println(words.forall(_.charCount > 2))
  println(words.find(_.charCount > 4))
  println(words.sortBy(_.charCount))
  println(words.map(_.charCount).sum)
  println(words.mkString(" & "))
```

```text output
2
true
true
Some(banana)
[fig, pear, kiwi, banana]
17
pear & fig & banana & kiwi
```

The [collections](/spec/collections/#methods-every-iterable-has) chapter of the specification lists
every method.

## Ranges and comprehensions

`1 .. 5` is a **range**, the numbers 1 to 5, and `0 ..< 5` the numbers 0 to 4. `by` sets a step. A
range is a collection like any other, so it has `map`, `filter` and the rest, and it holds no
elements in memory: it is only its ends and its step.

```text run
main()
  println((1 .. 5).toVector)
  println((0 ..< 20 by 5).toVector)
  println((10 .. 1 by -3).toVector)
  println((1 .. 100).sum)
  println((1 .. 10).filter(n -> 10 % n == 0).toVector)
```

```text output
[1, 2, 3, 4, 5]
[0, 5, 10, 15]
[10, 7, 4, 1]
5050
[1, 2, 5, 10]
```

A `for` with `yield` is a **comprehension**: instead of running its body for what it does, it
collects the body's values into a vector. Its generators may be followed by `if` conditions, and
several generators nest, the last one changing fastest:

```text run
main()
  val evensSquared = for n in 1 .. 10 if n % 2 == 0 yield n * n
  println(evensSquared)
  val pairs = for
    a in 1 .. 3
    b in a .. 3
  yield (a, b)
  println(pairs)
  val triples = for
    c in 1 .. 20
    b in 1 .. c
    a in 1 .. b
    if a * a + b * b == c * c
  yield (a, b, c)
  println(triples)
```

```text output
[4, 16, 36, 64, 100]
[(1, 1), (1, 2), (1, 3), (2, 2), (2, 3), (3, 3)]
[(3, 4, 5), (6, 8, 10), (5, 12, 13), (9, 12, 15), (8, 15, 17), (12, 16, 20)]
```

See [for comprehensions](/spec/collections/#for-comprehensions) for every form.

## Maps

A `Map` finds a value by its **key**. `{"ada": 36, "bo": 4}` is a `Map[string, int]`. `m(k)` reads
the value for a key, and stops the program if the key is not there; `m.get(k)` answers an `Option`
instead, and `m.getOrElse(k, d)` a default. A map never changes either: `updated` returns a new map
with one more key or a changed value.

```text run
main()
  val ages = {"ada": 36, "bo": 4}
  println(ages("ada"))
  println(ages.get("cy"))
  println(ages.getOrElse("cy", 0))
  val more = ages.updated("cy", 70)
  println(more.size)
  println(ages.size)
  for key in more.keys.sorted
    println(s"$key is ${more(key)}")
```

```text output
36
None
0
3
2
ada is 36
bo is 4
cy is 70
```

A map keeps its keys in no particular order, so the program above sorts them before printing. Counting
is a common use: here a map counts how often each letter appears.

```text run
main()
  var counts = Map.empty[char, int]()
  for c in "mississippi".chars
    counts = counts.updated(c, counts.getOrElse(c, 0) + 1)
  for (letter, n) in counts.toVector.sorted
    println(s"$letter: $n")
```

```text output
i: 4
m: 1
p: 2
s: 4
```

Counting by a key is so common that collections do it in one call. `groupBy` gathers the elements
that share a key, and `groupMapReduce` maps each element to a value and combines the values of each
key:

```text run
main()
  val words = ["apple", "avocado", "banana", "blueberry", "cherry"]
  val byLetter = words.groupBy(w -> w.charAt(0))
  for letter in byLetter.keys.sorted
    println(s"$letter: ${byLetter(letter)}")
  val lengths = words.groupMapReduce(w -> w.charAt(0), w -> w.charCount, (a, b) -> a + b)
  println(lengths.toVector.sorted)
```

```text output
a: [apple, avocado]
b: [banana, blueberry]
c: [cherry]
[(a, 12), (b, 15), (c, 6)]
```

## Sets

A `Set` holds each value at most once, and answers quickly whether it holds one. `{3, 1, 4, 1, 5}` is
a `Set[int]` of four elements. A set is also a function from values to `bool`: `primes(7)` asks
whether 7 is in it.

```text run
main()
  val primes = {2, 3, 5, 7, 11, 13}
  val odds = (1 .. 13 by 2).toSet
  println(primes(7))
  println(primes(9))
  println((primes intersect odds).toVector.sorted)
  println((primes diff odds).toVector.sorted)
  println({3, 1, 4, 1, 5}.size)
```

```text output
true
false
[3, 5, 7, 11, 13]
[2]
4
```

`primes intersect odds` calls the method `intersect` written between its two operands, which reads
well for sets. [Maps and sets](/spec/collections/#maps-and-sets) describes both in full.

## A list that never ends

A `LazyList` computes its elements only when they are first read, and keeps each one once it has. So
it may be endless: `LazyList.from(1)` is every positive number, and nothing is computed until
something asks.

```text run
main()
  val naturals = LazyList.from(1)
  println(naturals.take(5).toVector)
  val squares = naturals.map(n -> n * n)
  println(squares.filter(_ % 7 == 2).take(4).toVector)
```

```text output
[1, 2, 3, 4, 5]
[9, 16, 100, 121]
```

`#::` puts an element in front of a lazy list whose rest is computed only when it is first read. A
list may even be defined by itself, as long as it is a `lazy val`. Each Fibonacci number is the sum
of the two before it, so the list of them is 0, 1, and then the list added to its own tail:

```text run
main()
  lazy val fibs: LazyList[long] = 0 #:: 1 #:: fibs.zip(fibs.tail).map((a, b) -> a + b)
  println(fibs.take(15).toVector)
  println(fibs(90))
```

```text output
[0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377]
2880067194370816120
```

[LazyList](/spec/collections/#lazylist) in the specification explains when its elements are computed.

## Exercises

1. Use a comprehension to make the multiplication table from 1 × 1 to 9 × 9 as a vector of strings,
   one per row.
2. Count the words of a sentence by their length with `groupMapReduce`, and print the lengths in
   order.
3. Make the lazy list of the powers of two, and find the first one with more than 15 digits.
