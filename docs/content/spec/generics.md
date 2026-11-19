---
title: Generics and inference
summary: Reified, monomorphized generics with bounds and declaration-site variance; constructor parameters and the standard Functor and Monad traits; local type inference with subtyping.
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

`[T: Bound]` requires `T` to conform to `Bound`, which may be a trait or a class; `[T: A & B]` requires
both, and `[T: A & B & C]` all three. A bound declared on a type applies everywhere that type's parameters appear. An unbounded `T`
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
  they require, a trait's `val`s and a class bound's visible fields, which are read as on an
  instance of the class and, where `var`, assigned the same way. A call on it is checked against
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
  methods mention `Self` is named only here, and in an impl's header. A function's or a method's own
  type parameter may also be bounded by a [type parameter](#bounds-by-a-type-parameter) declared
  before it.
- **Several bounds are joined with `&`**, the one spelling, wherever type parameters are declared:
  on functions, methods, classes, data types, traits, actors and impl headers
  (`impl[T: Ord & Display] Display for Box[T]`), after a lower bound too (`[W >: T: Display & Ord]`).
  `+` between bounds is an error that says to write `&`. Each operand is a trait or a class, an
  instance of a generic trait among them (`[S: Source[A] & Named, A]`); **at most one is a class**,
  the others traits, since a class extends one superclass, so a second class is an error naming
  both, even a subclass of the first, whose bound alone says the same. **Each is written once**: the
  same trait at the same type arguments twice is an error, and `Make[int] & Make[long]` are two
  bounds. The order of the operands means nothing: `[T: A & B]` and `[T: B & A]` admit the same
  arguments and give `T` the same members.
- **The members of every operand are the parameter's**, as if each were its only bound. Methods of
  one name from two bounds are overloads of each other, chosen by parameter types as any overloads
  are: with `say(n: int)` from one and `say(s: string)` from the other, `x.say(1)` and `x.say("a")`
  each call one. Two of the same parameter types make a call that both accept ambiguous, an error
  at the generic definition naming both methods; it is not an error to write the bounds, only to
  make the call. A type argument meets every operand, and the error for one that misses names the
  bound it misses.
- **`A & B` is not a type**: an intersection is written only as a type parameter's bounds, so a
  parameter, a field, a `val`, a result, a type argument or a lower bound written `Named & Sized` is
  an error, which shows the form to write instead, `[T: Named & Sized](x: T)`. A value that is both
  is reached through such a parameter, whose instances call each type's methods directly.
- **An impl's parameters are its type's** where it names them as the type's arguments, in order
  (`impl[T] Ord for Box[T]`), and have the bounds the type declares. **An impl of a
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
- **An impl of a trait may be for an instance of a generic type** rather than for the type at its
  own parameters: `impl FromPg for Vector[byte]`, or a partly applied one, `impl[K] Show for
  Map[K, int]`, `impl[T] Show for Box[Option[T]]`. The type conforms to the trait where it is such
  an instance, seen through its superclasses, with its parts exactly the impl's (a `Vector[Nothing]`
  is not a `Vector[byte]` here); each of the impl's parameters is part of the type, takes the part at
  its place and is held to its bounds. The impl's members, static ones included, are the type's at
  those instances, wherever an impl is found: called on a value or on the type
  (`Vector[byte].fromPg(text)`), through a bound, in a bound held where a type argument is written,
  on a trait value, by a type pattern, and by a template walking a field. It belongs in the module of
  the type or of the trait, as any impl of a trait does. `Eq`, `Hash`, `Sendable` and `Boundary`
  are a type's at every instance alike, so their impls are for the type at its own parameters. An
  impl of `Display` or `Debug` for an instance of a data type is an impl of the trait like any
  other, so the type has no automatic one at its other instances.
- **Two impls of one trait never both hold of a type**: there is no "more specific wins". An
  instance impl beside the type's own impl of the trait is allowed only where the instance misses
  that impl's bounds, and two instance impls only where no instance matches both. A missed bound
  counts only if it is missed for good: where some module other than the impl's own and the core
  library could still write the impl meeting it (one able to name the bound and the type without
  importing a module that imports it), the two impls overlap. The error names both impls, and the
  module where one could arise.

```avin run
trait FromPg
  static fromPg(text: string) -> Option[Self]

impl FromPg for int
  static fromPg(text: string) -> Option[int] = text.toIntOption()

// `bytea`: the bytes themselves.
impl FromPg for Vector[byte]
  static fromPg(text: string) -> Option[Vector[byte]] = Some(text.bytes)

// An array: comma-separated elements. `byte` is no `FromPg`, so this impl never holds of a
// `Vector[byte]`, and the two do not overlap.
impl[T: FromPg] FromPg for Vector[T]
  static fromPg(text: string) -> Option[Vector[T]]
    var out: Vector[T] = []
    for part in text.split(',')
      T.fromPg(part) match
        Some(v) -> out = out :+ v
        None -> return None
    Some(out)

decode[T: FromPg](text: string) -> Option[T] = T.fromPg(text)

main()
  println(decode[Vector[byte]]("hi").map(v -> v.length))
  println(decode[Vector[int]]("1,2,3"))
```

```text output
Some(2)
Some([1, 2, 3])
```

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
  (`T(n)`, trapping when `n` does not fit, as integers do; `T.wrap(n)` keeps the low bits); each instance converts
  at its width. `T.MinValue` and `T.MaxValue` are the limits of the instance's width, of type `T` and
  folded to a constant in each instance (`-128` and `127` at `i8`); they read wherever `T` is in
  scope, a default argument's value included, and a type parameter without the `Integer` bound has
  no limits. The bitwise operators `&`, `|`, `^`, `<<` and `>>` also work on such a `T`, both
  operands of type `T` (`x >> T(1)`), as the machine's operators at each instance's width; a `T`
  with any other bound has them only as methods of those names. So do the wrapping operators `+%`,
  `-%`, `*%` and the saturating `+|`, `-|`, `*|` (and their compound forms), which a `T` with any other
  bound lacks.
- **`Numeric` is the numbers' bound**: every integer width and `real` meet it, the integers
  because `Integer` requires it. It requires `Add`, `Sub`, `Mul`, `Div`, `Rem`, `Neg`, `Ord`,
  `Display` and `Debug`, so `[T: Numeric]` has the five arithmetic operators, negation, the
  comparisons, both text forms and the identities `T.zero()` and `T.one()`; and it declares
  `toReal -> real`, the value as a `real` (exact for an integer up to 2⁵³ in magnitude, the nearest
  `real` beyond). In each instance an operator is the instance's own: integer overflow and integer
  division by zero trap, a `real` divides by zero to an infinity. A `T` bounded only by `Numeric` has
  nothing integer-only: no conversion by a call (`T(n)`, `real(x)`; `x.toReal` converts), no limits,
  no bitwise or wrapping operators and no integer methods, and it does not meet `Integer`. Unlike
  `Integer` it is open: a type of a program's own that implements the traits it requires and
  `toReal` is a number too, as a rational type is.

```text
mean[T: Numeric](xs: Vector[T]) -> real = xs.sum.toReal / real(xs.length)
```

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

## Lower bounds

`[W >: T]` requires every type argument for `W` to be a supertype of `T`. It is how a covariant type
takes a value of its own parameter in: a `Map[K, +V]` cannot have `updated(key: K, value: V)`, since
`V` would sit where a value is passed in, but it can have

