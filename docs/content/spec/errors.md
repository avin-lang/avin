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
A trap is not a value and cannot be caught.

**`fail(message)` is how a program says "this cannot happen".** It is in the prelude, stops the
program with `message` at the place it is called, and has type `Nothing`, so it stands where a value
of any type is expected:

```text
val n: int = if ok then 1 else fail("no")    // avin: trap: no at main.avin:2:36
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
`avin: trap: stack overflow`, after the output it had already written. The message names no place.
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
- **A type parameter converts through its bounds**: in
  `load[F: From[IoError] + From[ParseError]](path: string) -> Result[Doc, F]`, both errors convert,
  and each instance calls its type argument's `from`.

An `Option` `?` cannot exit a function returning `Result`, and the enclosing function's result type
must be able to carry the failure.

`?` returns from the function it is written in; inside a lambda, from the lambda. That function's
result type must be declared, or for a lambda known where it is written, as in
`val f: (string) -> Option[int] = s -> Some(parse(s)? + 1)`. A delayed argument and a class body
cannot be left by `?`, as they cannot by `return`.

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
| `isDefined()`, `isEmpty()` | `isOk()`, `isErr()` |
| `getOrElse(default: -> T) -> T` | `getOrElse(default: -> T) -> T` |
| `orElse(other: -> Option[T]) -> Option[T]` | `ok() -> Option[T]`, `err() -> Option[E]` |
| `map[U](f: (T) -> U) -> Option[U]` | `map[U](f: (T) -> U) -> Result[U, E]` |
| `flatMap[U](f: (T) -> Option[U]) -> Option[U]` | `mapErr[F](f: (E) -> F) -> Result[T, F]` |
| `filter(p: (T) -> bool) -> Option[T]` | `flatMap[U](f: (T) -> Result[U, E]) -> Result[U, E]` |
| `fold[U](ifNone: -> U, f: (T) -> U) -> U` | `fold[U](onErr: (E) -> U, onOk: (T) -> U) -> U` |
| `foreach(f: (T) -> unit)` | `foreach(f: (T) -> unit)` |
| `exists(p: (T) -> bool) -> bool` | `exists(p: (T) -> bool) -> bool` |
| `forall(p: (T) -> bool) -> bool` | `forall(p: (T) -> bool) -> bool` |
| `contains(x: T) -> bool` | `orElse[F](f: (E) -> Result[T, F]) -> Result[T, F]` |
| `toList() -> List[T]`, `toVector() -> Vector[T]` | `toList() -> List[T]` |
| `zip[U](other: Option[U]) -> Option[(T, U)]` | |
| `get() -> T`, trapping on `None` | |
| `filterNot(p: (T) -> bool) -> Option[T]` | |

An `Option` also has `toRight[L](left: -> L) -> Either[L, T]` and `toLeft[R](right: -> R) ->
Either[T, R]`, which put its value on one side of an [`Either`](#either) and the argument, evaluated
only for `None`, on the other. It also has `iterator()` (one element or none) and `knownSize()` (1 or 0), an
`Option[(A, B)]` has `unzip()` giving the two options, an `Option[Option[T]]` also has `flatten()`, and an `Option` has `nonEmpty()` beside `isDefined()` and
`isEmpty()`. There is no `orNull`, since Avin has no null.

`exists` is false and `forall` true for `None` and for `Err`, which hold no value. `toList` and
`toVector` give one element or none.

A default or an alternative is passed by name, so it is evaluated only when it is used. The methods
an `impl` adds are not [variance](/spec/generics/#variance) positions, so `T` may appear in a
parameter: on an `Option[Dog]` seen as an `Option[Animal]`, `getOrElse` takes an `Animal`.

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
| `isLeft()`, `isRight()` | false, true | true, false |
| `fold[C](fa: (L) -> C, fb: (R) -> C) -> C` | `fb(b)` | `fa(a)` |
| `swap() -> Either[R, L]` | `Left(b)` | `Right(a)` |
| `map[R2](f: (R) -> R2) -> Either[L, R2]` | `Right(f(b))` | itself |
| `flatMap[R2](f: (R) -> Either[L, R2]) -> Either[L, R2]` | `f(b)` | itself |
| `getOrElse(default: -> R) -> R` | `b` | `default` |
| `orElse(other: -> Either[L, R]) -> Either[L, R]` | itself | `other` |
| `contains(x: R) -> bool` | `b == x` | false |
| `exists(p: (R) -> bool) -> bool` | `p(b)` | false |
| `forall(p: (R) -> bool) -> bool` | `p(b)` | true |
| `filterOrElse(p: (R) -> bool, zero: -> L) -> Either[L, R]` | itself if `p(b)`, else `Left(zero)` | itself |
| `toOption() -> Option[R]` | `Some(b)` | `None` |
| `toList() -> List[R]`, `toVector() -> Vector[R]` | one element | empty |
| `foreach(f: (R) -> unit)` | calls `f(b)` | nothing |
| `iterator() -> Iterator[R]` | one element | empty |

Some methods are for an `Either` of a particular shape, added by an `impl` on it as `Option`'s
`flatten` is:

- `merge()` on an `Either[A, A]` gives the value from whichever side holds it.
- `joinLeft()` on an `Either[Either[C, R], R]` gives the inner either of a `Left`, and keeps a `Right`.
- `joinRight()` on an `Either[L, Either[L, C]]` gives the inner either of a `Right`, and keeps a `Left`.

`Either.cond[L, R](test: bool, right: -> R, left: -> L) -> Either[L, R]` is `Right(right)` when
`test` holds and `Left(left)` otherwise, evaluating only the side it uses. Defaults and alternatives
are passed by name, and `L` and `R` appear in parameters without lower bounds, as for `Option`.

A `match` covers an `Either` with a `Left` and a `Right` arm that each cover their value. Eithers are
equal, hash and show by side and value: `Right(4)`, `Left(no)`, and under `debug` `Left("no")`.
There is no left projection: `swap()` turns the left side into the one the methods work on.

## Not in Avin

`throw`, `try`/`catch`/`finally`, `Try`, checked or unchecked exceptions, and `null`. Releasing a
resource on every path uses a `defer`-style construct or a library helper such as `use`. Which of the
two Avin adopts is still open.
