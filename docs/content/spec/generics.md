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
- **An impl's parameters are its type's**, and have the bounds the type declares. **An impl of a
  trait may bound them further**, making the type conform only for the type arguments that meet the
  bounds: with `impl[T: Ord] Ord for Box[T]`, a `Box[int]` is an `Ord` and a `Box[Plain]` is not. Each
  type argument written or inferred for a bound is held to them, and the impl's methods have the
  bounds' members. Such an impl is of a data type or a class, and of any trait. Where the trait is
  also a type, a `Box[int]` converts to `Display` and a `Box[Plain]` does not, also as a covariant
  type argument (`List[Box[Plain]]` is no `List[Display]`); a `d: Display` type pattern on an `Any`
  matches only an instance that conforms; and the trait's methods, the impl's included, are called
  on an instance only where it conforms, in the type's own methods too, where its parameters have
  only the bounds the type declares. Inside generic code, `Box[T]` conforms where `T`'s bounds meet
  the impl's. An impl of `Eq` or `Hash` may be bounded only by them, since every type meets them.
- **An impl without a trait may bound its parameters too**, adding its methods only for the type
  arguments that meet the bounds: with `impl[T: Display] List[T]`, a `List[int]` has `mkString` and a
  `List[Plain]` does not. Calling such a method, or taking it as a value, on an instance that misses
  a bound is an error naming the bound; a subclass has the methods at the type arguments it gives
  its superclass; in generic code, `List[U]` has them where `U`'s bounds meet the impl's. Inside the
  impl its parameters have the bounds' members. Its methods overload the type's other methods by
  parameter types, as any method does, so two of the same parameter types are an error even where
  their bounds could never both hold.
- **An impl without a trait may add to a type built from its parameters**, as
  `impl[T] Option[Option[T]]` or `impl[A: Ord, B] Seq[(A, B)]`. Its parameters are not the type's
  but its own, learned at each call by matching the receiver's static type against the type it
  names, then held to their bounds; [declarations](declarations.md#impl) has the matching rule.
- **A call through a bound is direct**: each instance calls the implementation of its own type
  argument. Where the argument is a trait type or an open class, any conforming object may arrive,
  and the call goes through the object's method table as a call on that type does.
- The bounds of a method's own type parameters are contravariant positions for its class's
  parameters, as its parameter types are.
- **Operators on a bounded `T` come from its bounds**: `a + b` needs a bound that is `Add` or
  requires it, `a < b` one that is `Ord`, and `a == b` one that is `Eq` (`Ord` requires `Eq`). Every
  type meets `Eq` and `Hash`, so any type argument passes those bounds. In an instance at a built-in
  type the operator is the machine's operation, as written outside generic code.
- **`Integer` is a closed bound**: its members are the integer types, every width, which the
  compiler decides; no type implements it and no declaration names it after `:`. It requires `Add`,
  `Sub`, `Mul`, `Div`, `Rem`, `Neg`, `Ord`, `Display` and `Debug`, so `[T: Integer]` has every
  operator and both text forms. A value of such a `T` converts to the other integer types and to
  `real`, `byte` and `char` as an integer does (`long(x)`), and a call of `T` converts to it
  (`T(n)`, narrowing to the low bits as integers do); each instance converts at its width.

```text
digits[T: Integer](x: T) -> int
    var n = 1
    var rest = x / T(10)
    while rest != T(0)
        rest = rest / T(10)
        n += 1
    n
```

- **The core library implements a trait for every integer width at once** with an impl over a
  type parameter bounded by `Integer` alone: `impl[T: Integer] Display for T`. Only the core library
  writes such an impl, and only with `Integer`; a trait it implements this way has no other impl for
  any width, and a program implements none of the core library's traits for an integer type, since
  neither is declared in its module. A program's own trait may be implemented for one width
  (`impl Sized for i5`), and that width then has both.

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
- Branches that agree on no type of their own agree on the type expected of them when both are
  instances of it, also while its type arguments are still being inferred: in
  `v.flatMap(n -> if n == 2 then List.empty[int]() else v)` the lambda is expected to yield an
  `Iterable[U]`, so its branches agree on `Iterable[int]`, and `U` is `int`. The same holds for
  the arms of a `match` and for a loop's `else` and its `break`s.

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
  the type it is seen as, and data is immutable. The same holds for the methods an `impl` adds to a
  class whose fields are all immutable, so `Vector[+T]` takes `T` in `:+` and `contains`; a
  class with a `var` field keeps every method a position. So, too, for the methods an `impl` adds to
  a trait (`impl[T] Seq[T]`), which reach the value only through the trait's own methods.

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
- A type argument of a call given several arguments takes their join, as branches do; the first
  argument does not decide it alone. Each argument whose type mentions it where a value is read out
  (the whole parameter, a covariant argument of a generic type, a tuple part, a function's result)
  is a lower bound, so with nothing expected `cat :: dogs` is a `List[Animal]`, `pair(cat, dog)`
  for `pair[T](a: T, b: T)` takes `Animal`, and `firstOf(Some(cat), Some(dog))` takes it too. An
  expected type decides the type argument before the arguments do. An invariant or contravariant
  use (an argument of `Box[T]` for an invariant `T`, a lambda's parameter) decides it for good, as
  the type the lower bounds so far fit, and a later argument must be an instance of that. A number
  literal takes the type the other arguments decide. Two value types have no join, so
  `pair(1, "one")` is an error naming both, unless `Any` is expected.
- Two classes join to their nearest common superclass. Where they have none, they join to the trait
  they both conform to that every other trait they share is a parent of: a `Parrot` and a
  `Goldfish` that are both `Pet: Named` join to `Pet`. Where they share several unrelated traits,
  no one of them is chosen: the join is an error that names them, and the type is written, or
  given by the expected type.
- An argument of type `Nothing` (a `break`, a `return`, `fail(…)`) decides no type argument, since
  the call is never made: the other arguments and the expected type decide them as they would
  alone, and one that only such an argument names is `Nothing`. So `identity(break 42)` and
  `print(fail("unreachable"))` are well typed, while a `Nothing` argument beside `1` and `"one"`
  reconciles neither with the other, nor answers for a bound they break.
  Likewise a tuple or a collection literal with a part of type `Nothing` is never made, and has
  type `Nothing`.
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
