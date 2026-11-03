---
title: Generics and inference
summary: Reified, monomorphized generics with bounds and declaration-site variance; local type inference with subtyping.
weight: 14
---

## Generic declarations

Functions, `data`, classes, traits and `impl`s take type parameters in square brackets:

```text
largest[T: Ord](xs: List[T]) -> Option[T] = …

class Cache[K: Hash, V](capacity: int)

data Result[+T, +E]
    Ok(value: T)
    Err(error: E)
```

## Bounds

`[T: Bound]` requires `T` to conform to `Bound`, which may be a trait or a class; `[T: A + B]` requires
both. A bound declared on a type applies everywhere that type's parameters appear. An unbounded `T`
may be stored, passed and returned; using an operator, a method or a field on it needs the bound that
provides it. Errors are reported at the generic definition, not at an instantiation.

```text
trait Ordered
    less(other: Self) -> bool
    greater(other: Self) -> bool = other.less(self)

max[T: Ordered](a: T, b: T) -> T = if a.less(b) then b else a

class SortedPair[T: Ordered](first: T, second: T)
    high() -> T = max(first, second)
```

- **A value of a bounded `T` has the members of its bounds**: their methods, those of the traits
  they require, a trait's `val`s and a class bound's visible fields. A call on it is checked against
  the bound's signature with the bound's `Self` as `T`, and takes every form a method call takes:
  overloads, default and named arguments, infix calls, the method's own type arguments, and the
  method as a value. A member no bound has is an error naming the bounds.
- **A value of `T` is a value of each of its bounds' types**, where that type is a type: it passes
  where a `Shape` is expected when `T: Shape`.
- **Each type argument conforms to its parameter's bounds**, whether written or inferred, and on
  types written in declarations as at calls: a class or object through its header, its superclass
  and impls, a `data` type through its impls, a trait type to itself and the traits it requires, and
  a type parameter through its own bounds. The error names the argument, the bound and the
  parameter. A bound's type arguments are inferred from the argument, as `A` in
  `drain[S: Source[A], A](s: S)` is from the source passed.
- **A bound is a trait or a class**, and may name the declaration's other parameters. A trait whose
  methods mention `Self` is named only here, and in an impl's header.
- **An impl's parameters are its type's**, and have the bounds the type declares; an impl writes
  none of its own, since `impl[T: Show] Show for Box[T]` would make `Box[T]` conform for some type
  arguments only.
- **A call through a bound is direct**: each instance calls the implementation of its own type
  argument. Where the argument is a trait type or an open class, any conforming object may arrive,
  and the call goes through the object's method table as a call on that type does.
- The bounds of a method's own type parameters are contravariant positions for its class's
  parameters, as its parameter types are.
- Operators on a bounded `T` come with the core library's `Add`, `Ord` and `Eq`; until then a
  bound gives `T` methods only.

## Reification

Generics are monomorphized: each distinct set of type arguments produces its own code and layout.
`List[int]` stores unboxed `int`s. Every heap object's header names its exact type, including type
arguments, so:

- a type pattern such as `xs: List[string] -> …` is checked against the object's exact type: it
  matches when that type is the pattern's or a subtype of it, through [variance](#variance) too, so
  a `List[Dog]` held as an `Any` matches `xs: List[Animal]` and a `List[int]` does not match
  `xs: List[Any]`;
- generic code can create an `Array[T]` without any evidence parameter;
- there are no unchecked casts. A downcast is a `match` with a type pattern.

Avin compiles the whole program at once. That is what makes this possible for **generic methods on
traits** used dynamically (`it.map[B](f)` where `it: Iterable[A]`): the compiler knows every type
that implements the trait and every `B` the program uses, and generates the dispatch for each.

Trait-typed values give one shared copy of code where monomorphizing would be wasteful.

## Subtyping

`S <: T` holds when:

- `S` is a class whose superclass chain or declared traits include `T`, or an `impl T for S` exists;
- `T` is `Any`, or `S` is `Nothing`;
- both are function types, with parameters contravariant and results covariant;
- both are instances of one generic type and each argument follows its declared variance (below).

A value type is never a subtype of a trait or of `Any` without conversion. A value converts to a trait
type or `Any` by **boxing at a single site**, such as an argument or an assignment. Boxing never
happens inside a container: `List[int]` is not a `List[Display]`.

## Variance

Type parameters of `data` types, classes and traits declare variance: `+T` covariant, `-T`
contravariant, plain `T` invariant. A function's or an `impl`'s type parameters take none.

- Immutable collections are covariant: `List[+A]`, `Map[K, +V]`, `Option[+T]`.
- Mutable collections are invariant: `Array[T]`, `Buffer[T]`. A covariant mutable collection would
  accept a store that is only checkable at run time.
