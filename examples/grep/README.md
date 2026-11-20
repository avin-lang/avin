# grep

Prints the lines of its input that match a regular expression, with the options a line matcher is
expected to have: `-i`, `-v`, `-n`, `-c`, `-o`, `-w`, `-F` for plain text, `-m` to stop after so many
matches, and `-C N` for lines of context with `--` between groups that do not touch. The options are
declared once with `avin.cli`, which also writes `--help`; the options become one expression for
`avin.regex`; and the lines to print are chosen by a single pass that yields `Hit` and `Around`
values, which the printer takes apart with `match`. It ends 0 when something matched, 1 when nothing
did and 2 on a bad pattern. Try it with `avin run main.avin -n -i -C 1 'timed? ?out|refused' <
../grep.in`.
