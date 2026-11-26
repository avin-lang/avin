# nbody

The sun and the four giant planets, moved forward in steps of a hundredth of a year by a simple
symplectic integrator: every pair of bodies pulls on the other, then every body moves along its
velocity. The program prints the system's total energy before and after the given number of steps
(`./main 1000`), which stays nearly constant, so the two numbers are the program's check on itself.
Each body is a class with `var` fields that the integrator updates in place in an `Array`, the
offset between two bodies is a tuple taken apart by `val (dx, dy, dz) = …`, and the energies are
printed with an `f"…"` format.
