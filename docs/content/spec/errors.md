---
title: Errors
summary: Recoverable failure is a value; a bug is a trap. No exceptions, no null.
weight: 17
---

Avin uses Sysl's model. There are no exceptions.

**A failure that correct calling code can meet is a value.** A function that can fail returns
`Result[T, E]`, or `Option[T]` when there is no reason to report. The failure is part of the
signature, and the caller must deal with it.

**A bug is a trap.** An index out of bounds, division by zero, an invalid conversion, `next()` on an
exhausted iterator or a failed assertion stops the program with a message and the source location.
A trap is not a value and cannot be caught. What the program needs put back however it ends, a
terminal's mode, goes in an [exit hook](/spec/system-library/#exit-hooks), which runs after the
trap's report.

**`fail(message)` is how a program says "this cannot happen".** It is in the prelude, stops the
program with `message` at the place it is called, and has type `Nothing`, so it stands where a value
of any type is expected:

```text
val n: int = if ok then 1 else fail("no")    // avin: trap: no at main.avin:2:36
```

Beside it the prelude has two more that stop the program, each at its own call:

- **`require(condition)`** and **`require(condition, message)`** check what a function was given:
  where `condition` is false they trap with `requirement failed`, or `requirement failed: ` and the
  message. The message is passed [by name](/spec/by-name/), so it is built only when the requirement
  fails. `assert` (see [tests](/spec/declarations/#tests)) states what the program itself
  guarantees; `require` states what its caller owes.
- **`todo()`** and **`todo(message)`** stand for code not yet written. They have type `Nothing`, as
  `fail` has, so a branch of any type may be one; reached, they trap with `not implemented`, or
  `not implemented: ` and the message.

```text
half(n: int) -> int
    require(n % 2 == 0, s"$n is even")   // avin: trap: requirement failed: 3 is even at main.avin:2:5
    n / 2

area(s: Shape) -> real = s match
    Circle(r) -> r * r * 3.14159
    Polygon(_) -> todo("polygons")       // avin: trap: not implemented: polygons at main.avin:7:19
```

**The location is in the program's own code.** A trap reached inside the core library, whether the
library asked for it, as `m(k)` on a missing key does, or a check failed there, such as an overflow,
reports the place where the program last called into the library, with the library's reason:

```text
main()
    val b = Buffer[int]()
    b.remove(0)          // avin: trap: index out of range at main.avin:3:5
```

That holds however deep in the library the trap is, and whether the program called the library
directly, through a trait or through a function value. When the library calls back into the
program, as `Array.tabulate` calls the function it is given, a trap the program's code reaches in
the library again reports that inner call, the nearest one the program wrote.

**Running out of stack is a trap.** Recursion that never reaches a base case stops the program with
`avin: trap: stack overflow`, after the output it had already written. The message names no place, and no exit hook runs: the report comes from a handler on a stack of
its own, where no Avin code can run.
It holds on the main thread, whose stack is the operating system's to size, and on every thread
`spawn` starts, whose stack is 8 MiB unless `spawn` is asked for another size.

## The ? operator

`expr?` unwraps `Ok(v)` or `Some(v)` to `v`. On `Err(e)` or `None` it returns from the enclosing
function immediately. On a `Result`, the error is converted through `From` when the caller's error
type differs:

```text
data ConfigError
    Unreadable(cause: IoError)
    Malformed(cause: ParseError)
    Invalid(key: string)

impl From[IoError] for ConfigError
    static from(value: IoError) -> ConfigError = Unreadable(value)

impl From[ParseError] for ConfigError
    static from(value: ParseError) -> ConfigError = Malformed(value)

loadConfig(path: string) -> Result[Config, ConfigError]
    val text = readText(path)?
    val doc = parseDocument(text)?
    configOf(doc)
```

`From[E]` is a core trait with one static member, `static from(value: E) -> Self`, so an error type
implements it once for each error it wraps. On `Err(e)`, with `e` of type `E` and the enclosing
function yielding errors of `F`:

- **Where `E` is an `F`**, `e` is passed on unchanged.
- **Otherwise `F.from(e)`** makes the error passed on, through `F`'s conformance to `From[E]`. Where
  `F` has none, a conformance `From[X]` with `E` an `X` serves, if exactly one is more specific
  than every other: an error of a subclass converts through its superclass's.
- **No conformance is an error** naming the one missing, `From[E]`; several equally specific are an
  error naming them.
- **A union of errors needs no wrapper type.** "`E` is an `F`" includes `E` being a member of a
  union `F`, or a union whose members are all members of `F`; where `F` is a union, the one member
  implementing `From[E]` converts it; where `E` is a union and `F` is not, each member converts on
  its own. So `load(path) -> Result[Config, IoError | ParseError]` may `?` a `Result[_, IoError]` and a
  `Result[_, ParseError]`. The full rule is in [union types](/spec/unions/#errors-and).
- **A type parameter converts through its bounds**: in
  `load[F: From[IoError] + From[ParseError]](path: string) -> Result[Doc, F]`, both errors convert,
  and each instance calls its type argument's `from`.

An `Option` `?` cannot exit a function returning `Result`, and the enclosing function's result type
must be able to carry the failure. `main` may answer with `Result[unit, E]` or `Result[int, E]`,
so a whole program can use `?`: an `Err` reaching the end of `main` is written to standard error as
`error: ` and its `Display`, and the program exits with 1 (see [`main`](/spec/declarations/#main)).

`?` returns from the function it is written in; inside a lambda, from the lambda. That function's
result type must be declared, or for a lambda known where it is written, as in
`val f: (string) -> Option[int] = s -> Some(parse(s)? + 1)`. A delayed argument and a class body
cannot be left by `?`, as they cannot by `return`.

## The Error trait

An error meant for a person to read is an `Error`, a core trait in the prelude:

```text
trait Error: Display
    message -> string
    cause -> Option[Error] = None
    override display(out: Writer, spec: FormatSpec)   // message, then ": " and the cause
```

- **`message` is the error's own sentence**, without its cause's; `cause` is the failure that led to
  it, `None` by default. Both are [parameterless](/spec/declarations/#parameterless-methods): they
  read what the error holds, so `e.message` and `e.cause` take no parentheses. A data type's field
  named `message` implements the first.
- **An error shows as its whole chain.** The display is the message, then `: ` and the cause as
  it shows, so each level adds its sentence in front: `loading config: cannot read app.conf: no such
  file or directory`. A message that is empty is left out with its `: `. A data type implementing
  `Error` takes this display in place of the one it would have by its fields; one with a `Display`
  of its own keeps it, and its cause, if any, is then its own to show.
- **Every error type of the standard library is an `Error`**: `IoError`, `CommandError`,
  `HttpError`, `UrlError`, `TlsError`, `TimeParseError`, `ZoneError`, `PatternError`,
  `MatchError`, `AskError` and `Utf8Error`, and the json package's `JsonError`.
  An error that only wraps another one, as `CommandError.Io(e)` does, has `e`'s message and no
  cause of its own, so it shows once.
- **A type of the program's own becomes one** by giving it a message:

```text
data ParseError(line: int, text: string)

impl Error for ParseError
    message -> string = s"line $line: cannot read \"$text\""
```

**`context` puts an error in context.** On a `Result[T, E]` whose `E` is an `Error`,
`context(text)` keeps an `Ok` and wraps an `Err(e)` as `Err(ContextError(text, e))`, an error whose
message is `text` and whose cause is `e`. The text is passed [by name](/spec/by-name/), so it is
built only for an `Err`. Contexts nest, the outermost first when shown:

```text
loadConfig(path: string) -> Result[Config, ContextError]
    val text = fs.readText(path).context(s"loading $path")?
    parseConfig(text).context("reading the settings")
// Err(loading app.conf: cannot read app.conf: no such file or directory)
```

`ContextError(message: string, inner: Error)` is a data type of the prelude; a program reaches the
error it wraps by `inner`, or through `cause`, and takes it apart by a type pattern, `io: IoError`
in a `match` arm or `e.inner is _: IoError`, since its static type is the trait.
`context` takes any error that is an `Error`: a type parameter bounded by `Error`, and a
[union](/spec/unions/) of error types each of which is one, so `load(path).context("loading")` works
on a `Result[Config, IoError | ParseError]`. And `Error` is a type like any trait: a function
answering `Result[T, Error]` can `?` every error type, each passed on unchanged, which is what a
`main` that answers a `Result` wants.

## Option and Result

Both are `data` types of the core library, written in Avin:

```text
data Option[+T]
    Some(value: T)
    None

data Result[+T, +E]
    Ok(value: T)
    Err(error: E)
```

Their methods:

| `Option[T]` | `Result[T, E]` |
|---|---|
| `isDefined`, `isEmpty` | `isOk`, `isErr` |
| `getOrElse[B >: T](default: -> B) -> B` | `getOrElse[U >: T](default: -> U) -> U` |
| `orElse[B >: T](other: -> Option[B]) -> Option[B]` | `ok -> Option[T]`, `err -> Option[E]` |
| `map[U](f: (T) -> U) -> Option[U]` | `map[U](f: (T) -> U) -> Result[U, E]` |
| `flatMap[U](f: (T) -> Option[U]) -> Option[U]` | `mapErr[F](f: (E) -> F) -> Result[T, F]` |
| `filter(p: (T) -> bool) -> Option[T]` | `flatMap[U, F >: E](f: (T) -> Result[U, F]) -> Result[U, F]` |
| `fold[U](ifNone: -> U, f: (T) -> U) -> U` | `fold[U](onErr: (E) -> U, onOk: (T) -> U) -> U` |
| `foreach(f: (T) -> unit)` | `foreach(f: (T) -> unit)` |
| `exists(p: (T) -> bool) -> bool` | `exists(p: (T) -> bool) -> bool` |
| `forall(p: (T) -> bool) -> bool` | `forall(p: (T) -> bool) -> bool` |
| `contains[T1 >: T](x: T1) -> bool` | `orElse[U >: T, F](f: (E) -> Result[U, F]) -> Result[U, F]` |
| `toList -> List[T]`, `toVector -> Vector[T]` | `toList -> List[T]` |
| `zip[T1 >: T, U](other: Option[U]) -> Option[(T1, U)]` | |
| `get -> T`, trapping on `None` | |
| `filterNot(p: (T) -> bool) -> Option[T]` | |

An `Option` also has `toRight[L](left: -> L) -> Either[L, T]` and `toLeft[R](right: -> R) ->
Either[T, R]`, which put its value on one side of an [`Either`](#either) and the argument, evaluated
only for `None`, on the other. It also has `okOr[E](error: E) -> Result[T, E]` and
`okOrElse[E](f: () -> E) -> Result[T, E]`, giving `Ok` of the value or `Err` of the error, which
`okOrElse` calls only for `None` (`Result`'s `ok` and `err` go the other way). It also has `iterator()` (one element or none) and `knownSize` (1 or 0), and a second `flatMap` taking a function that answers a collection, which answers a `Vector` (empty for `None`); every collection's `flatMap` takes a function answering an option, so `xs.flatMap(_.toIntOption())` keeps the numbers and a `for` comprehension draws from options and collections alike ([collections](/spec/collections/#operations-without-higher-kinded-types)). An
`Option[(A, B)]` has `unzip` giving the two options, an `Option[Option[T]]` also has `flatten`, and an `Option` has `nonEmpty` beside `isDefined` and
`isEmpty`. There is no `orNull`, since Avin has no null.

The companion makes an option from a condition: `Option.when(condition, value)` is `Some(value)`
where `condition` holds and `None` where it does not, and `Option.unless(condition, value)` the other
way round. The value is passed by name, so it is evaluated only for `Some`, and once:
`Option.when(n > 0, expensive(n))` calls `expensive` only for a positive `n`.

An `Option[T]` is an `Ord` when `T` is: `None` orders before every `Some`, and two `Some`s order as
their values. So `derives Ord` works on a data type with an `Option` field, a `Vector[Option[int]]`
sorts and has a `max` and `min`, and `None < Some(0)`. The order nests (`None`, `Some(None)`,
`Some(Some(1))`), and an option of a type without `Ord` has none.

`exists` is false and `forall` true for `None` and for `Err`, which hold no value. `toList` and
`toVector` give one element or none.

A default or an alternative is passed by name, so it is evaluated only when it is used. A method
that takes in a value which may be wider than the one held takes it as a type with `T` (or `E`) as
its [lower bound](/spec/generics/#lower-bounds), and the result widens to it: on an `Option[Cat]`,
`getOrElse(dog)` is an `Animal`, `orElse(Some(dog))` an `Option[Animal]`, and `contains(animal)`
compares the cat with an `Animal`. On an `Option[int]` the bound admits only `int`.

`None` with nothing to settle its type is an `Option[Nothing]`, and `Ok(1)` a `Result[int,
Nothing]` (see [inference](/spec/generics/#inference)). No value has type `Nothing`, so either is an
instance of any `Option` or `Result` with the other arguments: a module's `val none = None` is an
`Option[int]` where one is wanted.

## Either

`Either` holds a value of one of two types. It is a `data` type of the core library, written in Avin,
covariant in both sides:

```text
data Either[+L, +R]
    Left(value: L)
    Right(value: R)
```

`Result` is the type for something that can fail, and it is what `?` takes apart; `Either` is the
plain choice of two, with no `?` and no `From`. Its methods are right-biased: those that look at a
value look at a `Right`'s and pass a `Left` through unchanged, so a `Left` is where a reason goes.

| Method | On a `Right(b)` | On a `Left(a)` |
|---|---|---|
| `isLeft`, `isRight` | false, true | true, false |
| `fold[C](fa: (L) -> C, fb: (R) -> C) -> C` | `fb(b)` | `fa(a)` |
| `swap -> Either[R, L]` | `Left(b)` | `Right(a)` |
| `map[R2](f: (R) -> R2) -> Either[L, R2]` | `Right(f(b))` | itself |
| `flatMap[L1 >: L, R2](f: (R) -> Either[L1, R2]) -> Either[L1, R2]` | `f(b)` | itself |
| `getOrElse[R1 >: R](default: -> R1) -> R1` | `b` | `default` |
| `orElse[L1 >: L, R1 >: R](other: -> Either[L1, R1]) -> Either[L1, R1]` | itself | `other` |
| `contains[R1 >: R](x: R1) -> bool` | `b == x` | false |
| `exists(p: (R) -> bool) -> bool` | `p(b)` | false |
| `forall(p: (R) -> bool) -> bool` | `p(b)` | true |
| `filterOrElse[L1 >: L](p: (R) -> bool, zero: -> L1) -> Either[L1, R]` | itself if `p(b)`, else `Left(zero)` | itself |
| `toOption -> Option[R]` | `Some(b)` | `None` |
| `toList -> List[R]`, `toVector -> Vector[R]` | one element | empty |
| `foreach(f: (R) -> unit)` | calls `f(b)` | nothing |
| `iterator() -> Iterator[R]` | one element | empty |

Some methods are for an `Either` of a particular shape, added by an `impl` on it as `Option`'s
`flatten` is:

- `merge` on an `Either[A, A]` gives the value from whichever side holds it.
- `joinLeft` on an `Either[Either[C, R], R]` gives the inner either of a `Left`, and keeps a `Right`.
- `joinRight` on an `Either[L, Either[L, C]]` gives the inner either of a `Right`, and keeps a `Left`.

`Either.cond[L, R](test: bool, right: -> R, left: -> L) -> Either[L, R]` is `Right(right)` when
`test` holds and `Left(left)` otherwise, evaluating only the side it uses. Defaults and alternatives
are passed by name, and a value taken in may be wider than its side, through a lower bound, as for
`Option`: on an `Either[string, Cat]`, `getOrElse(dog)` is an `Animal`. `Left(1)` with nothing to
settle its right side is an `Either[int, Nothing]`, an instance of any `Either[int, R]`.

A `match` covers an `Either` with a `Left` and a `Right` arm that each cover their value. Eithers are
equal, hash and show by side and value: `Right(4)`, `Left(no)`, and under `debug` `Left("no")`.
There is no left projection: `swap` turns the left side into the one the methods work on.

An `Either[L, R]` is an `Ord` when `L` and `R` are: every `Left` orders before every `Right`, and two
on the same side order as their values. A `Result[T, E]` is ordered the same way with `Err` in the
place of `Left` and `Ok` in the place of `Right`: every `Err` before every `Ok`, so a failure sorts
before a success exactly as a `Left` sorts before a `Right`. Both sort, have `max` and `min`, key a
`TreeMap` or `TreeSet`, and may be a field of a type that `derives Ord`; they nest with `Option` and
tuples, and one with a part of a type that has no `Ord` has none.

## Not in Avin

`throw`, `try`/`catch`/`finally`, `Try`, checked or unchecked exceptions, and `null`. A resource is
given back on every path, the early `return`s and `?`s included, by a
[`defer`](/spec/operators-and-control-flow/#defer) written beside the code that took it.
