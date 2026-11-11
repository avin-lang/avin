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

data JsonError(message: string, line: long, column: long, offset: long)
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
characters rather than bytes; `offset` is the byte offset in the text, counted from 0. All three are
`long`s, so a [stream](#reading-a-stream) of many gibibytes, on one line or many, is placed exactly.
It displays as its message and its place:

```text
unexpected ']' after ',' in an array at line 1, column 4
a high surrogate \uD800 with no low surrogate after it at line 3, column 12
a number with a leading zero at line 1, column 2
arrays and objects nested more than 512 deep at line 1, column 513
```

A string with no escape is one slice of the text, and the reading goes byte by byte with no
backtracking: a mebibyte document is read in tens of milliseconds.

## Reading a stream

A `JsonReader` reads JSON text as it arrives, from any [`Reader`](/spec/system-library/#reader) (an
open file, a TCP or Unix connection, or a source the program writes), and gives it back one event
at a time without building the whole value. It holds the piece of text it is working through
(64 KiB at most from each read), the token it is reading and one entry for each array or object it
is inside, and never calls itself per level, so a document larger than memory, or nested as deep as
its limit allows, reads in a bounded amount of it: a hundred megabytes of records read in about
twelve mebibytes of resident memory, the program included.

```avin
import json
import json.JsonReader
import avin.fs

// The sum of every record's "bytes", read from a log far larger than memory.
totalBytes(path: string) -> Result[long, json.JsonError]
    val r = JsonReader.fromReader(fs.open(path).ok().get())
    var sum: long = 0
    r.beginArray()?
    while r.nextElement()?
        r.beginObject()?
        loop
            r.nextKey()? match
                None -> break
                Some("bytes") -> sum += r.readLong()?
                Some(_) -> r.skipValue()?
    r.finish()?
    Ok(sum)
```

```text
data JsonEvent
    StartObject
    EndObject
    StartArray
    EndArray
    MemberName(name: string)
    StringValue(value: string)
    LongValue(value: long)
    RealValue(value: real)
    BoolValue(value: bool)
    NullValue
    EndOfText
```

| Made by | Reads |
| --- | --- |
| `JsonReader.fromReader(source: Reader, maxDepth = defaultMaxDepth)` | The text `source` gives, a piece at a time as events are asked for |
| `JsonReader.fromString(text, maxDepth = defaultMaxDepth)` | `text` |
| `JsonReader.fromBytes(bytes, maxDepth = defaultMaxDepth)` | Text given as its UTF-8 bytes |

| Call | Gives |
| --- | --- |
| `next()` | The next event, moving past it |
| `peek()` | The next event, which `next()` then gives again |
| `depth()` | How many arrays and objects the reader is inside |
| `beginArray()`, `beginObject()` | Moves into the array or object that must come next |
| `nextElement()` | Whether the array has another element; at its end, moves past it and answers `false` |
| `nextKey()` | The next member's name, or `None` at the object's end, moving past it |
| `readString()`, `readBool()` | The next value, which must be of that kind |
| `readLong()`, `readInt()`, `readReal()` | The next value, a number, taken as `asLong`, `asInt` and `asReal` take one |
| `takeNull()` | Whether the next value is `null`, moving past it when it is; anything else is left for the next call |
| `skipValue()` | Moves past the next value, whatever it holds |
| `readValue()` | The next value whole, as a `Json` |
| `readDocument()` | The text's one value whole, and the end of the text after it |
| `finish()` | The end of the text, with only white space after the value |
| `error(message)` | A `JsonError` saying `message`, placed where the last event given or peeked began |

Each call but `depth`, `takeNull` and `error` answers a `Result`, so a decoder chains them with `?`.

- **The grammar is `parse`'s, and so are its faults**: every text `parse` refuses, a stream refuses
  with the same message, line, column and offset, however its source cuts it into pieces. A
  character split between two reads, a surrogate pair's two escapes split anywhere, and a number or
  a word cut in two are read whole. UTF-8 is checked as it is read, so bytes that are not UTF-8 are
  an error at the first of them, as in `parse`; where a grammar fault comes before them in the text,
  the stream, which has not read them yet, reports the grammar fault.
- **One value, then the end of the text.** After the value the reader gives `EndOfText`, and gives
  it again on every later call; anything but white space after the value is the fault `unexpected
  'x' after the value`.
- **A number is a `LongValue` or a `RealValue` by `parse`'s rule**: a whole number that fits a
  `long` is a `LongValue`, any other a `RealValue`. `readInt` and `readLong` take a whole `RealValue`
  within range (`3.0`, `1e3`) as `asInt` and `asLong` do, and refuse one that is not, naming it.
- **A value of the wrong kind** is an error at the event that was found, naming what was expected
  and what was found: `expected a string, found a number at line 2, column 9`.
- **A fault ends the reading.** The first fault in the text, or a failure of the source to read
  (`the text could not be read: timed out`, placed where the reading stopped), is the `Err` of the
  call that met it and of every call after it. A token the failure cut short is not given.
- **Memory is bounded by the largest token**: a string or a number is held whole while it is read,
  and nothing else of the text is kept once its event is given.

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
it belongs to this package and reads and writes the grammar this chapter gives. The `JsonReader` a
decoder reads is the [streaming reader](#reading-a-stream) above.
