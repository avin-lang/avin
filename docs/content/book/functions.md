---
title: Functions
summary: One-line functions, blocks, recursion, clauses, and default and named arguments.
weight: 3
---

A function gives a computation a name. The shortest form fits on one line: the function's name, its
parameters in parentheses, `=`, and the expression it computes.

```text run
square(n: int) = n * n

area(width: real, height: real) = width * height

main()
  println(square(12))
  println(area(2.5, 4.0))
```

```text output
144
10
```

Every parameter has its type written, `n: int`. The result type is inferred from the expression, as a
`val`'s is, and may be written after `->` for the reader: `square(n: int) -> int = n * n`.

## Blocks

A longer function has a **block** for its body: indented lines under its signature. A block yields
its last expression, so there is no `return` at the end; the `val`s before it are steps on the way.

```text run
import avin.math

distance(x1: real, y1: real, x2: real, y2: real) -> real
  val dx = x2 - x1
  val dy = y2 - y1
  math.sqrt(dx * dx + dy * dy)

main()
  println(distance(0.0, 0.0, 3.0, 4.0))
```

```text output
5
```

`import avin.math` brings in the standard library's [`avin.math`](/spec/system-library/#avin-math)
module, so its functions are reached as `math.sqrt`, `math.sin` and so on.

A function with no `=` and no result type is run for what it does rather than for a value, as `main`
is. It yields `unit`, the type with one value, `()`, which carries no information:

```text run
banner(title: string)
  val line = "=".repeat(title.charCount + 4)
  println(line)
  println(s"| $title |")
  println(line)

main()
  banner("Avin")
```

```text output
========
| Avin |
========
```

`return` leaves a function early with a value. It is useful when an answer is known before the end,
and needs a declared result type:

```text run
firstVowel(word: string) -> char
  for c in word.chars
    if "aeiou".contains(c.toString()) then return c
  '?'

main()
  println(firstVowel("rhythm and blues"))
  println(firstVowel("crypt"))
```

```text output
a
?
```

## Recursion

A function may call itself. A recursive function must declare its result type, because the compiler
cannot infer a type from an expression that mentions the function being defined.

```text run
factorial(n: int) -> long = if n <= 1 then 1 else long(n) * factorial(n - 1)

main()
  println(factorial(5))
  println(factorial(20))
```

```text output
120
2432902008176640000
```

`if … then … else` chooses between two values; [Choices and patterns](/book/choices/) covers it.

## Clauses

A function that does different things for different arguments can be written as **clauses**: the
signature on a line of its own, then one line per case, each the function's name applied to a
pattern. A call runs the first clause whose patterns match its arguments.

```text run
fib(n: int) -> int
fib(n < 2) = n
fib(n) = fib(n - 1) + fib(n - 2)

gcd(a: int, b: int) -> int
gcd(a, 0) = a
gcd(a, b) = gcd(b, a % b)

main()
  println((0 ..< 12).map(fib))
  println(gcd(1071, 462))
```

```text output
[0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89]
21
```

`fib(n < 2)` is a **comparison pattern**: it matches any `n` less than 2 and names it `n`. `gcd(a, 0)`
matches when the second argument is zero. The clauses together must cover every possible argument,
and the compiler checks that they do. `(0 ..< 12).map(fib)` calls `fib` on each number from 0 up to
12, not including 12; [Collections](/book/collections/) has more. The specification describes clauses
under [functions](/spec/declarations/#clauses).

## Functions inside functions

A function declared inside a block is a **local function**. It can read the names around it, and is
a handy place for a helper no other code needs. Here `go` carries a running total. Its recursive
call is the last thing it does, a **tail call**, so it runs as a loop and uses no more stack however
deep it goes. `@tailrec` makes that a promise the compiler checks: a function marked with it whose
call to itself is not a tail call is an error ([tail calls](/spec/declarations/#tail-calls)).

```text run
sumDigits(n: int) -> int
  @tailrec
  go(rest: int, total: int) -> int = if rest == 0 then total else go(rest / 10, total + rest % 10)
  go(n, 0)

main()
  println(sumDigits(98765))
```

```text output
35
```

## Default and named arguments

A parameter may have a **default**, used when a call leaves it out. A call may also give an argument
by name, `name = value`, which makes a call with several arguments easier to read and lets it skip
the defaults it does not want to change.

```text run
greet(name: string, greeting: string = "Hello", ending: string = "!") -> string =
  s"$greeting, $name$ending"

main()
  println(greet("Ada"))
  println(greet("Ada", "Welcome"))
  println(greet("Ada", ending = "?"))
  println(greet(greeting = "Hi", name = "Bo"))
```

```text output
Hello, Ada!
Welcome, Ada!
Hello, Ada?
Hi, Bo!
```

Positional arguments come first, then named ones in any order. The full rules are in
[default and named arguments](/spec/declarations/#default-and-named-arguments).

## Exercises

1. Write `isLeapYear(year: int) -> bool`: a year divisible by 4 is a leap year, except one divisible
   by 100, unless it is also divisible by 400.
2. Write `power(base: long, exp: int) -> long` as clauses, using the fact that `base` to an even
   power is the square of `base` to half that power.
3. Give `banner` a parameter `border: char = '='` and call it with a different border by name.
