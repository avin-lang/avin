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
are supported, as described next.

### Default and named arguments

```text
area(width: int, height: int = width, scale: int = 1) -> int = width * height * scale

area(2)                                 // 4
area(2, scale = 10)                     // 40
area(height = 3, width = 2)             // 6
```

A parameter's **default** is evaluated at each call that omits its argument, never once for all
calls. It is evaluated where the function is declared, not where it is called: it sees what is in
scope at the declaration, including what a local function captures, and the parameters before its
own, but not the ones after it. A default does not decide a type argument of a generic function:
`f[T](x: T = 1)` is an error, because `1` is not a `T`.

A call writes its **positional** arguments first, then any **named** ones, `name = value`, in any
order; a positional argument after a named one is an error. Each parameter is given at most once,
by position or by name, and one given nothing must have a default. A name that is no parameter's is
an error. A named argument is checked against its parameter's type exactly as a positional one is,
so it takes part in inferring a generic call's type arguments, and a lambda written as one takes its
parameter types from the parameter.

The arguments written at a call are **evaluated in the order written**, whatever parameters they
are given to; then the defaults of the omitted parameters are evaluated in parameter order, each
from the values before it. An argument that never finishes, such as a `return`, ends the call there.

Names and defaults belong to a function's declaration, not to its type. A function value, such as a
lambda or a function passed as an argument, is called with positional arguments only, all of them.
A variant is called the same way as a function, so its fields may be given by name:
`Point(y = 2, x = 1)`. A function defined by [clauses](#clauses) writes its defaults on its
signature line, and its clauses match every parameter, defaulted or not. The default of a
[by-name](/spec/by-name/) parameter is delayed like any argument to it.

Functions and methods may be **overloaded**: several functions in one scope may share a name when
their parameter types differ. A call chooses among them by its arguments:

- The candidates are the overloads that accept the call's arguments: their number, their names and
  their types. Inference runs for each candidate separately, so a literal argument fits any overload
  whose parameter it can become.
- When more than one candidate remains, the most specific wins: the one whose every parameter type
  conforms to the corresponding parameter type of each other. A call with no candidate, or with no
  single most specific one, is an error that lists the overloads, except that an integer literal
  breaks a tie in favour of its own type (below).
- Overloads must differ in their parameter types; differing only in result type or parameter names
  is an error. A default argument that makes two overloads accept the same call makes that call
  ambiguous.
- A function used as a value takes its overload from the expected function type.

```text
describe(n: int) -> string = s"int $n"
describe(s: string) -> string = s"string $s"
which[T](x: T) -> string = "any"
which(x: int) -> string = "int"

describe(1)                             // int 1
which(1)                                // int: the generic one accepts an int, but not the reverse
which("x")                              // any
val f: (string) -> string = describe    // the string overload
```

Choosing an overload works as follows:

- **Which arguments decide.** An integer literal and a lambda take their types from their
  parameter, so each candidate tries them against its own: the literal fits a parameter whose type
  can hold its value (`int`, `long`, the other widths, `byte`, `real`), and the lambda a parameter
  that is a function of as many parameters. Every other argument is checked once, on its own, and
  only its type is tried against each candidate; when every candidate gives it the same parameter
  type it is checked against that type. Neither `int` and `long` nor `int` and `real` conforms to the
  other, so where no overload is the most specific an integer literal prefers its own type, `int`,
  or `long` for a literal too large for `int`: `measure(1)` calls `measure(n: int)` beside
  `measure(r: real)`, and `wide(1)` calls `wide(n: int)` beside `wide(n: long)`. The overload must
  also be the most specific by the arguments that are not literals. With `wide(n: long)` and
  `wide(r: real)`, `wide(1)` is ambiguous, since neither takes the literal's own type.
- **Generic overloads.** A generic overload is compared by the same rule: `which(x: int)` is more
  specific than `which[T](x: T)`, because `which[T]` accepts an `int` and `which(int)` does not
  accept a `T`. Of `f[T](x: T)` and `f[T](x: Maybe[T])`, the second is the more specific. Written
  type arguments, as in `which[int](1)`, leave only the overloads with at least that many type
  parameters.
- **Defaults.** The most specific is compared over the parameters the call's arguments are given
  to, so `sized(n: int)` and `sized(n: int, scale: int = 2)` both accept `sized(1)` and neither is
  more specific: the call is ambiguous. `sized(1, 3)` is not.
- **By-name parameters.** Whether a parameter is by-name does not tell two overloads apart:
  `f(x: -> int)` and `f(x: int)` are the same parameter types. An argument that some candidate takes
  by name is delayed, as that candidate needs it; when the chosen overload takes it strictly, it is
  evaluated once, at the call.
- **Scopes.** Overloads are the functions of one name in one scope: the top-level functions of one
  file, or the local functions of one block. A block's local functions of a name hide every outer
  function of that name, whatever their parameter types, and a program's own top-level functions of
  a name hide the prelude's.
- **Function values.** An overloaded name used as a value with no expected function type is an
  error, as is one whose expected type no overload's parameters accept. `main` cannot be
  overloaded, and a variant's name is not overloaded by a function of the same name.

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
  implements or derives them (`class Point(x: int, y: int) derives Eq, Hash, Display`). `===`
  compares identities, and `==` on a class that defines no equality is a compile error.
- A class is created by calling it, `Account("ada", 10)`, as a function is called; a generic class's
  type arguments are inferred from the arguments and the expected type, or written,
  `Box[string]("x")`. The class's name is also its constructor as a function value.
- Outside the class, `obj.field` reads a public field, `obj.field = value` assigns a public `var`
  one, and `obj.method(args)` calls a method. A method or a field declared `private` in the body,
  and a plain class parameter, are private to the class: its constructor and methods, and the
  lambdas and local functions in them, may use them on any instance of the class.
- Inside the class, names resolve innermost first: a local, a parameter or a local function hides
  a member of the same name, and a member hides a top-level declaration. `self.balance` always
  reaches the field, so a method's parameter may share a field's name. In the class body, a class
  parameter's name is its field, so it reads what an assignment to a `var` stored.
- Methods take the forms top-level functions take: a body after `=`, an indented body, or clauses,
  which match the arguments and not the receiver. A method may have type parameters of its own,
  after its class's.
- A field declared in the body without a type has its initializer's type. The body cannot
  `return`: it runs to its end to make the object.

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

An object's body is written like a class body with no parameters: its fields and statements run
once, before `main`, to make the one instance, and its methods have the same implicit receiver and
`self`. The object's name is a value of the object's own type, so it can be stored and passed like
any other; it cannot be called, since there is no second instance to make.

An object shares its name only with its companion class. Where both exist, the name in call
position, `Account(…)`, is the class's constructor, and the bare name `Account` or a member
`Account.open(…)` is the companion. The class and its companion each use the other's private
members.

```text
class Account(owner: string, private var balance: int)
    private val id = Account.issue()

object Account
    private var next = 0
    open(owner: string) -> Account = Account(owner, 0)
    balanceOf(a: Account) -> int = a.balance
    private issue() -> int
        next++
        next
```

A file may also declare `val`s and `var`s at module level, beside its functions. Each is a value of
the program, read and (for a `var`) assigned from any function by its name.

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
- Module-level `val`s, `var`s and `object`s initialize before `main`, in the order their
  initializers depend on one another. An initializer depends on every module value and object it
  reads, directly or through the functions it calls and the lambdas it makes; values that need
  nothing of one another initialize in declaration order. A dependency cycle is a compile error that
  names the chain, not a runtime surprise:

  ```text
  val a = b + 1
  val b = readA() * 2      // error: module-level values need one another to initialize: `a` → `b` → `a`
  readA() -> int = a
  ```
- Local helper functions may call one another in any order, but a call that would read a captured
  local before its initialization is rejected.
