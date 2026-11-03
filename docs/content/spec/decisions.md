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
- Programs are multi-threaded: threads run in parallel and share one garbage-collected heap.
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
  Same-named declarations with the same parameter types are an error.
- [Overloads](/spec/declarations/#functions) are the functions of one name in one scope. A block's
  local functions of a name hide every outer function of that name, whatever their parameter types,
  and a program's own top-level functions hide the prelude's of the same name.
- Choosing an overload tries integer literals and lambdas against each candidate's parameter; every
  other argument is checked once on its own, and only its type is tried. A literal fits any
  parameter that can hold it, so `f(1)` with `f(int)` and `f(long)` is ambiguous.
- A generic overload is compared by the same rule as any other: `f(x: int)` is more specific than
  `f[T](x: T)`, which accepts an `int` while `f(int)` does not accept a `T`.
- Whether a parameter is by-name does not distinguish overloads. An argument some candidate takes by
  name is delayed; an overload that takes it strictly evaluates it once, at the call.
- An overloaded function used as a value needs an expected function type, which chooses the
  overload. `main` cannot be overloaded.
- A class's parameter list is its one constructor. Class parameters are private immutable fields
  (`val` makes one public and read-only, `var` public and mutable), and the statements in the class
  body are the constructor's code, run top to bottom when an instance is created.
- Inside a class, a local, a parameter or a local function hides a member of the same name, and a
  member hides a top-level declaration; `self.name` always reaches the member. In the class body,
  a class parameter's name is its field, so it reads what an assignment to a `var` stored.
- A private member is private to its class, not to one instance: the class's constructor and
  methods, with the lambdas and local functions in them, may use it on any instance of the class.
- A class body cannot `return`. A field declared in the body without a type has its initializer's.
- Every function declaration parameter has an explicit type.
- A [default argument](/spec/declarations/#default-and-named-arguments) is evaluated at each call
  that omits it, in the declaring function's scope, and may use the parameters before it. Named
  arguments follow the positional ones, in any order; the arguments written at a call are evaluated
  in the order written, then the omitted ones' defaults in parameter order. Names and defaults belong
  to a declaration: a function value is called by position. A variant takes its fields by name too.
  The default of a by-name parameter is delayed like any argument to it.
- A function may be defined by [clauses](/spec/declarations/#clauses): its signature with no body,
  then lines `f(patterns) = body`, tried in order. A clause is guarded inline,
  `f(n) | n > 0 = body`, or by an indented block of `| condition = body` lines in which `| _` always
  holds; when every guard fails, the next clause is tried. Clauses must cover every argument, and a
  clause counts toward that only through an unguarded body.
- Comparison patterns bind a name or `_` beside one literal bound or between two (`n < 0`,
  `0 <= n < 10`), and `n + k` matches an integer of at least the positive literal `k`, binding the
  value minus `k`. Both go wherever a pattern does, on integers (comparisons also on bytes and
  chars), and they count toward exhaustiveness, as literals and ranges do.
- Variadic parameters are written `values: T*`; by-name parameters are written `value: -> T`, with no
  memoization.
- Blocks yield their last expression. `if`, `while` and loop values follow Sysl.
- All of Sysl's operators are supported, including comparison chains.
- Built-in types are lowercase (`int`, `bool`, `string`, `unit`, `char`, `real`, `iN`, `byte`);
  other types are PascalCase; functions and methods are lowerCamelCase. These are conventions, not
  syntax rules.
- `int` is `i32` and `long` is `i64`; they are the only integer aliases. Signed integers of any fixed width are available.
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
- A tuple needs no parentheses where it fills the rest of a line: `= a, b`, `return a, b`, a
  block's line. Bindings take tuples apart (`val q, r = divmod(n, d)`), and `a, b = b, a` is a
  multiple assignment: a statement that makes the whole right side before storing anything, with
  compound forms that read every place first.
- Actors are built into the language: `actor` declares one.
- State machines are built into the language: `machine` declares one, and `state` declares each of
  its states.
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
- Async and await.
- Partial-function literal syntax, which the user has explicitly left undecided.

## Proposed

- An integer literal prefers its own type when overloads differ only in the types a literal could
  take: `f(1)` with `f(int)` and `f(long)`, or with `f(int)` and `f(real)`, would call `f(int)`
  rather than be ambiguous. As settled, such a call is ambiguous.
- A method named without a call, `account.deposit`, is a function value bound to its receiver: a
  closure holding `account`, of the method's type without the receiver. Inside the class, a bare
  method name is the same closure over `self`.

## Open

- Resource cleanup: a `defer` construct or a library `use`.
- The thread model: how threads are started and joined, what a program may assume about mutable
  state other threads write (module-level `var`s and `object`s included), the synchronization the
  library provides, and how the collector coordinates the threads.
- How actors work: how messages are declared and sent, mailboxes and replies, how actors are
  scheduled onto threads, what an actor may share with others on the heap, and failure and
  supervision.
- How state machines work: events and transitions, data held by the machine and by each state,
  entry and exit actions, checking that every event is handled, and whether an actor's behaviour
  can be a machine.
- Whether `actor`, `machine` and `state` are reserved everywhere or only where a declaration
  starts; `state` is a common name for a variable or field.
