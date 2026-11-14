---
title: Debugging
summary: "`avin build -g`, and stopping a program at a line of its Avin source in lldb."
weight: 20
---

`avin build -g` builds a program with debug information: where each piece of its code comes from in
the Avin source, and its functions' parameters and locals. A debugger such as lldb then stops at a
line of an `.avin` file, shows the call stack as the program's own functions and reads the
variables. On macOS the build also writes `<executable>.dSYM` beside the executable, which is where
lldb finds the information; on other systems the executable carries it.

```avin
fact(n: int) -> int =
  if n <= 1 then 1
  else n * fact(n - 1)

main() =
  val x = fact(5)
  println(x)
```

```sh
avin build fact.avin -g -O0
lldb ./fact
```

```text
(lldb) b fact.avin:3
Breakpoint 1: where = fact`<program>.fact + … at fact.avin:3:17
(lldb) run
(lldb) bt
* frame #0: fact`<program>.fact(n=5) at fact.avin:3:17
  frame #1: fact`<program>.main at fact.avin:6:11
  frame #2: fact`main + 56
(lldb) c
(lldb) bt
* frame #0: fact`<program>.fact(n=4) at fact.avin:3:17
  frame #1: fact`<program>.fact(n=5) at fact.avin:3:12
  frame #2: fact`<program>.main at fact.avin:6:11
(lldb) frame variable
(int) n = 4
```

What the debugger sees:

- **Functions** by their symbols, `<program>.fact` for a function of the program's own module and the
  module's name for the library's; an instance of a generic function by its type arguments,
  `<program>.twice[int]`; a lambda as the function it is written in, numbered, `<program>.main.lambda#1`.
- **Lines and columns** of the source as the reader has it: a literate program's places are the
  lines of its `.lavin` document, so `b doubling.lavin:12` stops at that line of the document.
- **Variables**: every parameter, and every local but one a lambda shares (it lives in a box the
  debugger is not shown). `int`, `long`, `byte`, `char`, `bool` and `real` read as numbers,
  characters and truth values (lldb names them by its own words, `double` for `real` and `char32_t`
  for `char`); a string, an object or a function value reads as its address, typed by its Avin type,
  `(string *) s = 0x…`.

`-g` changes nothing about what the program does. It goes with either optimization level, but at
`-O2` the optimizer moves and merges code and keeps few variables, as optimized code always does, so
`-g -O0` is the build to debug. `avin emit-llvm -g` writes the module with its debug metadata, for
reading what the compiler made. `-g` is a flag of `build` and `emit-llvm` only: `avin run` and
`avin test` build into their own cache and never keep debug information.
