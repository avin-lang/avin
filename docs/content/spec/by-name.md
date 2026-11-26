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
  println("tick")
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

A delayed argument written at a call, whether by name, through a method or of a function value,
lives in the caller's frame for as long as the call runs, and making it allocates nothing; one
that captures nothing is a constant. Passing a by-name parameter on passes the same closure. The
one way to keep a delayed expression past the call is a closure that captures the parameter: a
lambda, a local function used as a value, or another delayed argument. That closure copies the
parameter's delayed expression to the heap where it is made, so it can still be evaluated after
the call has returned. A call that becomes a jump back to the start of its own function makes its
delayed arguments on the heap, since the frame they would live in is the one the jump reuses.

```text
log(on: bool, message: -> string) = if on then println(message)

later(value: -> int) -> () -> int = () -> value + 1

main()
  for i in 0 ..< 1000
    log(false, s"step $i")   // allocates nothing
  val f = later(41)          // the parameter is copied to the heap when the lambda is made
  println(f())
```
