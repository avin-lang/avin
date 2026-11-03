---
title: Types
summary: Built-in types, numbers, strings, unit, tuples, functions, and the top and bottom types.
weight: 12
---

## Built-in types

Built-in type names are lowercase and belong to the language itself: they have no module path and
cannot be shadowed by an import. Their methods are defined in the core standard library with
ordinary `impl` blocks.

| Type | Meaning |
| --- | --- |
| `int` | Alias of `i32` |
| `long` | Alias of `i64`; `int` and `long` are the only integer aliases |
| `iN` | Signed integer of exactly N bits, 1 ≤ N ≤ LLVM's limit (`i8`, `i64`, `i128`, `i257`, …) |
| `byte` | An octet, 0–255: the unit of binary data |
| `real` | IEEE-754 binary64. There is no single-precision type |
| `char` | A Unicode scalar value (U+0000–U+10FFFF, excluding surrogates) |
| `bool` | `true` or `false` |
| `string` | Immutable UTF-8 text |
| `unit` | One value, written `()` |

All other types — user types, library types, collections — are PascalCase by convention.

## Integers

Arithmetic keeps the operand width: `i8 + i8` is `i8`. Mixed widths need an explicit conversion
(`long(x)`); there is no implicit widening. Diagnostics name `i32` and `i64` by their aliases. A literal takes its width from context (an annotation,
a parameter, the other operand) and defaults to `int`; a literal that does not fit is a compile error.
Where a `real` is expected, an integer literal is that real (`x * 2`), provided it is exact: its
magnitude is at most 2^53.

Conversions: widening sign-extends; narrowing keeps the low bits (`i8(300)` is `44`); real-to-integer
truncates toward zero, saturates at the bounds and maps NaN to zero; `char(n)` traps on a value that
is not a Unicode scalar. Conversions are written as a call of the target type, and these exist: between
any two integer types; integers and `real` in both directions; an integer or `byte` to `byte` and
`char`; `byte` and `char` to any integer type; `byte` to `real`. A conversion to the same type is
allowed and does nothing.

**Arithmetic that overflows traps.** `+`, `-`, `*`, unary `-`, `++` and `--` trap when the
mathematical result does not fit the width, and so does `/` for the one quotient that cannot fit
(`MIN / -1`). `%` never overflows: `MIN % -1` is `0`. Division and remainder by zero trap. Code that
wants other behaviour says so with an operator: `+%`, `-%` and `*%` wrap, and `+|`, `-|` and `*|`
saturate at the bounds. Shifts never trap; a count at least as large as the width, or a negative
count, shifts every bit out (`>>` keeps the sign, so `-1 >> 40` is `-1`).

Integers are signed; there are no unsigned integer types. Binary data uses `byte`.

## byte

`byte` is an octet with a value from 0 to 255. It is the element of binary data — file contents,
network buffers, encoded text — rather than a small number, so it has equality, ordering, the
bitwise operators `&`, `|`, `^` and `~`, and shifts, but no arithmetic. `int(b)` gives its value
(0–255), and `byte(n)` makes one from an integer, trapping when `n` is outside 0–255. An integer
literal in a `byte` context must be in 0–255: `val mask: byte = 0xf0`.

## real and char

`real` arithmetic follows IEEE 754, including NaN and signed zero; comparisons with NaN are false
except `!=`. There is no implicit fused multiply-add. `char` supports ordering and equality but no
arithmetic; `int(c)` and `char(n)` convert.

## string

`string` is a built-in value type with no identity: two equal strings are interchangeable, and
`===` is not defined on them. It is immutable UTF-8.

- `s.byteLength()` is O(1). Iteration is by `char` (`s.chars()`) or by byte (`s.bytes()`).
- There is no `s(i)` returning a char by index, because that would be O(n) behind O(1) syntax.
  Slicing takes byte offsets and traps on an offset that is not a character boundary.
- `+` concatenates two strings and nothing else. Other values reach text through interpolation
  (`s"${x}"`) or `x.toString()`, both of which use `Display`.
- Ordering is by code point, which for UTF-8 is byte order.

## unit

`unit` has the single value `()`. It is an ordinary type: it can be a type argument, a field, a
parameter, and a pattern. A function whose body is an indented block with no `=` and no result type
returns `unit`.

## Tuples

`(A, B, …)` is a tuple type of any arity, written and matched with parentheses: `(1, "one")`,
`(n, name) -> …`. Elements are read with `t.0`, `t.1`. Tuples are values with no identity and derive
`Eq`, `Hash`, `Ord`, `Display` and `Debug` when their elements do.

Where a value fills the rest of a line, a tuple needs no parentheses: after `=` in a function, clause
or binding, after `return`, after `->` in a `match` arm, and as a line of a block, including its
last. Inside brackets and in the body of a one-line `if` or `while`, a comma separates other things,
so a tuple there is parenthesized.

```text
divmod(a: int, b: int) -> (int, int) = a / b, a % b

bounds(a: int, b: int) -> (int, int)
    if a > b then return b, a
    a, b
```

A tuple is taken apart by a binding (`val q, r = divmod(17, 5)`, `val (a, (b, c)) = t`), by a
[multiple assignment](/spec/operators-and-control-flow/#multiple-assignment), and by tuple
patterns in `match` and in clauses.

## Function types

`(A, B) -> C` is the type of a function taking `A` and `B` and returning `C`; `() -> C` takes nothing.
A by-name parameter type is written `-> T` inside a parameter list; see
[call-by-name](/spec/by-name/). Function types are traits: a class may list a function type among its
supertypes and implement `apply`, and its instances are then callable as `value(args)` and usable
wherever that function type is expected.

There is no arity limit.

## Any and Nothing

`Any` is the top type. Every reference type is a subtype of `Any`. A value type (built-ins, tuples,
`data`) converts to `Any` by boxing at the point of conversion; the box records the exact type, so a
`match` on an `Any` recovers the value.

`Nothing` is the bottom type: the type of `return`, `break`, `continue` and a call to a function that
never returns. It is a subtype of every type, and an expression of type `Nothing` never produces a
value. An empty collection literal or `None` does **not** have a `Nothing` type argument; its type
comes from context (see [inference](/spec/generics/#inference)).

## Reference types and value types

Class instances, `object`s, arrays, buffers, closures and boxes are **reference types**: they live on
the garbage-collected heap and have identity. Built-in types, tuples and `data` types are **value
types**: they have no identity, and the compiler chooses their representation. See
[equality, identity and memory](/spec/equality-and-memory/).
