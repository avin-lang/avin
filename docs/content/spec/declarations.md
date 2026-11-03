---
title: Declarations
summary: data, class, trait, object, impl, members, modules and initialization.
weight: 13
---

## Functions

```text
area(width: int, height: int) -> int = width * height

greet(name: string)
    print(s"hello, $name")
```

Every parameter has a declared type. The result type may be inferred, except for recursive functions.
An indented body with no `=` and no result type returns `unit`; with `=` or a result type it yields
its last expression. Default arguments (`size: int = 16`) and named arguments (`open(path, create = true)`)
are supported.

Functions and methods may be **overloaded**: several functions in one scope may share a name when
their parameter types differ. A call chooses among them by its arguments:

- The candidates are the overloads that accept the call's arguments: their number, their names and
  their types. Inference runs for each candidate separately, so a literal argument fits any overload
  whose parameter it can become.
- When more than one candidate remains, the most specific wins: the one whose every parameter type
  conforms to the corresponding parameter type of each other. A call with no candidate, or with no
  single most specific one, is an error that lists the overloads.
- Overloads must differ in their parameter types; differing only in result type or parameter names
  is an error. A default argument that makes two overloads accept the same call makes that call
  ambiguous.
- A function used as a value takes its overload from the expected function type.

Two declarations with the same name and the **same** parameter types are an error: a function that
chooses by its arguments' values is written with clauses, described next.

Variadic parameters are final and written `values: T*`. A call passes zero or more `T`s, or splices
an existing sequence with `f(xs*)`.

### Clauses

A function may be defined by **clauses**: its signature on one line with no body, then one clause
per line, each the function's name applied to a pattern per parameter:

```text
classify(n: int) -> string
classify(0) = "zero"
classify(n)
    | n < 0 = "negative"
    | n < 10 = "small"
    | _ = "large"

fib(n: int) -> int
fib(n < 2) = n
fib(n) = fib(n - 1) + fib(n - 2)

area(s: Shape) -> real
area(Circle(r)) = 3.14159 * r * r
area(Rect(w, h)) = w * h
```

The signature carries the types, the result type and any type parameters; a clause's patterns carry
none. A call tries the clauses in order and runs the first whose patterns match the arguments. The
names a clause's patterns bind are its own.

A clause may be guarded:

- `f(patterns) | condition = body` is taken only when the condition holds.
- `f(patterns)` followed by an indented block of `| condition = body` lines tries each guard in
  order; `| _ = body` always holds and ends the block.
- When the patterns match but every guard fails, the next clause is tried.

A body after `=` is one expression on the same line or an indented block. Without a declared result
type, the result is inferred from the clauses' bodies, which must agree; a recursive function
declares it.

The clauses must cover every combination of arguments, as the arms of a `match` must. A guarded
clause counts toward that only when one of its bodies has no guard, or `| _`, so the clauses above
end with one that matches anything. A local function may be defined by clauses too.

A function declared inside a block is a **local function**. It can be called from anywhere in its
block, including before its declaration, and it sees the locals declared before it, the enclosing
function's parameters, and the enclosing local functions. It reads and assigns those locals
themselves, not copies, so a `var` it changes is changed for the code around it:

```text
counter() -> int
    var count = 0
    bump(by: int)
        count += by
    bump(2)
    bump(3)
    count                // 5
```

A call made before the local function's declaration must not reach a local that is not yet
initialized there, directly or through the local functions it calls, and the function needs a
declared result type for such a call, as it does for recursion.

## data

`data` declares an algebraic data type: an immutable value with one or more variants.

```text
data Shape
    Circle(radius: real)
    Rect(width: real, height: real)

data Point(x: int, y: int)

data Option[+T]
    Some(value: T)
    None
```

The single-line form declares a one-variant type whose variant has the type's name. Variants are
constructors, not types: `Some(1)` has type `Option[int]`. `data` values have no identity, derive
`Eq`, `Hash`, `Display` and `Debug` when their fields do, and are matched exhaustively. Methods are
added in an `impl` block.

## class

