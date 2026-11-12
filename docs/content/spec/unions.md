---
title: Union types
summary: "A | B written in annotations: an anonymous tagged sum, made at a single site, taken apart by match, and the way ? combines distinct errors."
weight: 14
---

A **union type** `A | B` holds a value of `A` or a value of `B`. It is written in annotations only,
never inferred; it compiles to an anonymous tagged sum; a `match` takes it apart by type, and must
cover every member; and `?` uses it to pass on errors of several types from one function.

```text
type Num = int | long

describe(x: int | string) -> string = x match
    n: int -> s"number $n"
    s: string -> s"text $s"

load(path: string) -> Result[Config, IoError | ParseError]
    val text = readText(path)?           // a Result[string, IoError]
    val doc = parseDocument(text)?       // a Result[Document, ParseError]
    configOf(doc)

describe(3)          // the int becomes an `int | string` at the argument
```

A union is the light choice between types a program already has. A choice that has a name of its
own, variants of its own or methods is a `data` type, and a value of any type at all is an `Any`.

## Writing a union

A union is written wherever a type is written by hand: a parameter, a result, a `val` or `var`
annotation, a field of a data type or a class, a type argument, a part of a tuple or a function
type, and the body of a [type alias](/spec/declarations/#type).

```text
data Cell(content: int | string)
val cells: Vector[int | string] = [1, "two"]
val pair: (int | string, bool) = (1, true)
f[int | string](x)
type Key = int | string
```

**`|` binds more tightly than `->` and more loosely than everything else in a type.** So:

- `(A) -> B | C` is a function returning `B | C`, and `A | B -> C` a function taking an `A | B`.
  A union of function types puts each in parentheses: `((int) -> int) | string`.
- `Option[A | B]` is an option of a union, and `Option[A] | B` a union one of whose members is an
  option; the two are different types.
- `[]A | B` is `Vector[A] | B`, since the element of `[]` is a name or a parenthesized type;
  `[](A | B)` is a vector of the union.
- `@sendable (A) -> B | C` marks the function type, whose result is the union.
- Parentheses group as anywhere in a type: `(A | B)` is `A | B`.

A union written in a type pattern is parenthesized, as a function type there is, since a `|` in a
pattern separates [alternatives](/spec/operators-and-control-flow/#match): `x: (int | string) ->`
binds a union, and `_: int | _: string ->` is two alternatives, which bind nothing. A type pattern
followed by `|` and a type name is reported with the parenthesized form to write instead:

```text
v match
    x: int | string -> …     // error: a union in a type pattern is written in parentheses,
                             // `x: (int | string)`
```

**The parser tells the three uses of `|` apart by position.** Where a type is expected (after the
`:` of a parameter, a field, a binding or a type pattern, after `->` in a signature, inside the
brackets of type arguments, after `type Name =`) it is a union. In a pattern it separates
alternatives, and in a clause head after the arguments it begins a
[guard](/spec/declarations/#clauses). Everywhere else it is the
[bitwise or](/spec/operators-and-control-flow/#operators), or the method `|` of a type that has one.
The positions never overlap: an expression never stands where a type does, and an explicit type
argument list `f[int | string](x)` holds types. A union breaks across lines only inside brackets or
parentheses, after a `|`.

## Normal form

A union is a set of members. Before anything else is decided about it, it is brought to a normal
form, so that two unions that hold the same values are one type:

- **Nested unions flatten**: `A | (B | C)` is `A | B | C`, and so is a member that is an alias of a
  union (`Num | string` with `type Num = int | long` is `int | long | string`).
- **Order does not matter**: `A | B` and `B | A` are one type, so two overloads differing only in
  the order are the same overload, and a `Map[int | string, V]` is a `Map[string | int, V]`.
- **A member that converts to another member is absorbed into it**, at a single site without a
  change of meaning, as a subtype does and as a value boxes into a trait it implements: `Dog | Animal`
  is `Animal`, `int | Display` is `Display`, `A | Any` is `Any`, and `A | Nothing` is `A`.
- **A union left with one member is that member**: `int | int` is `int`.

Written out by hand, each of these but flattening and order is a mistake, and an error that names
the type to write:

```text
val a: int | int = 1              // error: `int` is named twice in this union
val b: Dog | Animal = rex         // error: a `Dog` is already an `Animal`, so `Dog | Animal` is
                                  // `Animal`; write `Animal`
val c: string | Any = "x"         // error: every member converts to `Any`, so this union is `Any`;
                                  // write `Any`
val d: int | Nothing = 1          // error: `Nothing` adds no value to a union; write `int`
```

Where the members come together through a type argument or an alias, the same cases are absorbed
silently, since nothing written was wrong: `T | int` at `T = int` is `int`, `T | Animal` at
`T = Dog` is `Animal`, `T | string` at `T = int | bool` is `int | bool | string`, and
`Key | string` with `type Key = int | string` is `int | string`.

Two members that are both supertypes of some third type stay apart: `Named | Pet`, for unrelated
traits `Named` and `Pet`, is a union of two members, even though a `Parrot` may be both (see
[making a union value](#making-a-union-value)). Instances of one generic type at different
arguments are different members: `Option[int] | Option[string]` has two.

A recursive union is a data type: `type Json = Vector[Json] | string` is an alias naming itself, an
error as it is for [any alias](/spec/declarations/#type). A union may name a data type that names
the union back: `data Node(kids: Vector[Node | string])`.

Compiler messages print a union with its aliases expanded and its members flattened, in the order
they were written: `int | long | string`.

## Unions are written, never inferred

Inference never forms a union from values of different types. The branches of an `if` and the arms
of a `match` join as they always have, and two that agree on no type are an error, which now names
the union to declare where both are wanted:

```text
val x = if ok then 1 else "none"
// error: the branches of this `if` agree on no type: `int` and `string`; to keep either, declare
// the type it yields, such as `int | string`

val y: int | string = if ok then 1 else "none"      // each branch becomes the union
val zs: Vector[int | string] = [1, "two", 3]         // each element too
```

The same holds for collection literals, generic calls and lambdas: `[1, "two"]` is an error, and
`pair[T](a: T, b: T)` called as `pair(1, "one")` is an error, not a `pair[int | string]`.

Inference does **carry** a union it is given. A value already of a union type gives it as any type
is given: `val u2 = u` is an `int | string`, `Some(u)` an `Option[int | string]`, and `id(u)` takes
`T = int | string`. Where a union and one of its members meet, they join to the union, as a class
and its superclass join to the superclass: `if c then u else 3` is an `int | string`.

A type parameter that is a member of a union in a parameter's type takes what the argument leaves
over once the other members are taken out:

```text
orText[T](x: T | string) -> string = …

orText(3)          // T = int
orText("a")        // `string` is a member already: T is not decided here, and with nothing else
                   // to decide it the call is an error naming `T`
orText(u)          // u: int | bool | string, so T = int | bool
```

## Making a union value

A value becomes a union value where a union is expected of it, at a single site, as a value becomes
an `Any`: an argument, a binding, an assignment, a field, a result, a part of a tuple written out,
each branch of an `if` or `match` expected to yield the union, each element of a collection literal,
and a field given in [`with { … }`](/spec/declarations/#changing-fields-with). This is the
**injection** of the value into the member it belongs to:

- A value whose type is a member is that member.
- Otherwise it is the one member its type converts to: a `Dog` given to an `Animal | int` is the
  `Animal`, and a `(string, int)` given to `(Any, int) | bool` is the tuple.
- A value that converts to several members is an error naming them, since the program has to say
  which one it means:

  ```text
  val p: Named | Pet = Parrot("polly")
  // error: a `Parrot` is both a `Named` and a `Pet`, so it could be either member of
  // `Named | Pet`; give it the type you mean first, as in `val n: Named = …`
  ```

- An integer or real literal is its own default type where that is a member (`int`, then `long`
  for a literal too large for `int`; `real`), otherwise the one numeric member it fits; where it
  fits several and its default is none of them, the error names them:
  `val n: byte | long = 3` is an error, and `val m: int | long = 3` is the `int`.
- `None`, an empty collection literal and `Ok(x)` take their type from the member they are given to,
  where exactly one member is an instance of their type: `val o: Option[int] | string = None` is an
  `Option[int]`.
- A union value given where a wider union is expected is the same value in the wider union:
  `int | string` converts to `int | string | bool`.
- A value of another union converts only where each of its members converts to a member of the
  target.

A value already made converts only where it already has the target's layout, as
[`Any`](/spec/types/#any-and-nothing) does: see [representation](#representation). A
`Vector[Dog]` is a `Vector[Dog | Cat]` with no conversion, and a `Vector[int]` is not a
`Vector[int | string]`.

**Overloads.** A union parameter accepts every value that injects into it, and an overload whose
parameter is the argument's own type is more specific than one taking a union it belongs to, just as
it is more specific than one taking a trait. A union is more specific than any type its members all
convert to:

```text
show(n: int) -> string = "int"
show(x: int | string) -> string = "int or string"
show(x: Any) -> string = "any"

show(1)            // int
show("a")          // int or string
show(true)         // any
```

Two unions neither of which converts to the other leave a call ambiguous where the argument fits
both: with `f(x: int | string)` and `f(x: int | bool)`, `f(1)` is an error listing them, and
`f(u)` with `u: int | string` calls the first. A union argument does not choose among overloads of
its members: `width(n: int)` and `width(s: string)` called with an `int | string` is an error, since
overloads are chosen while compiling and the member is known only while running; the message
suggests a `match`.

**References to named functions.** A named function or method written where a function type is
expected whose parameters inject into its union parameters is made at that site as a function that
injects them, as a lambda would be: `val f: (int) -> string = describe` with
`describe(x: int | string)`. A function value already made converts only where its layout already
fits, by the variance of [function types](/spec/generics/#variance).

## Subtyping

A union takes part in subtyping by its members:

- **`A <: A | B`**: a member converts to the union.
- **`A | B` converts to `C` exactly when each member does**: `Dog | Cat` is an `Animal`, `int | long`
  an `Any`, and `int | string` a `Display`.
- **`A | B` converts to `C | D` exactly when each member converts to a member of `C | D`.**
- **Through generics, variance holds where the layouts agree**, as the
  [variance rules](/spec/generics/#variance) already say for `Any`: where every member of the union
  is a type an `Any` holds without a box (a string, a tuple, a function, a data type, a class, a
  trait or `Any`), the union is laid out as an `Any` is, and a covariant argument converts through it
  with no conversion: `List[Dog] <: List[Dog | Cat]`, `Option[Dog | Cat] <: Option[Animal]`,
  `Result[T, IoError] <: Result[T, IoError | ParseError]`. A union with a member that would need a box
  (a number, a `char`, a `bool`, `unit`) is a value laid out by its tag, so `List[int]` is not a
  `List[int | string]`, nor is a `List[int | string]` a `List[int | string | bool]`.
- **Function types** follow the same terms: `(Dog | Cat) -> int` is a `(Dog) -> int`, and
  `() -> Dog` a `() -> Dog | Cat`; `(int | string) -> int` is not an `(int) -> int` as a value already
  made, though a named function is made into one where it is written, as above.

## Taking a union apart

### match

A `match` on a union value takes it apart by type, and must cover every member. A type pattern
covers a member when every value of the member matches it:

```text
data Token(text: string)

render(v: int | string | Token) -> string = v match
    n: int -> s"$n"
    s: string -> s
    Token(t) -> t
```

- **Type patterns** test the member, as they test any value: `n: int`, `_: string`.
- **A supertype of a member** covers that member: `a: Animal` on a `Dog | int` covers the `Dog` and
  binds it as an `Animal`.
- **A subtype of a member** covers part of it, by the usual rules for that type: `d: Dog` on an
  `Animal | int` needs another arm for the rest of the animals.
- **A sub-union pattern** covers each member it names and binds the smaller union:
  `x: (int | string) -> …` on an `int | string | bool` leaves the `bool`.
- **Any other pattern applies to the one member it can match.** A literal, a range, a comparison
  pattern, a constructor, a list or sequence pattern, a tuple and an extractor are each checked
  against the member they belong to, and count toward covering that member alone:

  ```text
  sign(v: int | string) -> string = v match
      n < 0 -> "negative"          // the int member
      0 -> "zero"
      _: int -> "positive"
      "" -> "empty"                // the string member
      _: string -> "text"
  ```

  A pattern that more than one member could match is an error that names them and suggests a type
  pattern first: `Some(x)` on an `Option[int] | Option[string]` must be written
  `o: Option[int]` and matched again, or `Some(n: int)`.
- **A type pattern that names no member and no type a member could hold** is an error, as a type
  pattern that can never match is on any value:

  ```text
  v match
      b: bool -> …      // error: `bool` is no member of `int | string`, so this pattern can never
                        // match
  ```

- **`_` and a bare binding** cover every member; a binding binds the union.

A `match` that leaves a member uncovered names one value it leaves out, as a pattern:

```text
kind(v: int | string | bool) -> string = v match
    _: int -> "int"
    _: string -> "string"
// error: this `match` does not cover every `int | string | bool` value, such as `_: bool`
```

Unreachable arms are errors as anywhere: `_: int` after an arm that already covered the int member.
Where a union of reference members holds a value that two arms' types both match (a `Parrot` in a
`Named | Pet`, tested by `n: Named` and then `p: Pet`), the first arm in order takes it, as for any
value; the program cannot tell which member the value was made as, and need not.

A function's [clauses](/spec/declarations/#clauses) take a union parameter apart the same way, each
clause naming a member:

```text
size(x: int | string) -> int
size(n: int) = n
size(s: string) = s.charCount
```

### is

`x is P` tests a union value against any pattern a `match` arm takes, and binds the names `P` binds
where the test is known to have matched, as [`is`](/spec/operators-and-control-flow/#is) does on any
value:

```text
lengthOf(v: int | string) -> int
    if v is s: string then return s.charCount
    0
```

The union value itself keeps its type: `if v is _: string then v.charCount` is an error, since `v`
is still an `int | string`; the pattern binds the narrower value to a name of its own.

### Generic members

A `match` in generic code is checked once, where it is written, against the union as written:
`T | int` is two members there, and an arm for each is required. At an instance where the members
come together (`T = int`), each value is matched by the first arm whose pattern it fits, so the arm
for `T` takes every `int` and the arm for `int` is never reached at that instance, which is no error.

## What a union value can do

**Equality and hashing.** Every union compares with `==` and hashes, since every type does
(see [equality](/spec/equality-and-memory/#equality)). Two union values are compared as two `Any`s
holding them would be: values of the same member compare as that member does, by value or by its own
`Eq`, and values of different members are unequal, with the sequence exception `Any` has. The hash is
the member value's own. So `Set[int | string]` holds `1` and `"1"` apart, and an `int | long` holding
`int` 1 is not equal to one holding `long` 1. `a == b` with `a` a union and `b` a member's value
injects `b` first; a `b` that is no member is an error, as comparing two types with no common type is.

**Identity.** `===` is defined where every member is a class or a function type; a union with any
other member may hold a value with no identity, and is an error with the same message
[identity](/spec/equality-and-memory/#identity) gives an `Any`.

**Traits a union conforms to.** A union conforms to a trait when every member does and the trait's
methods take `Self` only as their receiver: no parameter, result or static member of type `Self`.
`Display`, `Debug`, `Hash` and a library's `JsonEncoder` are such traits; a call through one runs the
member's own method. So `println(u)` writes the member as it would write it alone (`3`, `two`),
`debug(u)` its debug form (`"two"`), and a `[T: Display]` takes `T = int | string`. `Eq` takes
`Self` as a parameter, and a union has it by the rule above. `Ord` does not hold: a union has no order
between its members, and sorting a `Vector[int | string]` is an error saying so. `Sendable` holds where
every member is sendable.

**Member calls.** `u.m(args)` on a union is looked up where it would be on the members' join, the
type the branches of an `if` yielding each member agree on, and in the traits the union conforms to:

```text
pet(a: Dog | Cat) -> string = a.name()        // Dog and Cat join to Animal, which has `name`
shown(v: int | string) -> string = v.toString()     // Display
v.charCount
// error: `int | string` has no member `charCount`: its members share no type that has one; take it
// apart with a `match` or `is` first
```

A name found on more than one of those types is an error that names them. A trait with type
parameters counts through the one instance every member holds: `Vector[int] | List[int]` has
`Seq[int]`'s `size` and `map`. What is found is called, named as a value, or, for a field of the join
class, read, with the union seen as that type, as an `Any` holding its member is.

**Derivations.** A data type or class with a union field derives `Eq`, `Hash`, `Display` and `Debug`
as with any field. `derives Ord` is an error naming the field, as for any field type without `Ord`. A
[derive template](/spec/derivation/#derive-templates) sees a union field as a field of that type, and
needs the union to meet its bound, as above.

## Errors and `?`

A function may declare a union of errors, and [`?`](/spec/errors/#the-operator) passes each kind on
without a wrapper type:

```text
load(path: string) -> Result[Config, IoError | ParseError]
    val text = readText(path)?          // Result[string, IoError]
    val doc = parseDocument(text)?      // Result[Document, ParseError]
    configOf(doc)                       // Result[Config, ParseError], converted as a result
```

On `Err(e)`, with `e` of type `E` and the enclosing function yielding errors of `F`:

1. **Where `E` converts to `F`, `e` is passed on as an `F`.** This is the existing rule's "`E` is an
   `F`", widened to the conversions above: `E` a member of the union `F`, a subtype of a member, or
   itself a union every member of which converts to a member of `F` (a `Result[_, IoError | ParseError]`
   inside a function yielding `IoError | ParseError | Invalid`).
2. **Otherwise, where `F` is a union, the one member of `F` that implements `From[E]`** makes the
   error passed on: with `F = ConfigError | NetError` and `impl From[IoError] for ConfigError`, an
   `IoError` becomes a `ConfigError`. Two members implementing it is an error naming both.
3. **Otherwise, where `E` is a union, each member of `E` is converted on its own** by rules 1 and 2
   and the existing `From` rule, and the error names each member with no conversion:
   `Result[_, IoError | ParseError]` inside a function yielding `ConfigError`, which implements
   `From[IoError]` and `From[ParseError]`, converts each kind through its own `from`; inside one
   yielding `ConfigError | Invalid`, each kind becomes a `ConfigError` by rule 2.
4. **Otherwise the existing rule applies**: `F.from(e)` through `F`'s `From[E]`.

```text
fetch(url: Url) -> Result[Page, NetError]
load(path: string) -> Result[Config, IoError | ParseError]
    val page = fetch(url)?
// error: `?` cannot pass on a `NetError` from a function whose errors are `IoError | ParseError`:
// it is no member of the union, and no member implements `From[NetError]`
```

A union has no `impl`, so it never implements `From` itself: an error union is widened by naming the
new member in the result type, not by writing a conversion.

A `Result` already made converts to a wider error union with no conversion where both are laid out
as an `Any` is (every error a data type or a class, which is the common case): a
`Result[T, IoError]` returned from a function yielding `Result[T, IoError | ParseError]` is that
result. Where an error member is a number or another scalar, the result is rebuilt by
`r.mapErr(e -> e)`, whose lambda's result injects.

The function's error union is the one its result type declares; `?` never adds a member to it. A
function whose result type is inferred cannot use `?`, as before.

## Representation

A union is an anonymous tagged sum, and how it is laid out is the compiler's choice, invisible except
in cost. The layout is decided by the type alone, before any optimization, so a program behaves the
same at `-O0` and `-O2`:

- **Every member a type an `Any` holds without a box** (strings, tuples, functions, data types,
  classes, traits, `Any`): the union is one pointer, laid out exactly as an `Any` holding the member.
  The tag is the object's header, which names its exact type, so injecting a value costs what
  converting it to an `Any` costs, a `match` is the type tests it would be on an `Any`, and such a
  union nests in covariant containers with no conversion.
- **Any member a scalar** (a number, `char`, `bool`, `unit`): the union is a value of a tag and a
  payload. The tag says which scalar member it holds, or that it holds a reference, whose header then
  names its type. The scalar's bits and the reference are kept in slots of their own, so the
  collector's roots stay exact: the reference slot holds a pointer or nothing, never a number. A
  union of scalars alone has no reference slot. Injecting a scalar writes the tag and the bits, and
  allocates nothing; neither does passing the union, returning it, matching it, comparing it with
  `==`, hashing it or showing it, each of which reads the tag. Seen as an `Any` or as a trait, a
  scalar member is boxed there, as any scalar is.
- The tag numbering is fixed by the normal form, not by the order written, so `int | string` and
  `string | int` are one layout.
- An integer member wider than 64 bits does not fit the bits slot: the union keeps it boxed in its
  reference slot, as it keeps a reference, so `i128 | int` has a reference slot and injecting an
  `i128` allocates its box. Every bit of it is kept.
- Wherever a union with a scalar member is stored, it is stored as its slots: a field of a data
  type, a class or a tuple, an element of an `Array` (and so of a `Vector`, a `Buffer` or a map), a
  closure's captures, a captured `var`, a module-level value and an actor's message. Storing one and
  reading it back allocates nothing, and the collector traces its reference slot and never the tag
  or the bits. The one exception is `Atomic`, whose value is one word its operations read and write
  indivisibly: an `Atomic` of such a union holds a reference to an object of its slots.
- Generic code is compiled per instance, as all generic code is, so `Vector[T | int]` at `T = string`
  stores `string | int` values in the vector's own elements, unboxed, and at `T = int` stores `int`s.
- A union in a heap field, an array element or a module-level `var` is several words, so, as with
  an integer wider than 64 bits, a racing read may combine the words of two writes (see
  [threads](/spec/threads/)). It still reads as a value of one of the members: the tag is believed
  to name a reference only beside a reference, so a racing read never gives a reference that no
  write stored.

## What is refused

- **A union is never inferred** (above).
- **No `impl` names a union**, and no extension: an `impl` names a declared type.
  `` impl Display for int | string `` is an error: `` an `impl` names a declared type, and
  `int | string` is a union; declare a data type to give it methods ``. An alias of a union is
  refused the same way, as any alias is.
- **A union is no parent**: a class or trait cannot list `A | B` after `:`: `` a union is no parent:
  a class or trait lists a class and traits after `:`, and `A | B` is a union ``.
- **A union is no bound**: `[T: A | B]` is an error, since a bound is a trait: `` a bound is a trait
  or a class, and `A | B` is a union ``; `[T >: A | B]`, a lower bound, is a type and is allowed.
- **A union does not cross a binding.** A parameter, result or field of an `@sysl` or `@c`
  declaration, or of a mirror, may not be a union: `` `int | string` cannot cross a binding: a union
  has no Sysl or C type; bind one function per member, or pass a data type ``.
- **A field path does not go through a union** in `with { … }`: with `v: A | B`, `p with { v.x = 1 }`
  is an error saying `v` is a union, to be taken apart with a `match` first. Replacing the whole field,
  `p with { v = 1 }`, injects as any value does.
- **No member is reached without a test**: there is no cast, and a union is not callable as a
  function (`` a union is not called: `((int) -> int) | string` may hold a value that is no function;
  take it apart with a `match` first ``), constructed, or used as a companion.
- **`derives` and annotations** have nowhere to go on a union: it is not declared, and an alias takes
  none.
- **Intersection types** (`A & B`) are not part of Avin.
