---
title: Putting it together
summary: Four complete programs, a word counter, a calculator, the eight queens and the Game of Life, each using what the book has shown.
weight: 10
---

The chapters so far each showed one idea. Real programs use them all at once. Here are four complete
programs, each small enough to read in a few minutes, and each doing something worth doing. Read
them, run them, and change them: each section ends with ideas for where to take it next.

## Counting words

Which words does a text use most? This program splits a text into words, counts each one with
`groupMapReduce`, and prints the commonest with a bar for each. The text is a text block in the
program, so it prints the same every time.

```text run
val text = """
  The quick brown fox jumps over the lazy dog. The dog sleeps;
  the fox does not. A fox that jumps is a happy fox, and a dog
  that sleeps is a happy dog.
  """

// The words of a line, in lower case: a word is a run of letters.
words(line: string) -> Vector[string]
  var out = Vector.empty[string]()
  var word = ""
  for c in line.chars
    if c.isLetter then word = word + c.toLower.toString()
    elif word.nonEmpty
      out = out :+ word
      word = ""
  if word.nonEmpty then out = out :+ word
  out

main()
  val all = for line in text.lines, w in words(line) yield w
  val counts = all.groupMapReduce(w -> w, _ -> 1, _ + _)
  // Commonest first, and words counted alike in alphabetical order.
  val ranked = counts.toVector.sortBy((w, n) -> (-n, w)).take(5)
  println(s"${all.length} words, ${counts.size} different")
  for (w, n) in ranked
    println(f"${w}%-6s ${n}%2d ${"#".repeat(n)}")
```

```text output
33 words, 16 different
a       4 ####
dog     4 ####
fox     4 ####
the     4 ####
happy   2 ##
```

Sorting by the tuple `(-n, w)` puts larger counts first, and breaks ties alphabetically, because
tuples compare part by part.

