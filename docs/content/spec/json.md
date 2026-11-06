---
title: JSON
summary: "The json package — `Json` values, reading text with the line and column of any error, writing compact, indented or ASCII text, and the accessors that take a value apart."
weight: 29
---

A program reads and writes JSON through the **`json` package**, written in Avin. It lives in this
repository at `packages/json`, its module is `json`, and a program names it as a dependency, by path
from a checkout or by the repository's coordinate and the package's directory:

```hocon
dependencies {
  json { path = "../avin/packages/json" }
}
```

```avin
import json
import json.Json

main()
    val event = json.parse("{\"id\": 7, \"cmd\": \"make\", \"tags\": [\"a\", \"b\"]}") match
        Ok(j) -> j
        Err(e) -> fail(s"bad event: $e")
    println(event("cmd").asString())                    // Some(make)
    println(event.get("exit").flatMap(x -> x.asInt()))  // None
    println(event.with("exit", Json.num(0)))            // {"id":7,"cmd":"make","tags":["a","b"],"exit":0}
```

## Values

```text
data Json
    Null
    Bool(value: bool)
    Long(value: long)
    Real(value: real)
    Str(value: string)
    Arr(items: Vector[Json])
    Obj(members: Vector[(string, Json)])
```

- **A number is a `Long` when the text writes a whole number that fits a `long`**: digits with an
  optional `-` and neither a fraction nor an exponent, from `-9223372036854775808` to
  `9223372036854775807`. An identifier or a count is therefore exact however large. `-0` is the
  `Long` 0.
- **Every other number is a `Real`**: one with a fraction or an exponent (`1.0`, `1e3`, `-0.0`, which
  keeps its sign), and a whole number beyond a `long`, rounded to the nearest `real`. A number too
  large to be a finite `real` (`1e400`) is an error; one too small is 0 with its sign.
- **An object keeps every member in the order written**, a key written twice included, so a
  document read and written again comes back in its own order. `get` and `apply` answer the last
  member of a key written more than once.
- **Values compare by structure**: an object's members in order, so `{"a":1,"b":2}` and
  `{"b":2,"a":1}` are different values, and a `Long` is never equal to a `Real`.
- **A value displays as its compact text**, by `Display` and by `Debug`, so `println(j)` and
  `s"$j"` write it as JSON.

The companion makes values without spelling the variants: `Json.obj(("id", Json.num(7)))` is an
object of the members given, `Json.arr(items*)` an array, `Json.str(s)` a string and `Json.num(n)`
a `Long` for an `int` or a `long` and a `Real` for a `real`.

## Reading

```text
parse(text: string, maxDepth: int = defaultMaxDepth) -> Result[Json, JsonError]
parse(bytes: Array[byte], maxDepth: int = defaultMaxDepth) -> Result[Json, JsonError]

data JsonError(message: string, line: int, column: int, offset: int)
val defaultMaxDepth: int = 512
```

`parse` reads one value with nothing but white space around it, and answers the first thing wrong
with the text otherwise. The grammar is JSON's own and nothing more:

- **White space is space, tab, line feed and carriage return.** A form feed, a byte order mark or
  a no-break space is not white space, and there are no comments.
- **No trailing comma**, in an array or an object; an object's key is a string, followed by `:`.
- **A number has no leading zero, `+`, bare `.` or hexadecimal form**, and `NaN` and `Infinity` are
  not numbers. The words are `true`, `false` and `null`, whole and in lower case.
- **A string holds no control character below U+0020 unescaped.** Its escapes are `\"`, `\\`, `\/`,
  `\b`, `\f`, `\n`, `\r`, `\t` and `\u` with four hexadecimal digits in either case. A character
  beyond U+FFFF is a high surrogate escape followed at once by a low one, which read as the one
  character; a lone surrogate, high or low, is an error, since a `string` holds only characters.
  `\u0000` is the character U+0000.
- **Arrays and objects nest at most `maxDepth` deep**, so a hostile document cannot exhaust the
  stack: `[[1]]` is two deep, and a number, string or word alone is 0 deep. The limit is 512 unless
  a call gives another; the error is at the bracket that goes one deeper.
- **Bytes are read as UTF-8**; bytes that are not UTF-8 are an error at the first of them.

An error's `line` and `column` count from 1, a line ending at a line feed and a column counting
characters rather than bytes; `offset` is the byte offset in the text, counted from 0. It displays
as its message and its place:

```text
unexpected ']' after ',' in an array at line 1, column 4
a high surrogate \uD800 with no low surrogate after it at line 3, column 12
a number with a leading zero at line 1, column 2
arrays and objects nested more than 512 deep at line 1, column 513
```

A string with no escape is one slice of the text, and the reading goes byte by byte with no
backtracking: a mebibyte document is read in tens of milliseconds.

## Writing

- `render()` is the compact text, with no white space: `{"a":[1,2]}`.
- `pretty(indent = 2)` puts each element and member on its own line, `indent` spaces deeper than its
  container, with a space after each member's colon; an empty array or object stays `[]` or `{}`.
  `pretty(0)` is `render()`.
- `renderAscii()` is the compact text with every character beyond ASCII written as a `\u` escape,
  one beyond U+FFFF as its surrogate pair.
- `writeTo(out: Writer)` writes the compact text to a writer, and `quote(s, ascii = false)` is one
  string as a JSON string literal.

A string is written with `"`, `\` and the control characters below U+0020 escaped, as `\b`, `\f`,
`\n`, `\r` and `\t` where JSON names them and as `\u00XX` otherwise; `/` and every other character
are written as they are. A `Long` is written as its digits. A `Real` is written as the fewest
digits that read back as the same value, and always with a fraction or an exponent, so it reads back
as a `Real`: `2.5`, `1.0`, `-0.0`, `100000000000000000000.0`. From 1e-6 up to 1e21 it is written out
in full, and otherwise with an exponent: `1e+21`, `1.5e-7`, `5e-324`. A NaN or an infinity, which
JSON cannot write, is written `null`. So `parse(j.render())` is `j` for every value with no NaN or
infinity in it, as are `pretty` and `renderAscii` read back, and the text read is the text rendered
for any document already written compactly.

## Taking a value apart

| To | Write |
| --- | --- |
| A member, or None | `j.get("key")`, the last member where the key is written twice |
| An element, or None | `j.get(i)`, None outside the array |
| A member or element that must be there | `j("key")`, `j(i)`, which trap naming the key or index when it is not |
| A field's value of a kind | `asBool()`, `asString()`, `asVector()`, `asMembers()`, each an `Option` |
| A number | `asLong()`: a `Long`, or a `Real` that is a whole number within a `long` (`3.0`, `1e3`); `asInt()` where that fits an `int`; `asReal()`: a `Real`, or a `Long` rounded to the nearest `real` |
| Whether it is null, or has a member | `isNull()`, `has("key")` |
| An object's keys, the size of an array or object | `keys()`, `size()` (0 for any other value) |
| An object with a member set | `j.with("key", value)`: the member replaced in place, or added at the end; any other value is returned unchanged |

The accessors are Options so that they chain with `?` in a function answering an `Option`:

```avin
exitCode(event: Json) -> Option[int] = event.get("result")?.get("exit")?.asInt()
```

## A program's own types

Encoding and decoding a program's own types, `derives JsonEncoder` and `derives JsonDecoder` with a
`JsonWriter` and a `JsonReader` that stream, is designed in [derivation](/spec/derivation/#a-json-library);
it belongs to this package and reads and writes the grammar this chapter gives.