```text
open class Account(owner: string, var balance: int)
    if balance < 0 then panic("an account cannot open overdrawn")
    val opened = clock.now()
    print(s"opened an account for $owner")

    deposit(amount: int)
        balance += amount

    describe() -> string = s"$owner: $balance"

class Savings(owner: string, balance: int, rate: real): Account(owner, balance), Display
    override describe() -> string = "savings " + super.describe()
    display(out: Writer, spec: FormatSpec) = out.write(describe())
```

- **The class is its constructor.** The parameter list after the name is the constructor's
  parameter list, and there is no other constructor; a factory belongs in the companion `object`.
- **Class parameters are fields.** A plain parameter (`owner`) is a private, immutable field: every
  method of the class can read it, and nothing outside can. `val` makes it a public read-only field
  and `var` a public mutable one (`balance` above); `private var` keeps a mutable one private.
- **The class body is the constructor's body.** Everything in it that is not a member declaration —
  expressions, `if`, loops, calls — runs when an instance is created, top to bottom, together with
  the initializers of the `val` and `var` fields declared in the body (`opened`). Methods are
  declarations and do not run.
- The receiver is implicit. Members are in scope by their bare names, and `self` names the instance.
- After `:` comes at most one superclass, with its constructor arguments, followed by any number of
  traits.
- Classes are **final unless declared `open` or `abstract`**. Overriding requires `override`.
  An `abstract class` may leave members without bodies and cannot be instantiated.
- Instances are heap objects with identity. Equality, hashing and display exist only if the class
  implements or derives them (`class Point(x: int, y: int) derives Eq, Hash, Display`).

## trait

```text
trait Shape
    area() -> real
    describe() -> string = s"area ${area()}"
```

A trait declares methods, which may have default bodies, and abstract `val` members a class supplies
with a field. **A trait holds no state.** A trait may require others: `trait Iterator[+A]: Iterable[A]`.

A trait is both a type and a bound. Used as a type, a value of it is a reference and calls dispatch
dynamically. Used as a bound (`[T: Shape]`), calls are direct, because the code is monomorphized.
A trait whose methods mention `Self` other than as the receiver (`Add`, `Ord`, `Eq`) can only be used
as a bound, as in Sysl.

When a class inherits two implementations of one member, from its superclass or from traits, it must
override that member. There is no trait linearization.

## impl

`impl Trait for Type` makes an existing type conform to a trait after the fact, including the
built-in types:

```text
impl Display for Shape
    display(out: Writer, spec: FormatSpec) = …

impl Shape
    perimeter() -> real = …
```

`impl Type` without a trait adds methods to a type declared in the same module. Sysl's orphan rule
applies: an `impl` lives in the module of the trait or of the type. Declaring a trait in a class's
header and writing a separate `impl` for it are the same conformance; it may be stated once.

`impl` is how Avin expresses type classes (`Ord`, `Hash`, `Display`) and adds methods to types you
do not own. Resolution is coherent: one implementation per trait and type, found without search.

## object

```text
object Config
    val defaultPort = 8080
    var verbose = false
    port(env: Map[string, string]) -> int = …
```

An `object` is a singleton value. An `object` with the same name as a class in the same module is its
companion and may access its private members. `object`s may extend a class and traits.

## Visibility and modules

Declarations are public unless marked `private` (visible in the module). A directory is a module;
`module a.b` heads each file. A **package** is a build unit with an `avin.hocon` manifest; a
**project** lists packages. `import a.b.{X, y as z}` brings names into a file. The root module `avin`
is reserved for the standard library.

## Initialization

- Creating an instance runs the superclass's constructor first, then stores the class parameters,
  then runs the class body top to bottom. Until every field is assigned, `self` may not be passed,
  stored, captured, or used to call a method, and a body statement may not read a field declared
  below it. This is checked at compile time, so no field is ever observed uninitialized.
- Module-level `val`s and `object`s initialize before `main`, in the order their initializers depend
  on one another. A dependency cycle is a compile error, not a runtime surprise.
- Local helper functions may call one another in any order, but a call that would read a captured
  local before its initialization is rejected.
