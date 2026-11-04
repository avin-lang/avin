---
title: Strings and formatting
summary: String literals, interpolation as a method call on StringContext, user-defined interpolators, and formatting through Display and Debug.
weight: 22
---

Avin has Sysl's string literal, interpolation and formatting surface. The normative reference for
the details is Sysl's `docs/content/reference/strings.md`; Avin differs only where noted here.

| Form | Meaning |
| --- | --- |
| `"…"` | Ordinary string with escapes |
| `raw"…"` | Raw string, with no escape processing |
| Text blocks | Multi-line literals with Sysl's indentation and line-joining rules |
| `s"…"` | Interpolation: `$name`, `${expression}`, `$$` |
| `f"…"` | Formatted interpolation: a specifier after a hole (`${x}%08.3f`) |
| `name"…"` | Any interpolator: a call of the method `name` on a `StringContext` |

Sysl's specifier grammar applies: conversion codes, flags, width, precision, sign and justification.
Width and precision may be expressions. Specifiers are type-checked at compile time against the
hole's type.

## Interpolators

Every interpolated string is a method call. The text before the opening quote names a method of
`StringContext`, the literal pieces between the holes become the context's parts, and the holes
become the method's arguments:

```text
p"text $a and ${b + 1}"
// is exactly
StringContext(["text ", " and ", ""]).p(a, b + 1)
```

- `StringContext` holds `parts: Vector[string]`, always one more part than there are holes. A part
  is the text as written, with its escapes unprocessed; the method decides what they mean.
- The call is checked like any other call: the holes must suit the method's parameters, which may be
  a fixed list or variadic (`args: Display*`), and the method's result is the expression's value, of
  whatever type it declares.
- Holes are written the same way for every interpolator: `$name`, `${expression}`, and `$$` for a
  dollar sign. Each hole is evaluated once, left to right.
- `s`, `f` and `raw` are ordinary methods of `StringContext` in the core library. `s` and `f`
  process escapes and render each hole; `raw` keeps the parts as written. A program may call them
  directly, as `StringContext(parts).s(x)`.

A program adds its own interpolator by giving `StringContext` a method through a trait it declares
and an `impl` of that trait. The interpolator is available wherever the trait is imported:

```text
trait SqlInterpolation
    sql(args: SqlValue*) -> Query

impl SqlInterpolation for StringContext
    sql(args: SqlValue*) -> Query = Query(parts, args)

val q = sql"select * from users where id = $id and name = $name"
```

Here each hole must be a `SqlValue`, so `id` and `name` are passed as parameters of the query, never
pasted into its text.

## Rendering

A hole is rendered by its type's `Display` implementation, or by `Debug` for `%?`. The built-in
types display as Sysl renders them: an integer of any width as its decimal digits with a sign when
negative, a `byte` as its number, a `char` as itself, a `real` as the fewest digits that read back
as the same value, written out without an exponent (`2.5`, `1`, `0.1`), a `bool` as `true` or
`false`, `()` as `()`, and a string as itself. A value whose type
implements neither is a compile error. There is no fallback that prints an address. The specifier
applies to the whole rendered value.

The core library declares the traits:

```text
trait Writer
    write(text: string)
    writeChar(c: char) = …
    writeInt(n: long) = …
    writeField(text: string, spec: FormatSpec) = …
    writeSpaces(n: long) = …

data FormatSpec(width: int = 0, precision: int = -1, left: bool = false)

trait Display
    display(out: Writer, spec: FormatSpec)
    toString() -> string = …

trait Debug
    debug(out: Writer, spec: FormatSpec)
```

- `FormatSpec` is the field a value fills: `width` characters, padded with spaces on the left, or on
  the right when `left`; 0 asks for no padding, and wider text is written whole. `precision`, -1 for
  none, is read by a renderer that takes one, in its own terms. `writeField` writes a text in the
  field a spec describes, which is how most `display` methods end.
- `toString()` renders into a `StringWriter`, a writer building a string, and returns its `text()`.
  `StandardOutput` is the writer for standard output; it writes an integer's digits without making
  a string of them. `StandardError` is the writer for standard error, which keeps no buffer.
- `print(x)` writes `x`'s `Display` form and a new line to standard output; `eprint(x)` writes the
  same to standard error, as one write, so a line is never split by another writer's. Given several
  values, of any types that implement `Display`, each writes them separated by `, `:
  `print("total", 3, (1, 2))` writes `total, 3, (1, 2)`. `debug(x)` is `x`'s
  `Debug` form as a string, so `print(debug(x))` prints it. Inside an implementation of `Debug`, the
  bare name `debug` is the method, and a part is written with `part.debug(out, spec)`.
- The built-in types' `Debug` forms are their `Display` forms, except that a string and a `char`
  are written as the literals that make them: quoted, with the quote, the backslash and control
  characters escaped (`"a\"b\n"`, `'\''`).

## Lowering

For `s` and `f`, the compiler keeps literal segments, typed holes, parsed specifiers and source spans
in its intermediate representation, then lowers the interpolation to direct `Writer` calls into a
string builder instead of calling the method, with the same result. No value is boxed to format it.
Every other interpolator is compiled as the method call it denotes.
