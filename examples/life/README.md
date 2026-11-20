# life

Conway's Game of Life on a board whose edges wrap around. The board is an immutable set of live
cells; a generation is a `for … yield` over each cell's neighbours, a `groupMapReduce` that counts
how many live cells touch each square, and a rule written as three clauses (`survives(3, _) = true`).
Patterns are drawn as text, so a glider is `[".#.", "..#", "###"]`. The program runs a blinker, sends
a glider once round an 8 by 8 board and checks it comes home, and follows the R-pentomino's
population for forty generations.
