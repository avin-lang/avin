---
title: Strings and formatting
summary: String literals, interpolation as a method call on StringContext, user-defined interpolators, and formatting through Display and Debug.
weight: 22
---

String literals are written as Sysl's string reference describes them
(`docs/content/reference/strings.md`). Text blocks, interpolation, format specifiers and rendering
are specified here.

| Form | Meaning |
| --- | --- |
| `"…"` | Ordinary string with escapes |
| `raw"…"` | Raw string, with no escape processing: a backslash is an ordinary character (`raw"C:\dir"`), though `\"` does not end the literal |
| `"""…"""` | Text block: a literal over several lines, its incidental indentation dropped ([below](#text-blocks)); every prefix composes with it, `s"""…"""` and `raw"""…"""` included |
| `s"…"` | Interpolation: `$name`, `${expression}`, `$$` for a dollar sign, `$"` for a double quote |
| `f"…"` | Formatted interpolation: a specifier after a hole (`${x}%08.3f`) |
| `name"…"` | Any interpolator: a call of the method `name` on a `StringContext` |

## Text blocks

A literal that spans lines is written `"""` … `"""`. Its content begins on the line **after** the
opening delimiter, which ends its line, so the column the block opens at plays no part in its value
and a block reads the same at any depth. A block written on one line is an error.

```text
render() -> string
    """
    one
      two
    three
    """
// "one\n  two\nthree\n"
```

Three things happen to each line of a block that do not happen inside `"…"`:

- **Its incidental indentation is dropped.** The strip is the least indentation of the lines that
  carry content, together with that of the closing delimiter's line when the delimiter stands alone
  on it, so the closing delimiter is the control: further left, the value keeps more indentation.
  Lines holding only blanks say nothing about the strip and keep their line breaks. A tab and a space
  are each one column.
- **Its trailing blanks are dropped**, carriage returns among them, so whitespace nobody can see
  never enters the value, and a file means the same with either line ending. Escapes are read after
  the trimming, so a blank that is meant is written `\u{20}`.
- **A `\` at the end of a line joins it to the next**, the blanks before it going with the line's
  end, for data that must have no line breaks in its value.

Whether the value ends with a line break needs no rule of its own: a closing delimiter alone on its
line comes after the last line's break, and one written after content comes before any. A `"`, and
`""`, are text inside a block; the first `"""` closes it, so a block cannot end with a quote except as
the escape `\"`. An unterminated block is reported at its opening delimiter.

**Every prefix composes with the quote form**: `s"""`, `f"""`, `raw"""` and any `name"""`. A prefix
says how a literal is read, the quote form how it was written. Holes, `$$` and `$"` are as in
`name"…"`, and a hole may span lines, which are then code rather than text. The parts an interpolator
is given have the block's indentation and trailing blanks already dropped, and their escapes as
written; since joining a line is an escape, `raw"""…"""` and every interpolator but `s` and `f`
receive a `\` at a line's end and the line break after it as written.

## Format specifiers

In an `f"…"` literal, a `%` written right after a hole begins that hole's specifier, which says how
the value is laid out:

```text
val n = 7
println(f"${n}%03d|${2.5}%08.2f|${"hi"}%-5s|")     // 007|00002.50|hi   |
```

A specifier is `%`, then any flags, then a width, then `.` and a precision, then one conversion
letter; everything but the `%` and the letter may be left out. A hole with no specifier renders as in
`s"…"`. A `%` anywhere else, after text or after another `%`, is text: `f"${n}%d done, 100% sure"`.
There is no `%%`.

| Conversion | Takes | Writes |
| --- | --- | --- |
| `d`, `i` | an integer of any width, a `byte`, or a [`BigInt`](/spec/types/#bigint) | decimal |
| `x`, `X` | the same | hexadecimal, in lower or upper case |
| `o`, `b` | the same | octal, binary |
| `f` | a `real` | fixed point, 6 decimals unless a precision says otherwise |
| `e`, `E` | a `real` | one digit, decimals and an exponent of at least two digits |
| `g`, `G` | a `real` | `f` or `e`, whichever is shorter, the precision counting significant digits, trailing zeros dropped |
| `c` | a `char` | the character |
| `s` | a string, or any other value with `Display` that is not a number | its `Display` form |
| `?` | any value with `Debug` | its `Debug` form |

- **Integers are signed in every base.** A negative value is written as `-` and its magnitude's
  digits, in hexadecimal, octal and binary as in decimal (`${-42}%x` is `-2a`). There is no `%u`.
- **A `real`'s digits are the exact value's**, rounded half to even at the position the conversion
  asks for (`${0.125}%.2f` is `0.12`, `${2.5}%.0f` is `2`); an infinity is `inf` and a NaN `nan`,
  upper case under `E` and `G`.
- **Flags**: `-` pads on the right instead of the left, under every conversion. The others belong to
  the numbers: `+` writes a sign before a value that is not negative, a space writes a space there,
  `0` pads with zeros after the sign and any prefix, and `#` asks for the alternate form: `0x`, `0X`
  or `0b` before a non-zero integer, a leading `0` in octal, and a decimal point that stays in a
  `real`, with `g`'s trailing zeros. `0` gives way to `-`, and for integers to a precision. `#` does nothing under `d` and `i`.
- **`,` groups the digits** of an integer under `d` and `i`, three at a time from the right with a
  comma between groups, for every width of integer and for `byte`: `${1234567}%,d` is `1,234,567`,
  `${-2147483648}%,d` is `-2,147,483,648`, and a value under a thousand has no comma. The sign comes
  before the first group, and a width and `-` pad the grouped text with spaces
  (`${1234567}%,12d` is `   1,234,567`). The zeros a precision asks for are grouped with the
  digits (`${5}%,.6d` is `000,005`). The zeros `0` fills a width with are not: they come before the
  grouped digits, and the commas and the sign count in the width, so the field is exactly as wide as
  asked unless the number is wider (`${1234}%0,8d` is `0001,234`, `${-1234}%0,8d` is `-001,234`).
  Under any other conversion the flag has no effect, which is an error.
- **The width** is the least number of characters the field takes; a value wider is written whole.
- **The precision** is, for an integer, the least number of digits, so a precision of 0 writes zero as
  no digits; for `f`, `e` and `E` the number of decimals, and for `g` and `G` of significant digits;
  for `s` and `?` the most characters of the value's text that are written, whatever its type:
  the value is rendered whole and its first N characters, counted as code points, fill the field,
  so `${[1, 2, 3]}%.3s` is `[1,`, `${"abcdef"}%.2?` is `"a` and `${42}%.1?` is `4`. `%c` takes none.
- **A width or precision may be a hole of its own**, counted when the program runs, written where the
  digits would go: `${x}%.${n}f`, `${s}%-${w}s`, `${k}%0${n + 3}d`. The count is an integer of any
  width. A negative width pads on the right at its magnitude, a negative precision is as though none
  were written, and a count beyond `int` is the largest `int`. Flags written before a counted width
  still apply.
- Each hole, a count's included, is evaluated once, in the order written.

**A specifier is checked while compiling**, against the type of the hole it follows: the conversion
is one that type takes, each flag has an effect under it, and a width or precision written as a hole
is an integer.

```text
val n = 7
println(f"${n}%s")       // format `%s` expects a string or another value that is not a number, but the value has type `int`
println(f"${n}%#d")      // the flag `#` has no effect under `%d`
println(f"${n}%,x")      // the flag `,`, which groups decimal digits, has no effect under `%x`
println(f"${n}%u")       // `%u` is not a format conversion: integers are signed, so `%d` writes every one
println(f"${n}%")        // this format specifier has no conversion: …
println(f"${2.5}%.${2.5}f")  // a width or precision written as a hole is a count, so it has to be an integer, …
```

A value of a type that renders itself is handed what the specifier says in its `FormatSpec`: the
width, the precision, the flags and the conversion letter, the counts of holes included.

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
  directly, as `StringContext(parts).s(x)`. Called directly, `f` reads each specifier from the start
  of the part after its hole as it runs, a counted width or precision being an `int` or `long`
  argument of its own; there a malformed specifier, a count of another type and `%?` (its arguments
  are `Display` values) trap, and a value under a conversion its type does not take renders in its
  own form.

A program adds its own interpolator by giving `StringContext` a method through a trait it declares
and an `impl` of that trait. The interpolator is available wherever it is imported: in the module that
declares it, in a file that imports that module (`import sql`), and in a file that imports one of its
declarations by name (`import sql.{Query}`). Importing a module that itself imports `sql` does not
bring `sql"…"` into scope, and `s`, `f` and `raw` need no import. Using an interpolator that is not
in scope is an error that names the module to import:

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
types display as their literals read: an integer of any width as its decimal digits with a sign when
negative, a `byte` as its number, a `char` as itself, a `real` as the fewest digits that read back
as the same value, written out without an exponent (`2.5`, `1`, `0.1`), a `bool` as `true` or
`false`, `()` as `()`, and a string as itself. A value whose type
implements neither is a compile error. There is no fallback that prints an address. The specifier
applies to the whole rendered value.

A value whose static type is `Any` or a trait is shown by the type it has when the program runs.
`Any` and every trait meet `Display` and `Debug`, as every type meets `Eq`: such a value meets a
`Display` or `Debug` bound, has `display`, `debug` and `toString()`, and may be interpolated, whatever
the trait itself declares. It does not convert to `Display` or `Debug` as a value: a value of either
trait holds only objects whose types implement it. It renders through its own type's implementation, so a `Seq[int]` holding
a vector prints `[1, 2, 3]`, as the vector would, and a `Vector[Seq[int]]` prints
`[[1, 2, 3], [4]]`. A type with no `Display` (or no `Debug`) of its own renders as its name, `Cat` or
`Box[int]`, and a function value as `<function>`, in the field the spec describes. The rule is for
values whose type is decided at run time only: a value whose static type is a class or a data type
still needs that type's own `Display` to be interpolated, and a type parameter its bound's (`print`
and `println` take their arguments as `Any`, so they show such a value by its name). No impl gives a trait a
`Display` of its own; its values are always shown by their own types.

```avin
trait Pet
  name() -> string

class Cat(): Pet
  override name() -> string = "cat"

main()
  val xs: Seq[int] = [1, 2, 3]
  val pet: Pet = Cat()
  val anything: Any = (1, "two")
  println(s"$xs $pet $anything") // [1, 2, 3] Cat (1, two)
```

The core library declares the traits:

```text
trait Writer
  write(text: string)
  writeChar(c: char) = …
  writeInt(n: long) = …
  writeField(text: string, spec: FormatSpec) = …
  writeSpaces(n: long) = …

data FormatSpec(
    width: int = 0,
    precision: int = -1,
    left: bool = false,
    zeroPad: bool = false,
    plus: bool = false,
    space: bool = false,
    alternate: bool = false,
    conversion: char = 's',
    grouping: bool = false
)
  withWidth[T: Integer](n: T) -> FormatSpec = …
  withPrecision[T: Integer](n: T) -> FormatSpec = …

trait Display
  display(out: Writer, spec: FormatSpec)
  toString() -> string = …

trait Debug
  debug(out: Writer, spec: FormatSpec)

writeDisplayed[T: Display](out: Writer, value: T, spec: FormatSpec)
writeDebugged[T: Debug](out: Writer, value: T, spec: FormatSpec)
```

- `FormatSpec` is the field a value fills: `width` characters, padded with spaces on the left, or on
  the right when `left`; 0 asks for no padding, and wider text is written whole. `precision`, -1 for
  none, is read by a renderer that takes one, in its own terms, under every conversion but `s` and
  `?`. Under those two the precision is the formatter's: `writeDisplayed(out, value, spec)` and
  `writeDebugged(out, value, spec)`, which a formatted hole is written through, render the value
  with no width and no precision, keep its first `precision` characters and write them in the
  field, so a renderer never cuts its own text. Under those two the width is the formatter's as well:
  with no precision, the value is rendered with the whole spec, and whatever it writes is padded to
  the field, so a `display` that ignores its spec, or writes no text at all, still fills `%8s`, and
  one that fills the field itself is not padded twice. `zeroPad`, `plus`, `space` and
  `alternate` are the flags `0`, `+`, space and `#`, `conversion` the letter of the specifier,
  `s` when none was written, and `grouping` the flag `,`, which the integers read under `d`, `i`
  and `s`; a renderer reads the conversions its type takes and renders its own
  form under any other. `withWidth` and `withPrecision` set a count as a specifier's hole does.
  `writeField` writes a text in the field a spec describes, padded with spaces, which is how most
  `display` methods end.
- `toString()` renders into a `StringWriter`, a writer building a string, and returns its `text`.
  `StandardOutput` is the writer for standard output; it writes an integer's digits without making
  a string of them. A `StringWriter` also answers `length`, the number of bytes written so far (a
  character of several bytes counts them all), and `isEmpty`, and `clear()` forgets everything
  written so it is as new; a text already taken is unchanged. A `StringWriter` is itself `Display`:
  it shows, interpolates and answers `toString()` as its `text`, padded to a field's width as a
  string is. `StandardError` is the writer for standard error, which keeps no buffer.
- `print` and `println` are each one ordinary library function, `print(values: Any*)` and
  `println(values: Any*)`. `print` writes its arguments to standard output separated by `, ` (they
  are not a tuple), and `println` writes them and a new line: `println("total", 3, (1, 2))` writes
  `total, 3, (1, 2)`, `println(x)` writes just `x`, `print()` writes nothing and `println()` writes
  just a new line. Each argument is an `Any`, shown as that `Any` is shown in an interpolated string:
  by its type's own `Display`, or by the type's name for a type with none, so `println(pet)` with
  `pet: Pet`, `println(xs)` with `xs: Seq[int]` and `println(Cat())` for a class with no `Display` all
  print, the last as `Cat`, and a function value as `<function>`; `println(1, pet, xs)` writes
  `1, Cat, [1, 2, 3]`. A sequence is one argument: `println(xs)` prints it whole, and only `xs*`
  spreads it into the repeated parameter, where it has to be a sequence of `Any` or of references
  (`words*` with `words: Seq[string]`), since a number is never boxed inside a container. As a
  value, `println` is a `(Vector[Any]) -> unit`. Printing costs what any `Any` parameter does: a
  number, `char`, `bool` or `unit` argument is boxed, and a program that prints gives every type
  record its rendering entry. `eprint` and `eprintln` are the same for standard error, each a single
  write, so a line is never split by another writer's. `debug(x)` is `x`'s
  `Debug` form as a string, so `println(debug(x))` prints it. Inside an implementation of `Debug`, the
  bare name `debug` is the method, and a part is written with `part.debug(out, spec)`.
- The built-in types' `Debug` forms are their `Display` forms, except that a string and a `char`
  are written as the literals that make them: quoted, with the quote, the backslash and control
  characters escaped (`"a\"b\n"`, `'\''`).

## Lowering

For `s` and `f`, the compiler does not call the method. It parses the specifiers while compiling and
writes the literal out to calls on one `StringWriter` made for it, with the same result: each decoded
piece of text is written to it, each hole is rendered into it by `display`, or by `debug` under
`%?`, with the `FormatSpec` its specifier describes, a hole under `%s` or `%?` with a precision, or
with a width when its type is not a built-in one, through `writeDisplayed` or `writeDebugged`, and
the writer's text is the value. A string hole
with no specifier is written as it is. No value is boxed to format it. Every other interpolator is
compiled as the method call it denotes.
