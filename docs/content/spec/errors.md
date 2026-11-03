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

## Not in Avin

`throw`, `try`/`catch`/`finally`, `Try`, checked or unchecked exceptions, and `null`. Releasing a
resource on every path uses a `defer`-style construct or a library helper such as `use`. Which of the
two Avin adopts is still open.
