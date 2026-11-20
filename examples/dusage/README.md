# dusage

Shows how much room a directory tree takes, drawn as a tree with each directory's total, the largest
first, down to `--depth` levels; `--all` shows files as well, `--exclude` leaves out names matching a
pattern, and `--bytes` gives exact sizes. The tree is read into a `data Node` of files, links and
directories by a recursive `for … yield`, and the totals and names are functions written as clauses
on its variants. Symbolic links are shown with their targets and never followed. `--demo` makes a
sample tree in a temporary directory, removed when it is done (`defer`), so it runs anywhere: `avin
run main.avin --demo --all`.