To take it further: leave out words shorter than three letters, or read the text from a file with
[`avin.fs`](/spec/system-library/#avin-fs) and `fs.readText(path)`, which answers a `Result`.

## A calculator

This program reads arithmetic like `2 * -(3 - 10) / 7`, builds a tree of the expression, and
evaluates it. The tree is a data type, the parser is a class that walks the text with a position that
changes, and every step that can fail answers a `Result`, so `?` carries an error out from wherever it
happens. Each method of the parser handles one level of precedence, and calls the next for the
level that binds tighter.

```text run
data Expr
  Num(value: long)
  Neg(operand: Expr)
  Bin(op: char, left: Expr, right: Expr)

class Parser(text: string)
  val cs = text.chars.toVector
  var pos = 0

  // The next character that is not a space, or '\u{0}' at the end.
  peek() -> char
    while pos < cs.length && cs(pos) == ' ' do pos += 1
    if pos < cs.length then cs(pos) else '\u{0}'

  // What stands at the parser's place, for an error message.
  found() -> string = if peek() == '\u{0}' then "the end" else s"'${peek()}'"

  expect(c: char) -> Result[unit, string]
    if peek() != c then return Err(s"expected '$c', found ${found()}")
    pos += 1
    Ok(())

  // expr := term (('+' | '-') term)*
  expr() -> Result[Expr, string]
    var left = term()?
    while peek() == '+' || peek() == '-'
      val op = cs(pos)
      pos += 1
      left = Bin(op, left, term()?)
    Ok(left)

  // term := factor (('*' | '/') factor)*
  term() -> Result[Expr, string]
    var left = factor()?
    while peek() == '*' || peek() == '/'
      val op = cs(pos)
      pos += 1
      left = Bin(op, left, factor()?)
    Ok(left)

  // factor := number | '-' factor | '(' expr ')'
  factor() -> Result[Expr, string]
    val c = peek()
    if c == '-'
      pos += 1
      return Ok(Neg(factor()?))
    if c == '('
      pos += 1
      val inner = expr()?
      expect(')')?
      return Ok(inner)
    if !('0' <= c <= '9') then return Err(s"expected a number, found ${found()}")
    var n: long = 0
    while pos < cs.length && '0' <= cs(pos) <= '9'
      n = n * 10 + long(int(cs(pos)) - int('0'))
      pos += 1
    Ok(Num(n))

parse(text: string) -> Result[Expr, string]
  val p = Parser(text)
  val e = p.expr()?
  if p.peek() == '\u{0}' then Ok(e) else Err(s"expected the end, found ${p.found()}")

eval(e: Expr) -> Result[long, string]
eval(Num(n)) = Ok(n)
eval(Neg(x)) = Ok(-eval(x)?)
eval(Bin(op, l, r)) =
  val a = eval(l)?
  val b = eval(r)?
  op match
    '+' -> Ok(a + b)
    '-' -> Ok(a - b)
    '*' -> Ok(a * b)
    _ -> if b == 0 then Err("division by zero") else Ok(a / b)

// The expression with every operation in parentheses, to show how it was read.
show(e: Expr) -> string
show(Num(n)) = n.toString()
show(Neg(x)) = s"-${show(x)}"
show(Bin(op, l, r)) = s"(${show(l)} $op ${show(r)})"

calculate(text: string) -> Result[string, string]
  val e = parse(text)?
  val v = eval(e)?
  Ok(s"${show(e)} = $v")

main()
  val lines = ["1 + 2 * 3", "(1 + 2) * 3", "2 * -(3 - 10) / 7", "100 / (5 - 5)", "4 + * 2", "(1 + 2", "3 4"]
  for line in lines
    calculate(line) match
      Ok(answer) -> println(answer)
      Err(why) -> println(s"$line: $why")
```

```text output
(1 + (2 * 3)) = 7
((1 + 2) * 3) = 9
((2 * -(3 - 10)) / 7) = 2
100 / (5 - 5): division by zero
4 + * 2: expected a number, found '*'
(1 + 2: expected ')', found the end
3 4: expected the end, found '4'
```

The comments above `expr`, `term` and `factor` are the grammar: each line says what one method
reads. Because `term` reads a whole product before `expr` sees a `+`, multiplication binds tighter
than addition without any table of precedences.

To take it further: add `%` and a `^` for powers that groups to the right, so `2 ^ 3 ^ 2` is
`2 ^ 9`; or add variables, with an environment of type `Map[string, long]` passed to `eval`.

## The eight queens

Can eight queens stand on a chessboard so that none attacks another? A queen attacks along her row,
her column and both diagonals. The program places one queen per row, trying each column that is safe
from the queens already placed, and backtracks when a row has no safe column. A placement is a
vector of the queens' columns, the first row first.

```text run
// Whether a queen in the next row, at `col`, is safe from every queen placed.
safe(placed: Vector[int], col: int) -> bool =
  placed.zipWithIndex.forall((c, row) -> c != col && (c - col).abs != placed.length - row)

// Every way to finish `placed` on an `n` by `n` board.
solutions(n: int, placed: Vector[int] = []) -> Vector[Vector[int]] =
  if placed.length == n then [placed]
  else (0 ..< n).filter(col -> safe(placed, col)).flatMap(col -> solutions(n, placed :+ col))

draw(placement: Vector[int]) -> string =
  placement.map(col -> (0 ..< placement.length).map(c -> if c == col then "Q" else ".").mkString(" ")).mkString("\n")

main()
  for n in 1 .. 8
    println(s"a board of $n by $n: ${solutions(n).length}")
  println(draw(solutions(8)(0)))
```

```text output
a board of 1 by 1: 1
a board of 2 by 2: 0
a board of 3 by 3: 0
a board of 4 by 4: 2
a board of 5 by 5: 10
a board of 6 by 6: 4
a board of 7 by 7: 40
a board of 8 by 8: 92
Q . . . . . . .
. . . . Q . . .
. . . . . . . Q
. . . . . Q . .
. . Q . . . . .
. . . . . . Q .
. Q . . . . . .
. . . Q . . . .
```

`flatMap` is `map` followed by joining the results: each safe column gives a vector of solutions,
and `flatMap` makes one vector of them all. The search never changes a placement; each step makes a
longer one with `:+`, so backtracking is simply returning.

To take it further: count the solutions of larger boards, or use a `LazyList` so that asking for the
first solution stops the search as soon as it finds one.

## The Game of Life

Conway's Game of Life is played on a grid of cells, each alive or dead. At each step, a live cell with
two or three live neighbours stays alive, a dead cell with exactly three comes to life, and every other
cell dies or stays dead. Simple rules, and surprising patterns: a **glider** is five cells that crawl
diagonally across the board forever.

The board is the set of its live cells, and its edges wrap around, so a glider that walks off one
side comes back on the other. A step counts, for every cell next to a live one, how many live
neighbours it has, and keeps the cells the rules say live.

```text run
data Cell(row: int, col: int)

val size = 6

// The eight cells around `c`, on a board whose edges wrap around.
neighbours(c: Cell) -> Vector[Cell] =
  for
    dr in -1 .. 1
    dc in -1 .. 1
    if (dr, dc) != (0, 0)
  yield Cell((c.row + dr + size) % size, (c.col + dc + size) % size)

// Three neighbours bring a cell to life; two keep a live one alive.
lives(count: int, alive: bool) -> bool
lives(3, _) = true
lives(2, alive) = alive
lives(_, _) = false

step(live: Set[Cell]) -> Set[Cell]
  val counts = live.toVector.flatMap(neighbours).groupMapReduce(c -> c, _ -> 1, _ + _)
  (for (cell, n) in counts if lives(n, live(cell)) yield cell).toSet

draw(live: Set[Cell]) -> string =
  (0 ..< size).map(r -> (0 ..< size).map(c -> if live(Cell(r, c)) then '#' else '.').mkString("")).mkString("\n")

main()
  val glider: Set[Cell] = {Cell(0, 1), Cell(1, 2), Cell(2, 0), Cell(2, 1), Cell(2, 2)}
  var board = glider
  for g in 0 ..< 3
    println(s"generation $g")
    println(draw(board))
    board = step(board)
  for _ in 3 ..< 24 do board = step(board)
  println(s"after 24 generations, the glider is home: ${board == glider}")
```

```text output
generation 0
.#....
..#...
###...
......
......
......
generation 1
......
#.#...
.##...
.#....
......
......
generation 2
......
..#...
#.#...
.##...
......
......
after 24 generations, the glider is home: true
```

A glider moves one cell diagonally every four generations, so on a board six cells wide it is back
where it started after 24. Two sets are equal when they hold the same cells, which is the whole
test. The rules are the three clauses of `lives`, as short as their description.

To take it further: start from the R-pentomino, `.##`, `##.`, `.#.`, on a larger board and print its
population for a hundred generations; or read a pattern from text, `#` for a live cell, as the
`examples/life` program in Avin's repository does.

## Where next

The [specification](/spec/) defines everything this book has shown and much it has not: generics with
bounds, unions, actors and threads, refined variants, and Sysl and C bindings. The
[system library](/spec/system-library/) has files, processes, networking, time and more, and
`examples/` in the repository has larger programs, from a ray tracer to a chat room of actors.
