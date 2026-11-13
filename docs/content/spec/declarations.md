---
title: Declarations
summary: Names, data, class, trait, object, impl, members, modules and initialization.
weight: 13
---

## Names

A name may be written in any script. It begins with a letter or `_` and continues with letters,
digits, combining marks and connecting punctuation such as `_`; exactly, its first character has
Unicode's `XID_Start` property or is `_`, and each later one has `XID_Continue` (the identifier
rule of Unicode Standard Annex #31).

```text
data Ärger
    Δelta(größe: int)
    Ωmega

fläche(breite: int, höhe: int) -> int = breite * höhe

main()
    val 名前 = "名"
    val x٣ = 3
    println(s"$名前 ${fläche(x٣, 2)}")
```

- A digit of any script continues a name and none begins one: `x٣` is a name and `٣x` is refused.
  Number literals are written with ASCII digits only.
- A combining mark continues a name and cannot begin one.
- An emoji, a symbol, an operator character of another script and a space other than the ASCII
  space are no part of any name; each is reported with its code point (`U+1F600`).
- **A name is written in Unicode Normalization Form C.** A name spelled in another form (an `e`
  followed by a combining acute, where `é` is one character in NFC) is reported, with the NFC
  spelling to write instead. So two names that look the same are always the same name: nothing is
  normalized behind the programmer's back, and nothing that looks alike can differ invisibly in
  how it was composed. Names in different scripts that merely look alike (Latin `a`, Cyrillic `а`)
  are different names.
