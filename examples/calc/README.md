# calc

An interpreter for a small calculator language with values, recursive functions, comparisons and
`if … then … else`, run a line at a time from standard input (`./main < ../calc.in`). It has the three
classic stages: a lexer that cuts a line into tokens, a recursive-descent parser that builds a syntax
tree of `data` variants, and an evaluator that walks the tree in an immutable environment, so a
function's body sees the names that were defined when it is called. Every stage reports a mistake as
a `Result`, passed back with `?`, and the line is shown with a caret under the column at fault before
the session goes on.
