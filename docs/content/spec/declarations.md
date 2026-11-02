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
are supported. There is **no overloading**: a name denotes one function in a scope.

Variadic parameters are final and written `values: T*`. A call passes zero or more `T`s, or splices
an existing sequence with `f(xs*)`.

Multiple clauses with patterns and guards select by argument:

```text
fib(0: int) -> int = 0
fib(1: int) -> int = 1
fib(n: int) -> int | n > 1 = fib(n - 1) + fib(n - 2)
```

The exact clause and guard spelling is provisional.

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
    deposit(amount: int)
        balance += amount

    describe() -> string = s"$owner: $balance"

class Savings(owner: string, balance: int, rate: real): Account(owner, balance), Display
    override describe() -> string = "savings " + super.describe()
    display(out: Writer, spec: FormatSpec) = out.write(describe())
```

- The parameter list after the name is the **primary constructor**, and its parameters are fields
  (`var` makes one mutable; `private` hides one). There are no secondary constructors; a factory
  belongs in the companion `object`.
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

- A constructor runs the superclass constructor first, then assigns its own fields. Until every field
  is assigned, `self` may not be passed, stored, captured, or used to call a method. This is checked
  at compile time, so no field is ever observed uninitialized.
- Module-level `val`s and `object`s initialize before `main`, in the order their initializers depend
  on one another. A dependency cycle is a compile error, not a runtime surprise.
- Local helper functions may call one another in any order, but a call that would read a captured
  local before its initialization is rejected.
