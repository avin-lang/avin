---
title: Types
summary: Built-in types, numbers, strings, unit, tuples, functions, and the top and bottom types.
weight: 12
---

## Built-in types

Built-in type names are lowercase and belong to the language itself: they have no module path and
cannot be shadowed by an import. Their methods are defined in the core standard library with
ordinary `impl` blocks.

| Type | Meaning |
| --- | --- |
| `int` | Alias of `i32` |
| `long` | Alias of `i64`; `int` and `long` are the only integer aliases |
| `iN` | Signed integer of exactly N bits, 1 ≤ N ≤ LLVM's limit (`i8`, `i64`, `i128`, `i257`, …) |
| `byte` | An octet, 0–255: the unit of binary data |
| `real` | IEEE-754 binary64. There is no single-precision type |
| `char` | A Unicode scalar value (U+0000–U+10FFFF, excluding surrogates) |
| `bool` | `true` or `false` |
| `string` | Immutable UTF-8 text |
| `unit` | One value, written `()` |

All other types — user types, library types, collections — are PascalCase by convention.

## Integers

Arithmetic keeps the operand width: `i8 + i8` is `i8`. Mixed widths need an explicit conversion
(`long(x)`); there is no implicit widening. Diagnostics name `i32` and `i64` by their aliases, and
an operation or comparison on two number types names the conversion that joins them, pointing at
the operand to convert: ``error: `*` needs two numbers of one type, not long and int; write
`long(n)` `` (an integer converts to the wider integer or to `real`, a byte to the other side's
type; a compound assignment's target keeps its type, so there the value converts to it). A literal takes its width from context (an annotation,
a parameter, the other operand, the other branches of an `if` or elements of a collection literal) and defaults to `int`; a literal that does not fit is a compile error.
Where a `real` is expected, an integer literal is that real (`x * 2`), provided it is exact: its
magnitude is at most 2^53.

Conversions: widening sign-extends; narrowing traps when the value does not fit the narrower width
(`i8(300)` traps, `i8(127)` is `127`), and `i8.wrap(300)` is the explicit wrapping form, which keeps the low
bits (`44`); real-to-integer
truncates toward zero, saturates at the bounds and maps NaN to zero; `char(n)` traps on a value that
is not a Unicode scalar. Conversions are written as a call of the target type, and these exist: between
any two integer types; integers and `real` in both directions; an integer or `byte` to `byte` and
`char`; `byte` and `char` to any integer type; `byte` to `real`. A conversion to the same type is
allowed and does nothing.

**Limits.** Every integer type has its least and greatest value as constants, `MinValue` and `MaxValue`:
`int.MaxValue`, `long.MinValue`, `i8.MaxValue`, `i1.MinValue`, `i257.MaxValue`, and `byte.MaxValue` (255)
and `byte.MinValue` (0). `char.MaxValue` is U+10FFFF and `char.MinValue` U+0000. `real` has `MaxValue`
(the greatest finite value), `MinValue` (the least, its negation), `MinPositiveValue` (the least positive
value, subnormal), `PositiveInfinity`, `NegativeInfinity` and `NaN`. They are written on the type, are
folded at compile time, and have the type they are written on; a type parameter bounded by `Integer`
has `MinValue` and `MaxValue` too, folded at each instance's width ([generics](/spec/generics/)). Any other name written on one of these
types is an error.

**Arithmetic that overflows traps.** `+`, `-`, `*`, unary `-`, `++` and `--` trap when the
mathematical result does not fit the width, and so does `/` for the one quotient that cannot fit
(`MIN / -1`). `%` never overflows: `MIN % -1` is `0`. Division and remainder by zero trap. Code that
wants other behaviour says so with an operator: `+%`, `-%` and `*%` wrap, and `+|`, `-|` and `*|`
saturate at the bounds. Shifts never trap; a count at least as large as the width, or a negative
count, shifts every bit out (`>>` keeps the sign, so `-1 >> 40` is `-1`).

Integers are signed; there are no unsigned integer types. Binary data uses `byte`.

