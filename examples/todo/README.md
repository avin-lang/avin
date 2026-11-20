# todo

A to-do list kept in a text file and driven by subcommands, `add`, `list`, `done`, `remove` and
`stats`, each reading its own options with an `avin.cli` parser of its own. The subcommand is chosen
by a sequence pattern on the arguments (`["add", rest*]`, `[verb @ ("done" | "remove"), rest*]`),
tasks are `data` values changed with `with`, and the list is sorted by a tuple of done, priority and
number. `todo script` runs each line of standard input as a command against a list in a temporary
directory, so a whole session can be tried without touching a real list: `avin run main.avin script
< ../todo.in`.
