---
title: Regular expressions
summary: "Proposed: `avin.regex` — the pattern language, backtracking matching with leftmost-first results, `Regex`, `Match` and `MatchIterator`, replacing and splitting."
weight: 26
---

`avin.regex` is a regular-expression library written in Avin: a pattern is read into a syntax tree,
compiled into a program of matching operations, and run by a backtracking matcher over the text's
characters. It is imported like any module: `import avin.regex.{Regex, Match}` for the types, and
`import avin.regex` for the string method `r`.

```text
import avin.regex
import avin.regex.Regex

main()
    val date = Regex("(?<year>\\d{4})-(?<month>\\d\\d)")
    date.findFirstMatchIn("due 2024-06") match
        Some(m) -> println(m.group("month"))         // Some(06)
        None -> println("no date")
    println("[a-z]+".r().findAllIn("one, two").toList())   // [one, two]
    println(Regex("a+b").matches("aab"))              // true
```

## Text, characters and offsets

Matching goes **character by character**: `.` matches one Unicode scalar value however many bytes
encode it, a class tests a whole character, and a lookbehind's lengths count characters. Every
offset the API answers with is a **byte offset** into the text, as every string offset is, so
`m.start()` and `m.end()` slice the text directly. An empty match moves the next search of
`findAllIn` on by one character. A `PatternError`'s index counts the pattern's characters from 0.

## The pattern language

| Construct | Meaning |
| --- | --- |
| `x`, `\\`, `\.` | A literal character; a backslash before any character that is not a letter or digit makes it literal |
| `\t` `\n` `\r` `\f` `\a` `\e` | Tab, newline, return, form feed, bell, escape |
| `\0n` `\0nn` `\0mnn` | An octal character, `m` at most 3 |
| `\xhh` `\x{h…}` `\uhhhh` | A character by its code in hexadecimal; `😀` is the one character the pair encodes |
| `\cX` | The control character of `X` |
| `\Q…\E` | Every character between taken literally; without `\E` the quotation runs to the end |
| `.` | Any character but a line terminator; with `s`, any character; with `d`, any but `\n` |
| `[abc]` `[^abc]` `[a-z]` | A class, its negation, a range; `]` first and `-` first or last are literal |
| `[a[bc]]` `[a-z&&[^aeiou]]` | The union of nested classes, and an intersection |
| `\d` `\D` `\s` `\S` `\w` `\W` | ASCII digits `[0-9]`, white space `[ \t\n\x0B\f\r]`, word characters `[a-zA-Z_0-9]`, and their negations |
| `\h` `\H` `\v` `\V` | Horizontal and vertical white space, and their negations |
| `\p{Lu}` `\p{L}` `\pL` `\p{gc=Lu}` `\p{IsLu}` | A Unicode general category or a group of them (`L`, `M`, `N`, `P`, `S`, `Z`, `C`, `LC`, `LD`) |
| `\p{IsLetter}` `\p{IsDigit}` `\p{IsWhite_Space}` … | A Unicode binary property the general category decides; the name after `Is` ignores case |
| `\p{Alpha}` `\p{Digit}` `\p{Punct}` … | A POSIX class, over ASCII: `Lower`, `Upper`, `ASCII`, `Alpha`, `Digit`, `Alnum`, `Punct`, `Graph`, `Print`, `Blank`, `Cntrl`, `XDigit`, `Space`; also `L1` (Latin-1) and `all` |
| `\P{…}` | The negation of a property |
| `X?` `X*` `X+` `X{n}` `X{n,}` `X{n,m}` | Repetition, greedy; followed by `?` reluctant, by `+` possessive |
| `(X)` `(?<name>X)` | A capturing group, numbered from 1 by its opening parenthesis; a named one, whose name is a Latin letter then letters and digits |
| `(?:X)` `(?>X)` | A group that only groups; an atomic group, matched once and never tried another way |
| `(?=X)` `(?!X)` `(?<=X)` `(?<!X)` | Lookahead and lookbehind, each positive or negative |
| `\1` … `\k<name>` | The text a group matched, again |
| `^` `$` `\A` `\z` `\Z` `\G` | Start; end, or before a line terminator ending the input; start of input; end of input; end or before a final line terminator; where the previous match ended |
| `\b` `\B` | A word boundary, and anywhere else |
| `\R` | A line break: `\r\n`, or one of `\n`, `\x0B`, `\f`, `\r`, `\u0085`, ` `, ` ` |
| `X\|Y` | Alternatives, tried left to right |
| `(?imsxdu-imsxdu)` `(?i:X)` | Flags turned on and off from here to the end of the enclosing group, or for `X` alone |

