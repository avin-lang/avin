---
title: Collections
summary: A rich collection library without higher-kinded types, where Iterator is Iterable and arrays are ordinary collections.
weight: 18
---

The standard library's collections are a rich, immutable-by-default family with shared operation
families, written in Avin. Each transformation returns the most precise collection type the caller
can know, and the structure below achieves that without higher-kinded types, which Avin does not
have.

## Hierarchy

```text
Iterable[+A]                 iterator(), and the operation families below
├── Iterator[+A]             single-pass; iterator() returns itself
├── Seq[+A]                  ordered, (Int) -> A
│   ├── List[+A]             immutable linked list
│   ├── Vector[+A]           immutable, persistent tree; the default sequence
│   ├── ArraySeq[+A]         immutable, one array; built once, then read
│   ├── Range[A]             immutable arithmetic progression
│   ├── Array[A]             mutable, fixed length, contiguous
│   ├── Buffer[A]            mutable, growable at the end, contiguous
│   └── Deque[A]             mutable, growable at both ends
├── Set[A]                   immutable, by hash
├── TreeSet[A]               immutable, sorted by Ord
├── Map[K, +V]               immutable, by hash
└── TreeMap[K, +V]           immutable, sorted by Ord
```

`[]T` is shorthand for `Vector[T]` in any type position (`xs: []int`, `[][]int`); see
[types](/spec/types/).

The mutable `Set` and `Map` variants live in `avin.collections.mutable`. `Buffer` is the one
growable mutable sequence; code that adds and removes at both ends uses a `Deque`.

**Iterator is Iterable.** Asking an iterator for its iterator returns the same iterator, at its
current position, and traversing it consumes it. Every other collection returns a fresh iterator each
time. Generic code over `Iterable` must therefore not assume it can traverse its input twice. There
is no separate traverse-once supertype.

An iterator is two methods: `hasNext()` says whether an element remains, and `next()` yields it and
moves past it. `next()` on an iterator with nothing left traps. A class of the program's own is
iterable by conforming to `Iterable[A]` and returning an `Iterator[A]` of its own from `iterator()`.

## Operations without higher-kinded types

`Iterable` declares the operation families: transformation (`map`, `flatMap`, `filter`, `collect`,
`zip`), folds and reductions, predicates and searches, grouping and partitioning, and conversions
(`toList()`, `toVector()`, `toArray()`, `toSet()`, `toMap()`).

- A transformation declared on `Iterable` returns `Iterable[B]`. Each concrete collection
  **overrides** it with a more precise result type: `List.map` returns `List[B]`, `Set.map` returns
  `Set[B]`, and `Iterator.map` returns a lazy `Iterator[B]`. Covariant result types make each override
  legal.
- So code that knows it has a `List` gets a `List` back, and generic code over `Iterable` gets an
  `Iterable` whose run-time type is still the concrete collection.
