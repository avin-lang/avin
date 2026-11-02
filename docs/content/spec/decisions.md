---
title: Settled decisions
summary: Every language and project decision made so far, in one terse list — settled and open.
weight: 30
---

This page is the single record of decisions. A chapter of this specification elaborates each one; when
a decision changes, this page and the chapter change together.

## Settled

**Identity and direction**

- Avin is a natively compiled, garbage-collected application language with a deliberately small
  type system, a rich standard library and ergonomic syntax.
- Its type system, runtime and library are designed together: see [design choices](/spec/design/).
- Syntax stays very close to Sysl. Where Sysl has no form, this specification defines one in the
  same style.
- Execution speed is a primary goal.
- Typing is nominal. Classes instantiate objects; there are no duck-typed objects.
- Inference is a useful approximation of Hindley–Milner, not full HM: see
  [inference](/spec/generics/#inference).
- The compiler is written in Sysl. Self-hosting is optional.
- **The standard library is written in Avin.** Sysl is reached through
  [bindings written in Avin](/spec/sysl-bindings/), which the compiler turns into shims that Sysl
  checks. The runtime is written in Sysl.
- The domain is `avinlang.dev`. Compiler modules use `dev.avinlang.compiler`, and the standard
  library uses the reserved root `avin`.
- Building the Kutter video editor natively is a long-term application goal.
- Sysl's Ada-inspired features (contracts and the like) are a long-term direction.

**Syntax**

- Functions and function types use `->`, never `=>`.
- `match` is postfix, with indented `pattern -> body` arms.
- `data` declares algebraic data types.
- Classes are final unless declared `open` or `abstract`; overriding requires `override`.
- Inheritance is written with `:`: `class C(…): Base(…), Trait`, one superclass with its
  constructor arguments and then traits, as in `trait A: B`.
- The receiver is implicit: members are in scope by their bare names, and `self` names the instance.
- Functions and methods may be overloaded by parameter types. A call takes the most specific
  overload that accepts its arguments, chosen at compile time; an ambiguous call is an error.
  Same-named declarations with the same parameter types are clauses of one function.
- A class's parameter list is its one constructor. Class parameters are private immutable fields
  (`val` makes one public and read-only, `var` public and mutable), and the statements in the class
  body are the constructor's code, run top to bottom when an instance is created.
- Every function declaration parameter has an explicit type.
- Parameter patterns, multiple clauses selected by pattern, and clause guards are supported.
  Their exact spelling is provisional.
- Variadic parameters are written `values: T*`; by-name parameters are written `value: -> T`, with no
  memoization.
- Blocks yield their last expression. `if`, `while` and loop values follow Sysl.
- All of Sysl's operators are supported, including comparison chains.
- Built-in types are lowercase (`int`, `bool`, `string`, `unit`, `char`, `real`, `iN`, `byte`);
  other types are PascalCase; functions and methods are lowerCamelCase. These are conventions, not
  syntax rules.
- `int` is `i32`, and it is the only integer alias. Signed integers of any fixed width are available.
  There are no unsigned integer types. `real` is binary64, and there is no single-precision type.
  `char` is a Unicode scalar.
- `byte` is an octet (0–255) for binary data, with bitwise operations and no arithmetic.
- Integer overflow traps. `+%`, `-%`, `*%` wrap and `+|`, `-|`, `*|` saturate, explicitly.
- A sequence is indexed by calling it: `xs(i)`, and `xs(i) = v` to assign. Sequences implement
  function types, and any library type can implement a function type through `apply`.
- `[1, 2]` is an immutable `Vector`, `{1, 2}` a `Set` and `{"a": 1}` a `Map`, unless the expected
  type asks for another collection (`val b: Buffer[int] = [1, 2]`), which the literal then builds.
  `{a: 1}` means `{"a": 1}`. `{}` is the empty Set and `Map.empty` the empty Map.
- `x :: xs` prepends to an immutable `List`, and `head :: tail` is a list pattern, for recursive
  list code. `::` is right-associative.
- Sequences are matched with the brackets that build them: `[]` matches any empty sequence and
  `[a, b]` any sequence of exactly two elements.
- `Buffer` is the only growable mutable sequence, and `Deque` the mutable sequence for both ends.
  There is no mutable list type: every collection has a builder, through one library trait, and
  `List`'s builder appends in constant time and yields an immutable `List`.
- `..` and `..<` expressions construct iterable `Range` values.
- `for` is comprehension sugar over `map`, `flatMap`, filtering and `foreach`.
- The collection hierarchy is rooted at `Iterable`, and `Iterator` is itself `Iterable`.
- `print` is an ordinary library function. It renders with `Display`, separates its arguments with
  `, ` and ends with a newline. `print(debug(x))` selects `Debug`.
- Avin has Sysl's full string interpolation and formatting.
- An interpolated string is a method call on `StringContext`: `p"a $x b"` is
  `StringContext(["a ", " b"]).p(x)`. `s`, `f` and `raw` are library methods, and a program defines
  its own interpolator by implementing a trait for `StringContext`.
- Nested helper functions see their enclosing scope. Closures are full closures, and `return` inside a
  lambda is local to the lambda.
- `match` is a value-producing expression with full destructuring.
- Slate's `dal` is not part of Avin.

**Project and tooling**

- A project lists packages; a package has an `avin.hocon` manifest; a module is a directory.
- One `avin` command does everything: build, run, test, deps and config.
- Testing follows Sysl's model: `@test`, `@tests`, setup and teardown, and the built-in runner.
  Every feature ships with tests.
- `avin build file.avin` writes `./file` by default; `-o` overrides it. A failed build leaves
  existing output untouched.
- The collector is Sysl's `sh.sysl.gc`, shared with Slate. Its API may change if Slate changes with
  it.
- The compiler's untyped AST, typed HIR, lowered IR and LLVM output are kept as distinct stages.

**Deferred**

- Public C interoperability. Until then, C is reached through Sysl.
- Higher-kinded, path-dependent, union and intersection types; implicit parameters; macros.
- Async, await and concurrency.
- Partial-function literal syntax, which the user has explicitly left undecided.

## Open

- Resource cleanup: a `defer` construct or a library `use`.
- The exact spelling of function clauses, guards and parameter patterns.
