---
title: Operators and control flow
summary: Sysl's operator set and precedence, comparison chains, blocks that yield values, if, while, match and return.
weight: 20
---

## Operators

Avin has Sysl's operator set and precedence. Operators are trait methods (`+` is `Add`, `<` is `Ord`,
`==` is `Eq`), so a generic needs the bound that supplies one, and a user type gains one through an
`impl`. A data type or a class may also define the arithmetic and ordering operators as
[ordinary methods](#operator-methods), with any operand and result types: `start + delay` with an
`Instant` and a `Duration`.

| Operator | Trait | Method |
| --- | --- | --- |
| `+` `-` `*` `/` `%` | `Add` `Sub` `Mul` `Div` `Rem` | `add` `subtract` `multiply` `divide` `remainder` |
| unary `-` | `Neg` | `negate` |
| `==` `!=` | `Eq` | `equals`, and its negation |
| `<` `<=` `>` `>=` | `Ord` | `less` `lessOrEqual` `greater` `greaterOrEqual` |

Each method takes and yields the implementing type: `add(other: Self) -> Self`. `Ord` requires `Eq`;
a type implements its `compare(other: Self) -> int`, negative, zero or positive, and inherits the
four comparisons from it, implementing any of them as well where it can answer more directly. The
built-in types implement the traits their operators have, so `int` is an `Add` and an `Ord`, and
`string` is an `Add` (joining) and an `Ord`. Both operands of an operator reached through its trait
are of one type. The wrapping, saturating and shift operators belong to the built-in types alone;
`&`, `^` and `|` are the integers' and bytes' bitwise operators, and on any other type the
[methods](#methods-named-by-symbols) of those names.

`Add` and `Mul` also name their identities, as [static members](/spec/declarations/#static-members) of
the implementing type: `static zero() -> Self`, which `add` leaves a value unchanged by, and
`static one() -> Self`, which `multiply` does. Every implementation gives them: the integers' are `0`
and `1` at each width but `i1`, which holds 0 and -1 and has no `1`: `i1.one()` is a compile error, and
`one()` reached at `i1` through a generic traps; `real`'s `0.0` and `1.0`, and `string`'s `zero` is the empty string. Generic
code reaches them through the bound:

```text
total[T: Add](items: Vector[T]) -> T
    var sum = T.zero()
    for x in items
        sum = sum + x
    sum

data Point(x: int, y: int)

impl Add for Point
    add(other: Point) -> Point = Point(x + other.x, y + other.y)
    static zero() -> Point = Point(0, 0)
```

| Family | Operators |
| --- | --- |
| Arithmetic | unary `+` `-`; `+` `-` `*` `/` `%` |
| Explicit wrapping | `+%` `-%` `*%` |
| Explicit saturation | `+\|` `-\|` `*\|` |
| Bitwise | `~` `&` `^` `\|` |
| Shifts | `<<` `>>` (shifts bind like multiplication) |
| Logical | `!`, short-circuit `&&` `\|\|` |
| Comparison | `==` `!=` `<` `<=` `>` `>=`, chained |
| Identity | `===` `!==` (one side a reference type; see [identity](/spec/equality-and-memory/#identity)) |
| Range | `..` `..<` |
| Cons | `::`, prepending to a `List` (right-associative) |
| Pattern test | `is`, `is not` |
| Propagation | postfix `?` |
| Assignment | `=` and compound forms (`+=`, `<<=`, `+%=`, …) |
| Yielding assignment | `:=` and compound forms, which return the stored value |
| Update | prefix and postfix `++` `--`; prefix yields the new value, postfix the old |
| Symbol methods | any other run of operator characters names a method, called infix at the level of its first character, or tightest when it has a `~` in it: `xs ++ ys`, `x +: xs`, `a <~ b` |

An operator is the longest run of the characters `+ - * / % < > = ! & | ^ ~ : ?`, so a prefix
operator written directly after another operator needs a space: `a = -1`, `x * -y`. A run stops
before `//`, which begins a comment. The range operators `..` and `..<` are spelled with dots, which
are not operator characters.

Bitwise operators bind more tightly than comparisons: `x & mask == 0` tests the masked value.
Within the bitwise family, `&` binds tightest, then `^`, then `|`. Shifts bind at the multiplicative
level, so `1 + 2 << 3` is `17`. A shift count at least as large as the width shifts everything out.
It is not masked.

`::` binds more loosely than every arithmetic and bitwise operator and more tightly than ranges and
comparisons (the full table, with the methods named by symbols, is under
[methods named by symbols](#methods-named-by-symbols)), and it groups to the right: `n + 1 :: rest` is `(n + 1) :: rest`, and `1 :: 2 :: []` is
`1 :: (2 :: [])`.

Sysl's pointer operators (prefix `*` and `&`) and `await` are not part of Avin.

### Infix method calls

Any method that takes one argument can be called as an infix operator, without the dot and the
parentheses: `a max b` is `a.max(b)`, and `xs zip ys` is `xs.zip(ys)`. The caller chooses the form;
the method's author does not mark it. `a m (b, c)` passes the tuple `(b, c)` as the one argument.

An infix call by a word groups to the left and binds more loosely than the arithmetic, bitwise,
shift, cons and range operators, and more tightly than comparisons, `is`, `&&` and `||` (a method
named by a symbol binds by [its first character](#methods-named-by-symbols)): `a max b + 1` is
`a max (b + 1)`, `a max b max c` is `(a max b) max c`, and `xs contains x && ok` is
`(xs contains x) && ok`. There is no postfix form: `xs size` is an error, written `xs.size()`.

An infix call stays on one line. A line that ends with the method's name continues it: the argument
is the indented block on the lines that follow.

```text
val widest = current max
    measure(header) + padding
```

A name that starts a line never continues the line above, inside brackets too: end the line with the
name instead. A method named by a symbol is an operator and may start a
[continuation line](#continuation-lines). An indented block after a call is the call's [trailing block](#trailing-blocks), except
that one holding only a name and an operand is reported as this mistake.

The call is resolved exactly as `a.m(b)` is: overloads are chosen by the argument's type, a trait or
`open` method dispatches on the receiver's class, an `impl` adds methods that can be called this way,
and a generic method's type arguments are inferred. The name must be a method of the receiver's type
that can take one argument, which a method with a [repeated](/spec/declarations/#repeated-parameters)
last parameter can: `w surround "!"` gives `surround(marks: string*)` one mark. A field holding a
function is not called infix, and neither is a
function `m(a, b)` declared outside the type: `a m b` never means `m(a, b)`. A method of two
parameters is not called infix even with a tuple after its name: `a put (1, 2)` against
`put(i: int, j: int)` is an error that says to write `a.put(1, 2)`.

A word that keeps its meaning after an operand, such as `then`, `do`, `else`, `if`, `is` or
`match`, is never read as a method's name, so `if a max b then c` and `n is int` read as before.

### Methods named by symbols

A method may be named by a run of operator characters, declared like any other method in a class,
trait, object or impl, and called infix or dotted:

```text
class Bag(val items: Vector[int])
    ++(other: Bag) -> Bag = Bag(items ++ other.items)
    +:(x: int) -> Bag = Bag(x +: items)
```

`a ++ b` is `a.++(b)`, and `a.++(b)` may be written as such. A function outside a type cannot be
named by a symbol. Some of the language's own operators are not method names: declaring a method
named `==` is an error that says to implement `Eq`. The reserved spellings are the wrapping and
saturating `+%` `-%` `*%` `+|` `-|` `*|`, `!` `&&` `||`, `==` `!=` `===` `!==`, `::`, `=` `:=`
`:` `?` `->` `=>`, and each yielding compound form such as `+:=` and `<<:=`. Every other run is
free, `++`, `--`, `>>>` and the plain compound spellings `+=` and `-=` among them.

`&`, `^`, `|`, `~` between two operands, and the shifts `<<` and `>>`, are free too, on every type
but the integers and bytes, whose bitwise and shift operators are the machine's: a method of one of
these names on `int` or `byte`, in an extension too, is an error. On any other type, `a | b` calls
`a`'s method `|` and `a >> b` its method `>>`, each at its operator's own level, so a parser
combinator (`p >> f`), a set of flags, a stream or a query builder can be written with them. Prefix
`~` stays the integers' and bytes' complement, and `~a` on another type is an error.

```text
data Flags(bits: int)

impl Flags
    |(other: Flags) -> Flags = Flags(bits | other.bits)
    &(other: Flags) -> Flags = Flags(bits & other.bits)

val rw = Flags(1) | Flags(2)
```

#### Operator methods

The arithmetic operators `+` `-` `*` `/` `%` and the orderings `<` `<=` `>` `>=` are free method
names on a data type, a class or a trait, with any operand and result types, and overloaded by the
operand's type like any method. `a + b` calls `a`'s method `+` where `a`'s type, or a type
parameter's bound, has one taking one argument, at the built-in `+`'s own precedence, so
`a + b * k` is `a + (b * k)` whichever of them are methods:

```text
data Instant(epochMicros: long)

impl Instant
    +(d: Duration) -> Instant = Instant(epochMicros + d.micros)
    -(d: Duration) -> Instant = Instant(epochMicros - d.micros)
    -(earlier: Instant) -> Duration = Duration(epochMicros - earlier.epochMicros)

class Vec(val x: real, val y: real)
    *(k: real) -> Vec = Vec(x * k, y * k)
    *(other: Vec) -> real = x * other.x + y * other.y
    -() -> Vec = Vec(-x, -y)

val deadline = start + timeout
val waited = now - start
val scaled = v * 2.0
val flipped = -v
```

A prefix `-a` calls a method `-` taking no argument. Where a type has no method of an operator's
name, the operator is its core trait's, `Add`'s `add` for `+`, as it always is in generic code: a
type with both a method `+` and an `impl Add` has its method called by `a + b` and `add` called
by a function bounded by `Add`. `a < b` alone calls the method `<`, whatever it yields, so a query
builder's `age < 30` can build a condition; a comparison by a method does not chain, and
`lo <= x < hi` through one is an error that says to join the comparisons with `&&`. `x += y` and
the other compound forms assign through the method: `deadline += grace` is
`deadline = deadline + grace`.

The built-in types keep their operators: a method of one of these names on `int`, `real`, `byte`,
`char`, `string`, `bool` or a tuple, in an extension or a trait implementation, is an error, so `+`
on two `int`s is always the machine's addition. `==` and `!=` stay `Eq`'s.

A symbol method's call binds at the level of the language's operator its name's first character
belongs to, so a name reads with the operator it resembles. One character overrides the first: a
name with `~` anywhere in it binds at the tightest symbol level, so `~`, `~>`, `<~`, `<~>`, `~!` and
`^~` all bind together, the way a grammar's sequencing operators are read. The levels, tightest
first:

| Level | Operators | Symbol methods whose name starts with |
| --- | --- | --- |
| Prefix | unary `-` `+` `!` `~` | |
| Other symbols | | `~` `?`, and any character no lower level takes: `~>`, `??`; and every name with a `~` in it: `<~`, `^~` |
| Multiplicative | `*` `/` `%` `<<` `>>` `*%` `*\|` | `*` `/` `%`, `<<` and `>>`: `*:`, `<<<` |
| Additive | `+` `-` `+%` `-%` `+\|` `-\|` | `+` `-`: `++`, `+:`, `--` |
| Bitwise and | `&` | `&`: `&&&` |
| Bitwise exclusive or | `^` | `^`: `^^` |
| Bitwise or | `\|` | `\|`: `\|>` |
| Cons | `::` | `:`: `:+`, `:::` |
| Range | `..` `..<` | |
| Named calls | `a max b` | |
| Comparison | `==` `!=` `<` `<=` `>` `>=` `===` `!==`, `is` | `<` `>` `=` `!`: `<=>`, `<>`, `!!` |
| Logical and | `&&` | |
| Logical or | `\|\|` | |
| Assignment | `=` `:=` and compound forms | a name ending in `=` that is not a comparison: `+=`, `++=`, `\|=` |

So `a ++ b + c` is `(a ++ b) + c`, `a ++ b * c` is `a ++ (b * c)`, `xs ++ ys :+ z` is
`(xs ++ ys) :+ z`, and `xs ++ ys == zs` compares the joined sequence. A grammar written with
combinators reads as it would on paper: `a ~ b ^^ f | c ~ d` is `((a ~ b) ^^ f) | (c ~ d)`,
`"[" ~> items <~ "]" ^^ f` is `(("[" ~> items) <~ "]") ^^ f`, and `a <~ b ~ c` is `(a <~ b) ~ c`.
A `~` name ending in `=` is still a compound assignment: `p ~= q` is `p = p ~ q`.

A symbol call groups to the left, except that a name ending in `:` groups to the right and takes its
receiver on the right: `x +: xs` is `xs.+:(x)`, and `a +: b +: xs` is `a +: (b +: xs)`. The operands
are still evaluated in the order written, `x` before `xs`. Operators of one level that group in
opposite directions, whether symbol methods or the language's own, do not mix without parentheses:
`a ++ b +: c`, `a + b +: c` and `1 :: xs :+ 2` are errors, and `(a ++ b) :+ c` and `1 :: (xs :+ 2)`
are not. Operators of different levels need none: `1 +: xs :+ 2` is `(1 +: xs) :+ 2`.

There are no prefix or postfix symbol methods, beyond the [method `-`](#operator-methods) taking no
argument that a prefix `-` calls. `++` and `--` keep their update meaning when no
operand follows them on the line: `i++` and `++i` update `i`, and `xs ++ ys` calls `++`. An operand
here is a name, a literal or an opening bracket on the same line, or an indented block on the next;
so `a ++ -b`, with a prefix operator after `++`, is `(a++) - b`, written `a ++ (-b)` to join.

`x op= y` calls `x`'s method named `op=` when the type of `x` has one: `buffer += 3` appends, and
the buffer need not be a `var`. Otherwise `op=` assigns. Where `op` is one of the language's
arithmetic, wrapping, saturating, shift or bitwise operators, it is the compound assignment it has
always been; where `op` is a symbol method, or `&`, `^`, `|`, `<<`, `>>` or an
[operator method](#operator-methods) on a type that has a method of that name, `x op= y` is
`x = x op y`, so `flags |= Flags(4)` combines flags into a `var`, and `xs ++= ys` joins onto a
`var` vector and `xs :+= 4` appends to it. The target is a `var` local, a `var` field, a module
value or an element `xs(i)`, and its object and indices are evaluated once. A right-grouping name
has no compound form, and the yielding forms (`+:=`) belong to the language's operators alone. A
multiple assignment uses only the language's compound forms.

### Continuation lines

A line that starts with a binary operator, indented deeper than the line above it, continues the
expression that line left open:

```text
val atom = number
    | symbol("(") ~> defer(expr) <~ symbol(")")
    | ident ^^ Var

val total = a
    + b * c
    - d
```

The line break means nothing to precedence: the operators group exactly as they would on one line,
so `total` is `(a + (b * c)) - d`, `atom` has three alternatives, and `a + b` with `* c` on the
next line is `a + (b * c)`. A comparison chain goes on across the lines too.

- **What starts one.** Every operator that stands between two operands: the arithmetic, wrapping,
  saturating, shift, bitwise, comparison, logical, range and cons operators, a method named by a
  symbol, `~`, and `++` or `--` with an operand after it. `-` and `+` there are binary, written
  `- d` or `-d`. A word never starts one (a named infix call ends its line instead, as
  [above](#infix-method-calls)), and neither do `!`, `.`, `?`, a bracket, `=` and the compound
  assignments.
- **Where.** Wherever an expression ends a line and has a complete operand: a binding's value, an
  assignment, a `return` value, a call's argument, a `match` arm's or a lambda's body, a branch of a
  one-line `if`, an expression statement.
- **How far.** Each following line that starts with such an operator and is still deeper than the
  line the expression began on continues it, at the depth of a continuation line above it or deeper
  (indentation that matches no line above is an error, as in any block); the depth means nothing
  beyond that. The expression ends at the first line back at the depth it began
  on, or shallower. A line inside the continuation that does not start with an operator is an error,
  except a call's trailing block or an infix call's argument, indented under its own line as usual.
- **Blank lines and comments** between continuation lines are skipped, as they are everywhere.
- **Not after a header.** The indented lines after the test of `if`, `elif`, `while` and
  `do … while`, a `for` source and a class's parents are the form's body. There, and wherever else
  an indented block follows a word or a symbol that is not a complete operand (`=`, `->`, `else`,
  `do`, `then`, a declaration's signature), a line that starts with `-x` is a block's line:

```text
negate(x: int) -> int
    -x

sign(x: int) -> int
    if x < 0
        -1
    else
        1
```

Inside brackets a line break is not significant, so a line in a call's arguments may start with any
operator at any depth, a method named by a symbol included. A block opened inside the brackets, a
lambda's body, starts its own lines again.

### Trailing blocks

An indented block on the lines after a call is the call's last argument. The call's line ends right
after its `)`, and the block is given to the callee's **last parameter**:

```text
retry(3)
    fetch(url)

withFile("data.txt") f ->
    process(f)

zipWith(xs, ys) (x, y) ->
    x * y
```

A [by-name](/spec/by-name/) parameter (`body: -> T`) takes the block as its delayed expression,
evaluated at each read of the parameter and not at the call, so `retry(3)` runs `fetch(url)` as many
times as `retry` reads `body`. A function parameter takes a lambda whose body is the block: its
parameters end the call's line before `->`, one name (`f ->`) or a parenthesized list
(`(x, y) ->`, `(x: int) ->`), as an ordinary lambda's would be written.

A callee with no other arguments is written without parentheses: a name, a member or a type
application, then the block, or one parameter name and `->`:

```text
scope s ->
    s.spawn
        part(1)

xs.foreach x ->
    println(x)
```

A lambda of several parameters needs the call's parentheses before it, `run() (x, y) ->`: the
parentheses right after a callee's name always hold its arguments.

The block's position does not depend on the written arguments. With parentheses it comes after them,
and it still goes to the last parameter, past any defaulted one in between: against
`labelled(label: string, width: int = 4, body: -> string)`, `labelled("box")` and a block gives
`width` its default. Positional and named arguments bind as they would without the block, and
overloads are chosen with it: a candidate whose last parameter cannot take a block does not fit.

A trailing block applies wherever an expression ends a line: a statement, a binding's value
(`val r = retry(3)` and a block), an assignment, a `return` value, a lambda's or a `match` arm's
body, the argument of an infix call, the operand of an operator. Inside brackets a line break is
not significant, so a call written inside another call's parentheses takes no block. The forms that
already put an indented block after their line keep it:

- The test of `if`, `elif`, `while` and `do … while`, a `for` source and its `if` guards, and a
  class's or object's parents: the block is the form's body, so `if ready(x)` and a block tests
  `ready(x)`.
- A local function's header: `helper()` or `helper(n: int)` and an indented block declare a local
  function. A header's parameters are typed, so `retry(3)` and `retry(n)` are calls. A call with
  no arguments and a block is written without parentheses.
- A line that ends with an infix call's method name, `current max`: the block is its argument.
- `x match` and the bare `match` function: the block holds the arms.
- A block whose one line is a name and a single operand, `add 2`, reads as the method and argument
  of an infix call that should have ended the line above, and is reported as that.
- A block whose first line starts with a binary operator is a
  [continuation line](#continuation-lines): `f(1)` and `    - x` is `f(1) - x`.

It is an error to write a trailing block for a call whose last parameter is neither by-name nor a
function (the message names the parameter and its type), whose last parameter is already given, or
whose callee takes no arguments. A lambda on the call's own line, `f(1) x -> x`, is not a trailing
form; it is written inside the parentheses.

## Comparison chains

`a < b <= c` is one expression. Operands are evaluated left to right, each middle operand exactly
once, and evaluation stops at the first failing link. Parenthesizing a comparison makes it an
ordinary `bool` operand: `(a < b) == flag`.

## Blocks

A block yields its last expression. `val` bindings are immutable and `var` bindings mutable; both are
scoped to their block. A block whose last item is a declaration yields `unit`. In statement position a
value is discarded, and the branches of an unused `if` or `match` need not agree on a type.

```text
answer() -> int
    val result =
        val base = 21
        base * 2
    result
```

A binding may take a value apart by a pattern that cannot fail: `val q, r = divmod(17, 5)`, or
`var (lo, hi) = 0, 10`, in which every name is a `var`. A pattern that could fail, such as a literal
or a variant of a type with several, belongs in a `match`.

## Multiple assignment

`a, b = b, a` assigns several places at once. The whole right side is made before any place changes,
so this swaps `a` and `b` with no temporary. The right side is a tuple, written out or computed
(`q, r = divmod(n, 10)`), with one part per place; a place `_` discards its part. The places may be
parenthesized: `(a, b) = (b, a)`.

A place is a `var` local or a `var` field, written `obj.field`, `self.field`, or by its bare name
inside its class: `self.lo, self.hi = hi, lo`, or a module-level `var`, by its bare name or another
module's through that module's name: `low, store.high = high, low`. The object a field belongs to is evaluated first, left
to right across the places, before anything else; then the right side is made, then every place is
stored.

The compound forms read every place first, then make the right side, then combine and store:
`a, b += b, a` adds each to the other using the values the statement started with.

A multiple assignment is a statement, not an expression: it has no `:=` form, and it fills a whole
line, so it is not the body of a one-line `if` or `while`.

```text
extendedGcd(a: int, b: int) -> (int, int, int)
    var r0, r1 = a, b
    var s0, s1 = 1, 0
    var t0, t1 = 0, 1
    while r1 != 0
        val q = r0 / r1
        r0, r1 = r1, r0 - q * r1
        s0, s1 = s1, s0 - q * s1
        t0, t1 = t1, t0 - q * t1
    r0, s0, t0
```

## if

```text
label(n: int) -> string
    if n < 0 then "negative"
    elif n == 0 then "zero"
    else "positive"
```

A one-line branch needs `then`; an indented branch may omit it. The condition must be a `bool`.
Without an `else`, an `if` yields `unit`.

## while

```text
val found = while n < 10 do
    n += 1
    if n == 5 then break n
else -1
```

The condition is tested before each iteration, and `continue` returns to that test. `break` exits the
nearest loop and may carry a value, in which case every `break` value and the loop's `else` must
agree. The `else` runs on normal completion. Without an `else`, normal completion yields `unit`.
A `while true` loop that no `break` leaves never finishes, so its type is `Nothing`.

## loop

```text
var k = 0
val found = loop
    k += 5
    if k > 12 then break k * 2
```

`loop` repeats its body until a `break` or `return` leaves it; `continue` starts the body again. A
one-line loop is written `loop do step()`. Only a `break` ends a `loop`, so there is no `else`: with
`break value`, every `break` value must agree and is the value of the loop, and a `break` without
one makes the loop `unit`. A `loop` that no `break` leaves never finishes, so its type is
`Nothing`, and a function may end in one. `loop` is a keyword and cannot be used as a name.

## do … while

```text
digits(n: int) -> string
    var rest = n
    var s = ""

    do
        s = s"${rest % 10}" + s
        rest /= 10
    while rest > 0

    s

var i = 0
do i += 1 while i < 4
```

The test comes at the end, so the body runs at least once. The body is an indented block with
`while test` on the line that closes it, or a single statement on one line. The value rules are
`while`'s: a `break` carries the loop's value, and an `else` after the test runs when the test fails,
so every `break` value and the `else` must agree.

`continue` runs the test, not the body again. The test cannot read names the body declared, since the
body's scope has ended. A `do` that starts a line always opens this form; the `do` of a one-line
`while` body follows its header, so the two never meet.

## match

`subject match` is followed by indented `pattern -> body` arms and an optional `else`. It is an
expression, checked for exhaustiveness. Patterns include:

- literals, including negative numbers;
- inclusive and exclusive literal ranges (`'a'..'z'`, `0..<10`);
- comparison patterns, which bind the value to a name or `_` beside one literal bound or between
  two: `n < 0`, `n >= 100`, `0 <= n < 10`, `'a' <= c <= 'z'`. Between two bounds the comparisons
  are `<` or `<=`;
- `n + k`, where `k` is a positive integer literal: it matches an integer of at least `k` and binds
  `n` to the value minus `k`, which never overflows;
- constructors with nested patterns, tuples and `()`;
- [extractor patterns](#extractor-patterns), `Name(p1, p2)` where `Name` is a value with
  `unapply` or `unapplySeq`;
- list patterns: `head :: tail` matches a non-empty `List`, grouping to the right
  (`first :: second :: rest`);
- [sequence patterns](/spec/collections/#sequence-patterns): `[]` matches an empty sequence and
  `[a, b]` a sequence of exactly two elements;
- type patterns (`d: Dog`, `_: Dog`), which also take a [union](/spec/unions/#match) apart, member
  by member; a union in a type pattern is parenthesized, `x: (int | string)`;
- alternatives (`1 | 2 | 3`), which bind no names and may continue across lines with a leading or
  trailing `|`;
- named patterns (`whole @ Some(x)`);
- `_`.

Ranges and comparison patterns match integers, bytes and chars; `n + k` matches integers. Every
pattern form may appear wherever a pattern does, nested inside a constructor (`Some(n > 0)`) or in a
function's [clauses](/spec/declarations/#clauses).

A guard (`n if n > 0 ->`) is evaluated at most once per arm. Literals, ranges, comparison patterns
and `n + k` count toward exhaustiveness by the values they leave out, so arms that together reach
every integer need no catch-all:

```text
bucket(n: int) -> string = n match
    n < 0 -> "negative"
    0 <= n < 10 -> "digit"
    n >= 10 -> "large"
```

Guards do not count, since a guard may fail.

A **type pattern**, `x: T`, matches a value whose type at run time is `T` and binds it to `x` as a
`T`; `_: T` tests without binding. For a class or a trait it matches an instance of any subclass or
conforming class, and for a generic type it compares the type arguments exactly (`Box[int]` is not a
`Box[string]`). The value matched may be an `Any`, or of a trait's or an open class's type, to be
taken down to a class. A downcast is written this way, never as a cast:

```text
legs(a: Animal) -> int = a match
    d: Dog -> d.legs
    b: Bird -> b.legs
    _ -> 0
```

A tuple's type at run time is the one it was [made with](/spec/types/#any-and-nothing), so a tuple
type pattern compares that type, not the classes of the objects inside it: `t: (Animal, int)` holding
a `Dog` does not match `_: (Dog, int)`, nor does `(a, 1)` with `a: Animal` as an `Any`, while a
tuple made as a `(Dog, int)` matches it wherever it is held. Each part is tested on its own by a
tuple pattern with a type pattern inside it, `(d: Dog, n) -> …`, on a value of a tuple type; an `Any`
is first matched by its tuple type, `p: (Animal, int)`.

A type pattern that can never match, such as `s: string` on an `int`, is an error. A type pattern
covers every value only when its type is one the matched value already has, so a `match` on an `Any`
needs a `_` arm. A literal does not match an `Any`; match its type first (`n: int if n == 5`). In a
function's clauses a type pattern takes an argument apart like any other (`size(s: string) = …`),
but one naming the parameter's own type is an error, since the signature already says it. In a
destructuring binding, whose `:` begins the binding's type, a type pattern is in parentheses. A
function type in a type pattern is written in parentheses, because an arm's `->` follows it:
`f: ((int) -> int) -> …`.

A `match` that does not cover every value names one it leaves out, written as a pattern:
`` this `match` does not cover every Shape value, such as `Rect(_, _)` ``. A value is named only
where no arm can match it whatever its extractors answer; where only extractors stand between the
arms and every value, the message says so instead:
`` this `match` does not cover every int value, since an extractor may answer that it does not
match; add an arm for the rest, such as `_` ``.

An arm no value can reach is an error: one after `_` or a binding, one after a pattern that covers
the whole type, and one whose values earlier arms already match, alone or together. The message
names the earlier arm that covers it, by its pattern and line. A guard never hides the arms after
it, since it may fail; an extractor and a type test are taken to match every value in the arm
judged and none in the arms above it, so an arm is called unreachable only when it is, whatever
the extractors answer.

```text
data Phase
    Open
    Closing

code(p: Phase) -> int = p match
    Open -> 1
    Live -> 2         // binds a new name, matching every Phase
    Closing -> 3      // error: this arm is never reached: the arm `Live` on line 7 already matches
                      // every value it would; `Live` binds a new value there, since no variant
                      // `Live` is in scope
```

A bare name that is not a variant in scope binds, so a variant's name misspelled or not imported
matches everything; the message says so when the covering arm's name starts with a capital letter.

### Extractor patterns

A pattern `Name(p1, p2)` whose `Name` is not a variant is an **extractor**: a value, such as an
object, a class's companion or a local, whose type has an `unapply` or `unapplySeq` method. The
matched value is passed to the method, and its answer decides the match:

- `unapply(x) -> bool` matches where it answers `true`, and is written with no patterns, `Even()`;
- `unapply(x) -> Option[U]` matches its `Some`: one pattern matches the value whole, and several
  match the parts of a tuple of as many, `Split(key, value)` for an `Option[(string, string)]`;
- `unapplySeq(x) -> Option[S]`, `S` a sequence, matches its `Some` as a
  [sequence pattern](/spec/collections/#sequence-patterns) does, so `Words(first, second)` needs
  exactly two elements and `Words(first, rest*)` one or more.

```text
object Twice
    unapply(n: int) -> Option[int] = if n % 2 == 0 then Some(n / 2) else None

object Even
    unapply(n: int) -> bool = n % 2 == 0

describe(n: int) -> string = n match
    Twice(Twice(q)) -> s"four times $q"
    Twice(h) if h > 100 -> "a large double"
    Even() -> "even"
    _ -> "odd"

val date = Regex("(\\d{4})-(\\d\\d)-(\\d\\d)")
"2024-06-30" match
    date(year, month, day) -> …
```

A variant of the name is always the variant, so a data type's companion declares an `unapply` to be
called by name, not to change what its variant's pattern means. Where the type has both methods,
`unapply` is the one called. The method's parameter accepts the matched value's type, as any call's
argument would, and a generic method infers its type arguments from it. The method is called once
each time the pattern is tested, the patterns inside it are matched against its answer, and its
names are bound like any pattern's; extractors nest, take guards, and appear in `is`, alternatives,
clauses and match functions. An extractor may answer that it does not match, so it never counts
toward exhaustiveness: a `match` relying on one needs a catch-all, and a binding's or a `for`'s
pattern, which must cover every value, cannot hold one.

### Match functions

A `match` with nothing before it is a function of one value, which its arms match. The arms are
exactly those of a `match`: patterns, `if` guards, `->` and block bodies. One arm may stand on the
`match` line; several go in an indented block, which may open inside parentheses or after an infix
method's name at the end of a line:

```text
val radii = shapes.collect(match Circle(r) -> r)
val sizes = shapes.map(match
    Circle(r) -> r * r * 3.14
    Rect(w, h) -> w * h)
val firstBig = shapes collectFirst match
    Circle(r) if r > 10.0 -> r
```

The type of the value it matches comes from the type expected where it is written, as a lambda's
parameter type does; a `match` function with no expected type is an error asking for one. Where a
function type `A -> B` is expected, the function is `x -> x match …` and its arms must cover every
`A`, as any `match`'s do: `pairs.map(match (k, v) -> s"$k=$v")`. A function of two or more
parameters is not a `match` function's type.

Where a [`PartialFunction[A, B]`](/spec/types/#partial-functions) is expected, the arms need not
cover every `A`: the function is defined at the values some arm matches. A partial function made
this way tests its patterns and guards once each time it is called, lifted or given to
`applyOrElse`; `isDefinedAt` tests them without running an arm's body.

## is

`x is P` tests a value against any pattern a `match` arm takes and yields a `bool`; `x is not P` is
its negation. `not` is a keyword only there, directly after `is`, where it always negates: `x is
not(y)` tests the pattern `(y)`. Everywhere else `not` is an ordinary name, so a function, method,
parameter or local may be called `not` and `not(p)` calls it. The names `P` binds are in scope only
where the test is known to have matched:

```text
area(shape: Option[Shape]) -> int
    if shape is not Some(Rect(w, h)) then return 0
    w * h

total(xs: List[int]) -> int
    var rest = xs
    var sum = 0
    while rest is head :: more
        sum += head
        rest = more
    sum
```

- `if x is P` binds them in the `then` branch, `while x is P` in the body, and `a && b` in `b`.
- `x is not P` binds them where the test failed: the `else` of an `if`, the `else` of a `while`, and
  the right operand of `||`.
- `!` swaps the two: `!(x is P)` binds like `x is not P`.
- A condition binds its operands' names where it holds, for `&&`, or where it fails, for `||`. Neither
  side of `a is P(v) || b is Q(v)` holds wherever the whole does, so the `then` branch sees no `v`.
- After an `if` whose one branch never finishes (a `return`, `break`, `continue` or trap), the rest of
  the block sees the names bound for the other branch, as `w` and `h` above.

Nowhere else: not in the other branch, not after an `if` both of whose branches finish, not in the
test of `do … while`, and not after an `is` used as a value (`val big = o is Some(n) && n > 100`).
Using a name outside its scope is an error that says so. A condition binds a name once; its names
may shadow the enclosing block's, as an arm's may, except those the block keeps after an `if`.

## return and end markers

`return expr` exits the enclosing function. Inside a lambda it exits the lambda. Where a `return` or
`break` value fills a line, a comma list is a tuple without parentheses (`return a, b`); inside
brackets whose commas separate items (call arguments, tuple and collection literals) the value stops
at the comma, so `f(a = return 1, b = 2)` has two arguments and a tuple is written `return (1, 2)`.
An indented block inside the brackets starts a line again. Optional end markers
(`end if`, `end while`, `end match`, `end val`, `end name`) are checked against the construct they
close.