- Variance applies where the arguments are reference types: `Any`, strings, tuples, functions,
  `data` types, classes and traits. Because reference types share one representation (a pointer to
  an object whose header carries its type), `List[Dog]` is a `List[Animal]` with no conversion at
  all, and `List[string]` is a `List[Any]`. A value of a value type becomes an `Any` or a trait only
  by boxing, which never happens inside a container, so `List[int]` is not a `List[Any]`.
- Tuple parts are covariant and function types take contravariant parameters and covariant
  results, on the same terms: `(Animal) -> int` is a `(Dog) -> int`, and `() -> string` is a
  `() -> Any`, but `() -> int` is not.
- Where two branches yield instances of one generic type, they agree on the instance whose
  covariant arguments are their arguments' agreement: `if c then Some(dog) else Some(cat)` is an
  `Option[Animal]`.

The checker verifies each declared parameter's positions:

- A `+T` may appear where a value is read out: a method's result, an immutable field (every field
  of a `data` type, a `val` or plain parameter of a class), the covariant arguments of a superclass
  or trait the type conforms to.
- A `-T` may appear where a value is passed in: a method's parameter.
- A mutable field is both, so neither may appear in a `var`.
- A function type's parameters flip the position and its result keeps it; an argument of another
  generic type takes the position as that type's parameter's variance turns it, and an invariant
  parameter's argument is a position where neither may appear. So a `+T` may appear in a method
  parameter `f: (T) -> int`, and a `-T` in a result `() -> (T) -> unit`.
- Private members are checked like public ones: an instance seen through a wider type still runs
  them.
- The traits a `data` type's `impl`s make it conform to are parents, like a class's. The methods an
  `impl` adds to a `data` type are not positions: they receive the value as whichever instance of
  the type it is seen as, and data is immutable.

An error names the parameter, its variance and the position: a `+T` in the parameter `x` of `put`
is reported as a covariant type parameter appearing in a contravariant position.

Use-site variance (wildcards, existentials) is not part of Avin.

## Inference

Inference is a **practical approximation of Hindley–Milner**: within a function body, types flow
both from arguments and from the expected type, and unknowns are solved by constraint, as in HM.
Declared signatures at function boundaries keep it predictable, fast and its errors local. Full HM,
with principal types and implicit generalization, is not the goal; inferring what a reader would
infer is.

- Function parameter types are declared. Result types are inferred from the body, except in
  recursive functions.
- Local `val`/`var` types, lambda parameter and result types, and type arguments at calls and
  constructors are inferred from arguments **and** from the expected type. Within one function body,
  an inference variable may be settled by a later use.
- A type argument of a call that neither its arguments nor its expected type decide stays unknown
  until the end of the enclosing function body (a lambda's body is part of it), and the first use
  that needs it settles it:
  - a branch it joins: `if c then Some(x) else None` is an `Option[int]`, as is a `match` whose
    other arms are, and a lambda's `None` takes the type of its other return paths;
  - a store: `var best = None` then `best = Some(3)` makes `best` an `Option[int]`, from the body
    or from a local function or lambda that assigns it;
  - a use as a value of a known type: an argument, a value stored where its type is declared, an
    operand of an arithmetic or comparison operator, or a value matched against a literal pattern.
    `best match Some(b) if b >= i -> …` settles `b`, and with it `best`, as `i`'s type.

  A later use that disagrees with an earlier one is an error at the later use. An unknown still open
  at the end of the body is an error at the call that made it, asking for the type to be written. A
  field declared without a type has its initializer's type at once, since any function may read it.
  A type argument held to a bound is decided at its call.
- Constraints are subtype constraints. A type variable collects lower and upper bounds and is solved
  to the least type that satisfies them, so `if c then dog else cat` is an `Animal`, and the branches
  of a `match` agree on their least upper bound.
- `None`, `[]`, `{}` and `Map.empty` take their type arguments from context. Where nothing determines
  a type, the result is an error asking for an annotation, never a silent default to `Any` or
  `Nothing`.
- Nothing is generalized implicitly: a local `val f = (x) -> x` with no use that settles `x` is an
  error. Polymorphic functions are declared with type parameters.

Explicit type arguments may always be given: `parse[int](text)`, `Map.empty[string, int]`, or a
leading prefix with the rest inferred.

## Scope of the type system

The type system stays small: generics with bounds, subtyping with declaration-site variance, and
traits implemented through `impl`. It has no higher-kinded types, type members, union or
intersection types, implicit parameters, implicit conversions or macros. The
[collections chapter](/spec/collections/) shows how the library gets precise result types with
these tools alone.
