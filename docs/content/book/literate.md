---
title: Literate programs
summary: A program written as an essay, in a `.lavin` document whose prose is Markdown and whose code is the lines indented four columns.
weight: 9
---

Comments explain code a line at a time. Sometimes a program deserves more: an explanation with
headings and paragraphs, that says why before it says how, and shows the code in the order a reader
needs it. A **literate program** is that explanation, with the program inside it.

In Avin a literate program is a file whose name ends in `.lavin`. It is a Markdown document, and the
lines indented four columns are the program. Everything else is prose, which the compiler never
reads. Here is a small one:

```lavin run
# Halving

This program explains itself. Everything at the margin is prose, and the compiler never
sees it. The indented lines are the program:

    half(n: int) -> int = n / 2

    main()
      val h = half(9)

A paragraph in the middle of a function ends nothing, so a body can be explained a step
at a time. Integer division rounds toward zero, so half of nine is four:

      println(h)
```

```text output
4
```

The program in that document is exactly its indented lines, with the four columns taken off:

```text run
half(n: int) -> int = n / 2

main()
  val h = half(9)
  println(h)
```

```text output
4
```

A `.lavin` file runs like any other program:

```sh
avin run halving.lavin
```

## The rules

There are only a few, and they are Markdown's own:

- **Four columns of indent make a line code.** That is the indent Markdown shows as a code block, so
  what a reader sees as code is exactly what the compiler reads.
- **The program's own indentation starts after the four columns.** A declaration is four columns
  in, its body six, and so on, two more each level.
- **Prose between two indented blocks ends nothing.** The next block carries on where the program
  was, so the lines after a paragraph can still belong to the function above it.
- **Fenced code is only shown.** A block between ```` ``` ```` or `~~~` marks is an illustration:
  a wrong version beside the right one, or a command to type. None of it is compiled.

Every position the compiler reports, in an error, a trap or a failed test, is the line and column of
the document you have open. The [literate source](/spec/declarations/#literate-source) section of the
specification has the rules in full.

## Why write one

A literate program is for code whose reasons matter as much as its steps: an algorithm worth
understanding, a tricky piece of a larger system, a lesson. Written as an essay, the explanation sits
beside the code it explains, and because the essay *is* the program, it cannot fall out of date
without the program failing to build.

## An essay that builds a program

This document explains how Roman numerals work while it builds a program that writes and reads them,
and ends by testing the two halves against each other:

```lavin run
# Roman numerals

A Roman numeral writes a number as a row of letters, each worth a fixed amount, largest
first: `MDCLXVI` is 1000 + 500 + 100 + 50 + 10 + 5 + 1. Six pairs break the pattern by
putting a smaller letter before a larger one to subtract it: `IV` is 4 and `CM` is 900.

The trick to writing one is to treat those six pairs as letters of their own. Then a
table of thirteen values, largest first, is all the program needs to know:

    val numerals = [
      (1000, "M"),
      (900, "CM"),
      (500, "D"),
      (400, "CD"),
      (100, "C"),
      (90, "XC"),
      (50, "L"),
      (40, "XL"),
      (10, "X"),
      (9, "IX"),
      (5, "V"),
      (4, "IV"),
      (1, "I")
    ]

## Writing a number

Take the largest value that still fits, write its letters, and carry on with what is
left. Zero is written as nothing at all, which ends the recursion.

    toRoman(n: int) -> string
    toRoman(0) = ""
    toRoman(n) =
      val (value, letters) = numerals.find((v, _) -> v <= n).get
      letters + toRoman(n - value)

## Reading one back

Reading goes the other way, a letter at a time. Each letter's worth is added, unless the
letter after it is worth more, in which case it is subtracted: that one rule covers all
six pairs. A character that is no numeral is worth nothing.

    worth(c: char) -> int
    worth('M') = 1000
    worth('D') = 500
    worth('C') = 100
    worth('L') = 50
    worth('X') = 10
    worth('V') = 5
    worth('I') = 1
    worth(_) = 0

    fromRoman(s: string) -> int
      val values = s.chars.map(worth).toVector
      var total = 0
      for (v, i) in values.zipWithIndex
        if i + 1 < values.length && v < values(i + 1) then total -= v else total += v
      total

## Trying it

A few numbers first, to see the pairs at work:

    main()
      for n in [4, 9, 14, 1994, 2026, 3999]
        println(s"$n = ${toRoman(n)}")

Then the real test. Every number from 1 to 3999 must read back as itself, which checks
each half against the other:

      val wrong = (1 .. 3999).filter(n -> fromRoman(toRoman(n)) != n)
      println(s"round trips that fail: ${wrong.length}")
```

```text output
4 = IV
9 = IX
14 = XIV
1994 = MCMXCIV
2026 = MMXXVI
3999 = MMMCMXCIX
round trips that fail: 0
```

`val (value, letters) = …` takes the pair `find` answers apart into two names, as a pattern in a
`match` would. `find` answers an `Option`, and `.get` takes the value out of it, which is safe here
because the table ends with 1 and every number left to write is at least 1.

## Tools for literate programs

Two commands work with the two halves of a document. `avin tangle` prints the program with the
prose taken out, line for line with the document, and `avin weave` renders the document as a web page
with its program coloured:

```sh
avin tangle roman.lavin
avin weave roman.lavin -o roman.html
```

`avin fmt` formats the code of a `.lavin` file and leaves its prose alone. The language server,
`avin lsp`, reads a `.lavin` document as it reads an `.avin` file, so an editor shows its errors,
types and definitions at the document's own lines, and the VS Code extension highlights the program
blocks as Avin and the prose as Markdown. [Editors](/getting-started/editors/) has the details.

## Exercises

1. Rewrite the Collatz program from [Choices and patterns](/book/choices/) as a literate document
   that explains the conjecture before it tests it.
2. Add a check to the Roman numerals essay that `fromRoman` gives `0` for a text with no numerals in
   it, and explain why in a paragraph before it.
3. Run `avin tangle` on a document of your own and compare its line numbers with the document's.