**Flags.** `i` ignores case, for ASCII letters only unless `u` is on too, when it is Unicode's
simple case mapping; `m` makes `^` and `$` match at each line; `s` makes `.` match a line terminator;
`d` makes `\n` the only line terminator for `.`, `^` and `$`; `x` ignores white space and `#`
comments to the end of the line, outside `\Q…\E` and escapes, inside classes too. A flag set by
`(?i)` lasts to the end of the group it is written in, across later alternatives.

**Line terminators** are `\n`, `\r`, `\r\n`, `\u0085`, ` ` and ` `. In multiline mode `^`
matches at the start and after a line terminator but never at the very end of the input, and never
between the `\r` and `\n` of a pair; `$` matches before a line terminator and at the end.

**Word boundaries.** A word character for `\b` is one `\w` matches, or a non-spacing mark that
follows letters or digits.

**Back references.** `\n` with more digits takes them while the number names a group already opened;
`\11` after one group is `\1` and then `1`. A reference to a group that does not exist, or that has
not matched, matches nothing. Under `i` a reference matches the text in any case.

**Lookbehind** is bounded: its body must have a greatest length, so every repetition in it has an
upper bound, no group in it whose body has alternatives or a repetition of varying count is
repeated by a greedy or reluctant `*`, `+` or `{…}`, and it holds no back reference. It tries its body from the nearest
start first.

## Matching

Matching is **backtracking, leftmost-first**. The search tries each start from the left; at a start,
alternatives are tried in order, a greedy repetition takes as many repetitions as it can and gives
them back one at a time, a reluctant one takes as few as it can, and a possessive one and an atomic
group never give back. The first way the whole pattern matches is the match. There is no step limit.

What a group captured is what it matched on the path that succeeded, with these rules where a group
is repeated or tried more than once:

- A repeated group holds its **last repetition**. A group inside a repeated group that did not take
  part in a later repetition keeps what it matched in an earlier one: `(a|(b))+` on `ba` leaves
  group 2 `b`.
- A repetition that matched nothing ends the repetition.
- A group in a repeated group whose body has one length is not restored when a repetition is given
  back: `((a)b){0,2}abc` on `ababc` matches with group 1 `ab` at 0 and group 2 the `a` at 2.
- A group captured inside a lookahead keeps its capture once the lookahead has matched, whatever
  happens after it.
- A repetition of a lookaround or of an atomic group with `?` or `*` may skip it: `(?>a)?a` matches
  `a`.

## `Regex`