- `xs.view()` returns a lazy `Iterator`, for pipelines that should not build intermediate
  collections (see [views](#methods-every-iterable-has) below).
- Operations that need something of the elements are added by impls bounding the element type, so
  a collection has them only where its elements do. `mkString` joins elements that have `Display`
  into a string: `mkString(sep = ", ")` separates them, `mkString(start, sep, end)` also brackets
  them, so `[1, 2, 3].mkString()` is `1, 2, 3` and `.mkString("<", "|", ">")` is `<1|2|3>`. Every
  collection and iterator has it, through an impl adding it to `Iterable`, and a map keeps its own,
  whose entries join as `key: value`, the form a displayed map shows. `max()` and `min()` on every
  collection of `Ord` elements answer `Some` of the greatest or least, the first of equal ones, and
  `None` when empty. The free functions `max(first, rest*)` and `min(first, rest*)` take one value or more, with a
  [repeated](/spec/declarations/#repeated-parameters) parameter, and answer the greatest or least
  itself: `max(3, 9, 4)` is `9`, `min(x, ys*)` the least of `x` and the elements of `ys`.
  `sum()` and `product()` are added to `Iterable` the same way, so every collection has them, ranges
  and sets included, where its elements have `Add` or `Mul`: they answer the total or the product,
  and the identity (`T.zero()`, `T.one()`) when empty, so `[1, 2, 3].sum()` is `6`, `(1..5).product()`
  is `120`, and the `sum` of no `real`s is `0.0` and of no strings is `""`. Overflow traps as `+` does,
  and strings concatenate in `sum` but have no `product`.
- `collect(pf)` takes a [`PartialFunction`](/spec/types/#partial-functions) and keeps `pf` of each
  element it is defined at, in order, in the collection `map` would make: `shapes.collect(match
  Circle(r) -> r)` is the radii of the circles. Every collection with `map` has it, and so does
  `Option`. `collectFirst(pf)`, on every `Iterable`, is `Some` of `pf` of the first element it is
  defined at, or `None`, and reaches no element after that one. A `Map` collects its entries as
  pairs into a `Map`.

## Methods every Iterable has

Impls add these to `Iterable`, each written once over `iterator()`, so every collection has them:
lists, vectors, arrays, buffers, deques, ranges, sets, maps (whose elements are their entries, as
`(key, value)` pairs), the mutable sets and maps, iterators, and a class of the program's own that
conforms to `Iterable`. They visit the elements in iteration order. On an iterator they consume it.
A type with a faster way to answer one keeps its own method of the same name and parameter types,
which is found first wherever the type is known: a list walks itself for `foldLeft`, `find` and the
rest, a set and a range know at once whether they are empty, a vector is its own `toVector()`.
Collections they make are vectors, maps and sets; n is the number of elements.

| Method | Result | Cost |
|---|---|---|
| `isEmpty()`, `nonEmpty()` | `bool` | O(1) |
| `foldLeft(initial, f)` | `B` | O(n) |
| `reduce(f)` | `T`; traps when empty | O(n) |
| `reduceOption(f)` | `Option[T]`, `None` when empty | O(n) |
| `foreach(f)` | `unit` | O(n) |
| `exists(p)`, `forall(p)` | `bool`; stops at the first that decides | O(n) |
| `find(p)` | `Option[T]`, the first that satisfies `p` | O(n) |
| `count(p)` | `int` | O(n) |
| `minBy(key)`, `maxBy(key)` (`K: Ord`) | `Option[T]`, the first of equal keys | O(n), `key` once per element |
| `min()`, `max()` (`T: Ord`) | `Option[T]`, the first of equal ones | O(n) |
| `sum()` (`T: Add`), `product()` (`T: Mul`) | `T`; the identity when empty | O(n) |
| `mkString(sep)`, `mkString(start, sep, end)` (`T: Display`) | `string` | O(n) |
| `partition(p)` | `(Vector[T], Vector[T])`: those that satisfy `p`, those that do not | O(n) |
| `groupBy(key)` | `Map[K, Vector[T]]` | O(n) |
| `groupMap(key, f)` | `Map[K, Vector[V]]` | O(n) |
| `groupMapReduce(key, f, combine)` | `Map[K, V]` | O(n) |
| `toList()`, `toVector()`, `toArray()`, `toBuffer()` | the collection, in order | O(n) |
| `toSet()` | `Set[T]` | O(n) |
| `toMap()`, on pairs | `Map[K, V]`, a later key's value replacing | O(n) |
| `zipWithIndex()` | `Vector[(T, int)]`, positions from 0 | O(n) |
| `collectFirst(pf)` | `Option[B]` | O(n) |
| `view()` | `Iterator[T]`, lazy; see below | O(1) |

Each group of `groupBy` and `groupMap` keeps its elements' order; `groupMapReduce` combines a key's
values in that order, so `words.groupMapReduce(w -> w.take(1), w -> 1, (a, b) -> a + b)` counts the
words by their first letter. The map's own order is its keys' hashes.

```text
val (small, large) = [5, 12, 3, 40].partition(x -> x < 10)    // [5, 3], [12, 40]
(1..6).groupBy(x -> x % 3)                                     // {0: [3, 6], 1: [1, 4], 2: [2, 5]}
["b", "a"].zipWithIndex()                                      // [("b", 0), ("a", 1)]
```

**An iterator's transformations are lazy.** `Iterator` itself has `map(f)`, `filter(p)`,
`flatMap(f)`, `collect(pf)`, `take(n)`, `drop(n)`, `takeWhile(p)`, `dropWhile(p)`, `zip(other)` with
any collection, and `zipWithIndex()`, each answering an iterator that reads its source only as far as
its own `hasNext()` and `next()` ask: `it.map(f).take(2).toVector()` calls `f` twice however long `it`
is. `drop(n)` skips when the first element is asked for, `flatMap` calls `f` on an element only once
the collection made of the one before is used up, `zip` ends with the shorter side, and
`zipWithIndex` here is an `Iterator[(T, int)]` rather than a vector. The new iterator shares its
source, which must not then be read on its own. `withFilter(p)` is its `filter(p)`, already lazy, so a
comprehension over an iterator is an iterator, read element by element.

**A view is an iterator.** `xs.view()`, on every collection, is `xs.iterator()` taken as the start of
a lazy pipeline: nothing is computed until the view is read, each element is computed once as it is
read, and no intermediate collection is made. `toVector()`, `toList()` and the other conversions, or
any method every `Iterable` has, force it. A view is single-pass like any iterator, and it reads its
source as it goes, so a mutable source changed before the view is read is seen changed.

```text
(1..1000000000).view().filter(isPrime).map(x -> x * x).take(3).toList()   // reads up to 5
map.view().filter(e -> e.1 > 0).map(e -> e.0).toVector()                     // a map's entries as pairs
```

## Methods every Seq has

Impls add these to `Seq`, each written once over `length()`, `apply(i)` and `iterator()`, so every
sequence has them: lists, vectors, `ArraySeq`s, ranges, arrays, buffers, deques, and a class of the
program's own that conforms to `Seq` by giving `length()` and `apply(i)`. Through `Seq[T]` each one
that makes a sequence makes a `Vector`. Each concrete type keeps its own method of the same name and
parameter types where it can answer faster or with its own kind, and that one is found first
wherever the type is known:

- **A list, a vector and an `ArraySeq` answer with their own kind**: `list.takeWhile(p)` is a
  `List`, `vector.drop(2)` a `Vector`, `arraySeq.reverse()` an `ArraySeq`. A list shares what it
  can: `drop`, `dropWhile`, `takeRight`, the second half of `splitAt` and `span`, and what follows the
  replaced element in `updated` are its own tails.
- **An array, a buffer and a deque keep their own kind for the element-wise methods**, `map`,
  `flatMap`, `collect`, `filter` and `filterNot`, each a new collection, and answer the structural
  ones with a `Vector`, leaving themselves unchanged. Their in-place operations (`update`, `sort`,
  `add`, `insert`, `remove`) are their own.
- **A range answers with a `Vector`**, since it holds only a progression.
- `zip`, `zipAll`, `scanLeft`, `grouped`, `sliding` and `indices` have one form for every sequence.

**A count is held to the sequence**: below zero it is zero and past the length it is the length, so
`take(-1)` and `drop(100)` of three elements are both empty, and `splitAt(50)` is everything and
nothing. **An index range is not**: `slice(from, until)` traps unless `0 <= from <= until <=
length()`, as it does on a string and an array, and `updated(i, x)` traps where `xs(i)` would.
`tail()` and `init()` trap on the empty sequence.

**`intersect` and `diff` count as multisets.** Each occurrence in `other` matches the first
unmatched equal element of the receiver: `[1, 1, 2].intersect([1, 3])` is `[1]` and
`[1, 1, 2].diff([1, 3])` is `[1, 2]`; what is kept stays in the receiver's order. Equality is the
elements' own `==`.

n is the length of the receiver, m of the other operand, and k the count or size asked for. The cost
on a vector is O(log₃₂ n) where it says so; elsewhere a vector, an `ArraySeq`, an array, a buffer, a
deque and a range answer in the same time as a list, which walks from its front.

| Method | Result through `Seq[T]` | Own result | Cost |
|---|---|---|---|
| `map(f)`, `flatMap(f)`, `collect(pf)` | `Vector[U]` | each type's own kind; a range's is a vector | O(n) |
| `filter(p)`, `filterNot(p)` | `Vector[T]` | each type's own kind; a range's is a vector | O(n) |
| `withFilter(p)` | lazy, for a guard | each type's own | O(1) |
| `take(k)`, `drop(k)` | `Vector[T]` | List, Vector, ArraySeq | list O(k); vector O(log n); ArraySeq O(k) for `take`, O(n − k) for `drop`; others O(n) |
| `takeRight(k)`, `dropRight(k)` | `Vector[T]` | List, Vector, ArraySeq | vector O(log n); others O(n) |
| `takeWhile(p)`, `dropWhile(p)` | `Vector[T]` | List, ArraySeq; a vector's is a vector | O(n) |
| `slice(from, until)` | `Vector[T]`; traps out of order | List, Vector, ArraySeq, Array | vector O(log n); others O(n) |
| `splitAt(k)` | `(Vector[T], Vector[T])` | List, Vector, ArraySeq | vector O(log n); others O(n) |
| `span(p)` | `(Vector[T], Vector[T])` | List, ArraySeq; a vector's are vectors | O(n) |
| `tail()`, `init()` | `Vector[T]`; trap when empty | List, Vector, ArraySeq | list `tail` O(1); vector O(log n); others O(n) |
| `reverse()` | `Vector[T]` | List, ArraySeq; a vector's is a vector | O(n) |
| `xs ++ ys` | `Vector[T]`, `ys` any `Iterable[T]` | List (shares a list `ys`), Vector, ArraySeq | vector O(min(n, m)) for another vector; others O(n + m) |
| `xs :+ x`, `x +: xs` | `Vector[T]` | List, Vector, ArraySeq | list `+:` O(1), `:+` O(n); vector amortized O(1); others O(n) |
| `updated(i, x)` | `Vector[T]`; traps outside | List, Vector, ArraySeq | list O(i); vector O(log n); others O(n) |
| `padTo(total, x)` | `Vector[T]` | List, ArraySeq; a vector's is a vector | O(max(n, total)) |
| `distinct()`, `distinctBy(key)` | `Vector[T]`, the first of each | List, ArraySeq; a vector's is a vector | O(n) expected, a hash set |
| `intersect(other)`, `diff(other)` | `Vector[T]`, as multisets | List, ArraySeq; a vector's is a vector | O(n + m) expected, a hash map |
| `indices()` | `Range`, `0 ..< length()` | | O(1) after `length()` |
| `zip(other)` | `Vector[(T, U)]`, as long as the shorter | | O(min(n, m)) |
| `zipAll(other, thisElement, thatElement)` | `Vector[(T, U)]`, as long as the longer | | O(max(n, m)) |
| `scanLeft(initial, f)` | `Vector[B]`: `initial` and each partial fold, n + 1 | | O(n) |
| `grouped(size)` | `Iterator[Vector[T]]`; the last may be shorter; size < 1 traps | | O(n) |
| `sliding(size, step = 1)` | `Iterator[Vector[T]]`; ends with the first window to reach the last element, which may be shorter; size or step < 1 traps | | O(n) to copy the elements, then O(log n) a window |
| `lastIndexOf(x)`, `lastIndexWhere(p)` | `Option[int]` | | O(n), by `reverseIterator()` |
| `startsWith(prefix)`, `endsWith(suffix)` | `bool` | | O(m), plus `length()` for `endsWith` |
| `sameElements(other)` | `bool`: as many, each equal in order | | O(min(n, m)) |

Each also has `length()`, `apply(i)`, `get(i)`, `head()`, `last()` and their `Option` forms,
`indexWhere`, `indexOf`, `contains`, `foldRight`, `reverseIterator()`, the sorts
([sorting](#sorting)), `mkString`, `min` and `max`, besides everything an `Iterable` has.

```text
[1, 2, 3, 4, 5].sliding(3, 2).toVector()     // [[1, 2, 3], [3, 4, 5]]
[1, 2, 3, 4, 5].grouped(2).toVector()        // [[1, 2], [3, 4], [5]]
(1 .. 4).scanLeft(0, (s, x) -> s + x)        // [0, 1, 3, 6, 10]
val l: List[int] = [1, 2, 3]
val s: Seq[int] = l
l.takeWhile(x -> x < 3)                      // a List: [1, 2]
s.takeWhile(x -> x < 3)                      // a Vector: [1, 2]
[1, 2, 3].zipAll(["a"], 0, "-")              // [(1, "a"), (2, "-"), (3, "-")]
```

## Builders

Every collection type has a **builder**, through one library trait: `List.builder[int]()`,
`Vector.builder[string]()`, `Set.builder[A]()`. A builder takes elements with `add(x)` and
`addAll(xs)`, and `result()` hands over the finished collection; the builder is then spent, and using
it again traps. Transformations, conversions such as `toList()` and collection literals all build
through builders, and code that assembles a collection element by element uses one too. The trait
is `Builder[-A, +C]`: `add(element: A)`, `addAll(elements: Iterable[A])`, which adds each in turn
unless a builder does better, and `result() -> C`.

A builder is how an immutable collection is built efficiently without being mutable itself. `List`'s
builder appends in constant time and hands back an ordinary immutable `List`, so building a list in
order needs neither a second mutable list type nor a `reverse()` at the end:

```text
squares(n: int) -> List[int]
    val b = List.builder[int]()
    for i in 1..n do b.add(i * i)
    b.result()
```

## Maps and sets

`Map[K, +V]` and `Set[A]` find keys by their hash and compare them with `==`. Every type has both, so
any type is a key: strings and tuples by value, `data` values field by field, class instances by
identity unless their class implements `Eq` and `Hash`. Each key's hash is spread over all 64 bits
before it is used, so sequential integers and strings sharing long prefixes do not cluster.

The immutable map and set are persistent: `m.updated(k, v)`, `m.removed(k)`, `s.added(x)` and
`s.removed(x)` return a new collection sharing all but the changed path with the old one, which is
left as it was. They are hash tries, branching on one bit of the spread hash at a time, so an update
copies a path proportional to the logarithm of the size. Two maps are equal when they hold the same
keys bound to equal values, and two sets when they hold the same elements; equal collections hash
alike, whatever order they were built in.

```text
var counts = Map.empty[string, int]()
for w in words do counts = counts.updated(w, counts.getOrElse(w, 0) + 1)
for (word, n) in counts do print(s"$word $n")
```

`m(k)` reads a value and traps when the key is absent; `m.get(k)` returns an `Option`, and
`m.getOrElse(k, d)` evaluates `d` only when the key is absent. A set is a function from elements to
`bool`: `s(x)` is `true` when `x` is an element of `s` and `false` otherwise, `s.contains(x)`. Sets have `union`, `intersect`, `diff` and `subsetOf`, which read
well infix: `odds union evens`. Their symbol forms take any collection: `s ++ xs` is `s` with every
element of `xs` in it and `s -- xs` is `s` without them, an element of `xs` that `s` lacks being
ignored. The `+` and `-` spellings belong to `Add` and `Sub`, so a single element goes in with
`s.added(x)` and out with `s.removed(x)`.

An immutable map has the same two forms: `m ++ pairs` takes any collection of `(key, value)` pairs,
the last pair for a key winning, and `m -- keys` takes any collection of keys. `m.keys()` and
`m.values()` are vectors in the map's iteration order, the value at each index belonging to the key at
the same index, and `m.keySet()` is the set of the keys; `keysIterator()` and `valuesIterator()` visit
them lazily instead. `m.filterKeys(p)` keeps the entries whose key passes, `m.transform((k, v) -> w)`
binds each key to a value made from both, and `m.updatedWith(k, f)` calls `f` with `m.get(k)` and
binds `k` to the `Some` it answers or leaves it unbound for `None`, so it adds, replaces or removes.

`avin.collections.mutable` holds `Map[K, V]` and `Set[A]`, invariant, updated in place:
`m(k) = v` binds a key, `m(k) += 1` updates its value, and `remove` answers the value removed as an
`Option`. They are open-addressed tables, probed linearly, kept at most two thirds full. A program
importing them alongside the immutable ones renames them:

```text
import avin.collections.mutable.{Map as MutableMap}

val seen = MutableMap[string, int]()
seen("a") = 1
```

A mutable set or map changes through its compound-assignment methods, which need no `var`: `s += x`,
`s -= x`, `s ++= xs`, `s --= xs`, and for a map `m += (k, v)`, `m -= k`, `m ++= pairs`, `m --= keys`;
removing what is absent is not an error, and `s --= s` empties the set. `retain(p)` removes what fails `p`, and a map's
`updateWith(k, f)` does what `updatedWith` does, in place, answering the new binding. The methods
that build a collection — `map`, `flatMap`, `filter`, `mapValues`, `transform`, `filterKeys` and a
set's `union`, `intersect` and `diff` — answer a new mutable one; a mutable map's `keys()` and
`values()` are vector snapshots and its `keySet()` an immutable set.

Iteration visits a map's entries as `(key, value)` pairs in an order the program cannot rely on. For
the immutable collections it depends only on the keys' hashes, so equal maps iterate alike; for the
mutable ones it is the table's order.

Mutable maps and sets, like arrays, compare and hash by their contents at the moment they are
compared; `===` tells two of them apart. Every map and set displays in the shape of its literal,
`{a: 1, b: 2}` and `{1, 2, 3}`, each part in its own `Display`, and its `Debug` uses the parts'
`Debug`, `{"a": 1}`, each where the parts have it.

## Sorted maps and sets

`TreeMap[K, +V]` and `TreeSet[A]` keep their keys in the order of `compare`, so a key must be `Ord`:
`TreeSet(Point(1, 2))` is an error naming the bound when `Point` has no order. They are persistent
like `Map` and `Set`, an update copying one path, and they are balanced trees, so a lookup, an update
and each ordered operation below take time in the logarithm of the size. Iteration, `keys()`,
`values()`, `foreach`, `mkString` and the display all visit the keys in ascending order.

A tree map has every method of `Map`, and a tree set every method of `Set`, with the same meaning and
the same traits: each is `Iterable`, a function (`(K) -> V` and `(A) -> bool`), `Eq`, `Hash`,
`Display` and `Debug`, so code written over those traits takes either kind. A tree collection equals
another of its type holding the same entries, and hashes to what the `Map` or `Set` of the same
entries does. The methods that make a collection keyed by something new — `map`, `collect` and
`flatMap` — need the new key to be `Ord` and answer a tree collection; where two entries make the same
key, the later one in key order wins. A tree map's `keySet()` is a `TreeSet`.

The ordered operations, on both kinds, answering a map's entries as `(key, value)` pairs:

| Operation | Answers |
|---|---|
| `first()`, `last()` | the least and the greatest entry, as an `Option` |
| `floor(k)`, `ceiling(k)` | the entry with the greatest key at most `k`, and the least at least `k`, as an `Option` |
| `from(low)` | the entries whose keys are at least `low` |
| `until(high)` | the entries whose keys are less than `high` |
| `range(low, high)` | the entries from `low` until `high`; none when `high` is not greater |
| `reverseIterator()` | the entries in descending order |

The three slices are collections of the same kind, sharing their structure with the original.

```text
val scores = TreeMap(("cy", 7), ("al", 9), ("bo", 4))
print(scores)                     // {al: 9, bo: 4, cy: 7}
print(scores.range("b", "c"))     // {bo: 4}
print(scores.floor("bz"))         // Some((bo, 4))
val small: TreeSet[int] = [5, 1, 3]
print(small.until(4))             // {1, 3}
```

`TreeMap.empty`, `TreeMap.builder` and `TreeMap((k, v), …)`, and the same for `TreeSet`, make them as
their hash-based counterparts' companions do, and a sequence literal builds one where one is expected.

## Indexing and callable sequences

A `Seq[A]` is a function from index to element: `xs(i)` reads an element. Mutable sequences assign
with `xs(i) = v`. An index out of bounds traps; `xs.get(i)` returns an `Option`. Square brackets
never index; they are for types and literals.

## Collections as functions

A sequence, a set and a map are each a function, by subtyping: a [function type](/spec/types/#function-types)
is among their parents, and their `apply` is what a call of one as a function runs.

| Collection | Is a | `c(x)` is |
|---|---|---|
| `Seq[T]`: every list, vector, `ArraySeq`, range, array, buffer and deque | `(int) -> T` | the element at index `x`, trapping outside the sequence |
| `Set[A]`, immutable and mutable | `(A) -> bool` | whether `x` is an element |
| `Map[K, V]`, immutable and mutable | `(K) -> V` | the value bound to `x`, trapping when there is none |

So a collection is passed, stored and called wherever its function type is expected, with no
conversion, and a lambda of the same type may stand beside it:

```text
val words = ["zero", "one", "two"]
(0 ..< 2).map(words)                 // [zero, one]
val allowed = Set[string]().added("one")
words.filter(allowed)                // [one]
val table = Map[string, int]().updated("one", 1)
["one", "one"].map(table)            // [1, 1]
val fs: Vector[(int) -> string] = [words, i -> s"#$i"]
```

Variance holds through the function type: a `Vector[Dog]` is an `(int) -> Animal`, since `Seq` is
covariant in its element and a function in its result, and a `Map[string, Dog]` is a
`(string) -> Animal`. A set's element type and a map's key type are its own, so a `Set[int]` is no
`(string) -> bool`. A `Range` is a `Seq[int]`, so it is an `(int) -> int`: `(0 ..< 100 by 5)(3)` is
`15`.

## Array

`Array[T]` is an ordinary invariant collection class, implementing `Seq[T]`. It is a contiguous,
fixed-length, mutable buffer whose value elements are stored unboxed, so `Array[int]` holds `int`s and
`Array[Point]` holds `Point`s inline. It can be created in generic code (`Array.fill(n, x)`), compares
by contents, displays its contents, and has identity.

Avin has no null reference, so an array never holds an element that is not yet a value. Every array
is made with each element set:

```text
val zeros = Array.fill(8, 0)              // eight 0s; the value is evaluated once and shared
val squares = Array.tabulate(5, i -> i * i)
val none = Array.empty[string]()
```

`Array[int]()` is an error. `xs.length()` is the length, which never changes; `xs.slice(from, until)`
and `xs.copy()` make new arrays; `xs.get(i)` is an `Option`. `xs(i) += 1`, `xs(i) := v` and `xs(i)++`
work as they do on a variable, evaluating `xs` and `i` once. Because a call with untyped arguments
before `=` assigns an element, a local function's declaration always types its parameters or has
none: `f(x) = 1` is the assignment `f.update(x, 1)`.

## List and cons

`List[A]` is an immutable singly linked list, the collection for recursive code. `x :: xs` makes a
new list with `x` in front of `xs`, sharing `xs`, in constant time. The same spelling takes a list
apart in a pattern, and `[]` is the empty list:

```text
sum(xs: List[int]) -> int = xs match
    [] -> 0
    head :: tail -> head + sum(tail)

pairs[A](xs: List[A]) -> List[(A, A)] = xs match
    a :: b :: rest -> (a, b) :: pairs(rest)
    _ -> []
```

`::` needs a `List` on its right, so a literal there is built as one: in `1 :: 2 :: []`, the `[]` is
the empty `List[int]`. `xs(i)` on a list takes time proportional to `i`; code that indexes wants a
`Vector`.

`List` is a `data` type of the core library with two variants, `Cons(first: T, rest: List[T])` and
`Nil`; `x :: xs` is `Cons(x, xs)` and `[]` is `Nil`. `::`, `[]` built as a list, and list patterns
always mean the core library's `List`, even where a program declares its own `List`, `Cons` or
`Nil`. The element type of `x :: xs` is inferred as a call of `Cons` infers it, so a `Cat` goes in
front of a `List[Dog]` where a `List[Animal]` is expected.

Every operation on a list loops over it rather than recursing, so a list of any length is traversed,
transformed, compared and hashed without deepening the stack; a method that yields a list in order,
such as `map` or `filter`, builds it reversed and turns it around. `head()`, `tail()` and `last()`
trap on the empty list, and `headOption()` and `get(i)` return an `Option`. Lists compare and hash by
their contents, and order element by element when their elements are ordered, a list ordering
before every longer list it begins.

## Sequence patterns

A sequence is taken apart with the same brackets that build it. `[]` matches an empty sequence, and
`[a, b, c]` matches a sequence of exactly three elements, binding or testing each one; this works on
any `Seq` — `List`, `Vector`, `Array`, `Buffer`. On a `List`, `[a, b]` means `a :: b :: []`, and the
two forms mix: `a :: [b, c]` matches a list of exactly three elements. `::` patterns themselves are
for lists only, since splitting off a head is what a list does in constant time.

```text
describe(xs: Vector[string]) -> string = xs match
    [] -> "nobody"
    [one] -> one
    [first, second] -> s"$first and $second"
    _ -> s"${xs(0)} and ${xs.length() - 1} others"
```

For exhaustiveness, `[]` together with `head :: tail` covers every `List`; fixed-length patterns
cover only their lengths, so a match that uses only them needs a catch-all.

## Literals

| Literal | Value with no other expected type |
| --- | --- |
| `[1, 2, 3]`, `[]` | Immutable `Vector` |
| `{1, 2, 3}`, `{}` | Immutable `Set` |
| `{"a": 1, "b": 2}` | Immutable `Map` |
| `{a: 1, b: 2}` | The same map; a bare identifier directly before `:` is a string key |
| `Map.empty[string, int]()` | Empty `Map` |

**A literal builds the collection its expected type asks for.** With no expected collection type,
or one the default satisfies (`Seq[int]`, `Iterable[int]`), a sequence literal is a `Vector`, a set
literal a `Set` and a map literal a `Map`. When the expected type is another collection — from an
annotation, a parameter or a field — the literal builds that one instead:

```text
val xs = [1, 2, 3]                  // Vector[int]
val queue: Buffer[int] = [1, 2, 3]  // growable and mutable
val header: Array[byte] = [0x89, 0x50, 0x4e, 0x47]
val stack: List[string] = []
val seen: mutable.Set[int] = {}
```

A literal is built with its collection's [builder](#builders): sequence types (`List`, `Vector`,
`Array`, `Buffer`, `Deque`) from sequence literals, set types from set literals and map types from
map literals. A library or user collection with a builder can be written as a literal the same way:
a class or `data` type whose companion object has a `builder()` method, returning a `Builder` of the
elements and the type, is built by a sequence literal wherever that class is expected. The elements take their
types from the expected element type when there is one (`Array[byte]` makes each literal a
`byte`), and otherwise from the elements themselves, which agree on a type as the branches of an
`if` do: `[dog, cat]` is a `Vector[Animal]`, and `[1, "two"]` is an error. An empty literal with
nothing to say what it holds takes its type from a later use, as `None` does, and is otherwise an
error asking for an annotation. A literal compared with `==`, `!=` or an ordering takes the other
operand's type as its expected type, so `xs == [1, 2]` compares a `List` with a `List` when `xs` is
one.

Literals always build the library's types, even when a user type named `Vector` is in scope.

## Buffer

`Buffer[T]` is invariant, growable and mutable. `Buffer[int]()` is an empty buffer; `add(x)`, or
`b += x`, appends in amortized constant time, `addAll(xs)`, or `b ++= xs`, appends each in turn, `insert(i, x)` and `remove(i)` work at any index, and `clear()` empties
it. A buffer added to itself, `b ++= b`, appends the elements it held when the call began, once. `b(i)` and `b(i) = v` read and replace an element, and an index outside `0 ..< length()` traps
at the caller's line. Its elements live in an array with spare capacity, which never holds a value
that is read before one is written, so a buffer needs no value to fill it with.

## Vector

`Vector[+T]` is the default sequence: immutable, covariant and persistent. A changed vector shares
almost everything with the one it came from, so keeping both costs little, and either may be
changed again.

Its elements are in a tree of arrays of 32. The tree has a balanced middle and, at every level, a
prefix and a suffix beside it: the arrays at the two ends, which is where a vector grows. Reading
an element takes one array per level, and a vector of up to 2^31 elements has at most six. Adding
an element at either end makes a new array at that end of the bottom level, and only when that
array is full does the next level gain one; so `:+` and `+:` take amortized constant time.
`updated` copies the arrays on one path from the top of the tree to the element. Slicing cuts the
arrays at its two edges and shares the rest, so it takes time in the depth of the tree, not in the
length of the slice.

Every array in a vector's tree is exactly the size it holds and is never written once it is in the
tree. The library ends the filling of each with a release fence, before the array is stored in
another, and the vector's constructor ends with the fence [threads](/spec/threads/) describe, so a
vector made by one thread is whole to any other thread that reads it.

| Operation | Cost |
|---|---|
| `xs(i)`, `head()`, `last()` | O(log₃₂ n), at most six arrays |
| `xs :+ x`, `x +: xs` | amortized O(1) |
| `updated(i, x)` | O(log₃₂ n), one path copied |
| `slice`, `take`, `drop`, `takeRight`, `dropRight`, `splitAt`, `tail`, `init` | O(log₃₂ n) |
| `xs ++ ys` | O(min(length of xs, length of ys)) |
| `length()`, `isEmpty()` | O(1) |
| `iterator()`, `reverseIterator()`, `for`, `foldLeft`, `map`, `filter` | O(n), a leaf at a time |
| `Vector.builder()`, `toVector()` | O(n), each array made once |

A vector's iterators read one leaf after another, finding the next leaf only when one is used up.
A builder fills leaves of 32 as elements come and makes the tree above them once, at `result()`.

## ArraySeq

`ArraySeq[+T]` is the immutable sequence whose elements are in one array that nothing writes to
after it is made. There is no level between an index and its element, so `xs(i)` reads one array
however long the sequence is, but every operation that makes a changed sequence (`xs :+ x`,
`x +: xs`, `xs ++ ys`, `updated`) copies all of them: it is for elements built once and then read.
`slice` copies the elements it keeps. `ArraySeq.empty[T]()` and `ArraySeq.builder[T]()` make one, as
does `toArraySeq()` on any collection, or a literal whose expected type is an `ArraySeq`:

```avin
val xs: ArraySeq[int] = [1, 2, 3]
val ys = xs :+ 4                    // [1, 2, 3, 4], a copy; xs is unchanged
val v = ys.toVector()               // a Vector of the same elements
```

| Operation | Cost |
|---|---|
| `xs(i)`, `length()` | O(1) |
| `xs :+ x`, `x +: xs`, `updated(i, x)`, `slice` | O(n), a copy |
| `xs ++ ys` | O(length of xs + length of ys) |
| `map`, `filter`, `collect`, `flatMap`, iteration | O(n) |

Both `Vector` and `ArraySeq` compare and hash by their elements in order, and display as an array
does, `[1, 2, 3]`, where their elements display.

## Seq

`Seq[+T]` is what every indexed sequence shares, so a function over `Seq[T]` takes a list, a vector,
an `ArraySeq`, a range, an array, a buffer or a deque. A sequence supplies `length()` and `apply(index)`, and has the rest
from them: `get`, `isEmpty`, `nonEmpty`, `head` and `last` (trapping when empty) with `headOption` and
`lastOption`, `foldRight`, `indexWhere`, `toVector`, an iterator, and `reverseIterator`, which walks
from the last element to the first. The [methods every Iterable has](#methods-every-iterable-has),
`foldLeft`, `exists`, `count` and the rest, it has as every collection does. A sequence that reaches
its elements another way overrides the ones that would be slow: a list walks from its front, so on a
list each of them takes time proportional to its length.

**Equality is written once on `Seq`**: two sequences with equal elements in the same order are equal
and hash alike, whichever kinds they are, so `[1, 2, 3] == List(1, 2, 3)` and `[1, 2, 3] == (1..3)`
are `true`, and a `Seq[int]` key finds an entry stored under a vector, a list or a range of the same
elements. The two sides need the same element type. A class of the program's own that implements
`Seq` is compared the same way unless it implements `Eq` itself.

```text
total(xs: Seq[int]) -> int = xs.foldLeft(0, (acc, x) -> acc + x)

total([1, 2, 3])                   // a Vector
total(Array.fill(4, 1))            // an Array
```

Impls add more to every sequence ([impl](/spec/declarations/#impl)), written once on `Seq` and
reached through generic code over `Seq[T]` as on each sequence type: `contains(element)` and
`indexOf(element)`, which compare with `equals` and are `exists` and `indexWhere`, so a list walks
itself once; where the elements are ordered, the sorts, `sorted()`, `sortedBy` and `sortedWith`
([sorting](#sorting)); and the transformations and structural methods in
[methods every Seq has](#methods-every-seq-has).

```text
present[T](xs: Seq[T], x: T) -> bool = xs.contains(x)

present([3, 1, 4], 4)              // true
Array.fill(2, "a").mkString("-")   // a-a
```

## Deque

`Deque[T]` is invariant, growable and mutable at both ends. `Deque[int]()` is an empty deque;
`addFirst(x)` and `addLast(x)` put an element at the front or the back in amortized constant time, and
`removeFirst()` and `removeLast()` take one off and return it, trapping on an empty deque. So a deque
is a queue, `addLast` with `removeFirst`, and a stack, `addLast` with `removeLast`. `d(i)` and
`d(i) = v` read and replace the element `i` places from the front, an index outside
`0 ..< length()` trapping, and `clear()` empties it; `addAll(d)` of itself appends the elements it
held when the call began, once. Its elements live in an array as a ring that wraps
around from the array's end to its start, doubling as a buffer's does when full. A deque compares and
hashes by its elements in order, displays as `[1, 2, 3]`, has `mkString` where its elements display,
and is built by `Deque.builder` and by a sequence literal where one is expected.

```text
val pending: Deque[string] = ["build", "test"]
pending.addFirst("fetch")
while pending.nonEmpty() do run(pending.removeFirst())
```

## Sorting

Every sequence sorts into a new one, and the mutable sequences also sort in place. A comparison is
what `Ord.compare` answers: negative, zero or positive as its first argument orders before, with or
after its second.

- On every `Seq`: `sorted()` where the elements are `Ord`, ascending; `sortedBy(key)`, ascending by
  a key that is `Ord`; and `sortedWith(compare)`, by any comparison. They answer a `Vector` and leave
  the sequence as it was. A `List` has its own three, which answer a `List`, as its `map` and
  `filter` do: a list sorted is usually used as a list, walked from its front or matched with `::`,
  and the conversion would otherwise be the caller's every time. A vector needs no sorts of its own,
  since `Seq`'s already answer a vector.
- On `Array`, `Buffer` and `Deque`: `sort()`, `sortBy(key)` and `sortWith(compare)` reorder the
  elements in place and answer unit. A deque sorts front first, wherever its ring starts.

**Every sort is stable**: elements that compare equal keep the order they had, so sorting by one
field and then by another orders by the second with the first breaking ties. **Every sort takes
time proportional to n log n comparisons in the worst case** and memory for one copy of the
elements: it is a merge sort, which sorts runs of 16 elements by insertion and then merges them
pairwise between the elements and one scratch array, the runs doubling each pass.

**`sortBy` computes `key` once for each element**, before any comparison, and sorts the elements as
their keys sort; so a key that is costly to compute, such as a string built from the element, costs
n computations rather than one per comparison, at the price of an array of the keys. `sortWith`
calls `compare` as the sort needs it, and `sorted()` is `sortWith` by `compare`.

```text
data Entry(rank: int, name: string)

val entries = [Entry(2, "b"), Entry(1, "a"), Entry(2, "c")]
entries.sortedBy(e -> e.rank)                       // a, b, c: b still before c
entries.sortedWith((x, y) -> y.rank.compare(x.rank)) // b, c, a
val scores: Array[int] = [3, 1, 2]
scores.sort()                                        // [1, 2, 3]
```

Strings order by code point (their bytes, which in UTF-8 order the same), so every capital comes
before every small letter; reals order as IEEE 754 does. Sorting elements without `Ord` by
`sorted()` or `sort()` is a compile-time error naming the bound, and `sortWith` is the way to sort
them.

## Ranges

`a..b` (inclusive) and `a..<b` (exclusive) are expressions that construct an immutable
`Range[T]`, which is a `Seq`. Each endpoint is evaluated once, left to right. The default step is
+1, so a descending range is empty; `by` sets another step. Iteration stops without stepping past the
type's maximum. Range **patterns** in `match` arms are a separate feature: a test, not a value.

`a..b by s` is the infix call `(a..b).by(s)`: a negative step counts down, so `10..0 by -2` is 10, 8,
…, 0, and a step of zero traps. A range is empty when its start already lies past its limit in the
step's direction, and `r.isEmpty()` says so. Iteration never steps past the largest or smallest
value of the type: `for i in 2147483640..2147483647 by 5` yields 2147483640 and 2147483645 and stops.

A `Range` of `int`s is a `Seq[int]`, and answers its own sequence questions from its bounds in
constant time: `length()` (trapping for a range of more values than the largest `int`, such as
`int.min .. int.max`), `r(i)`, which is `start + i * step` and traps outside `0 ..< length()`, and
`contains(x)`, which is whether `x` lies between the first and last values a whole number of steps
from the first, so `(1 ..< 10 by 3).contains(7)` is true and `.contains(8)` false. `get(i)`, `head()`,
`last()` and their `Option` forms are answered from the bounds too, so they work on a range too long
for `length()`. Everything else
it has as [every sequence](#methods-every-seq-has) does, and what those make of it, `map` and
`filter` as much as `take` and `reverse`, is a `Vector`: `(1 .. 5).map(x -> x * x)` is
`[1, 4, 9, 16, 25]`.

## for comprehensions

A `for` with `yield` is sugar over collection methods: generators become `flatMap`/`map` and guards
become lazy filters, so any type with those methods works in it. Scope and the order of evaluation
follow the source. A `for` without `yield` is a loop over the source's `iterator()`, described next.

`for pattern in xs` followed by an indented body, or `for pattern in xs do body` on one line, runs the
body once per element, binding each by the pattern: a name, `_`, a tuple or a variant pattern that
every element matches. Each element is a new binding, so a closure made in the body keeps the
element it saw. `break` and `continue` work as in `while`. A range written in place (`0..<n`, of any
integer type) is counted without making a `Range`, and an array is indexed; anything else is
traversed through its `iterator()`, which is what `foreach` does for the library's collections, and
lets `break`, `continue` and `return` leave the body directly. A source with no iterator, such as an
`int`, is an error.

```text
for i in 0..<n do total += i
for (name, age) in people
    if age < 18 then continue
    print(name)
```

### Generators, guards and `yield`

A comprehension is `for`, one or more generators separated by commas, and `yield` with the value
to collect, on the rest of the line or in an indented block. A generator is `pattern in source`,
and any number of guards, `if condition`, may follow it. Between the generators, and after the last,
come value definitions, `pattern = value`, each with guards of its own. When the generators do not
fit on one line they go one to a line in an indented block after `for`, a line starting with `if`
adding a guard to the generator or definition above it, and `yield` starts the line after the block.
`yield` is a keyword.

```text
val pairs = for x in xs, y in ys if x < y yield (x, y)

val grid = for
    row in 0 ..< height
    if row % 2 == 0
    col in 0 ..< width
    cell = row * width + col
yield cell * cell

val priced = for item in order, (name, cost) = lookup(item) if cost > 0 yield s"$name: $cost"
```

The last generator's source is called with `map` and the yielded value, every earlier one with
`flatMap` and the rest of the comprehension, and each guard becomes `withFilter` on the source it
follows:

```text
for x in xs if p(x), y in ys yield f(x, y)
xs.withFilter(x -> p(x)).flatMap(x -> ys.map(y -> f(x, y)))
```

A value definition is evaluated once for each element that reaches it and binds by its pattern,
which matches every value as a `val`'s does, in its guards, the later generators and definitions,
and the yielded value. Definitions that no guard follows are bound beside the element in the
function passed to its generator's source, as `val`s are in a block:

```text
for x in xs, y = f(x), z in g(y) yield h(z)
xs.flatMap(x ->
    val y = f(x)
    g(y).map(z -> h(z)))
```

A guard after a definition has to see its value before the element goes on, so the definition, and
every one before it after the same generator, pairs each element with its value through `map`, and
the guard and everything after it take the pair apart. On a collection `map` makes a collection, so
such a definition is evaluated for every element before its guard tests any:

```text
for x in xs, y = f(x) if p(y) yield h(y)
xs.map(x -> (x, f(x))).withFilter(pair -> p(pair.1)).map(pair -> h(pair.1))
```

A definition binds in its generator's scope, as a `val` does in a block: it may not bind a name the
generator or an earlier definition after it binds, though a later generator may bind any name again.
A comprehension starts with a generator.

The calls are checked as if written out, so the first source decides the result's type: `map` and
`flatMap` on a `List` give a `List`, on a `Vector`, an `Array`, a `Buffer` or a `Deque` that collection, on a
`Set` a `Set`, on an `Option` an `Option`, and on a `Range`, which holds only a progression, or any
other `Seq`, a sequence of the program's own among them, a `Vector`, through the
[methods every Seq has](#methods-every-seq-has). A `Map`'s elements are its entries as `(key, value)` pairs, and a comprehension over one
yields pairs and makes a `Map`. Over a mutable `Set` or `Map` the result is a new mutable set or map,
as over an `Array`, a `Buffer` or a `Deque` it is a new one of those, and the source is unchanged. `flatMap` on the library's collections takes any `Iterable` from its
function, so a later generator may draw from another kind of collection than the first; an
`Option`'s `flatMap` takes an `Option`. A type of the program's own works in a comprehension by
having the methods it calls: `map` for one generator, `flatMap` for more, and `withFilter` for a
guard. A source without the method it is asked for is an error at that source.

`withFilter` is lazy. It returns a view of the collection that tests each element just before
`map`, `flatMap` or `foreach` uses it, and another `withFilter` on the view combines the tests. So
the order of evaluation follows the source element by element: the first source is evaluated once,
then for each of its elements its guards run and, for an element that passes them, the next
generator's source is evaluated anew and its elements taken in turn, down to the yielded value. A
guard that prints shows its output interleaved with the yielded values', not all before them.
`Option`'s `withFilter` is its `filter`, since with one value at most the two are the same.

Each generator binds by its pattern, as a `for` loop does: a name, `_`, a tuple or a variant pattern
that every element matches; a pattern some elements would not match is an error, not a filter. The
bindings are visible to the generator's guards and to everything after it, and each element is a
new binding. A guard is a `bool`. The guards, the value definitions and the yielded value are
functions passed to the sources' methods, so `return`, `break`, `continue` and `?` cannot leave
them, though a loop inside the yielded block may be left as usual. A `for` without `yield` has one
generator, no guards and no value definitions.
