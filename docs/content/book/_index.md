---
title: The Avin Book
summary: Learn Avin one idea at a time, through small programs that grow into complete ones.
weight: 7
---

This book teaches Avin by example. Each chapter starts with a small, complete program, adds one idea
at a time, and ends with a few exercises to try on your own. Every program in it is a whole program
you can run, and the output shown beside it is what it prints: the book's programs are built and run
with the compiler's own tests, so what you read here is what Avin does.

The [specification](/spec/) is where each feature is defined in full, and the
[system library](/spec/system-library/) is where its modules are documented. The book links to both
when an idea first appears, for when you want the whole story.

Before you start, [install Avin](/getting-started/). To run a program from the book, save it in a
file, say `hello.avin`, and run it:

```sh
avin run hello.avin
```

## Chapters

1. [Hello, Avin](/book/hello/): a first program, printing, and putting values into text.
2. [Values and types](/book/values/): `val` and `var`, numbers, text and truth values, and arithmetic.
3. [Functions](/book/functions/): one-line functions, blocks, recursion, clauses, and default and named
   arguments.
4. [Choices and patterns](/book/choices/): `if` as an expression, `match`, chained comparisons and loops.
5. [Data](/book/data/): your own types, taking them apart with patterns, `Option`, and failures as
   values with `Result`.
6. [Collections](/book/collections/): vectors, maps and sets, `map`, `filter` and `foldLeft`, ranges,
   comprehensions, and a list that never ends.
7. [Closures and higher-order functions](/book/closures/): functions as values, functions that make
   functions, and closures that remember.
8. [Classes and traits](/book/classes/): objects with state, shared behaviour through traits, and
   inheritance.
9. [Literate programs](/book/literate/): a program written as an essay, in a `.lavin` document.
10. [Putting it together](/book/programs/): four complete programs, a word counter, a calculator, the
    eight queens and the Game of Life.
