---
title: Operators and control flow
summary: Sysl's operator set and precedence, comparison chains, blocks that yield values, if, while, match and return.
weight: 19
---

## Operators

Avin has Sysl's operator set and precedence. Operators are trait methods (`+` is `Add`, `<` is `Ord`,
`==` is `Eq`), so a generic needs the bound that supplies one, and a user type gains one through an
`impl`.

| Family | Operators |
| --- | --- |
| Arithmetic | unary `+` `-`; `+` `-` `*` `/` `%` |
| Explicit wrapping | `+%` `-%` `*%` |
| Explicit saturation | `+\|` `-\|` `*\|` |
| Bitwise | `~` `&` `^` `\|` |
| Shifts | `<<` `>>` (shifts bind like multiplication) |
| Logical | `!`, short-circuit `&&` `\|\|` |
| Comparison | `==` `!=` `<` `<=` `>` `>=`, chained |
| Identity | `===` `!==` (reference types only) |
| Range | `..` `..<` |
| Cons | `::`, prepending to a `List` (right-associative) |
| Pattern test | `is`, `is not` |
| Propagation | postfix `?` |
| Assignment | `=` and compound forms (`+=`, `<<=`, `+%=`, …) |
| Yielding assignment | `:=` and compound forms, which return the stored value |
| Update | prefix and postfix `++` `--` |

Bitwise operators bind more tightly than comparisons: `x & mask == 0` tests the masked value.
Within the bitwise family, `&` binds tightest, then `^`, then `|`. Shifts bind at the multiplicative
level, so `1 + 2 << 3` is `17`. A shift count at least as large as the width shifts everything out.
It is not masked.

`::` binds more loosely than every arithmetic and bitwise operator and more tightly than ranges and
comparisons, and it groups to the right: `n + 1 :: rest` is `(n + 1) :: rest`, and `1 :: 2 :: []` is
`1 :: (2 :: [])`.

Sysl's pointer operators (prefix `*` and `&`) and `await` are not part of Avin.

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

## match

`subject match` is followed by indented `pattern -> body` arms and an optional `else`. It is an
expression, checked for exhaustiveness. Patterns include:

- literals, including negative numbers;
- inclusive and exclusive literal ranges (`'a'..'z'`, `0..<10`);
- constructors with nested patterns, tuples and `()`;
- list patterns: `head :: tail` matches a non-empty `List`, grouping to the right
  (`first :: second :: rest`);
- [sequence patterns](/spec/collections/#sequence-patterns): `[]` matches an empty sequence and
  `[a, b]` a sequence of exactly two elements;
- type patterns (`d: Dog`);
- alternatives (`1 | 2 | 3`), which bind no names and may continue across lines with a leading or
  trailing `|`;
- named patterns (`whole @ Some(x)`);
- `_`.

A guard (`n if n > 0 ->`) is evaluated at most once per arm. Range patterns do not count toward
exhaustiveness, so a scalar match that uses them needs a catch-all.

## return and end markers

`return expr` exits the enclosing function. Inside a lambda it exits the lambda. Optional end markers
(`end if`, `end while`, `end match`, `end val`, `end name`) are checked against the construct they
close.