| Member | What it does |
| --- | --- |
| `Regex(pattern)`, `Regex(pattern, names*)` | Compiles; traps with the error's message on a pattern that is not valid. The names, given, name the groups 1, 2, … for `group(name)` in place of the pattern's own |
| `Regex.compile(pattern, names*)` | Compiles, answering `Result[Regex, PatternError]` |
| `"pattern".r()`, `.r(names*)` | `Regex(pattern, names*)`, where `avin.regex` is imported |
| `matches(s)` | Whether all of `s` matches; for an unanchored regex, whether any part does |
| `findFirstIn(s)`, `findFirstMatchIn(s)` | The first match, as `Option[string]` and `Option[Match]` |
| `findPrefixOf(s)`, `findPrefixMatchOf(s)` | A match starting at the start of `s` |
| `findAllIn(s)` | A `MatchIterator`: every match, left to right, not overlapping |
| `findAllMatchIn(s)` | The same, as an `Iterator[Match]` |
| `replaceAllIn(s, replacement)` | `s` with every match replaced by the replacement, its group references filled in |
| `replaceAllIn(s, f)` | `s` with every match `m` replaced by `f(m)`, `f` a `(Match) -> string`; its result is read as a replacement |
| `replaceFirstIn(s, replacement)` | `s` with its first match replaced |
| `replaceSomeIn(s, f)` | `s` with each match `m` replaced where `f(m)`, an `Option[string]`, is `Some`, and kept where it is `None` |
| `split(s)` | `pattern().split(s)`: the pieces of `s` between the matches, as an `Array[string]` |
| `unanchored()`, `anchored()` | The same regex with `matches` asking about any part, or the whole |
| `regex()`, `toString()`, `pattern()` | The pattern as written; `pattern()` is a `Pattern`, with `pattern()`, `groupCount()` and `split` |
| `Regex.quote(text)` | A pattern matching `text` literally: `\Q`, the text, `\E`, each `\E` in the text written so it does not end the quotation |
| `Regex.quoteReplacement(text)` | A replacement writing `text` literally: each `\` and `$` with a backslash before it |

**Replacements.** In a replacement, `$` followed by digits is what a group matched: the first digit
always counts, and each digit after it is taken while the number still names a group the pattern
has, so `$10` with one group is group 1 and then `0`. `${name}` is the group the pattern names
`name` (the names given to the `Regex` do not count here). A group that took no part writes nothing.
A backslash makes the character after it literal, so `\$` writes `$` and `\\` one backslash. A
replacement computed by `f` is read the same way; `Regex.quoteReplacement` makes any text safe to
use. A reference that cannot be resolved traps, with these messages: `No group 2` for a number the
pattern does not have, `No group with name {n}`, `Illegal group reference` for `$` followed by
anything else, `Illegal group reference: group index is missing` for a `$` at the end, `named
capturing group has 0 length name`, `named capturing group is missing trailing '}'`, `capturing
group name {1n} starts with digit character`, and `character to be escaped is missing` for a
backslash at the end.

Matches are found as by `findAllIn`, so an empty match is replaced at each position between
characters: `Regex("").replaceAllIn("é😀", "-")` is `-é-😀-`, and `Regex("a*").replaceAllIn("baaac",
"-")` is `-b--c-`. Text with no match comes back unchanged.

**Splitting.** `Pattern.split(s, limit = 0)` gives the pieces of `s` between the matches, left to
right. An empty match at the very start makes no empty first piece, while a match of some length
there does (`Regex(",").split(",a")` is `["", "a"]`); a text the pattern does not match is one piece,
the empty text included. With `limit` above 0 there are at most `limit` pieces, the last the rest of
the text; with 0, the empty pieces at the end are dropped, so `Regex(",").split("a,,b,,")` is
`["a", "", "b"]` and `Regex(",").split(",,")` has none; below 0 every piece is kept.
A `PatternError` has a `description`, the `pattern` and the `index` of the character the error was
found at (-1 where no one character is to blame); `message()` writes the description, ` near index`
and the index, then the pattern on a line of its own and, where the index is inside it, a caret
under that character. The descriptions are those of the table below, among others:

```text
Unclosed group near index 3
(ab
Dangling meta character '*' near index 0
*a
^
```

`Unclosed group`, `Unmatched closing ')'`, `Unclosed character class`, `Illegal character range`,
`Dangling meta character '*'`, `Illegal repetition`, `Illegal repetition range`, `Unclosed counted
closure`, `Illegal/unsupported escape sequence`, `Illegal octal escape sequence`, `Illegal
hexadecimal escape sequence`, `Hexadecimal codepoint is too big`, `Illegal Unicode escape
sequence`, `Illegal control escape sequence`, `Unknown inline modifier`, `Unknown group type`,
`Look-behind group does not have an obvious maximum length`, `Named capturing group <n> is already
defined`, `named capturing group <n> does not exist`, `capturing group name does not start with a
Latin letter`, `named capturing group is missing trailing '>'`, `\k is not followed by '<' for named
capturing group`, `Unknown character property name {X}`, `Unclosed character family`, `Empty
character family`.

**Not supported**, each reported as a `PatternError` whose description begins `Unsupported`:
properties that need Unicode data beyond the general category — scripts (`\p{IsGreek}`,
`\p{sc=…}`), blocks (`\p{InGreek}`, `\p{blk=…}`), `Alphabetic`, `Ideographic`, `Uppercase`,
`Lowercase`, `Word`, `Alnum` and the emoji properties; the flags `U` and `c`; `\X`, `\b{g}` and
`\N{…}`.

## `Match` and `MatchIterator`

A `Match` holds its `source` text and where the match and each group start and end.

| Member | What it answers |
| --- | --- |
| `matched()` | The text matched |
| `start()`, `end()` | Where the match starts and ends |
| `start(i)`, `end(i)` | Where group `i` starts and ends, -1 when it took no part; group 0 is the whole match |
| `group(i)`, `group(name)` | `Option[string]`: the group's text, `None` when it took no part |
| `groupCount()` | The number of capturing groups |
| `subgroups()` | `List[Option[string]]`, groups 1 on |
| `before()`, `after()` | The text before and after the match |
| `before(i)`, `after(i)` | The text before and after group `i`, `None` when it took no part |
| `groupNames()` | The names given to the `Regex` |

A group number outside `0..groupCount()` traps, and so does a name no group has: by the names given
to the `Regex` where there are any (`""` is group 0), otherwise by the pattern's own.

A `MatchIterator` is an `Iterator[string]` of the matched texts. After `next()`, until the following
`hasNext()`, its `start()`, `end()`, `start(i)`, `end(i)`, `group(i)`, `groupCount()` and `matched()`
answer for that match, and before the first `next()` for the first match; asked when no match is
left, they trap. `matchData()` turns
it into an `Iterator[Match]`.
