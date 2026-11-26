---
title: Choices and patterns
summary: "`if` as an expression, `match` and its patterns, chained comparisons, and loops."
weight: 4
---

A program that does the same thing every time is not much of a program. This chapter is about
choosing: between two values with `if`, among many with `match`, and doing something again with a
loop.

```text run
main()
  val temperature = 23
  println(if temperature > 20 then "warm" else "cool")
```

```text output
warm
```

## `if` is an expression

`if` yields a value: the value of the branch it takes. That is why it can stand inside `println`
above, or on the right of a `val`, or as the whole body of a function. `elif` adds more conditions,
tried in order. A branch on the same line as its condition is written after `then`; a branch that
needs several lines is an indented block, and yields its last expression as a function body does.

```text run
sign(n: int) -> string = if n < 0 then "negative" elif n == 0 then "zero" else "positive"

main()
  println(sign(-3))
  println(sign(0))
  val n = 12
  val description = if n % 2 == 0
    val half = n / 2
    s"even, twice $half"
  else "odd"
  println(description)
```

```text output
negative
zero
even, twice 6
```

An `if` with no `else` yields nothing, `unit`, so it is used for what its branch does:
`if n < 0 then println("below zero")`. The [if](/spec/operators-and-control-flow/#if) section of the
specification has the rules.

## Chained comparisons

A comparison can have more than two sides: `lo <= x < hi` is true when `x` is at least `lo` and less
than `hi`, and is how Avin writes a range check. Each operand is evaluated once, from the left, and
the chain stops at the first link that fails.

```text run
isDigit(c: char) -> bool = '0' <= c <= '9'

main()
  val x = 5
  println(1 <= x <= 10)
  println(1 <= x < 5)
  println(isDigit('7'))
  println(isDigit('x'))
```

```text output
true
false
true
false
```

## `match`

`value match` is followed by indented **arms**, each a pattern, `->` and a result. The first arm
whose pattern matches is taken, and the whole `match` yields its result. Like `if`, a `match` is an
expression.

```text run
dayName(n: int) -> string = n match
  1 -> "Monday"
  2 -> "Tuesday"
  3 -> "Wednesday"
  4 -> "Thursday"
  5 -> "Friday"
  6 | 7 -> "the weekend"
  _ -> "not a day"

main()
  println(dayName(3))
  println(dayName(7))
  println(dayName(9))
```

```text output
Wednesday
the weekend
not a day
```

`6 | 7` matches either value, and `_` matches anything: it is the arm for everything else. A `match`
must have an arm for every possible value, and the compiler checks it. Without the `_` arm here, the
program would not compile, since `dayName(9)` would have no answer.

A pattern can be a range, `90 .. 100` with both ends included or `80 ..< 90` with the top left out,
or a comparison that names the value, `n < 0`. Arms like these that between them reach every integer
need no `_` at all:

```text run
grade(score: int) -> char = score match
  90 .. 100 -> 'A'
  80 ..< 90 -> 'B'
  70 ..< 80 -> 'C'
  _ -> 'F'

size(n: int) -> string = n match
  n < 0 -> "negative"
  0 -> "zero"
  1 <= n < 10 -> "small"
  n >= 10 -> "large"

main()
  println(s"${grade(95)} ${grade(85)} ${grade(42)}")
  println(s"${size(-4)} ${size(0)} ${size(7)} ${size(1000)}")
```

```text output
A B F
negative zero small large
```

Characters match the same way. `'a' <= c <= 'z'` as a pattern is a chained comparison that names the
character it matches:

```text run
kind(c: char) -> string = c match
  'a' | 'e' | 'i' | 'o' | 'u' -> "vowel"
  'a' <= c <= 'z' -> "consonant"
  '0' <= c <= '9' -> "digit"
  ' ' -> "space"
  _ -> "something else"

main()
  for c in "hi 2u!".chars
    println(s"$c: ${kind(c)}")
```

```text output
h: consonant
i: vowel
 : space
2: digit
u: vowel
!: something else
```

## Matching several values at once

Two values in parentheses make a **tuple**, `(a, b)`, and a tuple is matched part by part. FizzBuzz
counts up, saying "Fizz" for multiples of three, "Buzz" for multiples of five, and "FizzBuzz" for
both; matching on both remainders at once says it directly:

```text run
fizzBuzz(n: int) -> string = (n % 3, n % 5) match
  (0, 0) -> "FizzBuzz"
  (0, _) -> "Fizz"
  (_, 0) -> "Buzz"
  _ -> n.toString()

main()
  println((1 .. 15).map(fizzBuzz).mkString(" "))
```

```text output
1 2 Fizz 4 Buzz Fizz 7 8 Fizz Buzz 11 Fizz 13 14 FizzBuzz
```

An arm may have a **guard**, `if` and a condition after its pattern, and is taken only when the
condition holds too:

```text run
describe(point: (int, int)) -> string = point match
  (0, 0) -> "the origin"
  (x, 0) -> s"on the x axis at $x"
  (0, y) -> s"on the y axis at $y"
  (x, y) if x == y -> s"on the diagonal at $x"
  (x, y) -> s"at ($x, $y)"

main()
  println(describe((0, 0)))
  println(describe((3, 0)))
  println(describe((0, -2)))
  println(describe((4, 4)))
  println(describe((1, 2)))
```

```text output
the origin
on the x axis at 3
on the y axis at -2
on the diagonal at 4
at (1, 2)
```

The [match](/spec/operators-and-control-flow/#match) section of the specification lists every kind of
pattern.

## Loops

`for x in …` runs its body once for each value. `1 .. 5` is the numbers 1 to 5, both included, and
`0 ..< 5` leaves the top out; a string's `chars` are its characters. A one-line body follows `do`.

```text run
main()
  for i in 1 .. 3 do println(s"$i squared is ${i * i}")
  var total = 0
  for i in 0 ..< 100
    if i % 3 == 0 || i % 5 == 0 then total += i
  println(total)
```

```text output
1 squared is 1
2 squared is 4
3 squared is 9
2318
```

`while` runs its body for as long as a condition holds. The Collatz sequence halves an even number
and turns an odd one into three times it plus one; it seems always to reach 1, though nobody has
proved it:

```text run
main()
  var n = 27
  var steps = 0
  var highest = n
  while n != 1
    n = if n % 2 == 0 then n / 2 else 3 * n + 1
    if n > highest then highest = n
    steps += 1
  println(s"27 takes $steps steps, and climbs as high as $highest")
```

```text output
27 takes 111 steps, and climbs as high as 9232
```

`break` leaves a loop at once, and `continue` goes on to its next round:

```text run
main()
  for n in 1 .. 20
    if n % 2 == 0 then continue
    if n > 9 then break
    print(s"$n ")
  println("done")
```

```text output
1 3 5 7 9 done
```

## Exercises

1. Write `season(month: int) -> string` with a `match` on ranges of months.
2. Write `romanDigit(c: char) -> int` that gives the value of `'I'`, `'V'`, `'X'`, `'L'`, `'C'`, `'D'`
   and `'M'`, and `0` for any other character.
3. Find the number below 1000 whose Collatz sequence takes the most steps.
