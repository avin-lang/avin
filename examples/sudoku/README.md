# sudoku

A sudoku solver that reads puzzles from standard input, one 81-character line each
(`./main < ../sudoku.in`). Each square holds its remaining candidates as nine bits; placing a digit
removes it from the square's twenty peers, and a square left with one candidate is placed in turn,
which solves easy puzzles outright. Harder ones are finished by depth-first search that guesses at
the most constrained square on a copy of the board. A puzzle whose givens contradict each other, or a
line of the wrong length, is reported and the next one read. The peers of every square are computed
once, in a module-level `Vector` built by a `for` comprehension with a chained comparison in its guard.
