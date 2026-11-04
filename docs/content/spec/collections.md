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
│   ├── Vector[+A]           immutable indexed; the default sequence
│   ├── Range[A]             immutable arithmetic progression
│   ├── Array[A]             mutable, fixed length, contiguous
│   ├── Buffer[A]            mutable, growable at the end, contiguous
│   └── Deque[A]             mutable, growable at both ends
├── Set[A]                   immutable; HashSet, TreeSet
└── Map[K, +V]               immutable; HashMap, TreeMap
```

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
  collections.

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
var counts = Map[string, int]()
for w in words do counts = counts.updated(w, counts.getOrElse(w, 0) + 1)
for (word, n) in counts do print(s"$word $n")
```

`m(k)` reads a value and traps when the key is absent; `m.get(k)` returns an `Option`, and
`m.getOrElse(k, d)` evaluates `d` only when the key is absent. A set is a function from elements to
`bool`: `s(x)` is `s.contains(x)`. Sets have `union`, `intersect`, `diff` and `subsetOf`, which read
well infix: `odds union evens`.

`avin.collections.mutable` holds `Map[K, V]` and `Set[A]`, invariant, updated in place:
`m(k) = v` binds a key, `m(k) += 1` updates its value, and `remove` answers the value removed as an
`Option`. They are open-addressed tables, probed linearly, kept at most two thirds full. A program
importing them alongside the immutable ones renames them:

```text
import avin.collections.mutable.{Map as MutableMap}

val seen = MutableMap[string, int]()
seen("a") = 1
```

Iteration visits a map's entries as `(key, value)` pairs in an order the program cannot rely on. For
the immutable collections it depends only on the keys' hashes, so equal maps iterate alike; for the
mutable ones it is the table's order.

Mutable maps and sets, like arrays, compare and hash by their contents at the moment they are
compared; `===` tells two of them apart. Every map and set displays in the shape of its literal,
`{a: 1, b: 2}` and `{1, 2, 3}`, each part in its own `Display`, and its `Debug` uses the parts'
`Debug`, `{"a": 1}`, each where the parts have it.

## Indexing and callable sequences

A `Seq[A]` is a function from index to element: `xs(i)` reads an element, and a `Seq[A]` can be
passed where an `(int) -> A` is expected. Mutable sequences assign with `xs(i) = v`. An index out of
bounds traps; `xs.get(i)` returns an `Option`. Square brackets never index; they are for types and
literals.

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
| `Map.empty` | Empty `Map` |

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
a class whose companion object has a `builder()` method, returning a `Builder` of the elements and
the class, is built by a sequence literal wherever that class is expected. The elements take their
types from the expected element type when there is one (`Array[byte]` makes each literal a
`byte`), and otherwise from the elements themselves, which agree on a type as the branches of an
`if` do: `[dog, cat]` is a `Vector[Animal]`, and `[1, "two"]` is an error. An empty literal with
nothing to say what it holds takes its type from a later use, as `None` does, and is otherwise an
error asking for an annotation. A literal compared with `==`, `!=` or an ordering takes the other
operand's type as its expected type, so `xs == [1, 2]` compares a `List` with a `List` when `xs` is
one.

Literals always build the library's types, even when a user type named `Vector` is in scope.

## Buffer and Vector

`Buffer[T]` is invariant, growable and mutable. `Buffer[int]()` is an empty buffer; `add(x)` appends
in amortized constant time, `insert(i, x)` and `remove(i)` work at any index, and `clear()` empties
it. `b(i)` and `b(i) = v` read and replace an element, and an index outside `0 ..< length()` traps
at the caller's line. Its elements live in an array with spare capacity, which never holds a value
that is read before one is written, so a buffer needs no value to fill it with.

`Vector[+T]` is the default sequence: immutable, covariant and indexed in constant time. Its
elements are in one array that nothing writes to after the vector is made, so an operation that
makes a changed vector (`appended`, `prepended`, `concat`, `updated`) copies them. Both compare and
hash by their elements in order, and display as an array does, `[1, 2, 3]`, where their elements
display.

## Ranges

`a..b` (inclusive) and `a..<b` (exclusive) are expressions that construct an immutable
`Range[T]`, which is a `Seq`. Each endpoint is evaluated once, left to right. The default step is
+1, so a descending range is empty; `by` sets another step. Iteration stops without stepping past the
type's maximum. Range **patterns** in `match` arms are a separate feature: a test, not a value.

`a..b by s` is the infix call `(a..b).by(s)`: a negative step counts down, so `10..0 by -2` is 10, 8,
…, 0, and a step of zero traps. A range is empty when its start already lies past its limit in the
step's direction, and `r.isEmpty()` says so. Iteration never steps past the largest or smallest
value of the type: `for i in 2147483640..2147483647 by 5` yields 2147483640 and 2147483645 and stops.

## for comprehensions

`for` is sugar over collection methods: generators become `flatMap`/`map`, guards become
lazy filters, and a `for` without `yield` becomes `foreach`. Scope and the order of evaluation follow
the source. There is no separate loop protocol, so any type with these methods works in a `for`.

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
