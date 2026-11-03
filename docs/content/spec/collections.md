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
through builders, and code that assembles a collection element by element uses one too.

A builder is how an immutable collection is built efficiently without being mutable itself. `List`'s
builder appends in constant time and hands back an ordinary immutable `List`, so building a list in
order needs neither a second mutable list type nor a `reverse()` at the end:

```text
squares(n: int) -> List[int]
    val b = List.builder[int]()
    for i in 1..n do b.add(i * i)
    b.result()
```

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
map literals. A library or user collection with a builder can be written as a literal the same way. The elements take their types from the expected
element type when there is one (`Array[byte]` makes each literal a `byte`), and otherwise from the
elements themselves. An empty literal with nothing to say what it holds is an error asking for an
annotation.

Literals always build the library's types, even when a user type named `Vector` is in scope.

## Ranges

`a..b` (inclusive) and `a..<b` (exclusive) are expressions that construct an immutable
`Range[T]`, which is a `Seq`. Each endpoint is evaluated once, left to right. The default step is
+1, so a descending range is empty; `by` sets another step. Iteration stops without stepping past the
type's maximum. Range **patterns** in `match` arms are a separate feature: a test, not a value.

## for comprehensions

`for` is sugar over collection methods: generators become `flatMap`/`map`, guards become
lazy filters, and a `for` without `yield` becomes `foreach`. Scope and the order of evaluation follow
the source. There is no separate loop protocol, so any type with these methods works in a `for`.
