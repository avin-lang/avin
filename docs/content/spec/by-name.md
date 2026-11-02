---
title: Call-by-name
summary: Parameters written value -> T are evaluated on each use, never at the call and never cached.
weight: 20
---

A parameter declared `value: -> T` takes an expression that is **not** evaluated at the call. Each
read of the parameter evaluates the expression again, and a parameter that is never read never
evaluates it. There is no memoization; a body that needs the value twice binds it to a `val` once.

```text
twice(value: -> int) = value + value
unless(condition: bool, action: -> unit)
    if !condition then action
```

The arrow is Sysl's `->`, as in every Avin function type.

## Modes are part of the type

`(-> int) -> int` takes a by-name input; `(int) -> int` takes a strict one; `(() -> int) -> int`
takes a function value, which the body calls explicitly. These are three different types, and a value
of one does not convert implicitly to another.

Passing a by-name parameter on to another by-name parameter forwards the delayed expression without
evaluating it. `f: -> (int) -> int` delays the production of a function, and `f(1)` first evaluates
the delayed expression, then calls the result.

## Restrictions

`data` fields cannot be by-name, and neither can repeated (`T*`) parameters. Inside a delayed
expression, a `return`, `break` or `continue` that would leave the expression is a compile error.

## Representation

A delayed argument is a closure with no parameters. When it cannot escape, which is always the case
for a parameter that is only read or forwarded, its environment lives on the caller's stack.
