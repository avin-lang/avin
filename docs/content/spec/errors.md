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

## The ? operator

`expr?` unwraps `Ok(v)` or `Some(v)` to `v`. On `Err(e)` or `None` it returns from the enclosing
function immediately. On a `Result`, the error is converted through `From` when the caller's error
type differs:

```text
data ConfigError
    Unreadable(cause: IoError)
    Invalid(key: string)

impl From[IoError] for ConfigError
    from(e: IoError) -> ConfigError = Unreadable(e)

loadConfig(path: string) -> Result[Config, ConfigError]
    val text = readText(path)?
    parseConfig(text)
```

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
| `isSome()`, `isNone()` | `isOk()`, `isErr()` |
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

`exists` is false and `forall` true for `None` and for `Err`, which hold no value. `toList` and
`toVector` give one element or none.

A default or an alternative is passed by name, so it is evaluated only when it is used. The methods
an `impl` adds are not [variance](/spec/generics/#variance) positions, so `T` may appear in a
parameter: on an `Option[Dog]` seen as an `Option[Animal]`, `getOrElse` takes an `Animal`.

## Not in Avin

`throw`, `try`/`catch`/`finally`, `Try`, checked or unchecked exceptions, and `null`. Releasing a
resource on every path uses a `defer`-style construct or a library helper such as `use`. Which of the
two Avin adopts is still open.