The integer types are one family, `iN` for every width from `i1` to `i8388608`, and the closed core
trait `Integer` has exactly them as members (`byte` is not one). The core library implements its
traits once for the whole family, so every width compares, orders, prints in decimal and debugs
alike, and meets a bound such as `Ord` or `Display` as `int` does; a parameter bounded by `Integer`
has every integer operation and conversion ([Bounds](generics.md#bounds)). The integers and `real`
together are the numbers, the members of the core trait `Numeric`, which `Integer` requires: a
parameter bounded by it has the arithmetic, the ordering and `toReal`. A value that fits in a
`long` renders through it, at no cost for widths up to 64 bits; a wider one renders in chunks of
eighteen digits.

**Integer methods.** Every integer type has these methods, from one impl for the whole family, each
working at the width it is called at (an `i8` stays an `i8`) and trapping, as arithmetic does, when
a result does not fit:

| Method | Result |
| --- | --- |
| `abs` | The magnitude; traps for `MinValue`, whose magnitude does not fit |
| `sign` | `-1`, `0` or `1`, as an `int` at every width |
| `pow(n)` | The number to the power `n`, an `int`; `x.pow(0)` is `1`, `0.pow(0)` included. Traps when the result does not fit and when `n` is negative |
| `gcd(o)` | The greatest common divisor, never negative; `0.gcd(0)` is `0`. Traps only when it is `MinValue`'s magnitude, as in `MinValue.gcd(0)` |
| `lcm(o)` | The least positive common multiple, `0` when either is `0`; traps when it does not fit |
| `floorDiv(o)`, `floorMod(o)` | The quotient rounded towards negative infinity, and the remainder that takes the divisor's sign, so `a == a.floorDiv(d) * d + a.floorMod(d)`. They trap where `/` and `%` do: a zero divisor, and `MinValue.floorDiv(-1)` |
| `bitCount` | The number of one bits of the two's-complement form at the width: `(-1).bitCount` is `32` for an `int` |
| `leadingZeros`, `trailingZeros` | The zero bits above the highest one and below the lowest one, an `int`; the width for zero. `leadingZeros` is `0` for a negative number |
| `isPowerOfTwo` | Whether the number is `1`, `2`, `4`, and so on; never for zero or a negative number |
| `toRadix(r)` | The digits in radix `r` from 2 to 36, lower case, with a `-` for a negative number and no prefix, as `f"%x"` and `f"%b"` write them; any other radix traps |
| `toInt`, `toLong` | The number as an `int` or a `long`, exactly `int(n)` and `long(n)`: trapping when it does not fit |
| `toReal` | The number as a `real`, exactly `real(n)`; it is `Numeric`'s, so generic code has it too |

The bitwise operators `&`, `|`, `^` and the shifts also work on a type parameter bounded by
`Integer`, as the machine's operators at each width, with both operands of one type: `x >> T(1)`.
The methods above are not members of `Integer`, so generic code cannot call them on a `T`.

## byte

`byte` is an octet with a value from 0 to 255. It is the element of binary data — file contents,
network buffers, encoded text — rather than a small number, so it has equality, ordering, the
bitwise operators `&`, `|`, `^` and `~`, and shifts, but no arithmetic. `int(b)` gives its value
(0–255), and `byte(n)` makes one from an integer, trapping when `n` is outside 0–255. An integer
literal in a `byte` context must be in 0–255: `val mask: byte = 0xf0`.

## real and char

`real` arithmetic follows IEEE 754, including NaN and signed zero; comparisons with NaN are false
except `!=`. `x.isNaN`, `x.isInfinite` (either infinity) and `x.isFinite` (neither) classify a
value, and each value is exactly one of NaN, infinite or finite. `x.toInt` and `x.toLong` are
`int(x)` and `long(x)`, truncating toward zero, saturating at the bounds and mapping NaN to zero;
`x.toReal` is `x`. There is no implicit fused multiply-add. The square root, logarithms, trigonometry and
the other elementary functions are in the module [`avin.math`](/spec/system-library/#avin-math):
`math.sqrt(x)`. `char` supports ordering and equality but no
arithmetic; `int(c)` and `char(n)` convert.

A `char` has these methods, classifying by the Unicode Character Database:

| Method | Meaning |
|---|---|
| `isDigit` | A decimal digit of any script (category `Nd`) |
| `isLetter` | A letter of any script (categories `Lu`, `Ll`, `Lt`, `Lm`, `Lo`) |
| `isLetterOrDigit` | `isLetter` or `isDigit` |
| `isWhitespace` | A space separator other than the non-breaking ones (U+00A0, U+2007, U+202F), or U+0009 to U+000D, or U+001C to U+001F; this is not `strip`'s test, which is the `White_Space` property and so counts U+00A0 |
| `isSpaceChar` | A character of category `Zs`, `Zl` or `Zp` |
| `isUpper`, `isLower` | Cased and upper, cased and lower |
| `isControl` | U+0000 to U+001F and U+007F to U+009F |
| `toUpper`, `toLower` | The simple case mapping, one character for one: `'ß'.toUpper` is `'ß'` |
| `asDigit` | The digit's value in radix 36: a decimal digit of any script gives its value, an ASCII or fullwidth letter, either case, gives 10 to 35; any other character gives -1 |

## string

`string` is a built-in value type with no identity: two equal strings are interchangeable, and
`===` is not defined on them. It is immutable UTF-8.

`string` is a built-in type, not a class. It is final; its representation is its own, a length and its
bytes inline in one object; it gets its methods from the library through `impl string`; and it
conforms to traits through `impl` (`Eq`, `Hash`, `Ord`, `Display`). A type pattern `s: string` on an
`Any` matches it like any other type.

- `s.byteLength` is O(1). Iteration is by `char` (`s.chars`) or by byte (`s.bytes`).
- There is no `s(i)` returning a char by index, because that would be O(n) behind O(1) syntax.
  Slicing takes byte offsets and traps on an offset that is not a character boundary.
- `+` concatenates two strings and nothing else. Other values reach text through interpolation
  (`s"${x}"`) or `x.toString()`, both of which use `Display`.
- Ordering is by code point, which for UTF-8 is byte order.

The library's methods on `string`. Offsets and lengths count bytes and are `int`; a string holds at
most the largest `int` bytes.

| Method | Result |
| --- | --- |
| `byteLength` | The number of bytes, O(1) |
| `charCount` | The number of characters, O(n) |
| `isEmpty`, `nonEmpty` | Whether it has no bytes, or some |
| `startsWith(p)`, `endsWith(p)`, `contains(p)` | Whether `p` occurs at the start, the end, anywhere; the empty string occurs in every string |
| `startsWith(p, at)` | Whether `p` occurs at byte offset `at`; false when it would reach outside the string |
| `byteAt(at)`, `charAt(at)` | The byte at offset `at`, and the character whose encoding begins there; each traps when `at` is outside the string |
| `slice(from, until)` | The bytes from `from` up to `until`; traps unless `0 <= from <= until <= byteLength()` and both offsets are character boundaries |
| `toUpperCase`, `toLowerCase` | Each character's simple case mapping, one character for one: `"straße".toUpperCase` is `STRAßE` |
| `strip`, `stripLeading`, `stripTrailing` | Without the Unicode `White_Space` characters at both ends, the start, the end |
| `trim` | Without the characters up to U+0020, the ASCII controls and the space, at both ends |
| `repeat(n)` | `n` copies joined; empty for zero; traps when `n` is negative |
| `replace(old, new)` | Every occurrence of `old`, left to right and not overlapping, replaced by `new`; an empty `old` occurs nowhere |
| `indexOf(p)`, `indexOf(p, from)` | The byte offset of the first `p`, a string or a `char`, at or after `from` (default 0; below 0 counts as 0); -1 when there is none or `from` is past the end. The empty string is found at `from` |
| `lastIndexOf(p)`, `lastIndexOf(p, end)` | The byte offset of the last `p`, a string or a `char`, beginning at or before `end` (default `byteLength`); -1 when there is none. The empty string is found at `end`, or at `byteLength` when `end` is past it |
| `indexWhere(f)`, `indexWhere(f, from)` | The byte offset of the first character satisfying `f`, at or after `from`; -1 when none does |
| `lastIndexWhere(f)`, `lastIndexWhere(f, end)` | The byte offset of the last character satisfying `f` that begins at or before `end` (default: the whole string); -1 when none does or `end` is below 0 |
| `head`, `last` | The first and the last character; each traps on the empty string |
| `headOption`, `lastOption` | `Some` of the first or the last character; `None` for the empty string |
| `stripPrefix(p)`, `stripSuffix(s)` | Without `p` at the start, without `s` at the end; the string unchanged where it is not there |
| `capitalize` | With a lower-case first character mapped to upper case, and unchanged otherwise |
| `stripMargin()`, `stripMargin(c)` | Each line with its leading characters up to U+0020 and the margin character after them (a vertical bar, or `c`) removed; a line whose first other character is not the margin is unchanged, and the line terminators stay |
| `chars` | The characters, in order, as a `Vector[char]` |
| `bytes` | The bytes of the encoding, in order, as a `Vector[byte]` |
| `utf8` | The bytes of the encoding in a new `Array[byte]`, `byteLength` of them, which the caller owns: each call makes another, so writing to one changes neither the string nor any other. It is what a file, a socket or a request body is written from, and `string.fromUtf8(s.utf8)` is `Ok(s)` |
| `count(p)` | The number of characters satisfying `p` |
| `split(sep)` | The pieces between the occurrences of `sep`, as a `Vector[string]`; empty pieces are kept (`",a,,b,".split(",")` has five pieces), and a string without `sep` is one piece. An empty `sep` traps |
| `lines` | The lines as a `Vector[string]`, each ended by `\n` or `\r\n` and shown without it; a terminator at the very end starts no further line, so `"a\nb\n".lines` is two lines and `""` has none. A `\r` not before `\n` stays in its line |
| `isBlank` | Whether it has only `White_Space` characters, so the empty string is blank |
| `reverse` | The characters in the opposite order |
| `padStart(w, c)`, `padEnd(w, c)` | At least `w` characters, `c` (default `' '`) repeated before, after; `w` counts characters, not bytes, and a string already that long is unchanged |
| `padTo(n, c)` | As `padEnd(n, c)`, the name a sequence's padding has |
| `equalsIgnoreCase(o)` | Whether the two are equal after each character's simple lower-case mapping |
| `take(n)`, `drop(n)` | The first `n` characters, all but the first `n`; `n` counts characters, not bytes, and is clamped to the string, so 0 or less takes none |
| `takeRight(n)`, `dropRight(n)` | The last `n` characters, all but the last `n` |
| `toInt()`, `toLong()` | The number written in decimal: an optional `-` or `+`, then one or more ASCII digits and nothing else; a trap, `For input string: "…"`, otherwise or when it does not fit the type |
| `toReal()` | The number written as a decimal fraction, correctly rounded: an optional sign, digits with an optional point, an optional exponent (`3.25`, `-.5`, `1e10`); a trap, as `toInt`'s, for anything else, including white space, `nan`, `inf`, hexadecimal, and a number too large to be finite |
| `toIntOption()`, `toLongOption()`, `toRealOption()` | `Some` of the number `toInt`, `toLong` or `toReal` reads, and `None` where it would trap |
| `toInt(radix)`, `toLong(radix)` | As `toInt()` and `toLong()` with digits of base `radix`, 2 to 36: `0`–`9`, then `a`–`z` in either case; the same trap for text that is not such a number or does not fit, and a trap for a `radix` outside 2 to 36 |
| `toIntOption(radix)`, `toLongOption(radix)` | `Some` of the number the radix form reads, and `None` where its text would trap; a `radix` outside 2 to 36 still traps, being the caller's mistake and not the text's |
| `toBool()`, `toBoolOption()` | `true` for the text `true`, `false` for `false`, exactly (no other case, no white space); `toBool` traps, as `toInt` does, on any other text and `toBoolOption` answers `None` |
| `map(f)`, `flatMap(f)` | A string of `f` applied to each character: `f` answers a `char` for `map` and a `string` for `flatMap`, and the results are joined in order |
| `filter(p)`, `filterNot(p)` | The characters that do, or do not, satisfy `p`, in order |
| `foreach(f)` | Calls `f` on each character, in order |
| `exists(p)`, `forall(p)` | Whether some, or every, character satisfies `p`; false and true for the empty string |
| `find(p)` | `Some` of the first character satisfying `p`, or `None` |
| `takeWhile(p)`, `dropWhile(p)` | The longest prefix whose characters all satisfy `p`, and the string without it |
| `span(p)`, `partition(p)` | A pair of strings: the longest prefix satisfying `p` and the rest; the characters that satisfy `p` and those that do not |
| `splitAt(n)` | The pair of the first `n` characters and the rest; `n` is clamped as `take`'s is |
| `tail`, `init` | All but the first, and all but the last, character; each traps on the empty string |
| `charIterator()` | The characters as an `Iterator[char]` that decodes one character for each `next()`, making no vector |
| `split(c)`, `split(sep, limit)`, `split(c, limit)` | `split` with a `char` separator; and with at most `limit` pieces, the last holding the rest of the text with its separators. A `limit` of 0 or less sets none, 1 gives the whole string, and an empty string separator traps |
| `stripLineEnd` | Without one `\n` or `\r\n` at its end; a lone `\r`, or no terminator, leaves it unchanged |
| `compareIgnoreCase(o)` | The sign of the comparison after each character's simple lower-case mapping: negative, zero or positive |

```text
val word = "café"
println(s"${word.byteLength} ${word.charCount}")   // 5 4
println(word.slice(3, 5))                               // é
println(word.slice(0, 4))                               // traps: offset 4 is inside `é`
```

A string has no `length` or `size`, since its length is counted in characters or in bytes, and
asking for one names both; nor does `*` repeat it, and the error names `repeat`:

```text
word.length    // string has no field `length`; a string counts its characters with `charCount` and its bytes with `byteLength`
"-" * 3        // `*` needs two numbers of one type, not string and int; a string is repeated by `repeat`, as in `text.repeat(n)`
```

### Strings from bytes

Bytes become a string through two [static members](/spec/declarations/#static-members-of-built-in-types)
of `string`, which check that the bytes are UTF-8, since a string's bytes always are:

| Static member | Result |
| --- | --- |
| `string.fromUtf8(bytes: Array[byte])` | `Ok` of the text the bytes encode, or `Err` of a `Utf8Error` |
| `string.fromUtf8Lossy(bytes: Array[byte])` | The text, with U+FFFD, the replacement character, in place of each maximal run of bytes that is not UTF-8: a sequence the bytes end inside is one replacement, a stray continuation byte another |

`data Utf8Error(offset: long, truncated: bool)` says where and why: `offset` is the byte offset at
which the first sequence that is not a character begins, and `truncated` is true where the bytes end
inside a sequence that more bytes could complete, and false where they hold one no continuation
could make a character. Refused are a continuation byte that begins nothing, an overlong encoding
(`C0 AF` for `/`), a surrogate (`ED A0 80`), a value past U+10FFFF (`F4 90 80 80`), a byte no
sequence begins with (`FF`), and a sequence cut short.

```text
val bytes = "naïve".utf8
println(string.fromUtf8(bytes))                          // Ok(naïve)
val cut: Array[byte] = [0x61, 0xE2, 0x82]
println(string.fromUtf8(cut))                            // Err(Utf8Error(1, true))
println(string.fromUtf8Lossy(cut))                       // a�
```

## unit

`unit` has the single value `()`. It is an ordinary type: it can be a type argument, a field, a
parameter, and a pattern. A function whose body is an indented block with no `=` and no result type
returns `unit`.

## Tuples

`(A, B, …)` is a tuple type of any arity, written and matched with parentheses: `(1, "one")`,
`(n, name) -> …`. Elements are read with `t.0`, `t.1`. Tuples are values with no identity and derive
`Eq`, `Hash`, `Ord`, `Display` and `Debug` when their elements do. A pair has `swap`, the pair
with its parts the other way round: `(1, "one").swap` is `("one", 1)`. A program adds methods to
tuples with an [impl](/spec/declarations/#impl) naming a tuple type built from its parameters,
`impl[A, B] (A, B)` or `impl[A] (A, A)`; only the core library implements a trait for tuples.

Where a value fills the rest of a line, a tuple needs no parentheses: after `=` in a function, clause
or binding, after `return`, after `->` in a `match` arm, and as a line of a block, including its
last. Inside brackets and in the body of a one-line `if` or `while`, a comma separates other things,
so a tuple there is parenthesized.

```text
divmod(a: int, b: int) -> (int, int) = a / b, a % b

bounds(a: int, b: int) -> (int, int)
    if a > b then return b, a
    a, b
```

A tuple is taken apart by a binding (`val q, r = divmod(17, 5)`, `val (a, (b, c)) = t`), by a
[multiple assignment](/spec/operators-and-control-flow/#multiple-assignment), by tuple
patterns in `match` and in clauses, and by a lambda's parameters.

Where a function of one parameter whose type is a tuple of *n* elements is expected, *n* at least
two, a lambda of exactly *n* parameters takes the tuple apart: its parameters are the tuple's
elements, in order, each typed from its element. A lambda of one parameter takes the tuple whole,
as it would anywhere.

```text
val xs = [1, 2, 3]
xs.zip(xs).forall((x, y) -> x == y)          // true
xs.zip(["a", "b", "c"]).map((n, s) -> s"$s$n") // [a1, b2, c3]
[((1, 2), 3)].map((p, z) -> p.0 + p.1 + z)   // [6]: an inner tuple is one element
map.filter((k, _) -> k != "skip")
```

- A parameter written with a type takes an element of that type, or of a narrower one as any
  lambda's parameter may, and its body sees the type written: `(a: Animal, n) -> …` over
  `(Dog, int)` pairs. A declared type the element does not fit is an error naming the element.
- An element the expected type leaves unknown, as in a generic function's `((A, B)) -> C` whose
  `A` and `B` nothing else fixes, takes its type from the parameter's declaration; one with neither
  is an error asking for the type.
- `_` takes an element and binds nothing. Each parameter is a `val`, as any lambda's is.
- It applies only where the lambda's parameters are not as many as the expected function's: a
  lambda of two parameters where `(A, B) -> C` is expected takes two arguments. Among overloads, a
  candidate that has a lambda take a tuple apart is chosen only where no candidate takes it as
  written, so `Map`'s `map((k, v) -> (v, k))` is its `(K, V) -> (L, W)` overload.
- A lambda whose parameters are as many as neither the function's nor the tuple's elements is an
  error naming both shapes: `expected a function of 1 parameter, (int, int), but this lambda takes
  3: a lambda takes that tuple as one parameter or as 2 parameters, one for each element`.

A [`match` function](/spec/operators-and-control-flow/#match-functions), `match (x, y) -> …`, still
takes a tuple apart by a pattern, for shapes a list of names cannot write: nested tuples, literals,
variants.

## Function types

`(A, B) -> C` is the type of a function taking `A` and `B` and returning `C`; `() -> C` takes nothing.
A function taking one parameter may drop the parentheses: `int -> string`, `Option[T] -> bool`.
`(A) -> B` and `A -> B` are the same type. `->` groups to the right, so `int -> int -> int` is
`int -> (int -> int)`, a function returning a function; a function taking a function writes the
parameter in parentheses, `(int -> int) -> int`. A tuple parameter takes a second pair of
parentheses, `((A, B)) -> C`, since `(A, B) -> C` takes two parameters.

```text
twice(f: int -> int, x: int) -> int = f(f(x))
adder(n: int) -> int -> int = x -> x + n      // the result is a function type
val fs: List[int -> int] = [adder(1), adder(2)]
val add: int -> int -> int = a -> b -> a + b
```

The form works wherever a type does: parameters, fields, results, type arguments, tuple parts,
`val` annotations and supertypes. A result of function form needs no parentheses, since the
declaration's `=` ends it. A type pattern in a `match` arm is the exception: the arm's `->` follows
the type, so a function type there is written in parentheses, `f: (int -> int) -> body`.
Compiler messages print every function type with a parameter list, `(int) -> int`.
A by-name parameter type is written `-> T` inside a parameter list; see
[call-by-name](/spec/by-name/). Function types are traits: a class or trait may list a function type
among its parents after `:`, and a data type or class may name one in an impl, and its values are
then subtypes of that function type, callable as `value(args)` and usable wherever it is expected,
with no conversion. Such a type has an `apply` taking exactly the function type's parameters and
yielding its result or a subtype, and a call of the value as a function runs that `apply`.

```text
class Shout: (string) -> string
    apply(s: string) -> string = s + "!"

data Lengths(extra: int)

impl (string) -> int for Lengths
    apply(s: string) -> int = s.charCount + extra

val f: (string) -> string = Shout()
["a", "b"].map(Lengths(1))      // [2, 2]
```

A function type in a parent list follows the variance rules of any parent: its parameters are
contravariant positions and its result a covariant one. A type is at most one function type,
counting those its superclass and traits are, and a built-in type is none. The core library's
collections are functions this way: a `Seq[T]` is an `(int) -> T`, a `Set[A]` an `(A) -> bool` and a
`Map[K, V]` a `(K) -> V` (see [collections](/spec/collections/#collections-as-functions)).

There is no arity limit.

**Functions compose.** The core library adds to every function of one parameter `f.andThen(g)`, the
function that runs `f` and then `g` on its result, and `f.compose(g)`, which runs `g` first; and to
every function of two parameters `f.tupled()`, the function taking its two arguments as one pair. A
function type is a trait, so they reach every value that is a function of that shape: a lambda, a
reference to a function, a method value (of a method declared with a parameter list, one declared
with an empty list `()` only where a function type is expected; a
[parameterless method](/spec/declarations/#parameterless-methods) has none, and `() -> x.head` is the
function that reads one), and a value of a type naming a function type, a sequence or a map among
them. A type's own method of the same name and parameter types is found first.
`identity`, in the prelude, is the function that gives back its argument, for passing where a
function is asked for and nothing should change.

```text
val inc = (x: int) -> x + 1
inc.andThen(x -> x * 2)(3)              // 8
inc.compose((x: int) -> x * 2)(3)       // 7
((a: int, b: int) -> a + b).tupled()((2, 3))   // 5
[1, 2, 3].andThen(n -> n * 10)(0)       // 10: a sequence is a function of its index
[[1], [2, 3]].flatMap(identity)         // [1, 2, 3]
```

A program may add methods to function types the same way, with an
[impl](/spec/declarations/#impl) naming one: `impl[A] (A) -> A`, `impl[A, B, C] (A, B) -> C`. A
`@sendable (A) -> B` there adds only to `@sendable` functions.

**`@sendable` before a function type**, `@sendable (A) -> B`, makes the type of a function that may
run on any thread: it captures only sendable values and no `var`, and the code it runs reaches no
module-level mutable state ([threads](/spec/threads/#what-is-shared)). Such a type is `Sendable`, and
a plain function type is not, since it does not say what its values capture. Only a function type
may be marked; `@sendable int` is an error. A `@sendable (A) -> B` is a `(A) -> B`, with no
conversion, so it is passed wherever the plain type is expected and a vector of them is a vector of
the plain type; a plain function value is not a `@sendable` one. A lambda, a reference to a function
and a method value written where a `@sendable` function type is expected are made as one, and are
checked there as a thread's body is. Inside a function, a parameter marked
[`@sendable`](/spec/threads/#what-is-shared) that takes a function holds a `@sendable` function.

```text
val step: @sendable (int) -> int = (x) -> x + 1       // checked: captures nothing
val named: @sendable (int) -> int = double            // checked: what `double` reaches
val plain: (int) -> int = step                        // a @sendable function is a plain one
val back: @sendable (int) -> int = plain              // error: `plain`'s type does not say what it captures
adder(n: int) -> @sendable (int) -> int = (x) -> x + n
```

Two overloads that differ only in whether a function parameter is `@sendable` are the same
overload, as two that differ only in a by-name parameter are, since one lambda may be passed for
either. An override takes the parameter types of the method it overrides, the mark included, and
may yield a `@sendable` function where that method yields the plain type. Messages print the mark
as it is written, `@sendable (int) -> int`.

### Placeholder lambdas

A call's argument that holds `_` is a lambda: each `_` is a parameter of its own, in the order
written, and the argument is the body. A `val` or `var` with a declared type takes one the same way.

```text
xs.map(_ + 1)                 // xs.map(x -> x + 1)
people.map(_.name)            // people.map(p -> p.name)
xs.filter(0 < _ < 10)         // xs.filter(x -> 0 < x < 10)
xs.reduce(_ + _)              // xs.reduce((a, b) -> a + b)
people.sortBy(_.age)
xs.map(scale(_, 2))           // xs.map(x -> scale(x, 2))
orders.map(_.items.map(_ * 2))  // orders.map(o -> o.items.map(x -> x * 2))
val inc: (int) -> int = _ + 1
```

- **The lambda is the nearest argument holding the `_`.** A `_` is found through operators of every
  precedence, member access and method calls on it, tuple parts (`(_, 0)`), `is`, a range's bounds,
  `with`'s base and a call's callee, so `_.name.byteLength + 1` is one lambda. Another call's
  argument is a lambda of its own: in `orders.map(_.items.map(_ * 2))` the first `_` is an order and
  the second an item. A name ending in `:` is written after its argument, so in `_ +: _` the first
  `_` is the element.
- **A `_` that is a whole argument belongs to the argument around its call**: `xs.map(scale(_, 2))`
  is `xs.map(x -> scale(x, 2))`, and `apply2(scale(_, 2) + _)` is
  `apply2((a, b) -> scale(a, 2) + b)`. A lambda that would only call a named function with its
  parameters in order, `xs.map(double(_))`, is that function: it is an error saying to pass
  `double`. A constructor's name is no function's, so `xs.map(Some(_))` is `x -> Some(x)`.
- **A lambda, a block, an `if`, a `match`, a collection literal and an interpolation keep their
  own `_`**: none of them is searched, so `xs.map(n -> n + _)` is an error, as is any `_` outside a
  call's argument and a declared value, `val y = x + _`. A `_` alone as a whole argument of a call
  that is itself no argument, `scale(_, 2)` as a statement, is an error too.
- **The parameters take their types from the function expected**, as any lambda's do, and their
  count must be its parameter count; a lambda of two `_`s where a function of one tuple is
  expected [takes the tuple apart](#tuples), so `xs.zip(ys).map(_ + _)` adds the pairs. A count
  that fits neither is an error naming both counts, and a `_` whose type nothing gives is an error
  asking for the lambda to be written out with a type. Overloads see the lambda as they see one
  written out: the number of `_`s picks among overloads of different counts.
- Each `_` is used once, so a parameter read twice, or a parameter not read at all, is written as
  an ordinary lambda.

The other meanings of `_` are unchanged: a parameter `_ ->` or `(_, b) ->` binds nothing, a
pattern's `_` matches anything, `val _ =`, a discarded part `q, _ = …`, `for _ in`, a clause's `_`
argument, the last guard `| _ =`, the digit separator in `1_000`, and a name that begins with `_`,
such as `_offset`, which is an ordinary name.

### Partial functions

`PartialFunction[-A, +B]` is a core-library trait, and a subtype of `A -> B`: a function defined at
some values of `A` and not others. `isDefinedAt(a)` says whether it is defined at `a`; calling it,
`pf(a)`, at a value it is not defined at traps, and the trap names the program's call. `lift()` is
the total function `A -> Option[B]`, `None` where it is not defined; `applyOrElse(a, default)` is its
value at `a`, or `default(a)` where it is not defined; `pf.orElse(other)` is `pf` where it is
defined and `other` elsewhere. A [`match` with nothing before
it](/spec/operators-and-control-flow/#match-functions), written where a `PartialFunction` is
expected, makes one:

```text
val half: PartialFunction[int, int] = match n if n % 2 == 0 -> n / 2
half.isDefinedAt(3)           // false
half.applyOrElse(3, n -> n)   // 3
[2, 4].map(half)              // [1, 2]: a partial function passes where `int -> int` is expected
```

## Vector types

`[]T` is another way to write `Vector[T]`: the same type, in any position a type may stand.
`xs: []int`, `[][]int` (a `Vector[Vector[int]]`), `Option[[]string]` and `([]int, string)` all work,
and a program's entry point may be `main(args: []string)`. The element is a name or a parenthesized
type, so `[]int -> int` is a function taking a vector, `([]int) -> int`, and `[](int -> int)` is a
vector of functions. A type pattern in a `match` arm takes the form without parentheses,
`v: []int -> body`. There is no fixed-length `[N]T`; `[3]int` is an error, and `Array[T]` is the
fixed-length contiguous collection. Compiler messages print the type as `Vector[int]`.

## Any and Nothing

`Any` is the top type. Every reference type is a subtype of `Any`. A value type (built-ins, tuples,
`data`) converts to `Any` by boxing at the point of conversion; the box records the exact type, so a
`match` on an `Any` recovers the value.

```text
describe(x: Any) -> string = x match
    s: string -> s"text $s"
    n: int -> s"number $n"
    _ -> "something else"

describe(42)        // the int is boxed where it becomes an Any
```

A value converts to `Any` wherever one is expected of it, at a single site: an argument, a binding,
an assignment, a field, a result, a part of a tuple written out, or each branch of an `if` or
`match` expected to yield an `Any`. A tuple already made converts only where each part already is
what the wider tuple holds there: a part that is an object (a string, a `data` value, a tuple, an
instance of a class) is its own `Any`, so a `(string, int)` is a `(Any, int)`, and so is a `(P, int)`
for a data type `P`. A part that would need a box does not convert, since its layout differs and
boxing never happens inside a value already made: an `(int, int)` is not an `(Any, int)`, nor is a
`(bool, int)`. A tuple written out where the wider one is expected, `(n, 1)`, boxes its part there.

How a value is boxed is the compiler's choice, and invisible except in cost. A string, a tuple and a
`data` value are already objects whose header names their exact type, so they need no box; a
number, a `char`, a `bool` and `unit` get one. A tuple's type at run time is the type it was made
with: `(animal, 1)` with `animal: Animal` is an `(Animal, int)`, whatever class the animal is.

A value of a built-in type or a tuple converts the same way to a trait it implements, at the same
sites: `val shown: Vector[Display] = [1, "two", (3, 4)]`, `describe(d: Display)` called as
`describe(42)`. Calls through the trait reach the type's own implementation, and a type pattern
recovers the value: `d match n: int -> …`. An overload taking the value's own type is more specific
than one taking a trait it implements.

`==` and `!=` on an `Any` are decided at run time by the values' own types: values of one type
compare as that type does, by value or by identity, and values of different types are unequal (see
[equality](/spec/equality-and-memory/#equality)). `===` and `!==` need a side whose type is a class,
since an `Any` may hold a value with no identity: `x === obj` is defined, and `x === y` with both
`Any`s is an error (see [identity](/spec/equality-and-memory/#identity)). A type pattern recovers the
value's type, and with it the other comparisons that type has.

`Nothing` is the bottom type: the type of `return`, `break`, `continue` and a call to a function that
never returns. It is a subtype of every type, and an expression of type `Nothing` never produces a
value. An empty collection literal or `None` does **not** have a `Nothing` type argument; its type
comes from context (see [inference](/spec/generics/#inference)).

## Union types

`A | B` is a value of `A` or of `B`, written in annotations and never inferred, made at a single
site as an `Any` is, and taken apart by a `match` that covers every member. `?` uses it to pass on
errors of several types: a function yielding `Result[T, IoError | ParseError]` may `?` either kind.
`|` binds more tightly than `->`, so `(A) -> B | C` returns a union. The rules are in their own
chapter, [union types](/spec/unions/).

```text
describe(x: int | string) -> string = x match
    n: int -> s"number $n"
    s: string -> s"text $s"
```

## Reference types and value types

Class instances, `object`s, arrays, buffers, closures and boxes are **reference types**: they live on
the garbage-collected heap and have identity. Built-in types, tuples and `data` types are **value
types**: they have no identity, and the compiler chooses their representation. See
[equality, identity and memory](/spec/equality-and-memory/).
