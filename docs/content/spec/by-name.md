---
title: Call-by-name
summary: Parameters written value -> T are evaluated on each use, never at the call and never cached.
weight: 21
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

## Defaults

A by-name parameter may have a [default](/spec/declarations/#default-and-named-arguments), which is
a delayed expression like any argument to it: a call that omits the argument passes the default
unevaluated, each read of the parameter evaluates it again, and a parameter that is never read never
evaluates it. A default that reads an earlier by-name parameter evaluates that parameter's argument
at each of its own evaluations.

```text
orElse(value: int, fallback: -> int = expensive()) = if value >= 0 then value else fallback
```

A named argument to a by-name parameter is delayed too, though the arguments written around it are
evaluated at the call.

## Trailing blocks

A by-name last parameter takes an indented block written after the call's line as its
[trailing block](/spec/operators-and-control-flow/#trailing-blocks), which makes a function read like
a control form of the language:

```text
times(n: int, action: -> unit)
    var i = 0
    while i < n
        action
        i += 1

times(3)
    print("tick")
```

The block is the delayed expression, with every rule above: it runs at each read of `action`, never
at the call, it yields its last expression, and a `return`, `break` or `continue` leaving it is an
error.

## Restrictions

`data` fields cannot be by-name, and neither can repeated (`T*`) parameters, nor the parameters
of a function defined by clauses, whose patterns match the arguments' values. Inside a delayed
expression, a `return`, `break` or `continue` that would leave the expression is a compile error.

## Representation

A delayed argument is a closure with no parameters. When it cannot escape, which is always the case
for a parameter that is only read or forwarded, its environment lives on the caller's stack.
