---
title: Values and types
summary: "`val` and `var`, numbers, text and truth values, conversions, and arithmetic."
weight: 2
---

A program works with values: numbers, text, truth values. This chapter names them, says what types
they have, and computes with them.

```text run
main()
  val width = 7
  val height = 6
  println(width * height)
```

```text output
42
```

## `val` and `var`

`val` gives a value a name. The name keeps that value for as long as it is in scope: a `val` cannot
be assigned again. `var` makes a **variable** instead, a name whose value can be replaced with `=`,
or changed in place by a compound assignment such as `+=`:

```text run
main()
  val greeting = "hello"
  var count = 1
  count = count + 1
  count += 10
  count *= 2
  println(s"$greeting, $count")
```

```text output
hello, 24
```

Use `val` unless the value really has to change. A program whose names keep their values is easier
to read: what a name meant where it was made, it means everywhere after.

## Types

Every value has a type, and the compiler knows each one before the program runs. You seldom have to
write a type, because the compiler **infers** it from the value: `7` is an `int`, `2.5` a `real`. Where
you want to say it, or need to, it is written after the name with a colon:

```text run
main()
  val count: int = 3
  val price: real = 2.5
  val name: string = "lamp"
  val inStock: bool = true
  val initial: char = 'L'
  println(s"$count ${name}s at $price each, in stock: $inStock, filed under $initial")
```

```text output
3 lamps at 2.5 each, in stock: true, filed under L
```

The built-in types are written in lower case:

| Type | Values |
| --- | --- |
| `int` | Whole numbers from −2,147,483,648 to 2,147,483,647 (32 bits) |
| `long` | Whole numbers to about ±9.2 × 10¹⁸ (64 bits) |
| `real` | Numbers with a fraction, such as `2.5` or `1e-3` (64-bit floating point) |
| `bool` | `true` or `false` |
| `char` | One Unicode character, written in single quotes: `'a'`, `'é'`, `'🙂'` |
| `string` | Text, written in double quotes: `"hello"` |

Types you make yourself, and the library's, are written in PascalCase: `Vector`, `Option`, `Point`.
The [types](/spec/types/) chapter of the specification describes each built-in type in full.

## Arithmetic

The arithmetic operators are `+`, `-`, `*`, `/` and `%` (the remainder), with `*`, `/` and `%`
binding tighter than `+` and `-`, and parentheses grouping as usual. Dividing two integers gives a
whole number, rounding toward zero; dividing two `real`s gives a `real`.

```text run
main()
  println(2 + 3 * 4)
  println((2 + 3) * 4)
  println(17 / 5)
  println(17 % 5)
  println(-17 / 5)
  println(17.0 / 5.0)
  println(0.1 + 0.2)
```

```text output
14
20
3
2
-3
3.4
0.30000000000000004
```

The last line is not a mistake in Avin: `0.1` has no exact binary fraction, so a `real` holds the
nearest one there is, and the sum shows the difference.

The operands of an operator have one type. Avin never converts a number to another type on its own,
so an `int` and a `real` do not mix until you say which one you mean. A conversion is written as a
call of the type you want:

```text run
main()
  val apples = 7
  val people = 2
  println(apples / people)
  println(real(apples) / real(people))
  println(int(3.99))
  val big = long(apples) * 1_000_000_000
  println(big)
```

```text output
3
3.5
3
7000000000
```

An underscore inside a number is only there for the reader: `1_000_000_000` is a billion.

## Numbers that do not fit

An `int` has 32 bits, and arithmetic whose answer does not fit in them **traps**: the program stops
with a message naming the line, rather than carrying on with a wrong number. When wrapping around is
what you want, say so with `+%`, `-%` or `*%`; `+|`, `-|` and `*|` stop at the largest or smallest
value instead. Each integer type knows its limits as `MinValue` and `MaxValue`:

```text run
main()
  println(int.MaxValue)
  println(int.MaxValue +% 1)
  println(int.MaxValue +| 1)
  println(long.MaxValue)
```

```text output
2147483647
-2147483648
2147483647
9223372036854775807
```

[Integers](/spec/types/#integers) in the specification lists every integer type and operator.

## Text and characters

A `string` is a sequence of characters. It has methods, called with a dot, that answer questions
about it or make new strings from it; a string itself never changes.

```text run
main()
  val word = "Avin"
  println(word.charCount)
  println(word.toUpper)
  println(word + " " + word.toLower)
  println(word.contains("vi"))
  println("ab".repeat(3))
  println("  padded  ".trim)
  println("a,b,c".split(","))
  println("apple" < "banana")
```

```text output
4
AVIN
Avin avin
true
ababab
padded
[a, b, c]
true
```

A `char` is a single character. `int(c)` is its code, and `char(n)` the character of a code, so
arithmetic on characters is written against character literals: the digit `'7'` is worth
`int('7') - int('0')`.

```text run
main()
  val digit = '7'
  println(int(digit) - int('0'))
  println(char(int('a') + 2))
  println('q'.isLetter)
  println('Q'.toLower)
```

```text output
7
c
true
q
```

Reading a number from text may fail, since not every text is a number. `toIntOption()` answers
`Some` of the number, or `None` when there is none; [Data](/book/data/) shows what to do with the
answer.

```text run
main()
  println("42".toIntOption())
  println("forty-two".toIntOption())
  println(42.toString() + "!")
```

```text output
Some(42)
None
42!
```

## Truth values

`bool` has two values, `true` and `false`. Comparisons yield one: `==` and `!=` for equal and not
equal, and `<`, `<=`, `>`, `>=`. They combine with `&&` (and), `||` (or) and `!` (not); `&&` and `||`
only evaluate their right side when it can change the answer.

```text run
main()
  val age = 17
  val adult = age >= 18
  println(adult)
  println(!adult && age > 12)
  println(age == 17 || age == 71)
```

```text output
false
true
true
```

## Exercises

1. Convert a temperature of 21.5 degrees Celsius to Fahrenheit (`c * 9 / 5 + 32`), printing both.
2. Given a number of seconds, say `100000`, print it as days, hours, minutes and seconds using `/`
   and `%`.
3. Compute 2 to the power 40 by repeated multiplication. Which type does it need, and what happens
   with the other one?