- A module's name is a path of names, so each directory of a program is named by the same rule.
- **The keywords name no value, type or parameter**: `abstract`, `as`, `break`, `class`, `continue`,
  `data`, `def`, `derives`, `do`, `elif`, `else`, `false`, `for`, `if`, `impl`, `import`, `is`,
  `loop`, `match`, `module`, `object`, `override`, `private`, `return`, `static`, `super`, `then`,
  `trait`, `true`, `val`, `var`, `while` and `yield`. `object`, `trait` and `impl` followed by a
  parameter list name a function; `end`, `open` and `self` have rules of their own, and the words of
  actors and machines are keywords only where they [begin their forms](/spec/actors/#keywords).
- **Capitalized** means the first character is an uppercase or titlecase letter (Unicode `Lu` or
  `Lt`): `Ärger`, `Δelta` and `ǅemal` are capitalized. A letter with no case, as in Chinese,
  Japanese or Arabic, is not, so such a name is written where a lowercase one is. Types and
  variants are capitalized by convention, values and functions are lowerCamelCase, and where the
  language reads a name's case (`T.V` naming a variant, `int.MaxValue`) this is the test. A bare
  name in a pattern is a variant when a variant of that name is in scope, and otherwise binds,
  whatever its case.

## Functions

```text
area(width: int, height: int) -> int = width * height

greet(name: string)
    println(s"hello, $name")
```

Every parameter has a declared type. The result type may be inferred, except for recursive functions.
An indented body with no `=` and no result type returns `unit`; with `=` or a result type it yields
its last expression. Default arguments (`size: int = 16`) and named arguments (`open(path, create = true)`)
are supported, as described next.

**`def` may begin any function or method declaration**, and changes nothing about it: `def area(width:
int, height: int) -> int = …` is `area(width: int, height: int) -> int = …`. It is written only where
it is needed, which is a top-level or local function with no parameter list (see
[parameterless methods](#parameterless-methods)).

```text
def now -> int = clock.millis()

class Rect(w: real, h: real): Shape
    override def area() -> real = w * h
    private def half() -> real = w * h / 2.0
```

- `def` comes after the modifiers (`private`, `static`, `override`) and after the annotations, as in
  `@tailrec` then `private def go(…)`. It is written once; before the modifiers, or before a `val`,
  `var` or `lazy val`, it is refused.
- A function defined by clauses takes `def` on its signature, never on its clauses.
- `def` is a keyword: it names no function, type, value or parameter.

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
`Point(y = 2, x = 1)`, and its fields may have defaults, each a function of the fields before it:
`data Range(low: int = 0, high: int = low + 10)`. A function defined by [clauses](#clauses) writes its defaults on its
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
type implementing it. A method a type inherits, from its superclass or as a trait's default body,
keeps its own defaults, so where it also implements another trait's method, that method must
declare no defaults or the very same ones; otherwise the type is an error. A method of one required parameter whose others have defaults can be
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
- Where candidates differ only in what a function argument returns, `flatMap` taking `(T) ->
  Iterable[U]` or `(T) -> Option[U]`, a lambda's body decides its result first. An argument that
  cannot be checked that way, a lambda whose branches join only at the result a candidate expects
  (`if … then aList else aVector`) or a generic function named as a value (`flatMap(identity)`), is
  checked against each candidate in turn, and the one candidate that takes it is chosen. A lambda
  that never returns fits every candidate alike, and the first declared is chosen.

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
  that is a function of as many parameters, or a function of one tuple of as many elements, which
  the lambda [takes apart](/spec/types/#tuples). A candidate that has a lambda take a tuple apart is
  chosen only where no candidate takes it as written. Where the candidates left all give a lambda
  the same parameter types, known without their own type arguments, and differ only in the result
  they need of it, the lambda is checked once, with those parameter types and its result left to its
  body, and then fits a candidate as any other argument's type does: a map's
  `map((k, v) -> Summary(k, v))` takes the overload whose function may answer anything, and
  `map((k, v) -> (v, k))` the one that needs a pair, as the more specific. Every other argument is checked once, on its own, and
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
- **Repeated parameters.** An overload with a [repeated parameter](#repeated-parameters) is a
  candidate only where no overload without one accepts the call: `describe(n: int)` beside
  `describe(ns: int*)` takes `describe(1)`, and `pick(x: long)` beside `pick(xs: int*)` takes
  `pick(1)`, though the repeated one would take the literal as its own type. Among the candidates
  with one, each argument given to it is compared by the element type, so `kind(xs: int*)` and
  `kind(xs: string*)` are told apart by `kind(1, 2)` and `kind("a")`; `kind()` is ambiguous. Inside
  the function the parameter is a vector, so `f(xs: int*)` and `f(xs: Vector[int])` have the same
  parameter types.
- **Scopes.** Overloads are the functions of one name in one scope: the top-level functions of one
  module, in whichever of its files each is declared, or the local functions of one block. A block's local functions of a name hide every outer
  function of that name, whatever their parameter types, and a program's own top-level functions of
  a name hide the prelude's.
- **Function values.** An overloaded name used as a value with no expected function type is an
  error, as is one whose expected type no overload's parameters accept. `main` cannot be
  overloaded, and a variant's name is not overloaded by a function of the same name.

Two declarations with the same name and the **same** parameter types are an error: a function that
chooses by its arguments' values is written with clauses, described next.

### Repeated parameters

```text
total(values: int*) -> int
    var sum = 0
    for v in values do sum += v
    sum

total()                                 // 0
total(1, 2, 3)                          // 6
val xs = [4, 5]
total(xs*)                              // 9
total((1..10)*)                         // 55
max(0, xs*)                             // 5
```

A parameter written `values: T*` is **repeated** (variadic): a call gives it zero or more `T`s, and
inside the function it is a `Vector[T]` of them, in the order written. The rules:

- **It is the last parameter**, and there is at most one. It has a name, no default (a call that
  gives it nothing passes an empty vector), and is not [by-name](/spec/by-name/). The parameters
  before it may have defaults: `framed(left: string = "[", right: string = "]", items: int*)` is
  called `framed()`, `framed("<", ">", 1, 2)` or `framed(right = ")")`.
- **Its arguments are positional.** Every positional argument from its position on is one element,
  checked against `T` like any argument, so a literal takes `T` and a lambda its parameter types
  from it, and a generic `T` is inferred from them as from any arguments:
  `max[T: Ord](first: T, rest: T*)` is called `max(3, 9, 4)`. A repeated parameter cannot be named
  at a call, and the call's named arguments, which follow its positional ones, name only the
  parameters before it.
- **`xs*` splices a sequence**: written as the call's last argument, in the repeated parameter's
  position, it passes the elements of `xs`, which is any `Iterable[T]`: a vector, a list, an array,
  a range, a set. A `Vector[T]` is passed as it is; any other sequence is copied into a new vector.
  The spliced sequence is the parameter's only argument: `f(1, 2, xs*)` gives the parameter both
  `1, 2` and `xs`, and is an error, as is `xs*` given to a parameter that is not repeated, written
  before another argument, named, or anywhere but a call's argument list. Writing the other
  elements into the sequence, `f(([1, 2] ++ xs)*)`, says the same thing. The `*` binds as
  multiplication does, so a spliced expression of a looser operator is parenthesized.
- **The arguments are evaluated in the order written**, as any call's are, and the vector is made
  from them before the call.
- **Methods and constructors** take one the same way: a method of a class, a trait or an impl, a
  method of an object, a local function, a function defined by clauses (whose clauses match the
  vector: `f([]) = …`, `f([x]) = …`), and a class's constructor, whose field is then a vector:
  `class Path(val parts: string*)` is made by `Path("usr", "bin")`, and a subclass passes its
  superclass's repeated argument on with `: Path(parts*)`. A `data` field cannot be repeated. An
  override, and a method implementing a trait's, repeats the same last parameter as the method it
  overrides, or neither does.
- **As a value**, a function with a repeated parameter takes the vector itself:
  `val f: (Vector[int]) -> int = total` is called `f([1, 2])`, since a function value is called with
  all of its arguments by position.
- **Called infix**, a method whose parameters are a repeated one, or one required parameter and a
  repeated one, takes the one argument: `w surround "!"` calls `w.surround("!")`.
- **`main`** takes no repeated parameter: its one parameter is `args: Vector[string]`.

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
- A `|` that starts a line continues the expression above it, so a second guard written on its own
  line after a one-line guard is not a guard; it is reported, and the guards belong in the
  indented block.

A body after `=` is one expression on the same line or an indented block. Without a declared result
type, the result is inferred from the clauses' bodies, which must agree; a recursive function
declares it.

The clauses must cover every combination of arguments, as the arms of a `match` must. A guarded
clause counts toward that only when one of its bodies has no guard, or `| _`, so the clauses above
end with one that matches anything. A local function may be defined by clauses too.

A clause no argument can reach is an error, as an [unreachable arm](/spec/operators-and-control-flow/#match)
is: one whose arguments the clauses before it already match, counting a clause only when one of its
bodies has no guard. Within a clause, a body after one with no guard (or `| _`) is never tried, and
is an error too.

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

A local function's name is its block's: a `val`, a `var` or a pattern binding of the same name in
the same block is an error, whether it is written before or after the function, because the
function is visible across the whole block. A block nested inside, a lambda's body and the enclosing
function's parameters are other scopes, and a local there may hide the function as any local hides
an outer name; likewise a local function declared in a nested block or a lambda's body hides an
enclosing value, parameter or local function of its name there, so the innermost declaration wins
whichever kind each is. The block's own local functions of one name are overloads of each other.

A local function may declare type parameters of its own, with bounds, exactly as a top-level
function does. Its calls write its type arguments or leave them to be inferred, and used as a value
it takes them from the expected function type. Inside a generic function it also sees the enclosing
type parameters, which every call shares and none writes. A type parameter of a local function may
not reuse the name of one already in scope:

```text
pairs[T: Display](first: T) -> string
    pair[U](x: U) -> (T, U) = (first, x)
    s"${pair(1)} ${pair("a")} ${pair[real](2.5)}"

pairs("p")               // "(p, 1) (p, a) (p, 2.5)"
```

### Tail calls

A call is in **tail position** when its result is the function's result and nothing is left to do
after it: the body itself, the last item of a block in tail position, a branch of an `if`, an arm of
a `match` or a clause body in tail position, the right operand of `&&` or `||` in tail position, and
the value of a `return`, wherever it is written. A call whose result is used is not: by an operator,
a conversion, another call, a store, a pattern, a guard or a condition; nor is one inside a loop, a
lambda, a by-name argument or a local function, which runs later or from somewhere else.

**A function that calls itself in tail position runs as a loop.** The arguments are evaluated, every
one before any parameter changes, then become the parameters, and the body starts again, so the
stack does not grow however deep the recursion goes. This holds at every optimization level, of a
call that names the function itself at its own type arguments, for every function no other body can
replace: a top-level or local function, a method of a final class, of an object or of a data type,
and a private method of any class. A call of a method on another receiver of the same type is a loop
too, with that receiver as `self`. Each pass has parameters of its own: a lambda made in one pass
keeps that pass's values, and a `var` its body declares is a new one each pass.

```text
sum(n: long, acc: long) -> long = if n == 0 then acc else sum(n - 1, acc + n)

sum(10000000, 0)         // 50000005000000, in constant stack
```

**`@tailrec` makes it a guarantee.** Written before a function, a method or a local function, it
asks the compiler to prove that every call of the function in its own body is such a loop, and each
failure is an error at the call, saying why:

```text
@tailrec
factorial(n: long) -> long = if n == 0 then 1 else n * factorial(n - 1)
// error: this recursive call of `factorial` is not in tail position: its result is used by `*`
```

It is an error for the function to have no recursive call; for it to be a method that can be
overridden, a trait's or a public method of an `open` or `abstract` class, since a call of it may run
an override; and for a recursive call to give a generic function other type arguments, which calls
another instance rather than looping. A function defined by clauses may be `@tailrec`, written before
its signature.

**Every other call in tail position replaces the caller's frame.** A call of another function, a
method, a local function, a method through a trait or an open class, or a function value, made in tail
position, does not keep the caller waiting on the stack: the callee runs in the caller's place and
returns straight to the caller's caller. So functions that call each other, state machines written as
functions, and continuation passing run in constant stack however deep they go, at every
optimization level, with no annotation:

```text
isEven(n: long) -> bool = if n == 0 then true else isOdd(n - 1)
isOdd(n: long) -> bool = n != 0 && isEven(n - 1)

isEven(10000000)         // true, in constant stack

sumTo(n: long, k: (long) -> long) -> long =
    if n == 0 then k(0) else sumTo(n - 1, s -> k(s + n))

sumTo(1000000, s -> s)   // 500000500000: a million continuations, each calling the next
```

A call in tail position keeps its frame in a few places, where something of the caller is still
needed once the callee has started:

- a call of a local function that is given the place of a local it captures, which lives in the
  caller's frame;
- a call into the program's code from the core library, which puts back the place a trap in the
  library reports once the call returns;
- a call in a class's body, which completes the object after the body;
- a call of a function whose arguments do not all travel in registers: one with more than five
  parameters that are integers, bools or references (a method's receiver and the values a local
  function captures count), more than eight `real`s, or an integer wider than `long` among its
  parameters or as its result; the call is made in its own frame, and so is a tail call such a
  function makes;
- a call of a lambda or local function value that captures a local of the function it is written in
  (not only its parameters): the closure's entry holds the local where the code can reach it, so each
  such call adds the entry's frame.

A trap reports the place it happened as always, but the frames a tail call replaced are gone: a
program stopped after a million tail calls was not a million calls deep.

### `main`

A program starts at `main`, a function of the root module written `main()` or
`main(args: Vector[string])`, whose `args` are the program's arguments after its name. It is not
generic, overloaded, or given a default or a repeated parameter. What it yields decides how the
program exits:

```text
import avin.fs
import avin.io.IoError

main(args: Vector[string]) -> Result[unit, IoError | ParseError]
    for file in args
        val settings = parse(fs.readText(file)?)?
        println(s"$file: ${settings.size} settings")
    Ok(())
```

- **unit**: the program exits with status 0.
- **An integer type**: the program exits with that integer as its status (its low 8 bits are what
  the operating system keeps).
- **`Nothing`**: `main` never returns, so whatever ends it, `exit` or a trap, sets the status.
- **`Result[unit, E]` or `Result[int, E]`**, so that `?` can be used in `main`. `Ok(())` exits with 0, and
  `Ok(n)` exits with `n`. `Ok` may hold any integer type, and `Nothing` for a `main` that only
  answers `Err`, such as `main() = Err("nothing to do")`. **`Err(e)` writes `error: ` and `e`'s
  `Display` to standard error as one line, and exits with status 1.** It does this after whatever
  `main` wrote to standard output and before the exit hooks run. `E` must implement `Display`;
  an error type with no `Display`, even one with `Debug`, is refused. A union of errors, `IoError |
  ParseError`, is shown by the `Display` of the member it holds, so every member must have one. An
  `Ok` of any other type, such as `Result[string, E]`, is refused, since there is no status to read
  from it.
- Any other type is refused.

Returning from `main`, with any of these, ends the program the same way: standard output is flushed,
the exit hooks run on the main thread, and the process exits, ending the
[threads](/spec/threads/) and actors still running. A trap in `main`, or in the error's `Display`
while it is written, stops the program as a trap does anywhere, so no `error: ` line is written.
A program built by `avin test` runs its tests and not `main`, so `main` is checked but not run.

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
constructors, not types: `Some(1)` has type `Option[int]`. A variant is a name of its module, and
several data types of one module, in one file or in several, may each have a variant of one name:

```text
data Slant
    Normal
    Italic

data Weight
    Normal
    Bold

heavy(w: Weight) -> bool = w match
    Normal -> false
    Bold -> true

val s: Slant = Normal
println(heavy(Normal))
```

A bare use of a shared name is the variant of the type expected where it is written: a parameter's
type, an annotated binding's, a function's result, a field's, a collection's element, the other
operand of a comparison (`w == Normal`), and in a pattern the type of the value matched, so a nested
pattern decides each name by its field's type. Where the parameter's type is a type argument of a
generic function, what the call's other arguments decide of it decides the name, on whichever side
of them it is written: with `same[T](a: T, b: T)`, both `same(w, Normal)` and `same(Normal, w)` take
`Weight`'s. The elements of a collection literal (`[Normal, w]`) and the branches of an `if` agree
the same way. A `match` is checked for coverage over the matched type's own variants. Where nothing expected decides it, the use is an error that names the types and
the qualified forms:

```text
val a = Normal
error: `Normal` is a variant of `Slant` and `Weight`, and nothing here expects one of those types: write `Slant.Normal` or `Weight.Normal`
```

Within one type each variant has its own name. A shared name is still one name of the module, so no
function, module-level value or object of the module may have it, and a type's companion object may
not have a name another type's variant has. From another module a shared name is reached as any
variant is, `fonts.Normal` or by `import fonts.Normal`, and decided the same way; a private type's
variant is not seen there, so where only one of the types is public its variant needs no deciding.

The same expected type also reaches past a nearer declaration to the core library's variants, which
a module's own variant or an imported one of the same name hides (`None`, `Some`, `Ok`, `Err`,
`Left`, `Right`). A mirror of a C enum with an enumerator `NONE` declares a variant `None`, and
wherever an `Option` is expected the bare `None` is still `Option`'s; everywhere else it is the
module's own, as the [scoping order](#visibility-and-modules) says:

```avin
data ImageFormat
    None
    Png

find(n: int) -> Option[int] = if n > 0 then Some(n) else None   // Option's

val f = None                           // ImageFormat's: nothing expects an Option
val o: Option[ImageFormat] = Some(None)  // Option's Some, ImageFormat's None
```

Only the hidden variant's own type chooses it: an expected `int`, `Any` or a trait leaves the
nearer variant, which is then reported against what was expected.

A variant is also reached through its type, `Type.Variant`: `Shape.Circle(1.0)`, `Option.None`,
and in patterns `case Shape.Circle(r)`. The qualified name is the same constructor as the bare one,
so the two forms mix freely in one expression or one `match`, and a `match` over both is checked for
coverage as one over either. A generic type's arguments are inferred as for the bare name, or written
once, on the type (`Option[int].None`) or on the variant (`Option.None[int]`,
`Option.Some[long](9)`), all of them or a leading prefix with the rest inferred. A pattern writes
none, `case Option.Some(x)`: the value matched decides them. Another module's
type is reached through the module, `palette.Color.Red`, or by its own name where it is imported,
`Color.Red`, and only where the type is public. A local, a parameter or a member named like the type
hides it, so `Color.Red` is then that value's member. `Type.Name`, with `Name` capitalized and
neither a variant of the type nor a member of its companion or a static member of the type, is
reported with the variants the type has; on a class it is reported as a class having none. A
companion object cannot declare a method or field named after one of its type's variants, since
`Type.Variant` names the variant.

```text
area(s: Shape) -> real = s match
    Shape.Circle(r) -> 3.0 * r * r
    Rect(w, h) -> w * h

main()
    println(area(Shape.Rect(2.0, 3.0)))   // 6
    val none: Option[string] = Option.None
```

`data` values have no identity, derive
`Eq`, `Hash`, `Display` and `Debug` when their fields do, without `derives`, and are matched
exhaustively. Methods are added in an `impl` block; an impl of `Display` or `Debug` replaces the
derived one ([Display and Debug](/spec/equality-and-memory/#display-and-debug)). A data type whose
variants have no fields is an enumeration, and `derives Enumerable` gives it `values`, `ordinal`,
`name` and `valueOf(name)` ([Enumerations](/spec/derivation/#enumerations)).

### Changing fields: `with`

A data value never changes, so a changed one is a new value: `base with { f = v, … }` is a value of
the base's type with each named field replaced and every other field the base's. The left of each
`=` is a field's name or a **path** of field names, `pos.x`, reaching a field inside a field at any
depth; the base is left as it was.

```avin
data Point(x: int, y: int)
data Sprite(pos: Point, name: string)

main()
    val s = Sprite(Point(1, 2), "ship")
    val t = s with { pos.x = 3, name = "ghost" }
    println(s)   // Sprite(Point(1, 2), ship)
    println(t)   // Sprite(Point(3, 2), ghost)
```

- **A path makes again only the values along it.** `s with { pos.x = 3 }` is a new `Sprite` holding
  a new `Point`; every field no path reaches is the base's own value, shared, so a class object or an
  array a field holds is the same object in both. Paths with a common prefix make that prefix's value
  once: `s with { pos.x = 1, pos.y = 2 }` makes one `Point`.
- **Each value is read against the type of the field its path ends at**, so a literal takes that
  type, as an argument does: `{ ratio = 1 }` into a `real` field, `{ items = [4, 5] }` into a `List`,
  `{ next = None }` into an `Option`, a lambda into a function field.
- **The result has the base's type.** A generic data value keeps its type arguments, so
  `Box(1, "a") with { v = 2 }` is a `Box[int]` and `with { v = "s" }` on it is a type error.
- **Order.** The base is evaluated once, first, then the values in the order written, and the new
  value is made after the last of them. A value that leaves the function (`return`, `?`) leaves
  before anything is made.
- **The base is a data type with one variant.** On a type with several the fields a value has are
  decided at run time, so `with` is refused there, and a `match` makes the new variant. A class, a
  type parameter or any other type is refused too: a class's object is changed through its `var`
  fields.
- **A path goes through data fields only**: each name before the last must hold a data type with one
  variant. A field holding a class, a type with several variants (`Option` among them), a type
  parameter or a built-in type is refused, with the suggestion to replace that field whole.
- **Only fields are named.** A method of the type, even one taking no arguments, is refused; a name
  the type does not have is reported with the type's name and its nearest field.
- **No two changes overlap**: a path named twice is an error (`` `pos.x` is changed twice in one
  `with` ``), and so is a field changed whole beside a path inside it, `pos = q, pos.x = 1`, in either
  order.
- **An element is not a field.** `with { xs(0) = 1 }` is a syntax error that suggests replacing the
  whole field, `xs = …`. Compound assignments (`+=`) are not written there, and `with { }` names
  nothing and is an error.

`with` is a postfix tail like a call's parentheses: it binds to the operand before it (`-p with { … }`
is `-(p with { … })`, `a + p with { … }` changes `p`), its base may be any expression, a call, a
field or another `with`, and a member read may follow the brace (`s with { pos.x = 3 }.pos`). A brace
suspends the off-side rule, so the changes may stand one per line, and the last may carry a comma.
`with` is a keyword only after an operand and before `{`; everywhere else it is a name, so a field,
a method or a local may be called `with`, and `a with b` stays an infix call of a method `with`.

## class

```text
open class Account(owner: string, var balance: int)
    if balance < 0 then panic("an account cannot open overdrawn")
    val opened = clock.now()
    println(s"opened an account for $owner")

    deposit(amount: int)
        balance += amount

    describe() -> string = s"$owner: $balance"

class Savings(owner: string, balance: int, rate: real): Account(owner, balance), Display
    override describe() -> string = "savings " + super.describe()
    override display(out: Writer, spec: FormatSpec) = out.write(describe())
```

- **The class is its constructor.** The parameter list after the name is the constructor's
  parameter list, and there is no other constructor; a factory belongs in the companion `object`,
  whose `apply` methods the class's name also calls, as "A companion's `apply`" below describes.
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
  An `abstract class` may leave members without bodies and cannot be instantiated. `open` and
  `abstract` mark a class only: a method takes neither, since every public method of such a class
  may be overridden and one without a body is abstract already; nor does a trait, a data type, an
  object or a top-level function. `override` marks a method alone. Each misplaced
  word is an error that says to remove it.
- Instances are heap objects with identity. Unless the class implements or derives `Eq`, `==` and
  the hash are by identity, like `===`; `class Point(x: int, y: int) derives Eq, Hash, Display`
  compares and hashes by its fields, which for `derives` are its constructor parameters. Display
  exists only if the class implements or derives it. See
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
- **A field may share its name with a trait's method that takes a parameter list**, so a field of
  any name leaves a type its `Display`, `Debug`, `Eq`, `Hash`, `Ord`, `Iterable` and every other
  trait's methods, written or derived: `data Config(host: string, debug: bool)` prints, compares
  and hashes. `x.name` and a bare `name` read the field; `x.name(args)` and a bare `name(args)` call
  the method, so a field holding a function is called by reading it first, `val f = x.name`;
  through the trait, or a type parameter it bounds, only the method is seen. Every
  other method of a field's name is an error at its declaration: a method of no trait, and a
  method of no parameter list, which the field implements instead.
- Methods take the forms top-level functions take: a body after `=`, an indented body, or clauses,
  which match the arguments and not the receiver. A method may have type parameters of its own,
  after its class's.
- A method declared with a parameter list, `()` included, and named without a call is a function
  value bound to its receiver: `val f = acct.deposit` is an `(int) -> unit` that deposits into
  `acct`, which is evaluated once, where the value is made. Inside the class a bare method name is
  the same over `self`. Through a superclass's or a trait's type the value calls the implementation
  the object's class has. A method's own type parameters are written, `box.pair[bool]`, or come
  from the expected function type. The same holds for the methods an `impl` adds to any type,
  `"hey".repeat` included. A method declared with no parameter list has no such value; see below.
- **A method declared with an empty parameter list is a value only where a function type is
  expected.** `val f: () -> int = c.bump`, `apply(c.bump)` with `apply(f: () -> int)`, and an
  element of a `Vector[() -> int]` make the value; anywhere else, `println(c.bump)`,
  `s"${c.bump}"`, `c.bump == 0`, `val v = c.bump` with no type, `xs.iterator.next()`, or an
  argument whose parameter is a type parameter or `Any`, naming it without a call is an error
  naming both fixes: "`bump` is called with `()`: write `c.bump()`, or `() -> c.bump()` for a
  function value". The fix is written on the receiver as the program named it: a local or a
  parameter by its name, `self`, a type for a static member, and nothing for the implicit `self` of
  a bare name. A placeholder's is `_.toInt()` alone, since a `_` cannot stand in a lambda of its
  own; any other receiver, `Some(1).iterator`, is not repeated, and the fix is `.iterator()`. Among overloads that include one of no
  parameters, a bare name with no expected function type gets the same error. A missing `()` is
  far likelier there than a wanted function, which would otherwise surface only as wrong output.
  A method with parameters, `acct.deposit`, is a value wherever it is named.
- A field declared in the body without a type has its initializer's type. The body cannot
  `return`: it runs to its end to make the object.

### Parameterless methods

```text
class Chain(items: Vector[int])
    head -> int = items(0)
    tail -> Chain = Chain(items.slice(1, items.length))
    second -> int = tail.head
    clear() -> Chain = Chain([])

val c = Chain([1, 2, 3])
c.tail.head        // 2
c.clear()          // `()`: declared with a parameter list
val f = () -> c.head
```

- **A method declared with no parameter list is called by its name alone.** `head -> int = …`
  declares a method that `c.head` calls, with no parentheses; inside the class `head` and
  `self.head` call it too. Each read runs the method again, on a receiver evaluated once for that
  read, so `c.tail.head` and `s"${c.head}"` read as a field would and compute as a call does.
- **A method declared with `()` keeps them at the call**: `buf.clear()`, `iter.next()`. The
  parentheses say the call does something, so a method with an effect is declared with them.
- **A parameterless method has no function value.** Naming it calls it; where a function is
  wanted, a lambda says so, `() -> c.head`. Where a function type is expected and the method yields
  something else, the error names the lambda.
- **Parentheses after one call what it yields.** `t.adder(4)`, where `adder -> (int) -> int`, calls
  the function `adder` yields, and `t.items(2)` indexes the sequence `items` yields. Where the
  result is neither a function nor a sequence given an index, `c.head` is an error naming the fix,
  `head`.
- **It is the only method of its name**: a class, a trait or the impls of a type cannot also have a
  method of that name with a parameter list, in the type itself or inherited, since `x.head` would
  leave no call to choose between them.
- **The shape is part of the signature.** An override, or an implementation of a trait's method, is
  declared without a parameter list exactly where the method it replaces is: overriding `head` with
  `head()`, or the reverse, is an error.
- **A public field implements an abstract one.** A trait's or an abstract class's `size -> int`
  with no body is implemented by a method of the name, or by a public `val` or `var` field of the
  name — a class parameter declared `val` among them — or, for a data type, by a field of its one
  variant. The field's type is the method's result. A private field does not implement it, and a
  field cannot share its name with a method that has a body.
- **A member is declared without a parameter list as it is; a function at the top level or in a
  block, with `def`** (below). A parameterless method takes no type parameters of its own,
  since `x.first[int]` would read as indexing its result; its class's type parameters it has.
- **A static one is read on its type the same way**: `Color.values`, and through a type parameter
  bounded by its trait, `T.values`, the type argument's own in each instance; `T.values()` is the
  error naming the fix.
- **The standard library declares every pure accessor without a parameter list**: a method of no
  parameters that answers from its receiver, or a static one from its type, changing nothing and
  handing out nothing to change or use up. `opt.isDefined`, `opt.get`, `r.isOk`, `e.swap`,
  `s.trim`, `s.headOption`, `c.isDigit`, `n.abs`, `x.isNaN`, `d.toSeconds`, `date.year`,
  `5.seconds`, `m.groupCount`, `err.message`, `json.keys`. What keeps `()` is what has an effect or
  hands out a fresh mutable object or an iterator (`next()`, `iterator()`, `toBuffer()`,
  `charIterator()`); what reaches a resource outside the program (a file, a socket, a child process,
  a lock, a future, a signal); a constructor or factory (`Vector.empty[int]()`, `newBuilder()`); a
  name that also has an overload with parameters (`s.toInt()` beside `s.toInt(16)`, and with it the
  whole family `toLong()`, `toReal()`, `toBool()` and their `…Option()`; `stripMargin()` beside
  `stripMargin('#')`); and the methods of the traits every type implements and derives produce,
  `toString()`, `hash()`, `zero()`, `one()`, `negate()` and `shape()`.
- **With `def`, the result type may be left to inference**: `def perimeter = 2.0 * (w + h)`. Without
  `def`, a name followed by `=` is an assignment.
- It is not a field: `c.head = 3` is an error, and a class cannot declare a field and a method of
  one name. `super.head` reads the superclass's implementation of one, where `super.describe` names
  no function value of a method with a parameter list.

#### Functions with no parameter list

```text
var ticks = 0

def now -> int
    ticks += 1
    ticks

def adder -> (int) -> int = x -> x + now

main()
    println(s"$now $now")      // 1 2
    def step -> int = now * 10
    println(step)              // 30
    println(adder(100))        // 104
    val f = () -> now
```

- **A function at the top level or in a block is declared without a parameter list with `def`**:
  `def now -> int = …`. It follows the rules of a parameterless method: it is called by its bare
  name, `now`, its body running again at every read; parentheses after it call what it yields, a
  function or a sequence given an index, and are otherwise an error naming the fix, `now`; it has
  no function value, so where a function is wanted a lambda names it, `() -> now`, and the error
  where one is missing says so; it takes no type parameters of its own; and it is no variable, so
  `now = 3` is an error.
- **Without `def`, `now -> int = …` at the top level or in a block is an error** that names `def now
  -> int = …`, `val now = …` for a value computed once, and `now() -> …`.
- **It is the only function of its name in its scope**, and shares the scope's one namespace: a
  `val` and a `def` of one name in one module or one block, or a `def now` beside a `now(x: int)`,
  is an error.
- A local one captures what is in scope where it is declared, as a local function does, and may be
  read before its declaration in its block. A public one is read from another module as
  `clock.now`, or by its imported name.

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
  A superclass or trait from another module is named as an import makes it visible, bare or
  through its module: `class Body: shapes.Base("b"), io.Reader`.
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
  arguments substituted) and yields the same result type or one seen as it with no conversion, since
  a result is a covariant position: a subclass, a tuple whose parts are (`(Dog, Dog)` for
  `(Animal, Animal)`), an instance of a covariant generic type at narrower arguments
  (`HashMap[K, V]` for `Map[K, V]`, `Option[Dog]` for `Option[Animal]`), a function with a narrower
  result, or a string or object for `Any`. A result that would need a box is not one: `-> int`
  does not override `-> Any`, nor `-> (int, int)` `-> (Any, int)`. The same holds for a method
  implementing a trait's. A method
  with type parameters of its own is overridden by one declaring as many, matched by position:
  `override tag[V](item: V)` overrides `tag[U](item: U)`. An override assumes no more of its type
  arguments than the method it overrides, since a call through the base type passes any type the
  base's bounds admit: each bound it writes is implied by the overridden method's bounds on the same
  parameter, being one of them, a trait or class one of them conforms to (`[U: Shape]` over
  `[U: Named]` where `trait Named: Shape`), or `Eq` or `Hash`, which every type has. It may keep the
  bounds, weaken them or drop them; `override tag[V: Ord](item: V)` over `tag[U](item: U)` is an
  error naming the bound and the method. The same holds of a class's or an impl's method
  implementing a trait's, for a data type or a built-in type, of a static member, and of a trait's
  override. A method sharing an inherited method's name and parameter types without
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

A trait declares methods, which may have default bodies, abstract `val` members a class supplies
with a field, and fields with values, which every class conforming to it stores (below). A trait may
require others: `trait Iterator[+A]: Iterable[A]`.

A trait is both a type and a bound. Used as a type, a value of it is a reference and calls dispatch
dynamically. Used as a bound (`[T: Shape]`), calls are direct, because the code is monomorphized.
A trait whose methods mention `Self` other than as the receiver (`Add`, `Ord`, `Eq`) can only be used
as a bound.

- **`Self` in a trait's methods is the type implementing the trait.** A method `less(other: Self)`
  is implemented by `Money` as `less(other: Money)`; a call substitutes the receiver's type, so
  through `[T: Ordered]` it takes a `T`. `Self` is written only in a trait's methods.
- A trait mentions `Self` when its methods or `val`s do, or a trait it requires does. Naming it as a
  type (a parameter, a field, a type pattern) is an error; it is named as a bound, after a class's
  `:` and in an impl's header.
- Inside such a trait's default bodies `self` is a `Self`, so it passes where a parameter takes one:
  `greater(other: Self) -> bool = other.less(self)`. The body is shared by every conforming type,
  and its calls on `Self` values go through their method tables.

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
- A trait's body declares methods and fields, all public, and no statements: it has no constructor.
  A trait cannot be instantiated.
- **A trait may override a method of a trait it requires**, marked `override` and with a body:
  `trait Square: Shape` with `override describe() -> string = …`. In every class and data type
  conforming to the trait, its body replaces the required trait's default, or gives an abstract
  method one, so a call through either trait's type reaches it. Like a class's override, it takes
  the defaults of the method it overrides. Declaring a required trait's method
  again without `override` is an error. Two traits overriding one default differently are two
  implementations, which a class conforming to both overrides.
- **A trait's override may have no body**, re-declaring the method with a narrower result:
  `trait SortedMap[K, +V]: Map[K, V]` with `override updated[W >: V](key: K, value: W) ->
  SortedMap[K, W]`, so `sorted.updated(k, v).headOption` type-checks through the sub-trait. It takes
  the overridden method's parameters, type parameters and lower bounds, and yields its result or,
  where that is a class or trait, one conforming to it; its result is checked against the trait's
  variance. It shares the overridden method's position, so a type conforming to the sub-trait has one
  implementation, reached through either trait, and that implementation yields the narrowest
  result. A re-declaration without a body of a method that has a default makes it abstract again:
  the default is not inherited, and each implementer of the sub-trait gives its own (or a trait
  further down gives a body again). A method a class inherits from its superclass implements the
  re-declaration only if it yields the narrower result.
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

### Fields

```text
data Position(line: int, column: int)

trait Positional
    var pos: Option[Position] = None
    at(line: int, column: int) = pos = Some(Position(line, column))

class Token(val text: string): Positional

val t = Token("x")
t.at(3, 4)
val p: Positional = t
p.pos = None
```

- **A trait may declare fields with values**, `val name: T = expr` and `var name: T = expr`. Each
  declares its type, and is public. A field without a value is the abstract `val` above, and is
  never `var`. A conforming type supplies an abstract `val` only with a field; an abstract
  [parameterless method](#parameterless-methods), `size -> int`, is implemented by a method or by a
  public field.
- **Every class and object conforming to the trait stores the trait's fields**, each of its own. A
  subclass of a class that conforms stores them once, in the superclass; a generic trait's fields
  take the type arguments the class names it with, `class Ints: Box[int]`.
- **They are read and assigned as fields**: by bare name and through `self` in the trait's methods,
  and as `x.name` outside. Through a class type, `x.name` is the class's field; through the trait,
  or a type parameter the trait bounds, it reaches the field of the object's own class through its
  method table, for reading and, for a `var`, for assigning. A trait's field supplies the abstract
  `val` of its name of any other trait the class conforms to.
- **Construction runs from the top down**: the superclass's constructor, then the class's
  parameters are stored, then the fields of the traits the superclass does not already conform to
  are computed, then the class body runs. The traits come in the order of the `:` list, each after
  the traits it requires and each once, and each trait's fields in the order it declares them.
- **A field's value is computed with `self` as the trait's receiver**, in the trait's module. It may
  read the fields initialized before it: those of a superclass, the class's parameters (as the
  trait's `val`s they supply), and the trait fields computed earlier. It uses `self` for nothing
  else: it calls no method of the object and does not pass it, as a class body before its last field
  does not.
- **Only a class or an object conforms to a trait with fields**, or to one requiring such a trait: an
  impl of it for a `data` type, a built-in type or a tuple is an error. A trait's field is never
  overridden: a class declaring a field or a method of its name is an error, and so is a class
  conforming to two traits that declare fields of one name.
- A `var` field makes it an invariant position: a covariant trait's type parameter cannot appear in
  its type, as in a class's.

### Static members

```text
trait Add
    add(other: Self) -> Self
    static zero() -> Self

impl Add for Point
    add(other: Point) -> Point = Point(x + other.x, y + other.y)
    static zero() -> Point = Point(0, 0)

total[T: Add](a: T, b: T) -> T = T.zero() + a + b

val origin = Point.zero()
```

A trait may declare **`static` members**, which belong to the implementing type rather than to a
value: `Add`'s `zero` and `Mul`'s `one` are the identities of their operators.

- **A static member is called on a type**: a type's name (`int.zero()`, `Point.zero()`,
  `Pair[int, string].zero()`) or a type parameter whose bounds have it (`T.zero()` with `T: Add`).
  Through a bound each instance calls its type argument's own directly, an open class's included,
  since no object is involved. Calling one on a value is an error, and so is calling a value's method
  on a type. A local, a parameter, a member or a module value of the name hides the type.
- **The name of a data type whose variant of its own name has no fields is also a value**, so
  `Lone.hi()` of `data Lone()` calls the method `hi` of that value, as `val l = Lone; l.hi()` does,
  and `Lone.hi` without a call is a function value bound to it. Where the type has a static member
  of the name, `Lone.unit()`, the name is the type's and calls the static member; a value's method
  of that name is then called on a value.
- **In a trait a static member has no body**: a trait's default bodies are shared by every
  conforming type, and a static member is each type's own. `Self` in its signature is the
  implementing type, as in a method's. A trait never overrides a static member, and its default
  bodies cannot call one on `Self`, which there is no one type.
- **Every implementation gives it, with a body**, marked `static` as the trait's member is: an impl,
  and a class's body with `override static`. An abstract class gives it too, since it belongs to the
  type and not to a subclass's objects. A static member is static exactly where the trait's member
  is, and is a trait's member, an implementation of one, or a
  [built-in type's own](#static-members-of-built-in-types). A class or `data` type keeps what belongs
  to the type in its companion object.
- **A static member's body has no `self`**: neither `self` nor the members of a value by bare name.
  It may have parameters, defaults and type parameters as a method does.
- **A trait with a static member is a bound and not a type**, as one mentioning `Self` is: a value of
  the trait's type would have no type to give the member.
- **A static member named without a call is a function value** of its parameters, as a method of a
  value is: `xs.map(string.fromUtf8Lossy)`, `val zero = Point.zero`. Where the member is overloaded,
  the function type expected says which, and without one the value is an error.

### Static members of built-in types

```text
impl string
    static fromUtf8(bytes: Array[byte]) -> Result[string, Utf8Error] = …
    static fromUtf8Lossy(bytes: Array[byte]) -> string = …

impl int
    static parity(n: int) -> string = if n % 2 == 0 then "even" else "odd"

val text = string.fromUtf8Lossy(bytes)
```

A built-in type has no companion object, since it cannot be declared, so what belongs to the type
itself is declared as a **`static` member in an impl of the type without a trait**: `string`,
`char`, `bool`, `real`, `byte` and each integer type. The core library declares the built-in types'
own (`string.fromUtf8`); a program adds one as it adds a method, in an [extension](#extensions), seen
where its module is imported. Settled by the user 2026-11-04.

- **It is called on the type's name**, `string.fromUtf8(bytes)`, and is a value without a call, as
  any static member is; calling it on a value is an error, and so is calling a value's method on the
  type. Its body has no `self`, and calls the type's other static members on the type,
  `string.other()`.
- **A name is a static member or a method of the type's values, not both**, so `string.name` and
  `s.name` never mean two different functions. Static members of one name overload one another by
  parameter types, as methods do.
- **What the language gives a built-in type cannot be redeclared**: `wrap` on every built-in type,
  which `i8.wrap(x)` reads, and the limits of the numeric types and `char` (`MaxValue`, `MinValue`,
  and `real`'s `MinPositiveValue`, `PositiveInfinity`, `NegativeInfinity`, `NaN`).
- An extension's static member with the parameter types of one the type has is an error where it is
  declared, as an extension's method is; a `private` one is its module's.
- A generic impl, an impl adding to a trait and an impl of a class or `data` type declare no static
  member of their own.

## impl

`impl Trait for Type` makes an existing type conform to a trait after the fact, including the
built-in types:

```text
impl Display for Shape
    display(out: Writer, spec: FormatSpec) = …

impl Shape
    perimeter() -> real = …
```

`impl Type` without a trait adds methods to a type, and may be written in any module: in the type's
own module its methods are the type's, and in another module the impl is an **extension**, whose
methods are seen where that module is imported (below). A conformance, `impl Trait for Type`, lives in
the module of the trait or of the type, so a type has one answer to it everywhere; the built-in types
belong to the core library's module, `avin`. Declaring a trait in a class's header and writing a
separate `impl` for it are the same conformance; it may be stated once.

`impl` is how Avin expresses type classes (`Ord`, `Hash`, `Display`) and adds methods to types you
do not own. Resolution is coherent: one implementation per trait and type, found without search.

**A generic trait whose members are all static, and which requires no other trait, may be
implemented by one type at several type arguments**, once at each: `impl From[IoError] for
AppError` beside `impl From[ParseError] for AppError`, or `class Note : Make[int], Make[string]`.
No value of such a trait exists, so each call names a type, and the member called is the one taking
the arguments' types: `AppError.from(e)` as any overloaded method, and on a type parameter bounded
by several instances, `T: Make[int] + Make[string]`, the instance whose member the arguments fit.
Each conformance gives every member of the trait at its own type arguments. Any other trait is
implemented once, at one list of type arguments.

A built-in type conforms to a trait only through an impl, and meets the trait as a bound: each
instance of generic code calls the impl's method directly. A built-in value is not an object, so a
trait's default bodies, which every conforming object shares, do not reach it: an impl for a
built-in type implements every method of the trait, defaults included.

- **An impl adds methods, never state.** Its body declares methods only: no fields and no
  statements. A method an impl in a class's or an object's module adds to it is one of its methods,
  as if the class's body declared it: it sees the private fields, is inherited by a subclass, and a
  subclass may override it. A method with the name and parameter types of one the type already has
  is an error; one with other parameter types overloads it.
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
  of a trait always does: a conformance holds for every instance of a type, so an impl of a trait
  for particular type arguments (`impl Show for Box[int]`) or for a type built from its parameters
  (`impl[T] Show for Box[List[T]]`) is an error, and no two conformances of one type can overlap.
- **An impl without `for` may add methods to any type built from its parameters**:
  `impl[T] Option[Option[T]]`, `impl[A, B] Pair[A, List[B]]`, and, adding to a trait,
  `impl[T] Seq[Seq[T]]` and `impl[K, V] Iterable[(K, V)]`. Each of its parameters is part of that
  type, so a call learns them from the receiver; one that is not is an error. The type is matched
  against the receiver's static type: `Option[Option[int]]` matches `Option[Option[T]]` with `T` as
  `int`, and `flatten` on an `Option[int]` is an error saying what the receiver would need to be
  ("`flatten` is for an `Option` of an `Option`; this is `Option[int]`"). Where a part of the type is
  covariant, a reference matches through its traits and superclasses, as it would be passed:
  a `Vector[List[int]]` is a `Seq[Seq[int]]`, with `T` as `int`. Elsewhere a part matches only its
  own type, and a parameter written twice (`impl[T] Pair[T, T]`, `impl[T, E] Result[Result[T, E],
  E]`) takes one type. The impl's bounds apply as for any impl: with `impl[A: Ord, B] Seq[(A, B)]`,
  a sequence of pairs whose first parts are not ordered lacks its methods. Inside the methods `self`
  has the type the impl names (or, adding to a trait, is a `Self` of it), and the methods are
  called directly, as other added methods are.
- **Two impls whose types both match a receiver** and that add a method of one name and parameter
  types make a call of it an error, as two traits' added methods do; so does a method of the type's
  own with the name and parameter types of one an impl adds to a type built from its parameters.
  Impls whose types no receiver matches both, as `Box[List[T]]` and `Box[Option[T]]`, may add
  methods of one name and parameter types, and the receiver says which is called.
- **An impl may bound the type's parameters further** (`impl[T: Display] List[T]`,
  `impl[T: Ord] Ord for Box[T]`), and then holds only for the type arguments that meet the bounds:
  the trait's conformance, or the methods it adds. Calling one of those methods on an instance that
  misses a bound is an error naming the bound. [Generics](generics.md) has the rules.
- A method added to a built-in type (`impl string`, `impl int`) is called on a value of that type,
  `"hi".shout()`, `7.squared()`.
- **An impl without `for` may add methods to tuples and to functions**, naming a tuple or a function
  type built from its parameters: `impl[A, B] (A, B)` adds to every pair, `impl[A] (A, A)` to the
  pairs whose parts are of one type, `impl[A, B] (A) -> B` to every function of one parameter. The
  tuple and function types are the core library's, so such an impl elsewhere is an extension. The
  receiver matches as for any type built from parameters, and a call on a tuple or a function of
  another shape says what it would need to be ("`swap` is for a pair; this is `(int, int, int)`").
  A function type is a trait, so its methods reach a value of a type naming that function type, a
  sequence or a map among them, and the type's own method of the same name and parameter types is
  found first; a `@sendable` function type adds only to `@sendable` functions. Neither form has
  static members, and a tuple's operators stay the language's. An impl of a trait is never for a
  function type, and one for tuples is the core library's alone. The impl has at least one type
  parameter, since `impl (`… with none begins a function named `impl`; a part may still be written
  out, as `impl[A] (A, int)` adds to the pairs whose second part is an `int`.
- **An impl without `for` whose type is a trait adds methods to the trait** (`impl[T] Seq[T]`,
  `impl[T: Display] Seq[T]`). Every value whose type conforms to the trait has
  them: a class or `data` type implementing it, the trait's own type, a type parameter it bounds.
  Inside them `self` is the value and `Self` its type; the trait's methods, and those other impls add
  to it, are in scope by bare name. They reach the value only through the trait's methods, so they
  are not variance positions: `contains(element: T)` is written on `Seq[+T]`. The methods an impl
  adds to an `open` or `abstract` class are, by contrast, in the class's method table, and so are
  variance positions like the class's own ([generics](generics.md)).
- **A call of an added method is compiled for the receiver's static type.** Where that is a class or
  a `data` type, the trait's methods the added one calls are that type's, called directly; where it
  is the trait, they go through the object's method table. An added method is in no method table and
  nothing overrides it; it may not take the name of a method the trait declares.
- **A type's own methods are found before those added to its traits.** An added method with the name
  and parameter types of a method the receiver's type has (its own, an inherited one, or one of a trait
  it conforms to) is passed over, so a type keeps a faster version of its own wherever its static type
  is known; code that sees only the trait calls the added one. An added method with other parameter
  types overloads the type's. Where impls add a method of one name and parameter types to two traits
  a type conforms to, the one added to the nearer trait is the type's when that trait requires the
  other: `impl[T] Seq[T]`'s `map` hides `impl[T] Iterable[T]`'s on every sequence. Where neither
  trait requires the other, a call of it on that type is an error.

```text
impl[T] Seq[T]
    contains(element: T) -> bool = exists(x -> x.equals(element))

present[T](xs: Seq[T], x: T) -> bool = xs.contains(x)
```

### Extensions

An impl without `for` written outside the module of the type or trait it adds to is an
**extension**. Every form above may be one: `impl string`, `impl[T: Ord] Seq[T]`,
`impl[T] Option[Option[T]]`, `impl Box` for a class another module declares.

```text
// words/words.avin
module words

impl string
    shout() -> string = s"${self}!"

impl[T: Ord] Seq[T]
    median() -> T = sorted().apply(length / 2)

// main.avin
import words

main() = println("hey".shout())       // hey!
```

- **An extension's methods are seen by its own module and by every file that imports the module**:
  whole (`import a.b`, `import a.b as c`), or any declaration of it (`import a.b.X`,
  `import a.b.{X, y}`), directly or through a module that exports a declaration of it
  ([modules](#visibility-and-modules)). Nothing else is needed, and a file that imports neither does not see them:
  a call there is an error naming the module that adds the method and saying the file does not
  import it. The core library's impls are seen everywhere. Importing is per file, as for names.
- **An extension adds methods, never meaning.** Its methods are not the type's: they reach no private
  field or method of a type of another module, are in no method table, are not inherited as the
  type's and are not overridden; a call is compiled for the receiver's static type, as the methods
  added to a trait are. Inside them `self` is the receiver, and the type's public fields and methods
  are in scope by bare name.
- **A type's own methods are found first.** An extension's method with the name and parameter types
  of a method the type has, its own or an inherited one, could never be called, and is an error where
  it is declared; one with other parameter types overloads the type's. A method an extension adds to
  a trait is passed over for a type that has one of its name and parameter types, as other methods
  added to a trait are, so an extension never changes what an existing call means.
- **Two extensions a file sees, from different modules**, that add a method of one name and
  parameter types to types a receiver matches make a call of it on that receiver an error naming both
  modules. Within one module the second is an error where it is declared.
- **A `private` method of an extension is private to its module**: the module's other code calls it,
  and a file importing the module does not.
- **Infix calls, method values and calls through a bound see extensions as dotted calls do.** In
  `middle[S: Seq[int]](xs: S) -> int = xs.median()`, `median` is found where `middle` is written,
  so `middle`'s own file must see the extension; each instance calls it at its type arguments.

## object

```text
object Config
    val defaultPort = 8080
    var verbose = false
    port(env: Map[string, string]) -> int = …
```

An `object` is a singleton value. An `object` with the same name as a class or `data` type in the same
module is its companion and may access its private members. `object`s may extend a class and traits.

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
any other. There is no second instance to make, so calling the object's name, `Name(…)`, calls the
`apply` methods it declares; an object without one cannot be called.

```text
object Twice
    apply(n: int) -> int = n * 2
    apply(s: string) -> string = s + s

main()
    println(Twice(21))       // 42
    println(Twice("ab"))     // abab
```

An object shares its name only with its companion class or `data` type. Where both exist, the name in
call position, `Account(…)`, is the class's constructor, and the bare name `Account` or a member
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

A `data` type has a companion the same way: an object of its name in its module. A variant of the
type's own name stays its constructor in call position, so `Point(1, 2)` makes a point while
`Point.origin()` is the companion's; a type with no variant of its name, such as `List`, has
nothing of that name to call but its companion's `apply`. The methods the type's impls add and the
companion's methods each use the other's private members. A companion cannot share its name with a
variant of another type, nor with a variant of its own type's name that has no fields, whose bare
name is already a value.

```text
data Point(x: int, y: int)

object Point
    origin() -> Point = Point(0, 0)

object List
    newBuilder[T]() -> ListBuilder[T] = ListBuilder()
```

### A companion's `apply`

A companion's `apply` methods overload the constructor its name has in call position: the class's,
or the data type's variant of the type's name. `Name(…)` chooses among the constructor and every
`apply` by the rules of any overloaded call: the candidates the arguments fit, by number, name and
type, and of those the most specific by parameter types, a candidate with a repeated parameter only
where none without one fits. By-name parameters, defaults, repeated parameters, generic inference and
a trailing block all work as for any call, so a by-name `apply` takes an indented block after the
name.

```text
class Future[T](private val cell: Cell[T])

object Future
    apply[T](body: -> T) -> Future[T] = Future(Cell(body))

main()
    val direct = Future(Cell(1))       // the constructor: a `Cell` fits only it
    val page = Future(fetch(url))      // `apply`: the body runs inside it
    val sum = Future
        val a = 20
        a + 1
```

Inside the `apply`, `Future(Cell(body))` is the constructor: both candidates accept a `Cell[T]`, and
the constructor's parameter type is the more specific.

- **Type arguments** written at the call, `Future[int](…)`, are the chosen candidate's own: the
  class's or the data type's type parameters for the constructor, the method's for an `apply`.
- **Same parameter types.** An `apply` taking the same parameter types as the constructor, up to the
  names of type parameters, is refused where it is declared, since no call could choose between
  them; whether a parameter is by-name does not tell two apart.
- **No constructor of the name.** Where the name has no constructor in call position (a data type
  with no variant of its name, such as `List`, or an abstract class), `Name(…)` calls the
  companion's `apply` alone. The library gives `List`, `Set` and `Map` one taking their elements:
  `List(1, 2, 3)`, `Set("a", "b")`, `Map((1, "one"), (2, "two"))`. A plain object's `apply` is
  called by the object's name the same way.
- **As a function value.** Where a function type is expected, the name of an object that declares
  `apply` is its companion class's constructor or the `apply` whose parameters accept that type's,
  chosen as for any overloaded function used as a value: `[1, 2].map(Pair)`. A variant with fields
  is not a value, so a data type's companion offers its `apply` methods alone. Elsewhere the bare
  name is the object.
  An object that is itself of a function type stays that value.
- `Name.apply(…)` is an ordinary member call, and chooses among the `apply` methods alone.

A file may also declare `val`s and `var`s at module level, beside its functions. Each is a value of
the program, read and (for a `var`) assigned from any function by its name. Another module reaches a
public one as it reaches any declaration, by the name an import brings in or through the module's
name, and assigns a `var` the same way: `store.latest = b`, `s.latest += 1` after `import app.store
as s`, or `latest = b` after `import app.store.latest`. A private one is reached by neither, and only
a `var` is assigned; a field of a module's object is assigned as any field is, `store.Tally.count =
1`. Each of these is also a place of a multiple assignment: `store.latest, n = n, store.latest`.
Writing module state from a thread's body is refused as reading it is
([threads](/spec/threads/)).

## type

`type Name = T` declares a **type alias**: another name for the type `T`, declared at the top level
of a file beside data types and classes. A generic alias takes type parameters, and each use gives
it type arguments. An alias earns its place where a type is long or repeated: a function type, a
nested generic, a generic with some of its arguments fixed:

```text
type Handler = (Request) -> Result[Response, HttpError]
type Index = Map[string, Vector[(int, string)]]
type Parsed[T] = Result[T, ParseError]
type Lookup = (Index, string) -> Parsed[int]

serve(path: string, h: Handler) -> unit = …
val log: (string) -> unit = println(_)
```

A short type needs no alias, and a value with its own identity and named fields is a `data` type
rather than an alias of a tuple: `data Point(x: int, y: int)`, not `type Point = (int, int)`.

An alias is **transparent**: a use of it is its expansion, the body with each type argument in place
of its parameter, and nothing else. It makes no new type and needs no conversion: a `Parsed[int]` is a
`Result[int, ParseError]` and the other way round, two overloads whose parameters differ only by an alias take
the same parameter types, and a diagnostic names the expansion, the type compared. An alias may be
written wherever a type is: a parameter, a result, a binding, a data or class field, a type
argument, a tuple, a function type, a type pattern (`r: Parsed[int]`), and another alias's body, which may
name an alias declared after it.

- A generic alias takes exactly as many type arguments as it has parameters, and one without
  parameters takes none.
- Its parameters are plain names: no variance, no bounds, no lower bound. The types its body names
  hold each type argument to their own bounds wherever the alias is used, as the expansion written
  out would be: with `type Sorted[T] = TreeSet[T]`, `Sorted[Opaque]` is refused where `Opaque` is not
  `Ord`, and `f[T: Ord](s: Sorted[T])` is accepted. A body naming no parameter is held to its bounds
  where the alias is declared.
- Every parameter appears in the body: one that does not would make `A[int]` and `A[string]` one
  type.
- The body sees the alias's own parameters and the file's types, and no function's type parameters
  and no `Self`.
- An alias cannot name itself, directly (`type A = Vector[A]`) or through other aliases: its
  expansion would never end. A recursive type is a `data` type.
- An alias's name is a type's: it may not be the name of another type of its module (a data type,
  a class, an object or an alias), of a built-in type, or `Self`. It names no value, so a function
  or a module value of the same name is no clash.
- An alias names only a type. It is not a value, a constructor or a pattern: `type P = Point` does not
  make `P(1, 2)` or match `P(x, y)`, and the diagnostic names `Point`, which does. An `impl` names the
  type itself rather than an alias of it, and so does a class's list of its superclass and traits.
- An alias takes no annotations.
- An alias is public unless marked `private`, imported by name (`import geometry.{Point}`) or reached
  through its module (`geometry.Point`) as any type is; a private one is seen only in its module, its
  expansion reaching others through the signatures that use it.

`type` declares an alias only where a name follows it; anywhere else it is an ordinary name, so a
function, a parameter, a field or a local may be called `type`.

## Visibility and modules

Declarations are public unless marked `private` (visible in the module). A directory is a module;
`module a.b` heads each file. A **package** is a build unit with an `avin.hocon` manifest; a
**project** lists packages ([packages](/spec/packages/)). `import a.b.{X, y as z}` brings names into a file, and `export a.b.{X}` makes another module's
declaration one of the module's own names. The root module `avin`
is reserved for the standard library.

```text
// shapes/circle.avin
module shapes

import geometry.Point

class Circle(val r: int)
    center() -> Point = Point(r, 0)

area(c: Circle) -> int = scale() * c.r * c.r

private scale() -> int = 3          // seen by every file of `shapes`, and nowhere else

// main.avin, in the program's root directory
import shapes.{Circle, area as circleArea}

main() = println(s"${circleArea(Circle(2))}")
```

- **A program is a directory tree.** Each directory under the program's root is a module named by
  its path from the root, `geometry/shapes` being `geometry.shapes`, so a directory is named as a
  declaration is: letters, digits and `_`. Every file of a module begins with `module` and the
  module's name, which must agree with its directory. The root directory's own files are the **root
  module**, which has no name, holds `main` (`main()` or `main(args: Vector[string])`) and takes no header. A program of a single file is
  compiled alone; its header, if any, names its module.
- **A module's files are one namespace.** Each sees every declaration of every other file of its
  module by its bare name, private ones included, and they may not declare one name twice, except
  as overloads of a function. A class's companion object may be in another file of the class's
  module, never in another module. A name declared twice, whether in one file or in two, is
  reported once, at the second declaration, with the place of the first; types, functions of the
  same parameter types, values, objects and variants clash with one another. The program's other
  diagnostics follow from a clash between two files, so while one is unresolved only duplicates
  are reported. The same name in different modules is no clash.
- **Another module's declarations are imported, per file.** `import a.b.X` brings `X` into the file;
  `import a.b.{X, y as z}` brings several, and `as` names one differently. There is no wildcard
  import. Importing a name the module does not declare, a private declaration, a name the file's own
  module declares or one another import already brings in is an error; an import the file does not
  use is not.
- **A module may be imported whole.** `import a.b` binds the module under its last segment and
  `import a.b as c` under `c`; `b.X` then reaches `a.b`'s public `X`, whatever it is: a type
  (`b.Point`, in type position too), function, value, object or variant (`b.Some(x)`, in patterns
  as well). A private `X` or one the module does not declare is an error. The name is the module's
  alone: a declaration of the file's module or another import under the same name is an error at the
  import, and a local, parameter or member of the enclosing class named like the module hides it.
  Importing `a.b` makes only `a.b` reachable, not its submodules; each module named in code is
  imported.
- **Importing a module, or any declaration of it, also brings in the methods its
  [extensions](#extensions) add**, so `"hey".shout()` works in every file that imports `words`.
- **A module may export declarations of other modules**, presenting one surface over modules it
  keeps internal:

  ```text
  // shapes/shapes.avin
  module shapes

  export shapes.geometry.{Point, distance}
  export shapes.color.{Color, Red, Blue as Navy}

  // main.avin
  import shapes.{Point, Color, Navy}
  import shapes

  main() = println(shapes.distance(Point(0, 0), Point(3, 4)))
  ```

  An export is written as an import is, at the top of any file of the module: one name, a `{…}`
  group, `as` naming one differently; there is no wildcard. `export` means this only at the start
  of a top-level line and before a name; everywhere else it is an ordinary name.
- **An export is a name of its module, for the very declaration it names**: no copy and no wrapper.
  Every file of the exporting module sees it by its bare name without an import, and other modules
  reach it as any declaration of the module, imported by name (`import shapes.Point`) or through the
  module imported whole (`shapes.Point`, in types and patterns too). A value made through it is of
  the declaring module's type, so `shapes.Point` and `shapes.geometry.Point` are one type, and a
  `with`, a `match`, an impl or a conformance sees no difference. Exporting a function's name exports
  every public overload of it; a type's variants are exported by their own names, and are reached
  through an exported type as `Color.Red` either way. Types, aliases, classes and their companion
  objects, objects, traits, machines, functions, module-level values and variants may all be
  exported; a `var` is assigned through any name of it.
- **An export names another module's public declaration**, directly or through that module's own
  export, so exports chain: a module exporting `mid.Token`, which `mid` exports from `base`, presents
  `base.Token`. Exporting a private declaration, one the module does not have, a whole module, a
  name of the exporting module itself, or anything from the root module, which nothing imports, is
  an error; so is marking an export `private`. Two modules may export each other's declarations;
  exports that lead round to one another without reaching a declaration are an error at each of
  them.
- **An exported name clashes as a declaration does**: with a declaration of its module, with an
  import of a different declaration by a file of the module, with a module imported under its name,
  and with another export of the name reaching a different declaration. Two paths to one declaration
  are never a clash: a module may export a name and import the same declaration, export it from two
  files, and a file may import it both directly and through an export.
- **A module that exports a declaration of another presents that module's extensions too**: a file
  importing the exporting module, whole or any name of it, and the exporting module's own files, see
  the methods the other module's extensions add, as a file importing the other module would. Only
  the modules whose declarations it exports are presented; one it merely imports is not.
- **What a name means** is decided in that order: the file's module, its declarations and then its
  exports, then the file's imports, then the
  public declarations of the standard library's module `avin`, which every file sees without an
  import. The nearer hides the farther, whatever kind each declaration is, except that a core
  library variant a nearer one hides is still chosen where its own type is expected
  ([`data`](#data)). A program's own modules
  may not be `avin` or under it.
- **Modules may import one another**, in a cycle if need be: the program is compiled whole. Its
  module-level values initialize before `main`, each after the values its initializer needs, across
  modules as within one.

## Literate source

**A file named `.lavin` is a Markdown document, and the part of it indented four columns is the
program.** Everything else is prose and is not compiled. The file's **name** alone decides which
kind of file it is: an `.avin` file is never read this way, whatever its indentation, and a `.lavin`
file with no prose in it is an `.avin` file indented four spaces. There is no other marker: no name
on a block, no directive that opens one, and no way to say that a block ends.

````text
# Halving

This program explains itself. Everything at the margin is prose, and the compiler never
sees it.

    half(n: int) -> int = n / 2

    main()
        val h = half(9)

A paragraph between two indented blocks ends nothing, so a body can be explained a step
at a time and picked up again where it left off.

        println(h)

What a reader should *not* run gets a fence instead of an indent:

~~~
half(n) = n / 2      // the wrong version, with no types
~~~
````

The program that file holds is the indented lines, and it is an ordinary Avin program:

```text
half(n: int) -> int = n / 2

main()
    val h = half(9)
    println(h)
```

The code appears in the order it runs and nothing reorders it, so the file reads correctly as it
is: to a person, to a Markdown renderer, and to `grep`. What it adds over comments is room for a
full explanation between two functions, or between two lines of one.

A `.lavin` file is accepted wherever an `.avin` file is: as a program of one file, as a file of a
module in a directory program (a module may mix the two), in the standard library's directory, and
by `avin test`, which finds its `@test` functions as it finds an ordinary file's. The language
server reads it the same way.

### The rules

- **Four columns is the threshold**, which is Markdown's own, so what a renderer shows as a code
  block is exactly what the compiler reads.
- **The program's indentation starts after the four columns.** A declaration is written four
  columns in, its body eight, and so on; the program's blocks open and close by its own
  indentation, measured from there.
- **Consecutive indented blocks are one program, and prose between them is blank lines.** A
  paragraph between two blocks ends nothing: the next block's lines carry on at whatever depth the
  program had reached, so a function body may be explained a step at a time, its later lines
  indented as deep as its earlier ones. A program line less deep than the block it is in closes
  that block, as it would in an `.avin` file.
- **A fenced block is an illustration.** Both ` ``` ` and `~~~` mark code that is to be read rather
  than run (a wrong version beside the right one, a shell transcript, a fragment of another
  language) and none of it is compiled, however it is indented. Code that runs is indented; code
  that is only shown is fenced. A fence closes at a line of the same character, at least as long,
  and nothing else.
- **What is under a list item is prose.** An indented block inside a list item belongs to that
  item, so a list may hold examples without any of them entering the program. An executable block
  therefore never sits directly under a list; a line of prose at the margin separates the two.

### Positions are the document's

Every line of program text stays where it was written, so every position the compiler reports, in
a diagnostic, a trap's report, a failed test or the language server, is the line and column of the
document the reader has open, the four columns included:

```text
error: `nope` is not defined
 --> doubling.lavin:8:16
  |
8 |         double(nope)
  |                ^^^^
```

### Two forms Markdown accepts are refused

Both would silently drop part of the program, and neither could be diagnosed later, because what
reaches the compiler is simply a smaller program than the author wrote.

**A tab in the indentation.** A tab is as wide as whatever displays it, so a tab-indented line could
be program text in one editor and prose in another. It is refused *before* the four-column test,
because failing that test would silently turn the line into prose and drop a statement from a
function.

```text
error: a tab in the indentation of a literate file — what makes a line program text is four columns of indent, and a tab is as wide as whatever happens to be displaying it, so this line is code in one editor and prose in another
```

**A fence that is never closed.** Markdown runs an unclosed fence to the end of the document. In a
literate file that would turn every declaration below it into an illustration, and the resulting
diagnostic would point somewhere far below the cause. The fence is refused at the line that opened
it.

```text
error: this fence is never closed, so everything below it is an illustration and none of it is compiled — close it, or indent the lines that are meant to run
```

### `tangle` and `weave`

The two halves of a literate system are named for what they do to the source. Every build of a
`.lavin` file tangles it; `avin tangle` prints the program with the prose stripped, line for line
with the document, and `avin weave` renders the document as HTML with its program coloured. Both
read the file as a build does, so a file the compiler refuses is refused with the same diagnostic.
[Getting started](/getting-started/#literate-programs) shows them.

## Tests

A test is a top-level function marked `@test`. `avin test` calls it with nothing, so it takes no
parameters and has no type parameters; it passes by returning, and fails by stopping, which a failed
assertion does. It yields unit, or a `Result[unit, E]` whose error has `Display`: an `Ok` passes, and
an `Err` fails the test with the error shown as the reason (`test answered Err: empty input`,
reported at the test's name).

```text
parse(text: string) -> Result[int, string] = if text == "" then Err("empty input") else Ok(text.byteLength)

@test("reads a word")
readsAWord() -> Result[unit, string]
    val n = parse("word")?
    assertEq(n, 4)
    Ok(())
```

`@test` names the test after its function, or after a description written first, a string that is
not empty. After it come, each at most once:

| written | means |
|---|---|
| `@test(should_trap)` | the test **passes** by trapping, and fails by returning |
| `@test(should_trap: "index out of range")` | …and what the run printed contains the text |
| `@test(timeout: 30)` | the test may run for 30 seconds, in place of the run's limit of 60; `0` is no limit |
| `@test(ignore: "why")` | the test is compiled and checked, and **not run**; the reason is required |

So `@test("keeps both arms", should_trap: "index out of range", timeout: 5, ignore: "the parser drops
the second arm")` is one annotation. A timeout is a whole number of seconds from 0 to 2147483. A test
past its limit is stopped and fails, reading `timed out after Ns`. An ignored test is reported with
its reason, `skip  name  ignored: why`, starts no process, runs none of its module's hooks and fails
nothing; the summary counts it, `3 passed, 1 failed, 1 ignored`.

### Hooks

Four more annotations each mark a top-level function `avin test` calls around a module's tests:
`@setup` before each test and `@teardown` after it, `@setup_all` once before the module's first test
and `@teardown_all` once after its last. A hook takes nothing, yields unit and takes no arguments in
its annotation, and a module declares at most one of each. Each test runs in a process of its own,
which decides what a hook can share:

- `@setup` runs in the test's own process, immediately before the test, so what it stores in a
  module-level `var` is there when the test reads it.
- `@teardown` runs in the test's own process after a test that returned, and in a process of its own
  after a test that trapped or was stopped, so it can rely on releasing only what outlives a process:
  a file, a directory, a path. It runs after a failed test too.
- `@setup_all` and `@teardown_all` each run in a process of their own, sharing nothing with the tests
  but what outlives a process.

A `@setup` that fails fails the test without running it, reading ``not run: `@setup` `name` failed:``
and the reason; neither the test nor the teardown runs. A `@setup_all` that fails is a failure of its
own, and each of the module's tests fails without running; its `@teardown_all` does not run. A
`@teardown` or `@teardown_all` that fails is a failure of its own, on a row naming the hook, beside
the verdict the test already had.

### `@tests` files

A file whose header says `@tests` holds what the tests need and the program does not: fixtures,
helpers, types made for the tests, and tests themselves.

```text
module shapes
@tests

unitSquare() -> Square = Square(1)

@test
perimeterOfTheUnitSquare()
    assertEq(unitSquare().perimeter(), 4)
```

Every command checks it with the rest of the program, and only `avin test` builds it: a program that
runs leaves out what it declares, and its module-level values are never computed. So nothing else may
name what it declares, and the restriction is on the code that names it: a `@test` function or a hook
in any file may, and so may another `@tests` file; a lambda counts as the function it is written in.
Anything else is refused where the name is written:

```text
`unitSquare` is declared in a `@tests` file, so only `avin test` builds it: a `@test` function, a test hook or another `@tests` file may name it, and nothing else
```

An impl in a `@tests` file is for a type or of a trait that file declares: an impl of anything else
would make a type behave one way while the tests run and another in the program that ships. And a
program does not start in a `@tests` file: `main` is declared in an ordinary one.

### The core library's tests

The core library's own tests are in `library/tests`, beside its modules, as a package's are in its
`tests` directory: a program of `@test` functions that uses the library through its public names, as
any program does, and that `avin test library/tests` runs. That directory is no part of the library
every program is compiled with, and `avin test` on a program runs that program's tests only, never a
test the core library or a dependency declares.

## Initialization

- Creating an instance runs the superclass's constructor first, then stores the class parameters,
  then runs the class body top to bottom. Until every field is assigned, `self` may not be passed,
  stored, captured, or used to call a method, and a body statement may not read a field declared
  below it. This is checked at compile time, so no field is ever observed uninitialized:

  ```text
  class Counter(start: int)
      var count = start
      println(describe())            // error: `describe` is called before `label` is initialized
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
- **`lazy val name = expr`** (its type optional, as for `val`) is computed on its first read instead,
  exactly once, and every later read gives the value kept. One that is never read is never
  computed. It is declared at module level or in a class or object body, where each instance has
  its own; `lazy` is a modifier only before `val`, and elsewhere an ordinary name. A `lazy var` is
  an error, since the value is never assigned, and so is `lazy` on a parameter, a class parameter, a
  data field or a local, each of which is given its value where it is made.

  ```avin
  lazy val table: Vector[string] = loadTable()     // read the file when something first needs it

  class Report(title: string)
      lazy val heading = s"== $title =="           // per instance, on the instance's first read
  ```

  A module-level `lazy val` takes no place in the initialization order: its initializer runs where
  it is first read, and an eager value that reads it depends on what that initializer reads. Its
  initializer may read every other module-level value. One whose initializer needs its own value,
  directly or through other lazy values, traps at the read made while it is being initialized:
  ``avin: trap: `first` is read while it is being initialized``. A field's initializer may read every
  field and call every method, since it runs once the object is complete; the class body reads the
  field only where it could call a method, after its last field and not at all in an `open` or
  `abstract` class. A trap inside an initializer stops the program as any trap does.

  The first read is safe between threads: when several threads read the value first at once,
  exactly one runs the initializer and the others wait for it, [safe for the collector](threads.md),
  then all see the same, whole value. A read after the value is stored is one atomic load and a
  branch. A trap in the initializer stops the program while the others still wait, so none of them
  goes on without the value. The read that traps as needing its own value is one made by the thread
  running the initializer, whichever thread that is; a read by any other thread waits.
- Local helper functions may call one another in any order, but a call that would read a captured
  local before its initialization is rejected.