```text
trait Bag[K, +V]
  put[W >: V](key: K, value: W) -> Bag[K, W]

data Chain[+A]
  Link(head: A, tail: Chain[A])
  End

impl[A] Chain[A]
  prepended[W >: A](x: W) -> Chain[W] = Link(x, self)
```

- **Only a function's or a method's own type parameters have a lower bound**: those of a function,
  of a trait's, class's or impl's method, and of an extension's method. A type's parameters and an
  impl's take none. A lower bound is any type, and may name the declaration's other parameters; a
  chain of lower bounds may not lead back to the parameter it starts from.
- **It is written first, then any bounds**: `[W >: T: Display]` is a supertype of `T` that conforms
  to `Display`. A value of `W` has the bounds' members, as any bounded parameter's does.
- **A value of the lower bound is a value of the parameter, with no conversion**: inside
  `prepended`, `self` is a `Chain[A]` and so a `Chain[W]`, and an `A` is a `W`. Each type argument
  is held to it at the call: the bound must be the argument, or be seen as it as a covariant part is
  (both references, as [variance](#variance) requires). So a `long` bound admits `long` and no
  `Any`, and `longs.prepended[Any](3)` is an error naming the bound.
- **Inference starts from the bound**: the type argument is the join of the lower bound and the
  arguments a value is read from, so `cats.prepended(dog)` is a `Chain[Animal]`, `cats.prepended(cat)`
  stays a `Chain[Cat]`, and a literal takes the bound's type (`longs.prepended(3)` is a
  `Chain[long]`). Where the bound and the arguments are references with no nearer common type, the
  join is `Any`, the top: `cats.prepended("hello")` is a `Chain[Any]`. Two value types still have no
  join. A delayed (by-name) argument widens it as any other argument does, so on a map of cats
  `getOrElse(key, dog)` is an `Animal`. A written type argument is held to the bound. The expected
  type decides a type argument only where it admits the bound; otherwise the argument is inferred as
  if nothing were expected and the result then passed as any value is, so `m.getOrElse(k, "none")`
  on a map of strings, passed where a `Display` is expected, takes `W` as `string`.
- **An override writes the overridden method's lower bound, or none**: it may assume no more of its
  type arguments than the method it overrides does. Calls through a trait dispatch as any generic
  trait method's do.

## Bounds by a type parameter

`[J: I]`, where `I` is a type parameter in scope, requires every type argument for `J` to be a subtype
of the one for `I`. It is the twin of the [lower bound](#lower-bounds): where `[W >: T]` lets a
covariant type take a value of its parameter in, `[J: I]` lets a contravariant one narrow the values
it reads. A `Rule[-I: Reader, +T]` reads any `I`; joined with a rule that reads only an `Input`, the
join reads an `Input`:

```text
abstract class Reader(val length: int)
  var offset = 0

class Input(val source: string): Reader(source.byteLength)

class Rule[-I: Reader, +T](val run: I -> Option[T])

impl[I: Reader, T] Rule[I, T]
  |[J: I](other: -> Rule[J, T]) -> Rule[J, T] = Rule(input ->
    val start = input.offset
    run(input) match
      Some(a) -> Some(a)
      None ->
        input.offset = start
        other.run(input))
```

- **Only a function's or a method's own type parameters are bounded by a type parameter**, as only
  they have a lower bound. The bounding one is the enclosing type's or impl's, or one declared
  earlier in the same list (`[A: Named, B: A]`); a later one is an error that shows the order to
  write, so a chain of such bounds never leads back to where it starts, and `[A: B, B: A]` is refused
  at its first bound. A parameter has one at most, and may have trait and class bounds beside it
  (`[B: A & Loud]`). A constructor parameter, `F[_]`, bounds nothing.
- **A value of the parameter is a value of the bounding one, with no conversion**: inside `|`, an
  `input` of type `J` is passed to `run`, which takes an `I`, and `self`, a `Rule[I, T]`, is a
  `Rule[J, T]`. It has the bounding parameter's bounds and their members (`input.offset`, through
  `Reader`), and through a chain, each one's further along: in `[A: Named, B: A, C: B]` a `C` is a `B`
  and an `A`, and has `Named`'s methods. Two branches yielding a `J` and an `I` agree on `I`.
- **Each type argument is held to it at the call**: the argument must be the bounding argument, or
  be seen as it as a covariant part is (both references, as [variance](#variance) requires). So a
  `Pen[Dog]`'s `swap[U: T]` refuses a `Cat`, a `Pen[Animal]`'s refuses an `int`, and the error names
  the argument, the type the bounding parameter is and both parameters.
- **Inference takes the narrower**: a type argument decided where a value is passed in, as `J` is
  by the `Rule[J, T]` given for `other`, is decided as the type given; where that type is not a
  subtype of the bounding argument but the bounding argument is a subtype of it, the bounding argument
  is taken instead, which the given value is also an instance of there. So `err[long]("no") | digit()`
  (`I` a `Reader`, `other` reading an `Input`) and `digit() | success(4)` (`I` an `Input`, `other`
  reading any `Reader`) are both `Rule[Input, long]`s. A bounding parameter nothing else decides is
  decided as the argument of the parameter it bounds: `describe[A: Named, B: A](b: B)` called with a
  `Dog` takes `A` as `Dog`.
- **It is a position where a value is passed in**, as every bound of a method's own parameter is
  ([variance](#variance)): a `-I` may bound one and a `+T` may not. That mirrors the lower bound's
  rule, and for the same reason: a `Rule[Reader, T]` seen as a `Rule[Input, T]` is called with a `J`
  that is an `Input`, which is a `Reader`, so the bound the method was written against still holds;
  a `Box[Dog]` seen as a `Box[Animal]` could be called with a `J` that is a `Cat`, which is no `Dog`.
- **An override writes the overridden method's bound, or none**, as for a lower bound: it may assume
  no more of its type arguments than the method it overrides does.

## Reification

Generics are monomorphized: each distinct set of type arguments produces its own code and layout.
`List[int]` stores unboxed `int`s. Every heap object's header names its exact type, including type
arguments, so:

- a type pattern such as `xs: List[string] -> …` is checked against the object's exact type: it
  matches when that type is the pattern's or a subtype of it, through [variance](#variance) too, so
  a `List[Dog]` held as an `Any` matches `xs: List[Animal]` and a `List[int]` does not match
  `xs: List[Any]`;
- a function value's header names its function type, so `f: ((Dog) -> Animal) -> …` matches a
  `(Animal) -> Dog` held as an `Any`, and a value of a class extending a function type, a `Vector[int]`
  among them, matches that function type;
- generic code can create an `Array[T]` without any evidence parameter;
- there are no unchecked casts. A downcast is a `match` with a type pattern.

Avin compiles the whole program at once. That is what makes this possible for **generic methods on
traits** used dynamically (`it.map[B](f)` where `it: Iterable[A]`): the compiler knows every type
that implements the trait and every `B` the program uses, and generates the dispatch for each.

Trait-typed values give one shared copy of code where monomorphizing would be wasteful.

### Polymorphic recursion

Because every list of type arguments gets its own copy, a program must need only finitely many of
them. **Polymorphic recursion**, a cycle of generic code whose type arguments grow on every round,
needs infinitely many, and is a compile-time error naming the cycle:

```avin
data Nest[T](x: T)

depth[T](x: T, n: int) -> int = if n == 0 then 0 else depth(Nest(x), n - 1)
// error: polymorphic recursion: each round of the cycle `depth` -> `depth` passes `Nest[T]`
// for `T`, so its type arguments grow without end and the code has no finite set of copies
```

The cycle may run through any generic code: functions, methods, generic local functions, lambdas,
and calls through a trait or a bound, where a class's implementation is reached with a larger
instance of the class. A type argument grows when it is built from the cycle's own type parameter,
as `Nest[T]`, `(T, T)`, `Vector[T]` or `(T) -> int` are from `T`.

Recursion whose type arguments do not grow is ordinary: `f[T]` calling `f[T]`, `f[A, B]` calling
`f[B, A]`, a call at a fixed type such as `f[int]`, and a type that grows once outside any cycle all
have finitely many copies.

A method an impl with bounds of its own adds, `impl[T: Ord] Ord for Box[T]`, has copies only at the
type arguments that meet them. A cycle through it counts only the type arguments that may: a
`compare` that builds a `Box[Plain[T]]`, where `Plain` has no `Ord`, needs no `compare` of that box
and compiles, while one building a `Box[Wide[T]]` of an ordered `Wide` grows without end and is
rejected. A type argument whose outermost type is not known where it reaches the impl, such as a
part taken apart from a larger type, is counted as one that may.

A data type naming itself at a larger instance in its fields, a **nested data type** such as

```avin
data Tree[T]
  Leaf(value: T)
  Deeper(inner: Tree[Nest[T]])
```

is rejected the same way, at the type: every data type compares, hashes and shows its values field
by field, and each of those needs a copy for every depth. The larger instance may sit inside another
type, as `inner: Option[Tree[List[T]]]` does, or reach back through a second data type; the error
names the cycle of types. A type naming itself at its own type arguments, however wrapped, as
`rest: Option[Chain[T]]`, `kids: Vector[Rose[T]]` or a pair of types naming each other at `T`, is
ordinary recursion: it compiles, and its derived and structural methods have one copy per instance.

## Subtyping

`S <: T` holds when:

- `S` is a class whose superclass chain or declared traits include `T`, or an `impl T for S` exists;
- `T` is `Any`, or `S` is `Nothing`;
- both are function types, with parameters contravariant and results covariant;
- both are instances of one generic type and each argument follows its declared variance (below);
- `T` is a type parameter with a [lower bound](#lower-bounds) that `S` is, or is a subtype of as a
  covariant part is;
- `S` is a type parameter [bounded by](#bounds-by-a-type-parameter) the type parameter `T`, or by
  one that is;
- `T` is a [union](/spec/unions/#subtyping) with a member `S` is a subtype of, or `S` is a union
  each of whose members is a subtype of `T`.

A value type is never a subtype of a trait or of `Any` without conversion. A value converts to a trait
type or `Any` by **boxing at a single site**, such as an argument or an assignment. Boxing never
happens inside a container: `List[int]` is not a `List[Display]`.

## Variance

Type parameters of `data` types, classes and traits declare variance: `+T` covariant, `-T`
contravariant, plain `T` invariant. A function's or an `impl`'s type parameters take none; the
mark on a [constructor parameter's hole](#constructor-parameters), `F[+_]`, is a demand on the
constructor passed, not a variance of the parameter.

- Immutable collections are covariant: `List[+A]`, `Map[K, +V]`, `Option[+T]`, `Either[+L, +R]`.
- Mutable collections are invariant: `Array[T]`, `Buffer[T]`. A covariant mutable collection would
  accept a store that is only checkable at run time.
- Variance applies where the arguments are reference types: `Any`, strings, tuples, functions,
  `data` types, classes and traits. Because reference types share one representation (a pointer to
  an object whose header carries its type), `List[Dog]` is a `List[Animal]` with no conversion at
  all, and `List[string]` is a `List[Any]`. A value of a value type becomes an `Any` or a trait only
  by boxing, which never happens inside a container, so `List[int]` is not a `List[Any]`.
  `Nothing` is the exception: no value has it, so a covariant `Nothing` argument is any type, and
  `List[Nothing]` is a `List[int]` too.
- Tuple parts are covariant and function types take contravariant parameters and covariant
  results, on the same terms: `(Animal) -> int` is a `(Dog) -> int`, and `() -> string` is a
  `() -> Any`, but `() -> int` is not. A lambda may declare its parameter that wide where the
  narrower function is expected, `dogs.map((a: Animal) -> a.name)`, and its body sees an `Animal`;
  `(x: Any) -> …` where an `(int) -> …` is expected is still an error.
- Where two branches yield instances of one generic type, they agree on the instance whose
  covariant arguments are their arguments' agreement and whose contravariant arguments are their
  arguments' meet, the one of the two that is a subtype of the other: `if c then Some(dog) else
  Some(cat)` is an `Option[Animal]`, and for `Sink[-T]`, `if c then sinkOfAnimals else
  sinkOfDogs` is a `Sink[Dog]`, whichever is written first, and so is the `match` or the loop
  that yields them. `Nothing` is below every type, so a `Sink[Nothing]` and a `Sink[Dog]` agree
  on `Sink[Nothing]`. Two arguments neither of which is a subtype of the other (a `Sink[Dog]` and
  a `Sink[string]`) have a meet no type names, so their instances agree on no instance and fall
  back as two classes do (below): to the trait they share, or to none. An invariant argument that
  differs has no agreement either. A function type's parameters follow the same rule as
  contravariant arguments, its result as a covariant one: `(Animal) -> int` and `(Dog) -> int`
  agree on `(Dog) -> int`.
- Branches that agree on a type that is not an instance of the fully known type expected of them
  agree on the expected type when both are instances of it: a `Dog` and a `Cat` that are both
  `Animal`s and `Pet`s agree on `Pet` where a `Pet` is expected. Where a branch is not an instance
  of the expected type and neither is what they agree on, that branch is the error, named as the
  one that misses it.
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
- The bounds of a method's own type parameter are positions where a value is passed in; its
  [lower bound](#lower-bounds) is one where a value is read out, so a `+T` may be the lower bound
  of a method's parameter, `put[W >: V](key: K, value: W)`, and a `-T` may not. A `+V` in the
  parameter itself, `put(key: K, value: V)`, is still an error. A [type parameter bounding
  it](#bounds-by-a-type-parameter) is a bound like any other: a `-I` may bound a method's own
  parameter, `|[J: I](other: -> Rule[J, T])`, and a `+T` may not.
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
  final class whose fields are all immutable, which are called directly at the type the receiver is
  seen as; a class with a `var` field keeps every method a position. So, too, for the methods an
  `impl` adds to a trait (`impl[T] Seq[T]`), which reach the value only through the trait's own
  methods.
- The methods an `impl` adds to an `open` or `abstract` class are positions like its own: they are
  in its method table, so a call through a wider view runs the copy for the object's own type
  arguments, and an `abstract class Box[+T]` adding `with(y: T) -> Box[T]` would let a `Box[Dog]`
  seen as a `Box[Animal]` make a `Box[Dog]` holding a cat. Such a method takes an element through a
  lower bound instead: `Vector[+T]` declares `:+[W >: T](element: W) -> Vector[W]`.

An error names the parameter, its variance and the position: a `+T` in the parameter `x` of `put`
is reported as a covariant type parameter appearing in a contravariant position.

A [refined variant](/spec/refined-variants/#variance) may fix only an invariant parameter of its
type; a parameter it fixes or hides has no variance in its fields.

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
  at the end of the body that the call's parameters and result name only where a value is read out
  (the whole type, a covariant argument, a tuple part, a function's result) is `Nothing`, the least
  type: `val none = None` alone is an `Option[Nothing]`, `Left(1)` an `Either[int, Nothing]`, `Ok(1)`
  a `Result[int, Nothing]`, `[]` a `Vector[Nothing]`, and `show(None)` for `show[T](o: Option[T])`
  takes `T` as `Nothing`. Any other unknown still open is an error at the call that made it, asking
  for the type to be written: `Buffer()`, `Map.empty()` (its `K` is invariant), `{}` (a `Set`), and a
  type argument nothing at the call names. A field or module value declared without a type has its
  initializer's type at once, since any function may read it, so `val none = None` there is an
  `Option[Nothing]`.
  A type argument held to a bound is decided at its call.
- Constraints are subtype constraints. A type variable collects lower and upper bounds and is solved
  to the least type that satisfies them, so `if c then dog else cat` is an `Animal`, and the branches
  of a `match` agree on their least upper bound.
- A type argument of a call given several arguments takes their join, as branches do; the first
  argument does not decide it alone. Each argument whose type mentions it where a value is read out
  (the whole parameter, a covariant argument of a generic type, a tuple part, a function's result)
  is a lower bound, so with nothing expected `cat :: dogs` is a `List[Animal]`, `pair(cat, dog)`
  for `pair[T](a: T, b: T)` takes `Animal`, and `firstOf(Some(cat), Some(dog))` takes it too. An
  expected type decides the type argument before the arguments do, except where the result names it
  as a contravariant argument of a generic type: there the expected type bounds it only from below
  (a `Sink[T]` is a `Sink[Nothing]` whatever `T` is), so the arguments decide it, and the expected
  type only where they leave it open. `watch(spawn(Worker()), …)`, whose parameter is an
  `ActorRef[Nothing]`, takes `M` from the `Behaviour` passed to `spawn`. An invariant or contravariant
  use (an argument of `Box[T]` for an invariant `T`, a lambda's parameter) decides it for good, as
  the type the lower bounds so far fit, and a later argument must be an instance of that. An argument
  is held to its parameter by each type argument's variance: at a contravariant argument of a
  generic type or a function type's parameter it may name a supertype of what the parameter names,
  so `take(x)` for `take[T](b: Box[B, T])`, `Box[-I, +T]`, accepts an `x: Box[A, int]` where `B` is an
  `A`, and takes `T` as `int`; a type argument named there is decided as the type the argument
  gives, the most it may be. A number
  literal takes the type the other arguments decide, and so does one inside a value built around
  it, a collection literal, a tuple or a call such as `Some(1)`, wherever it is written: with
  `f() -> Vector[long]`, `assertEq(f(), [1, 2])` and `assertEq(Some(1), someLong())` are well typed.
  Only the numbers are taken; the classes in the decided type are still widened by such an argument,
  so `same(dogs, [Cat(2)])` is a `Vector[Animal]`; an invariant part of it, such as an `Array`'s
  element, is taken whole. An expression of literals alone (`1_000_000_000_000 + 2001`, `-(4 * 5)`,
  `if c then 0 else 6`) takes the type the other arguments decide, the values built around literals
  among them. Of two arguments that both hold literals, the
  first written decides for the second, so `same((1, x), (x, 2))` with `x: long` is an error. Two value types have no join, so
  `pair(1, "one")` is an error naming both, unless `Any` is expected.
- The lambdas and `match` functions of a call take their parameter types from what the other
  arguments decide, so they are checked after them, and in turn among themselves: each time, the
  first one in the call whose parameter types are known, declared by it or decided by the arguments
  checked so far. So what a typed lambda decides reaches one written before it:
  `both(match n -> n + 1, (x: int) -> x * 10)` for `both[A, B](g: (A) -> B, f: (A) -> B)`,
  and `back(match s -> s.byteLength, (x: int) -> s"<$x>")` for `back[A, B](g: (B) -> A, f: (A) -> B)`,
  whose `match` takes `B` from the lambda's result. Where none is known, the first one left is
  checked and reports what it cannot infer.
- A lambda whose parameter types are known takes part in the tiers above by what it yields, as any
  other argument does: one yielding a value with a type of its own is checked with the arguments
  that hold no literal, after them, so a number literal beside it takes the type it decides. For
  `pick[T](read: (string) -> Option[T], absent: T)`, `pick(s -> s.toLongOption(), 0)` takes
  `T = long`, whichever argument is written first, and `pick(s -> s.toRealOption(), 0)` takes
  `real`. A lambda yielding a value built around a literal (`s -> Some(1)`) or a literal alone
  (`s -> 1`) is checked in that tier, after the other arguments of it, and takes their numbers:
  `yielding(s -> 1, long(5))` is a `long`. The value a lambda yields is the last item of its block,
  and an `if` or a `match` yields in the lowest tier of its branches. A lambda that leaves its body
  by `return` or `?`, and a `match` function, are still checked after every other argument, since
  such a body needs its result type known before it is checked.
- Two classes join to their nearest common superclass. Where they have none, they join to the trait
  they both conform to that every other trait they share is a parent of: a `Parrot` and a
  `Goldfish` that are both `Pet: Named` join to `Pet`. Where they share several unrelated traits,
  no one of them is chosen: the join is an error that names them, and the type is written, or
  given by the expected type. Two instances of one generic type join at a contravariant argument
  to the meet of the two arguments, where one is a subtype of the other ([Variance](#variance)).
- Two tuples of as many parts join part by part, since a tuple's parts are read out: with `x: long`,
  `same((dog, x), (cat, x))` takes `(Animal, long)`, as do `if c then (dog, x) else (cat, x)` and
  `[(dog, x), (cat, x)]`, and so does a tuple inside a covariant argument (`Some((dog, x))` and
  `Some((cat, x))` join to `Option[(Animal, long)]`). A part joins only where its types do: an
  invariant `(Array[Dog], long)` and `(Array[Cat], long)` have no join, nor have `(Dog, long)` and
  `(Cat, int)`, nor tuples of different sizes.
- A number literal among values that agree on one type (the branches of an `if`, the arms of a
  `match`, the elements of a collection literal, the keys or the values of a map literal) takes the
  type the others decide, wherever it is written, as an argument's literal does: `[long(1), 2]` and
  `[2, long(1)]` are `Vector[long]`, `[1.5, 2]` is a `Vector[real]`, `if c then 2 else big` a `long`
  for `big: long`, and `[Some(long(1)), Some(2)]` a `Vector[Option[long]]`. A value built around a
  literal (`Some(2)`, `(2, "b")`, `[2]`) and an expression of literals alone (`2 * 3`, `-5`) do the
  same; a `match` arm does so where it is an expression of literals alone. A literal that does not
  fit the type decided is an error (`[i8(1), 128]`), and literals with no other value beside them
  take the first one's type, so `[1, 5000000000]` is an error, as the `int` decided by `1` cannot
  hold the second. Values that agree on no type are reported in the order they are written.
- An argument of type `Nothing` (a `break`, a `return`, `fail(…)`) decides no type argument, since
  the call is never made: the other arguments and the expected type decide them as they would
  alone, and one that only such an argument names is `Nothing`. So `identity(break 42)` and
  `println(fail("unreachable"))` are well typed, while a `Nothing` argument beside `1` and `"one"`
  reconciles neither with the other, nor answers for a bound they break.
  Likewise a tuple or a collection literal with a part of type `Nothing` is never made, and has
  type `Nothing`.
- `None`, `[]`, `{}` and `Map.empty` take their type arguments from context. Where nothing determines
  a type, the result is `Nothing` where the type argument is only read out, as above, and otherwise
  an error asking for an annotation; never a silent default to `Any`.
- No value has type `Nothing`, so an instance whose covariant argument is `Nothing` is an instance
  with any type there, a value type's too: an `Option[Nothing]` is an `Option[int]` and an
  `Either[int, Nothing]` an `Either[int, string]`, with no conversion, in a declared type, an
  argument, a branch, and a type pattern alike.
- Nothing is generalized implicitly: a local `val f = (x) -> x` with no use that settles `x` is an
  error. Polymorphic functions are declared with type parameters.

Explicit type arguments may always be given: `parse[int](text)`, `Map.empty[string, int]`, or a
leading prefix with the rest inferred.

## Higher-kinded types

A type parameter may stand for a **type constructor**: a generic type not yet given its arguments.
`F[_]` is a parameter that takes one type argument, so `F[int]` and `F[(A, B)]` are types wherever
`F` is in scope. Bounded by one of the [standard traits](#functor-applicative-monad-traverse), one
function serves options, results, sequences and a program's own types alike:

```text run
pairUp[F[_]: Monad, A, B](fa: F[A], fb: F[B]) -> F[(A, B)] =
  for a in fa, b in fb yield (a, b)

data AgeError(text: string)

age(text: string) -> Result[int, AgeError] = text.toIntOption() match
  Some(n) -> Ok(n)
  None -> Err(AgeError(text))

main()
  println(pairUp(Some(1), Some("one")))
  println(pairUp([1, 2], ["a", "b"]))
  println(pairUp(age("30"), age("41")))
  println(pairUp(age("30"), age("old")))
```

```text output
Some((1, one))
[(1, a), (1, b), (2, a), (2, b)]
Ok((30, 41))
Err(AgeError(old))
```

Each call compiles its own copy, at `Option`, at `Vector` and at `Result[_, AgeError]`, as any
generic call does.

### Constructor parameters

- **`F[_]` declares a constructor parameter of one argument**, `F[_, _]` one of two. It is declared
  wherever a type parameter is: on a function, a method, a `data` type, a class, a trait and an impl,
  so `data Nested[F[_], A](inner: F[Option[A]])` is a type. Its arguments are types, never
  constructors: `F[_[_]]` is an error.
- **`F` is written with exactly its number of arguments**, and is then a type: `F[A]`,
  `F[Vector[int]]`. Written bare it is a constructor, which stands only where a constructor is taken:
  a type argument for another constructor parameter of its arity (`pairUp[F, A, B](…)`), and the
  receiver of a static member (`F.pure(x)`).
- **A hole may declare a variance**: `F[+_]` takes only constructors covariant at that argument, and
  in return an `F[Dog]` is an `F[Animal]` inside the declaration. A plain `_` is invariant: `F[Dog]`
  and `F[Animal]` are unrelated where nothing more is known of `F`. The mark is a demand on the
  argument, not a variance of the parameter, so a function's and an impl's constructor parameters
  write it too; the parameter itself is invariant.
- **A hole may carry bounds**: `F[_: Sendable]` is applied only to types that meet them, which is
  checked wherever the declaration writes `F[X]`, and in return it takes constructors whose
  parameter at that hole has those bounds, `Future` and `TreeSet` (`F[_: Ord]`) among them. A
  variance comes first, as for any parameter: `F[+_: Ord]`.
- **A constructor parameter's bounds are [constructor traits](#constructor-traits)**:
  `[F[_]: Monad]`, `[F[_]: Traverse & Monad]`. `[F[_]: Ord]` is an error ("`Ord` is a bound for a
  type, and `F` is a constructor"), as is `[T: Monad]` ("`Monad` is a bound for a constructor of one
  argument; declare `T[_]`").

### Constructor arguments

What a parameter `F[_]` takes, written or inferred:

- **A generic type named bare** that has as many type parameters as `F` takes holes: a data type, a
  class or a trait, `Option`, `List`, `Seq`. A tuple or a function type is no constructor.
- **A partial application**: the type named with `_` in place of the arguments left open, as many as
  `F` takes, the others written out: `Result[_, ParseError]`, `Either[string, _]`,
  `Map[string, _]`. The holes are filled in order, so `Result[_, ParseError]` applied to `int` is
  `Result[int, ParseError]`. A `_` is a direct argument of the type named: `Option[List[_]]` is an
  error, since only the type's own arguments are left open.
- **A generic alias** of as many parameters, whose expansion is a type with each parameter as one of
  its direct arguments: with `type Parsed[T] = Result[T, ParseError]`, `Parsed` is the constructor
  `Result[_, ParseError]`. An alias whose parameter sits deeper, or twice, is no constructor.
- **Another constructor parameter** of that arity, or a partial application of one with more
  (`G[int, _]` for a `G[_, _]`).

- **A constructor is not a type.** A value, a field, a parameter, a result or a type pattern of type
  `Option` or `Result[_, string]` is an error saying the type takes arguments; a type where a
  constructor is taken is an error too (`pairUp[int, …]`: "`int` is a type; `F[_]` takes a
  constructor of one argument, such as `Option`"), and so is one of the wrong arity (`Result` for
  `F[_]`: "`Result` takes two arguments and `F` one; leave one open, as `Result[_, E]`").
- **Two constructors are one** when they name one type with the same arguments written out at the
  same places: `Parsed` and `Result[_, ParseError]` are one constructor, and so one instance.
- **A type's bounds at a hole must be the hole's**: a type whose parameter there is bounded, as
  `TreeSet[A: Ord]`'s is, is a constructor only for a hole whose bounds imply that one (`F[_: Ord]`),
  never for a plain `_`, which generic code may apply to any type. `Eq` and `Hash` are no obstacle,
  since every type meets them. The arguments written out are held to the type's bounds where they
  are written, as any type argument is.
- **An application of a constructor is a reference type**: its head is a data type, a class or a
  trait, so an `F[A]` passes as an `Any` with no boxing in every instance.

### Constructor traits

```text
trait Functor[A]
  map[B](f: (A) -> B) -> Self[B]
```

**A trait whose methods write `Self` with type arguments is a constructor trait**: what implements it
is a constructor, and `Self[B]` is that constructor applied to `B`. The trait's parameters are the
holes: `Self` takes as many arguments as the trait has parameters, and a value of a conforming type
is a `Self[A]`, `self` among them.

- **Its parameters take no variance**, and bare `Self` is an error in it. Like any trait whose
  methods mention `Self`, it is a bound and never a type.
- **A bound on one of its parameters is the bound of that hole**: `trait SendableMonad[A: Sendable]`
  is the bound of an `F[_: Sendable]`, and is implemented by constructors whose parameter there is
  bounded by no more than `Sendable`, an unbounded one among them (`Future[T]`): the trait applies
  the constructor only to types meeting the bound, so the conformance asks nothing of the type's
  parameter. A trait requiring it bounds its own parameter as it does.
- **A trait requiring a constructor trait is one**, and names it at its own parameters, in order:
  `trait Monad[A]: Applicative[A]`. It may require a trait for types that bounds constructors, the
  same way: `trait Traverse[A]: Functor[A], Foldable[A]`.
- **A trait for types whose members never name `Self` beyond the receiver bounds constructors
  too**, where it has parameters, no static member, and requires only such traits: `Foldable[A]`
  is one. A constructor conforms to it where each of its applications does with the trait's
  parameters at the holes, so `impl Foldable for Option`, or any conformance stating
  `Foldable[T]` for an `Option[T]`, makes `Option` a constructor for `[F[_]: Foldable]`, and a
  value of `F[X]` there has its members with `X` for the trait's parameters. It stays a trait for
  types in every other place, so a `Foldable[int]` is a type, and a type conforming to it at no
  parameter of its own (`impl Foldable[int] for Ints`) is no constructor of it. A trait naming
  `Self` otherwise (`same(other: Self)`) or declaring a static member bounds types only, and one
  writing `Self[X]` is a constructor trait and bounds constructors only.
- **A static member names no parameter of the trait**, since a call of it names only the
  constructor: `static pure[X](value: X) -> Self[X]`. It is called on a constructor, a bounded
  parameter (`F.pure(1)`), a type named bare (`Option.pure(1)`) or a partial application
  (`Result[_, string].pure(1)`), each call direct. A constructor has the static members of the
  constructor traits it implements at exactly the holes it leaves open, so `Result[int, _].pure`
  is an error, as is `F.pure` where no bound of `F` declares `pure`; a value of `F[X]` has none,
  its static members being its constructor's. Named without a call, `F.pure` and `Option.pure` are
  function values.
- **Its default bodies are compiled for each constructor that implements it**, as a call through a
  bound is, so unlike other traits' defaults they may call a static member on `Self`:
  `override map[B](f: (A) -> B) -> Self[B] = flatMap(a -> Self.pure(f(a)))`.
- **A value of `F[X]`, for `F[_]` bounded by the trait, has the trait's members** with `Self` as `F`
  and the trait's parameters as `X`, so `fa.map(f)` is an `F[B]`. A `for` comprehension draws from
  it by its bounds, [nominally](/spec/collections/#for-comprehensions): the last generator needs
  `Functor`, an earlier one `Monad` and a guard `Filter`, so with `F[_]: Functor` alone a second
  generator is an error naming the bound it needs.
- A constructor trait of more parameters works the same way: `trait Bifunctor[A, B]` writes
  `Self[C, D]`, is the bound of an `F[_, _]`, and is implemented with two holes.

### Implementing a constructor trait

```text
impl[T, E] Monad[T] for Result[T, E]
  static pure[X](value: X) -> Result[X, E] = Ok(value)

impl Monad for Option
  static pure[X](value: X) -> Option[X] = Some(value)
```

- **The impl names the type's parameters, in order, as an impl of a trait always does, and gives
  the trait the ones it leaves open**: each trait argument is a different parameter of the type, and
  those are the holes, the others written out. `impl[T, E] Monad[T] for Result[T, E]` makes `Self`
  the constructor `Result[_, E]`, and `impl[L, R] Monad[R] for Either[L, R]` makes it
  `Either[L, _]`.
- **Where the trait's arguments are all the type's parameters, in order, both may be left off**:
  `impl Monad for Option`, `impl Traverse for List`. The type's parameters are then in scope by the
  names its declaration gives them.
- **The written-out parameters may be bounded further**, making the constructor conform only where
  they meet the bounds, as any impl may (`impl[T, E: Display] Monad[T] for Report[T, E]`); the holes
  take no bounds.
- **A type implements a constructor trait once**, at one choice of holes: an `Either` that is a
  `Functor` on its right is not one on its left. The conformance is the constructor's own: a
  subclass, or a type conforming to a trait that has one, does not inherit it, and is matched as the
  type that does ([inference](#inferring-a-constructor)). It lives in the module of the trait or of
  the type, as every conformance does.
- **The type's own methods implement the trait's wherever the trait's call is a call of them.** A
  trait method is met by the type's method of its name whose parameters take the trait method's
  parameter types with no conversion, whose own type parameters that call decides (a lower bound by
  its bound, so `Result`'s `flatMap[U, F >: E]` meets `Monad`'s `flatMap` with `F` as `E`), whose
  result is the trait's or conforms to it, and which has no bound the trait's method lacks. So
  `List`'s `flatMap(f: (T) -> Iterable[U]) -> List[U]` meets `flatMap[B](f: (A) -> List[B]) ->
  List[B]`, and `impl Monad for List` gives only `pure`. A method of that name that does not meet it
  is an error naming what it lacks (`TreeSet`'s `map` bounds `B` by `Ord`), and so are two
  overloads that both meet it; a method the type does not have is written in the impl, and is then
  the type's own (`Option` gains `foldLeft`). The methods that meet are the type's own, never a
  superclass's.
- **A call through the bound calls the meeting method directly** in each instance, at the type
  arguments the meeting decided.

A program's own type implements the family by giving `pure` and `flatMap`; `map` and `map2` come
from `Monad`'s defaults:

```text run
data Parser[A](run: (string) -> Option[(A, string)])

impl Monad for Parser
  static pure[X](value: X) -> Parser[X] = Parser(s -> Some((value, s)))

  flatMap[B](f: (A) -> Parser[B]) -> Parser[B] =
    Parser(s -> run(s).flatMap(match (a, rest) -> f(a).run(rest)))

pairUp[F[_]: Monad, A, B](fa: F[A], fb: F[B]) -> F[(A, B)] =
  for a in fa, b in fb yield (a, b)

val digit = Parser(s -> s.headOption.filter(c -> '0' <= c <= '9').map(c -> (int(c) - int('0'), s.drop(1))))

val twoDigits = pairUp(digit, digit).map(match (a, b) -> a * 10 + b)

main()
  println(twoDigits.run("42!"))
  println(twoDigits.run("4x"))
  println(Parser.pure(7).run("rest"))
```

```text output
Some((42, !))
None
Some((7, rest))
```

### Inferring a constructor

- **A constructor parameter is inferred where an argument's type or the expected type is an
  application of it**, by matching `F[X]` against that type, alongside the call's other type
  arguments and in the same tiers.
- **With a constructor-trait bound, the type's conformance says which arguments are the holes**:
  `Result`'s `Monad` leaves `T` open, so a `Result[int, AgeError]` makes `F` `Result[_, AgeError]`
  and `X` `int`. A type with no conformance of its own is matched as the nearest superclass that
  has one, a `TallShelf[int]` as a `Shelf[int]`. A type with none is an error naming the bound and
  the type. With several bounds, the conformances must leave the same arguments open; a type that
  implements one bound at one hole and another at another is an error naming both.
- **A subclass passes as its superclass only where a value is read out**, as for any type
  argument: a `Box[TallShelf[int]]` given for a `Box[F[int]]` is refused where `Box` is invariant,
  since the generic code may store a `Shelf` in it.
- **Without a bound**, a type of exactly as many parameters as `F` takes holes gives `F` that type
  bare; a type of more is an error asking for `F` to be written, since nothing says which arguments
  are open.
- **The arguments written out are inferred as type arguments are**: each argument naming `F`
  contributes, and where the type's parameter is covariant they join, so `Ok(1)`, a
  `Result[int, Nothing]`, beside a `Result[int, AgeError]` makes `F` `Result[_, AgeError]`, and a
  `Result[int, Dog]` beside a `Result[int, Cat]` makes it `Result[_, Animal]`; at an invariant
  parameter the first decides. An expected type decides them first, as it decides any type
  argument. The type named must agree: an `Option` and a `List` for
  one `F` is an error naming both, never a join to a trait they share, which a written argument
  asks for where that trait conforms to the bound: `both[Seq]([1, 2], List(3))` for a
  `both[F[_]: Foldable](a: F[int], b: F[int])`.
- **The expected type decides `F` as an argument does**: with
  `wrap[F[_]: Applicative, A](a: A) -> F[A] = F.pure(a)`, `val o: Option[int] = wrap(3)` takes
  `F` as `Option`; with nothing expected and no argument naming `F`, the call is an error asking for
  it, `wrap[Option](3)`.
- A lambda's result decides a constructor as any type argument it yields does: in
  `words.traverse(_.toIntOption())` the lambda yields an `Option[int]`, so `G` is `Option`.

### Impls over a constructor parameter

An impl without `for` may add methods to the applications of a bounded constructor parameter,
`impl[F[_]: Monad, A] F[A]`, or to a type built from them,
`impl[F[_]: Traverse, G[_]: Applicative, A] F[G[A]]`. The receiver's static type is matched against
it as for any [type built from parameters](declarations.md#impl), each constructor found through its
bound's conformance as [inference](#inferring-a-constructor) finds it, so a `List[Option[int]]`
matches `F[G[A]]` with `F` as `List`, `G` as `Option` and `A` as `int`. Every rule of impls adding to
a type built from parameters holds: the type's own methods are found first, such an impl in another
module is an [extension](declarations.md#extensions), seen where its module is imported, and two
that add one name and parameter types to a receiver both match make a call of it an error.

- **It belongs to the trait bounding the constructor it applies**: written in the module of one of
  `F`'s bounds it is seen everywhere, as an impl adding to that trait is, and elsewhere it is an
  extension. Like an impl adding to a trait, it never hides or clashes with a method the type has:
  the type's own method, or the bound's, of the name and parameter types is called, and generic code
  that sees only the bound calls the added one. An extension adding a method the bound declares is
  an error where it is declared.
- **A value of `F[X]` in generic code has the methods** where `F`'s bounds include the impl's: in
  `go[F[_]: Monad](x: F[int]) = x.twice(f)` each instance calls the impl's `twice` at its constructor.
- **A receiver no such impl matches is told why**: `twice` on a `Vector[int]`, where `Vector`
  implements no `Monad`, says so; on an `F[int]` whose `F` lacks the bound, that it is bounded by no
  `Monad`. A further bound on the element (`impl[F[_]: Monad, A: Display] F[A]`) is reported as any
  impl's is.
- An unbounded constructor parameter matches as [inference](#inferring-a-constructor) matches one: a
  type of exactly as many arguments as it has holes, named bare. Such an impl states no conformance
  (`impl[F[_]: Monad, A] Display for F[A]` is an error) and declares no static member.

### Constructors and reification

Each instance of generic code knows every constructor argument, so `F[A]` in it is the type written
out, laid out and reified as that type is: an `F[int]` at `List` stores unboxed `int`s, its header
names `List[int]`, and a type pattern `x: F[int]` is checked against the object's exact type. The
copies are one per list of type arguments, constructors included: `pairUp` at `Option` and at
`Result[_, AgeError]` are two, and at `Parsed` and `Result[_, ParseError]` one.
[Polymorphic recursion](#polymorphic-recursion) counts constructors too: a cycle that passes
`Result[_, Nest[E]]` for a constructor parameter where it received `Result[_, E]` grows without end
and is rejected.

### Why it is sound

Every rule above is there so that generic code checked once is correct at every instance, with no
check left for run time and no unchecked conversion anywhere:

- **Kinds.** Every type written is checked for its kind where it is written, so a constructor never
  reaches a place a value's type is taken, and every application has exactly its arity. Substituting
  a constructor for `F` therefore turns each `F[X]` into a type that is well formed by the rules for
  types without constructors, and nothing else changes.
- **Holes are direct arguments.** A constructor is one nominal type with some of its own arguments
  left open, so applying it is substitution, two constructors are equal exactly when they look
  equal, and inference has at most one answer. There is no computation at the type level to
  diverge or to disagree between the checker and the instances.
- **Hole variance.** `F[Dog]` is an `F[Animal]` only where the hole is `+_`, and only a constructor
  covariant at that argument is accepted for it, so the subtyping assumed inside the declaration is
  the subtyping the [variance](#variance) rules grant each instance. A plain `_` assumes none. The
  arguments written out of a partial application keep their own type's variance, as in any type.
- **Bounded holes.** Every `F[X]` a declaration writes is checked against the hole's bounds where it
  is written, and a constructor is accepted for the hole only where the hole's bounds imply its own
  parameter's, so no instance applies a type to an argument its declaration refuses, and a generic
  definition's errors are still reported at the definition.
- **Coherence.** A type implements a constructor trait once, at one choice of holes, in the module
  of the trait or the type, so the constructor inferred for an argument is unique and the same in
  every file, and each instance's calls through the bound reach one implementation.
- **Meeting.** An existing method implements a trait's only where the trait's call, at every type
  argument, is an ordinary well-typed call of it with no conversion and no bound the trait does not
  give. The call each instance makes is one the checker could have written out.
- **No inherited conformance.** A subclass's inherited `map` answers the superclass's type, so if it
  inherited the conformance, `Self[B]` would claim a subclass instance the method never makes; it is
  matched as the superclass instead, which is exactly what it is.
- **Static members and defaults per constructor.** A static member is reached only through a
  constructor that is known in each instance, and a default body is compiled once per implementing
  constructor, so `Self.pure` always names one implementation.
- **Traits for types at holes.** A trait for types that bounds constructors names no `Self` beyond
  the receiver and has no static member, so its members read the same from every application, and
  a constructor conforms to it only where a conformance states the trait's parameters as the
  type's own. Every `F[X]` an instance makes is then a `Foldable[X]` by an ordinary conformance, and
  a call through the bound is the ordinary call on that type.
- **A bound, never a type.** No value has a constructor trait as its type, so there is no object
  whose `Self` is unknown, no dynamic dispatch over constructors and no existential to open. Each
  instance is monomorphized, its values reified at their exact types, and a downcast stays a
  type-checked `match`.

## Functor, Applicative, Monad, Traverse

The core library declares the standard constructor traits, in module `avin`, and `Foldable`, a
trait for types that bounds constructors as well, since its method never writes `Self`:

```text
trait Functor[A]
  map[B](f: (A) -> B) -> Self[B]

trait Foldable[+A]
  foldLeft[B](initial: B, f: (B, A) -> B) -> B

trait Applicative[A]: Functor[A]
  static pure[X](value: X) -> Self[X]
  map2[B, C](other: Self[B], f: (A, B) -> C) -> Self[C]

trait Monad[A]: Applicative[A]
  flatMap[B](f: (A) -> Self[B]) -> Self[B]
  override map[B](f: (A) -> B) -> Self[B] = flatMap(a -> Self.pure(f(a)))
  override map2[B, C](other: Self[B], f: (A, B) -> C) -> Self[C] =
    flatMap(a -> other.map(b -> f(a, b)))

trait Filter[A]: Functor[A]
  filter(p: (A) -> bool) -> Self[A]

trait Traverse[A]: Functor[A], Foldable[A]
  traverse[G[_]: Applicative, B](f: (A) -> G[B]) -> G[Self[B]]

impl[F[_]: Traverse, G[_]: Applicative, A] F[G[A]]
  sequence() -> G[F[A]] = traverse(x -> x)
```

A value that crosses to another thread is `Sendable`, and a function run on another thread is
`@sendable`, so a constructor whose combinators run on a pool thread, as `Future`'s do, meets the
sendable pair instead, whose holes and functions carry those marks:

```text
trait SendableFunctor[A: Sendable]
  map[B: Sendable](@sendable f: (A) -> B) -> Self[B]

trait SendableMonad[A: Sendable]: SendableFunctor[A]
  static pure[X: Sendable](value: X) -> Self[X]
  flatMap[B: Sendable](@sendable f: (A) -> Self[B]) -> Self[B]
```

`impl SendableMonad for Future` gives only `pure` (`Future.successful`); `Future`'s own `map` and
`flatMap` meet the rest.

The impls are for concrete types only, one per row; code generic over sequences takes an
`F[_]: Monad` (or the trait it needs) rather than a `Seq`, which as a trait for types has no impl of
a constructor trait:

| Constructor | `Functor` | `Foldable` | `Applicative`, `Monad` | `Filter` | `Traverse` |
| --- | --- | --- | --- | --- | --- |
| `Option` | yes | yes | yes (`pure` is `Some`) | yes | yes |
| `Result[_, E]` | yes | yes | yes (`Ok`) | no: no error to give | yes |
| `Either[L, _]` | yes | yes | yes (`Right`) | no | yes |
| `List`, `Vector`, `ArraySeq`, `Queue` | yes | yes | yes (one element) | yes | yes |
| `LazyList` | yes | yes | yes | yes | no: an infinite list never finishes |
| `Array`, `Buffer`, `Deque` | yes | yes | no: mutable | yes | no |
| `HashSet`, `OrderedSet`, `TreeSet` | no: `map` makes a `Set` | yes | no | no | no |

- **The traits are nominal.** A type is a `Functor` because an impl says so, and only then: a type
  with a `map` of the right shape and no `impl Functor` is no `Functor`, cannot be passed for an
  `F[_]: Functor`, and is no source of a [`for` comprehension](/spec/collections/#for-comprehensions)
  unless it is an `Iterable`.
- **Every `Iterable[A]` is a `Foldable[A]`**: `Iterable` requires it and gives `foldLeft`, so every
  collection of one parameter is a constructor for `[F[_]: Foldable]` with no impl of its own, the
  sets and the sorted collections too, where the hole promises their bound (`TreeSet` for an
  `F[_: Ord]`). `Option`, `Result` and `Either` gain `foldLeft` in their impls of `Traverse`.
- **The traits add no method a collection already has and change none.** `List`'s own `map`,
  `flatMap` and `foldLeft` implement them, so `xs.map(f)` on a `List` is the call it always was, with
  the result it always had; the impls write only what the types lack: `pure`, `traverse`, and
  `foldLeft` on `Option`, `Result` and `Either`. `map2` is `Monad`'s, from `flatMap`, and `sequence`
  is added to every traversable of applicatives.
- **A sequence's `map2` and `flatMap` pair every element with every element**, as a two-generator
  `for` does; `zip` still pairs them by position, and is no trait's.
- **`traverse` and `sequence` answer the first `None`, `Err` or `Left`** where there is one, and
  otherwise the applicative of all the results. A sequence's `traverse` gathers through `map2`, so
  `f` is applied to every element, in order, and the first failure decides the result; it is the
  result that stops at the first failure, never the evaluation. `flatten` keeps the values that are
  present and drops the rest:

```text run
data Line(name: string, size: int)

data ParseError(line: string)

parseLine(text: string) -> Result[Line, ParseError] = text.split(" ") match
  [name, size] -> size.toIntOption().map(n -> Line(name, n)).okOr(ParseError(text))
  _ -> Err(ParseError(text))

main()
  println(["1", "2"].traverse(_.toIntOption()))
  println(["1", "two", "x"].traverse(_.toIntOption()))
  println([Some(1), None].sequence())
  println([Some(1), None].flatten)
  println(["a 1", "b 2"].traverse(parseLine))
  println(["a 1", "b", "c x"].traverse(parseLine))
```

```text output
Some([1, 2])
None
None
[1]
Ok([Line(a, 1), Line(b, 2)])
Err(ParseError(b))
```

- Not implemented, each for a reason a reader can see in its signature: `Map` (its `map` takes the
  entries, so it folds them as an `Iterable[(K, V)]` and is no constructor), the sets and the sorted
  collections as functors (their `map` makes a `Set` and needs `Eq & Hash` or `Ord` of its result),
  `Iterator` as a functor (single-pass), `Range` (its `map` makes a `Vector`, not a range), `Seq`
  and `Set` (traits for types, which no impl of a constructor trait is for) and `Future` (its `map`
  needs a `Sendable` result and a `@sendable` function, so it is a `SendableMonad` instead);
  `Future.traverse`, a static member taking a collection, is unrelated and stays.
- **Each implementation keeps the laws**, which the library's tests check for every type in the
  table: `map(x -> x)` is the value, `map(f).map(g)` is `map(x -> g(f(x)))`; `pure(a).flatMap(f)` is
  `f(a)`, `m.flatMap(Self.pure)` is `m`, and `flatMap` is associative.

## Scope of the type system

The type system stays small: generics with bounds, constructor parameters bounded by constructor
traits, subtyping with declaration-site variance, traits implemented through `impl`,
[union types](/spec/unions/) written in annotations,
[opaque types](/spec/declarations/#opaque-types), new types laid out as an existing one, and
[refined variants](/spec/refined-variants/), whose `match` arms learn type equalities. It has no type lambdas beyond the partial
application of one type's own arguments, no constructors of constructors, and no type members,
inferred unions, implicit conversions or macros. Intersections exist only as
[bounds](#bounds), `[T: A & B]`: there is no intersection type to annotate a value with, so a value
of two traits at once is always a bounded parameter's, monomorphized at its type argument. A
context a call needs (a transaction, a logger) may be passed as a
[contextual parameter](/spec/contextual-parameters/), filled from a `given` in scope; that is a value
and never a conformance: a bound is met only by the type's `impl`, whatever givens are in scope, and
a contextual parameter's type names no type parameter. The
[collections chapter](/spec/collections/) shows how the library gets precise result types without
constructor parameters; the standard constructor traits stand beside those methods rather than
replacing them.
