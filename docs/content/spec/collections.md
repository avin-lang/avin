---
title: Collections
summary: A rich collection library without higher-kinded types, where Iterator is Iterable and arrays are ordinary collections.
weight: 17
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
│   ├── Vector[+A]           immutable indexed
│   ├── Range[A]             immutable arithmetic progression
│   ├── Array[A]             mutable, fixed length, contiguous
│   └── Buffer[A]            mutable, growable, contiguous
├── Set[A]                   immutable; HashSet, TreeSet
└── Map[K, +V]               immutable; HashMap, TreeMap
```

The mutable `Set` and `Map` variants live in `avin.collections.mutable`.

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

## Literals

| Literal | Value |
| --- | --- |
| `[1, 2, 3]`, `[]` | Sequence literal. **Open:** immutable `List`, or a growable `Buffer` (see [decisions](/spec/decisions/#open)) |
| `{1, 2, 3}`, `{}` | Immutable `Set` |
| `{"a": 1, "b": 2}` | Immutable `Map` |
| `{a: 1, b: 2}` | The same map; a bare identifier directly before `:` is a string key |
| `Map.empty` | Empty `Map` |

A literal's element types are inferred from its elements and from context. Literals always build the
canonical library types, even when a user type named `List` is in scope.

## Ranges

`a..b` (inclusive) and `a..<b` (exclusive) are expressions that construct an immutable
`Range[T]`, which is a `Seq`. Each endpoint is evaluated once, left to right. The default step is
+1, so a descending range is empty; `by` sets another step. Iteration stops without stepping past the
type's maximum. Range **patterns** in `match` arms are a separate feature: a test, not a value.

## for comprehensions

`for` is sugar over collection methods: generators become `flatMap`/`map`, guards become
lazy filters, and a `for` without `yield` becomes `foreach`. Scope and the order of evaluation follow
the source. There is no separate loop protocol, so any type with these methods works in a `for`.
