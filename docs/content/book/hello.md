---
title: Hello, Avin
summary: A first program, printing, and putting values into text.
weight: 1
---

Here is a whole Avin program:

```text run
main()
  println("Hello, Avin!")
```

```text output
Hello, Avin!
```

Save it as `hello.avin` and run it:

```sh
avin run hello.avin
```

`avin run` compiles the program to a native executable and runs it at once. The next run of an
unchanged program starts straight away, because the executable is kept. `avin build hello.avin`
writes the executable as `./hello` instead, to run as often as you like.

## `main` and `println`

A program starts at the function called `main`. The line `main()` declares it, with an empty list of
parameters, and the lines indented under it are its body. Avin marks blocks by indentation, not
braces: a block is the lines indented further than the line that opens it, and it ends where the
indentation comes back. Two spaces a level is the house style, and `avin fmt` keeps code that way.

`println` writes a line of text to standard output and ends it with a line break. `print` writes
without the break, so several `print`s build up one line:

```text run
main()
  print("one, ")
  print("two, ")
  println("three")
  println("and a line of its own")
```

```text output
one, two, three
and a line of its own
```

A `//` starts a comment, which runs to the end of the line:

```text run
// A program that prints a greeting.
main()
  println("Hi!") // the only thing it does
```

```text output
Hi!
```

## Putting values into text

A string written with an `s` in front, `s"…"`, is an **interpolated** string: `$name` in it is
replaced by the value of `name`, and `${…}` by the value of any expression. A `val` gives a value a
name, which the next chapter covers in full.

```text run
main()
  val name = "Avin"
  val year = 2026
  println(s"Hello, $name!")
  println(s"$name has ${name.charCount} letters, and ${name.toUpper} in capitals")
  println(s"Next year is ${year + 1}.")
```

```text output
Hello, Avin!
Avin has 4 letters, and AVIN in capitals
Next year is 2027.
```

A dollar sign that is meant as text is written twice, `$$`. Ordinary escapes work in every string:
`\"` for a double quote, `\t` for a tab and `\n` for a line break.

```text run
main()
  val price = 5
  println(s"It costs $$$price.")
  println("She said \"hello\".")
  println("first line\nsecond line")
```

```text output
It costs $5.
She said "hello".
first line
second line
```

An `f"…"` string is interpolated too, and a hole in it may be followed by a **format specifier**,
which says how the value is laid out: `%.2f` writes a number with two decimals, `%5d` an integer
right-aligned in five columns, and `%-8s` text left-aligned in eight.

```text run
main()
  val pi = 3.14159265
  println(f"pi is about ${pi}%.2f")
  println(f"[${42}%5d]")
  println(f"[${"left"}%-8s]")
  println(f"${1234567}%,d people")
```

```text output
pi is about 3.14
[   42]
[left    ]
1,234,567 people
```

A text that runs over several lines is a **text block**, written between `"""` marks. Its content
starts on the line after the opening marks, and the indentation the lines share is dropped:

```text run
main()
  val poem = """
    Roses are red,
      violets are blue,
    Avin is compiled,
      and quick to run too.
    """
  print(poem)
```

```text output
Roses are red,
  violets are blue,
Avin is compiled,
  and quick to run too.
```

The [strings and formatting](/spec/strings-and-formatting/) chapter of the specification has every
form of string literal, and every [format specifier](/spec/strings-and-formatting/#format-specifiers).

## Exercises

1. Write a program that prints your name in a box of `*` characters, three lines tall.
2. Store your age in a `val` and print how old you will be in ten years, in a sentence.
3. Use `f"…"` to print a small table of three items and their prices, with the prices lined up on
   the right and two decimals each.
