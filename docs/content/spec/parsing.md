---
title: Parsing
summary: Proposed — avin.parsing, parser combinators in the core library; a grammar is written as Avin expressions over small parsers.
weight: 24
---

**Proposed.** `avin.parsing` builds parsers out of smaller ones. A grammar is a set of Avin values
joined by operators that read like the grammar itself: `p ~ q` is `p` then `q`, `p | q` is `p` or
else `q`, and `p ^^ f` turns what `p` read into a value. The module is in the core library for now,
and moves to a package of its own once packages exist.

```avin
// Arithmetic expressions parsed with `avin.parsing` and evaluated as they are parsed: `+` and `-`
// below `*` and `/`, below a right-grouping `^`, with unary minus and parentheses.
import avin.parsing.{Parser, chainLeft, chainRight, defer, integer, symbol, token, whitespace}

// `base` multiplied by itself `exponent` times.
power(base: long, exponent: long) -> long
    var result: long = 1
    for _ in 0..<exponent
        result *= base
    result

// An operator: the symbol, yielding the function it applies.
op(text: string, f: (long, long) -> long) -> Parser[(long, long) -> long] = symbol(text) ^^^ f

lazy val number: Parser[long] = token(integer)

// The grammar refers to `expr` and `atom` before they are complete, so it reads them through `defer`.
lazy val atom: Parser[long] = number
    | symbol("(") ~> defer(expr) <~ symbol(")")
    | symbol("-") ~> defer(atom) ^^ (n -> -n)

lazy val factor: Parser[long] = chainRight(atom, op("^", power))

lazy val term: Parser[long] = chainLeft(factor, op("*", (a, b) -> a * b) | op("/", (a, b) -> a / b))

lazy val expr: Parser[long] = chainLeft(term, op("+", (a, b) -> a + b) | op("-", (a, b) -> a - b))

lazy val calculation: Parser[long] = whitespace ~> expr

main()
    for text in ["1 + 2 * 3", "(1 + 2) * 3", "2 ^ 3 ^ 2", "10 - 4 - 3", " -(2 + 3) * -2 ", "2 * (3 + 4", "1 + * 2"]
        calculation.parse(text) match
            Ok(n) -> print(s"$text = $n")
            Err(e) -> print(s"$text: $e")
```

This is `examples/arithmetic_parser.avin`, which the test suite runs. It prints:

```text
1 + 2 * 3 = 7
(1 + 2) * 3 = 9
2 ^ 3 ^ 2 = 512
10 - 4 - 3 = 3
 -(2 + 3) * -2  = 10
2 * (3 + 4: 1:11: expected `^`, `*`, `/`, `+`, `-` or `)`, found end of input
1 + * 2: 1:5: expected integer, `(` or `-`, found '*'
```

## Parsers and input

A `Parser[+T]` is a class holding one function, `run: Input -> Option[T]`. An `Input` is the text
and the state of reading it: the byte `offset` of the next character, the furthest offset at which
anything failed together with what was expected there, and whether a failure has passed a
commit point. A parser that succeeds answers `Some(value)` and leaves the offset past what it read;
one that fails answers `None` after recording what it expected. Success allocates nothing beyond
the value and its `Some`: no input object is copied, since the offset is moved in place.

`Parser[+T]` is covariant, so a `Parser[Dog]` is a `Parser[Animal]`. The combinators are methods of
an `impl`, which are not variance positions, so `|` takes a `Parser[T]`.

A parser the library does not offer is made by giving the function: `Parser(input -> …)`, using
`input.atEnd()`, `input.current()` (the character at the offset), `input.offset`, and
`input.fail(what)`, which records `what` as expected and answers `None`.

## Combinators

| Form | Parses | Value |
| --- | --- | --- |
| `p ~ q` | `p` then `q` | `(a, b)` |
| `p ~> q` | `p` then `q` | `q`'s |
| `p <~ q` | `p` then `q` | `p`'s |
| `p ~! q` | `p` then `q`, committed: once `p` has succeeded, `q`'s failure fails the parse | `(a, b)` |
| `p \| q` | `p`, or else `q` from where `p` started | either's |
| `p ^^ f`, `p.map(f)` | `p` | `f(a)` |
| `p ^^^ v` | `p` | `v` |
| `p.flatMap(f)` | `p`, then the parser `f(a)` | that parser's |
| `p.filter(what, pred)` | `p`, failing where `pred` refuses its value | `p`'s |
| `p.label(what)` | `p`, reported as expecting `what` | `p`'s |
| `rep(p)`, `p.rep()` | `p` as many times as it succeeds, none included | `Vector[T]` |
| `rep1(p)`, `p.rep1()` | the same, at least once | `Vector[T]` |
| `opt(p)`, `p.opt()` | `p` or nothing | `Option[T]` |
| `repSep(p, sep)`, `p.repSep(sep)` | `p` separated by `sep`, none included | `Vector[T]` |
| `rep1Sep(p, sep)`, `p.rep1Sep(sep)` | the same, at least one | `Vector[T]` |
| `chainLeft(p, op)` | `p` separated by `op`, a `Parser[(T, T) -> T]`, combined from the left | `T` |
| `chainRight(p, op)` | the same, combined from the right | `T` |
| `commit(p)`, `p.commit()` | `p`, whose failure fails the parse | `p`'s |
| `positioned(p)`, `p.positioned()` | `p` | `(a, Position)` of where it began |
| `guard(p)` | `p`, reading nothing | `p`'s |
| `notFollowedBy(p)` | nothing, where `p` fails | `()` |
| `defer(p)` | the parser `p` names, found when it first runs | `p`'s |
| `success(v)`, `failure(what)` | nothing; `failure` fails expecting `what` | `v` |

Alternatives backtrack: `p | q` restores the offset `p` started at and runs `q`, whatever `p` read
before it failed. A repetition, `opt` and a chain stop where their parser fails and keep what they
had. A run of a repeated parser that reads nothing ends the repetition without adding its value, so
`rep` of a parser that can succeed on nothing does not loop. A separator must be followed by a
value: `[1, 2,]` fails at the `]`.

The operators bind as every method named by symbols does
([operators](/spec/operators-and-control-flow/#methods-named-by-symbols)): `~`, `~>`, `<~` and
`~!`, every name with a `~` in it, tightest and grouping left, then `^^` and `^^^`, then `|`. So
`a ~ b ^^ f | c ~ d` is `((a ~ b) ^^ f) | (c ~ d)`, and a rule reads as written, with no
parentheses:

```text
lazy val factor: Parser[long] = number | symbol("(") ~> defer(expr) <~ symbol(")")
lazy val array: Parser[Json] = symbol("[") ~> items <~ symbol("]") ^^ (xs -> JArray(xs))
```

`factor` is `number | ((symbol("(") ~> defer(expr)) <~ symbol(")"))`. A grammar of several
alternatives spans lines, each line after the first indented and starting with its `|`, a
[continuation line](/spec/operators-and-control-flow/#continuation-lines):

```text
lazy val value: Parser[Json] = symbol("null") ^^^ JNull
    | symbol("true") ^^^ JBool(true)
    | token(decimal) ^^ (x -> JNumber(x))
    | defer(array)
```

A function of the pair a `~` yields is written as a `match` function:
`ident ~ token(integer) ^^ (match (name, n) -> Setting(name, n))`.

## Recursive grammars

Rules refer to one another, and a rule to itself. Module-level rules are `lazy val`s, so each is
made when first read, and a rule that refers to one not yet complete reads it through `defer`, whose
parameter is [by name](/spec/by-name/): it is evaluated the first time the deferred parser runs and
kept. `defer(expr)` inside `expr`'s own definition is the usual case; every cycle of rules needs a
`defer` somewhere on it. Rules may equally be the `lazy val`s of an `object` or of a class.

Each level of nesting is a few calls deep, so the depth a grammar reaches is bounded by its thread's
stack. The example's grammar takes about 3,000 levels of parentheses on the main thread's 8 MiB
unoptimized and 5,000 at `-O2`; a parse run on a thread [spawned](/spec/threads/) with a larger
stack goes deeper, 10,000 levels in 64 MiB. A stack exhausted ends the program.

## Leaf parsers

| Parser | Reads | Value |
| --- | --- | --- |
| `lit("text")` | exactly `text` | `string` |
| `lit('c')` | exactly the character `c` | `char` |
| `satisfy(what, pred)` | one character `pred` accepts | `char` |
| `takeWhile(pred)` | the characters `pred` accepts, none included | `string` |
| `takeWhile1(what, pred)` | the same, at least one | `string` |
| `digits` | ASCII decimal digits, at least one | `string` |
| `ident` | a letter or `_`, then letters, digits and `_`; letters in any script | `string` |
| `whitespace` | white space, none included | `()` |
| `eof` | nothing, at the end of the text | `()` |
| `integer` | an optional `-` and decimal digits, fitting a `long` | `long` |
| `decimal` | a number as JSON writes it: `-`, digits, a fraction, an exponent | `real` |
| `stringLiteral` | a double-quoted string with JSON's escapes, `\u` pairs included | `string` |
| `token(p)` | `p`, then white space | `p`'s |
| `symbol("text")` | `token(lit("text"))` | `string` |

`isDigit`, `isLetter` and `isSpace` are the character tests they use. There is no regular-expression
parser: a regular expression would need a compiled pattern to cross from Sysl as a handle, and
bindings do not yet carry handles.

## Running and errors

`p.parse(text) -> Result[T, ParseError]` parses the whole text; a value that stops short of the end
is an error expecting `end of input`. `p.parsePrefix(text) -> Result[(T, int), ParseError]` parses
as much as `p` takes and answers the byte offset it stopped at.

A `ParseError` holds the `Position` of the furthest offset any alternative reached (`offset`, and
`line` and `column` counted from 1, the column in characters), every distinct thing expected there
in the order the grammar tried them, and what was found: the character in its quoted, escaped form,
or `end of input`. It displays as one line:

```text
2:5: expected `,` or `]`, found '4'
```

A literal is expected as itself in backquotes; the other leaf parsers name what they wanted:
`integer`, `number`, `string`, `identifier`. `p.label("value")` reports a failure that got no
further than `p`'s first character as expecting `value`, in place of the expectations of what `p`
is made of; a failure further in keeps its own, which say more.

A commit point stops backtracking where the grammar knows which rule it is in. After
`lit("let") ~! ident`, a missing name is reported as expected after `let`, rather than the parser
going back to try the next kind of statement from the start and reporting that instead.
