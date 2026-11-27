# graphs

Classic graph algorithms over a description read from standard input (`./main < ../graphs.in`):
`road A B 12` lines make a weighted undirected graph and `task a needs b c` lines a dependency graph.
The program prints the shortest route from the first town to every other (Dijkstra's algorithm on a
`PriorityQueue`), the connected groups of towns and the cheapest roads that join each group
(Kruskal's algorithm on a union-find), and an order to do the tasks in (Kahn's algorithm, taking the
alphabetically first ready task from a `TreeSet`), naming the tasks a cycle keeps from ever being
ready. Lines are recognized with sequence patterns such as `["road", a, b, n]` and
`["task", t, "needs", needs*]`, and a line that fits none is reported by number.
