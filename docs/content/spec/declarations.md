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

Methods and class constructors take defaults and named arguments the same way:

```text
class Account(owner: string, var balance: int = 0)
    deposit(amount: int, note: string = s"$owner +$amount") -> string
        balance += amount
        note

Account("ann").deposit(5)               // "ann +5"
Account(balance = 3, owner = "bo")
```

A method's default is evaluated as the method's body is, so it sees `self` and the receiver's
members as well as the parameters before it; the receiver is the one the call is made on. A
constructor's default sees the parameters before it, but no `self`: the object does not exist yet.
A subclass passing its superclass's constructor arguments may omit those with defaults.

Defaults belong to the method that introduces a parameter. An `override`, and a method that
implements a trait's method, takes that method's defaults and cannot declare its own, so a call
omits the same arguments and gets the same values whichever class's method then runs. A trait's
method declares its defaults in the trait, and they apply through the trait's type and on every
type implementing it. A method of one required parameter whose others have defaults can be
[called infix](/spec/operators-and-control-flow/#infix-method-calls).

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

A clause's arguments are followed by `=`, a guard or an indented block of guards; a line of the same
name followed by `->` or by nothing is another declaration, as two abstract overloads of a method
are. The signature carries the types, the result type and any type parameters; a clause's patterns
carry none. A call tries the clauses in order and runs the first whose patterns match the arguments. The
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
    override display(out: Writer, spec: FormatSpec) = out.write(describe())
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
- Instances are heap objects with identity. Unless the class implements or derives `Eq`, `==` and
  the hash are by identity, like `===`; `class Point(x: int, y: int) derives Eq, Hash, Display`
  compares and hashes by its fields. Display exists only if the class implements or derives it. See
  [equality](/spec/equality-and-memory/#equality).
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
- A method named without a call is a function value bound to its receiver: `val f = acct.deposit`
  is an `(int) -> unit` that deposits into `acct`, which is evaluated once, where the value is made.
  Inside the class a bare method name is the same over `self`. Through a superclass's or a trait's
  type the value calls the implementation the object's class has. A method's own type parameters
  are written, `box.pair[bool]`, or come from the expected function type. The same holds for the
  methods an `impl` adds to any type, `"hey".shout` included.
- A field declared in the body without a type has its initializer's type. The body cannot
  `return`: it runs to its end to make the object.

### Inheritance

```text
abstract class Shape(val label: string)
    area() -> int
    describe() -> string = s"$label: ${area()}"

open class Rect(label: string, val w: int, val h: int): Shape(label)
    override area() -> int = w * h

class Square(label: string, side: int): Rect(label, side, side)
    override describe() -> string = "square " + super.describe()
```

- **Only an `open` or `abstract` class can be extended**; extending a final class is an error. A
  generic superclass is written with its type arguments, `class IntBox(x: int): Box[int](x)`.
- **Construction runs from the top down.** A subclass's constructor first evaluates the arguments
  written after the superclass and runs the superclass's constructor body on the new object, then
  stores its own parameters and runs its own body. Those arguments run before the object is ready:
  a parameter's name in them reads the argument itself, and they cannot call a method or use
  `self`.
- **A subclass has every field of its superclass**, first and in the superclass's order, then its
  own. A field cannot be overridden: a public field may not share the name of an inherited public
  field or method. A private member stays private to the class that declares it.
- **A method a subclass may override** is a public method of an `open` or `abstract` class. An
  override is marked `override`, takes the same parameter types (with the superclass's type
  arguments substituted) and yields the same result type or, for a class, a subclass of it. A method
  with type parameters of its own is overridden by one declaring as many, matched by position:
  `override tag[V](item: V)` overrides `tag[U](item: U)`. A method sharing an inherited method's name and parameter types without
  `override`, and `override` on a method that overrides nothing, are errors. An override cannot be
  private.
- **Methods overload as functions do.** A class, object, trait or impl may declare several methods
  of one name with different parameter types, and a call chooses among the methods of that name its
  receiver's static type has, its own and inherited ones together, by the rules for functions above.
  Each overload is a method of its own: a subclass overrides one by its parameter types and inherits
  the others, and may add an overload of an inherited name without `override`. A call through an
  `open` class or a trait reaches the override of the overload it chose. `override` that matches no
  inherited method's parameter types is an error naming the inherited overloads of that name, or,
  where there is one, how its parameters differ. An overloaded method used as a value takes its
  overload from the expected function type, and without one it is ambiguous.
- **An abstract method** is one without a body, allowed only in an `abstract` class. A class that is
  not abstract implements every abstract method it inherits. An abstract class cannot be
  instantiated, but its constructor runs as part of each subclass's.
- **A call reaches the override of the object's own class**, whatever the receiver's static type,
  when that type is an `open` or `abstract` class. `super.name(args)` calls the superclass's
  implementation directly; it cannot name an abstract method.
- **A subclass instance conforms to its superclasses.** It passes wherever a superclass is expected,
  and the branches of an `if` or `match` that yield different subclasses join to their nearest
  common superclass. `===` compares an instance of a class with one of its sub- or superclasses.

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

```text
trait Named
    label() -> string

trait Tagged: Named
    tag() -> string = s"<${label()}>"

class Item(text: string): Tagged
    override label() -> string = text

val t: Tagged = Item("x")
val n: Named = t
```

- A class conforms to the traits after its superclass in the `:` list, and to every trait those
  require; a subclass conforms to its superclass's traits. A trait is named there with its type
  arguments and without constructor arguments.
- **Implementing a trait's method is overriding it**: the class's method is marked `override`,
  whether the trait's method is abstract or has a default body, and takes its parameter types and
  its result or a type conforming to it. A method the class inherits from its superclass implements
  a trait's method of the same name without being repeated.
- A trait's default body is inherited like a superclass's method. A class that is not abstract
  implements every abstract method of its traits; an abstract class may leave them to a subclass.
- A trait's body declares only methods, all public. A trait cannot be instantiated.
- **A trait may override a method of a trait it requires**, marked `override` and with a body:
  `trait Square: Shape` with `override describe() -> string = …`. In every class and data type
  conforming to the trait, its body replaces the required trait's default, or gives an abstract
  method one, so a call through either trait's type reaches it. Like a class's override, it takes
  the defaults of the method it overrides. Declaring a required trait's method
  again without `override` is an error. Two traits overriding one default differently are two
  implementations, which a class conforming to both overrides.
- **A trait's method may have type parameters of its own**, as `map[B](f: (A) -> B) -> Iterable[B]`.
  Its implementations declare as many, matched by position. A call through the trait's type
  reaches the implementation of the object's class at the call's type arguments: the whole program
  is compiled at once, so every type argument list a call through the trait uses is known, and each
  has its own method table slot.
- A class conforms to a trait once: `Source[int]` and `Source[string]` on one class, written or
  reached through other traits, is an error.
- **A conforming instance is an instance of the trait**: it passes wherever the trait, or a trait
  the trait requires, is expected, with no conversion, and `===` compares it with values of those
  types. Branches of an `if` or `match` yielding two classes with no common superclass agree on the
  expected type when both conform to it.
- Inside a default body, `self` has the trait's type, so its calls dispatch as any trait-typed
  call does.

```text
trait Named
    val name: string
    greet() -> string = s"hello, $name"

class Person(val name: string, age: int): Named
class Counter(var name: string): Named
data Book(name: string, pages: int)
impl Named for Book
```

- **An abstract `val`** is written `val name: T` with no value, and only a trait declares one. A
  trait's `val` is public, is never `var`, and shares its name with no other member of the trait.
- **A conforming type supplies it with a public field of that name**: a class parameter marked `val`
  or `var`, a `val` or `var` of the class body, a field inherited from the superclass, or, through an
  impl, a field of a `data` type with one variant. The field's type is the `val`'s or, where that is a
  class or a trait, one conforming to it. A `var` field supplies a `val`: the class assigns it, and a
  read through the trait sees the current value. A method does not supply a `val`, and neither does a
  private field. A class that is not abstract supplies every `val` of its traits; an abstract class
  may leave one to a subclass.
- **It is read with field syntax**, `x.name`, and by bare name inside the trait's default bodies.
  Read through a class type it is the field itself; read through the trait it reaches the field of
  the value's own type through its method table. It is never assigned through the trait, and
  `x.name()` calls the value it holds, which only a function-typed `val` can be.
- An abstract class's body cannot read a trait's `val` it leaves to a subclass: it runs before the
  subclass's fields are set.

## impl

`impl Trait for Type` makes an existing type conform to a trait after the fact, including the
built-in types:

```text
impl Display for Shape
    display(out: Writer, spec: FormatSpec) = …

impl Shape
    perimeter() -> real = …
```

`impl Type` without a trait adds methods to a type declared in the same module. An `impl` lives in
the module of the trait or of the type; the built-in types belong to the core library's module,
`avin`, which is where `impl string` is written. Declaring a trait in a class's header and writing a
separate `impl` for it are the same conformance; it may be stated once.

`impl` is how Avin expresses type classes (`Ord`, `Hash`, `Display`) and adds methods to types you
do not own. Resolution is coherent: one implementation per trait and type, found without search.

- **An impl adds methods, never state.** Its body declares methods only: no fields and no
  statements. A method an impl adds to a class or an object is one of its methods, as if the class's
  body declared it: it sees the private fields, is inherited by a subclass, and a subclass may
  override it. A method with the name and parameter types of one the type already has is an error;
  one with other parameter types overloads it.
- **Inside an impl's methods the receiver is implicit**, as in a class: `self` is the value, the
  type's methods are in scope by bare name, and so are the fields of a `data` type with one variant,
  the fields `self.name` reaches. A type with several variants takes `self` apart with `match`.
  A `private` method of an impl is callable only from the methods of that type.
- **The methods of `impl Trait for Type` implement the trait's methods**, so they are not marked
  `override`, and each must be a method of the trait or of a trait it requires. A default body the
  impl leaves alone stays, and every abstract method must be implemented. One method of a name and
  parameter types implements every trait method of that name and parameter types the type conforms
  to, as a class's does; a `data`
  type's method implementing a trait's method is declared in an impl of that trait. An impl of a
  trait with no body states a conformance the type's methods already satisfy.
- **A `data` value of a type that conforms to a trait is a value of the trait's type.** Its calls
  through the trait dispatch as a class instance's do, a variant without fields included; a call on
  the data type itself is direct.
- **A generic impl names the type's parameters**, in order: `impl[T] Show for Box[T]`,
  `impl[K, V] Map[K, V]`. The names are the impl's own and need not be the declaration's. An impl
  for particular type arguments (`impl Show for Box[int]`) is not part of the language.
- A method added to a built-in type (`impl string`, `impl int`) is called on a value of that type,
  `"hi".shout()`, `7.squared()`.

## object

```text
object Config
    val defaultPort = 8080
    var verbose = false
    port(env: Map[string, string]) -> int = …
```

An `object` is a singleton value. An `object` with the same name as a class in the same module is its
companion and may access its private members. `object`s may extend a class and traits.

```text
object Default: Account("none", 0), Display
    override describe() -> string = "default " + super.describe()
    override display(out: Writer, spec: FormatSpec) = out.write(describe())
```

An object extending a class is made as a subclass's instance is: the superclass's constructor runs
first with the arguments written after `:`, then the object's own body. The object overrides methods
with `override`, implements every abstract method it inherits (it cannot be left abstract), and
passes wherever the superclass is expected. The superclass is `open` or `abstract`, as for any
subclass. What the superclass's arguments and constructor read counts toward the order in which
module values and objects are made.

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
  below it. This is checked at compile time, so no field is ever observed uninitialized:

  ```text
  class Counter(start: int)
      var count = start
      print(describe())            // error: `describe` is called before `label` is initialized
      register(self)               // error: `self` is passed, stored or returned before `label` is initialized
      val reader = () -> count     // error: a lambda that captures `self` is made before `label` is initialized
      val label = s"counter $start"
      register(self)               // every field is set: the body may use `self` freely
      describe() -> string = s"$label: $count"
  ```

  Up to and including the initializer of its last field, a body uses `self` only to read a field
  already set and to assign a field. Calling a method is an error however it is spelled — a bare
  name, `self.describe()`, `super.describe()` — and so are passing, storing or returning `self` and
  making a lambda, a by-name argument or a local function that uses a field or `self`, or a method
  value of `self` (`describe` named without a call), since each can read a field before it is set.
  An object's body follows the same rule.
- An `open` or `abstract` class's body runs before any subclass's fields are set, so the object is
  never complete while it runs: the limits above hold for the whole of it. After its own last field
  it may call only a method that cannot be overridden (a private one) and that is itself safe: one
  that uses `self` only to read and assign fields and to call other safe methods. A call that could
  reach an override, directly or through such a method, is an error, as is passing, storing or
  capturing `self`, a method value of `self` included.
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
