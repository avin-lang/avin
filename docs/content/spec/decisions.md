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
- A call's type argument that nothing at the call decides stays open until the end of the
  enclosing function body, settled by the first later use that needs it (a branch it joins, a
  store, a use as a value of a known type), and is an error at the call if nothing settles it.
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
- Any method taking one argument can be called infix, `a max b` for `a.max(b)`, at the caller's
  choice: there is no annotation that permits or forbids it. There is no postfix form.
- An infix call names a method of its receiver's type, resolved as the dotted call is; it never
  falls back to a function `m(a, b)`, and a method of two parameters is not called infix with a
  tuple. It stays on one line unless the line ends with the method's name, and then the argument is
  the indented block below. Keywords such as `then`, `do`, `is` and `match` never name the method.
- A subclass's constructor evaluates the superclass's arguments, runs the superclass's body on the
  object, then stores its own parameters and runs its own body. The superclass's arguments cannot
  call a method or use `self`; a parameter's name in them reads the argument.
- A subclass's fields are its superclass's, in order, then its own. Fields are never overridden.
- Only public methods can be overridden. An override takes the same parameter types and yields the
  same result or, for a class result, a subclass of it (covariant results). An override cannot be
  private. A method with type parameters of its own is overridden by one declaring as many, matched
  by position (bounds, once they exist, must match too).
- An abstract method is a method without a body, allowed only in an `abstract` class; an abstract
  class cannot be instantiated, and every concrete subclass implements each abstract method.
- A call through a receiver whose static type is an `open` or `abstract` class reaches the override
  of the object's class; a call through a final class's type, and `super.m()`, are direct.
- `if` and `match` branches yielding different classes join to their nearest common superclass;
  where they have none, to the expected type when both conform to it, such as a trait.
- A class implementing a trait's method marks it `override`, whether the trait's method is abstract
  or has a default body, as an override of a superclass's method is marked. A trait's methods are
  public. A trait may override a default of a trait it requires, marked `override` and with a body;
  in every type conforming to it, its body replaces the default. Redeclaring a required trait's
  method without `override` is an error.
- A class conforms to a trait once: two different type argument lists for one trait, reached by any
  paths, are an error. A trait's own default bodies count as inherited implementations, so a
  superclass's method and a trait's default of one name, or two traits' defaults, must be overridden.
- A trait-typed call reaches the implementation through the class's method table, at a position the
  trait has in every conforming class's table (the whole program is known), so it costs one virtual
  call.
- A trait's abstract `val name: T` is supplied by a public field of that name of each conforming
  type: a class's `val` or `var` field, own or inherited, or a one-variant `data` type's field through
  an impl. A `var` field satisfies a `val`; a method or a private field does not. Read through the
  trait it is a method table entry the compiler fills with a method that loads the field; read
  through a class type it is the field. It is never assigned through the trait.
- An `object` may extend an `open` or `abstract` class, constructed from the top down as a subclass
  is, overriding its methods and conforming to it; it implements every abstract member it inherits.
- A trait's method, or an overridable class method, may have type parameters of its own and be
  called dynamically. The whole program is known, so each list of its own type arguments that a call
  through a table uses gets a slot of its own in the table of every type implementing the method,
  filled with that type's implementation instantiated at those arguments; such a call also costs one
  virtual call.
- An `impl` adds methods and conformances, never state. Its methods are a class's or an object's own
  methods; for a `data` or built-in type they are called directly. Inside them the receiver is
  implicit, as in a class, with a one-variant `data` type's fields in scope by bare name.
- The methods of `impl Trait for Type` implement the trait's methods without `override`, and each is
  a method of the trait or of one it requires. A `data` type's method implementing a trait method is
  declared in an impl of that trait.
- A `data` type conforming to a trait gives every variant's descriptor the same method table, at the
  trait positions classes use, so a trait-typed call costs the same whatever the value is.
- A generic impl writes the type's parameters, in order, as its own: `impl[T] Show for Box[T]`.
- Settled 2026-11-03: [bounds](/spec/generics/#bounds) are traits or classes, written `[T: A + B]`.
  A value of a bounded `T` has its bounds' methods, `val`s and visible fields, and is a value of
  each bound's type; type arguments are held to their bounds where written and where inferred, and
  a bound's own type arguments are inferred from the argument. A call through a bound is direct in
  each instance, and goes through the method table only where the type argument is a trait type or
  an open class. An impl's type parameters take its type's bounds and write none of their own.
- Settled 2026-11-03: `Self` is written only in a trait's methods and names the implementing type;
  a trait mentioning it, itself or through a trait it requires, is only a bound. In its default
  bodies `self` is a `Self`. A trait type is a type argument for its own bound when it does not
  mention `Self`.
- [Variance](/spec/generics/#variance) applies only where both type arguments are reference types,
  so it never converts: `List[string]` is a `List[Any]`, `List[int]` is not. Tuple parts and
  function results are covariant and function parameters contravariant on the same terms.
- A variance error is reported in private members as in public ones; there is no escape for a
  private `var`. The methods an impl adds to a `data` type are not variance positions; the traits
  its impls name are.
- A type pattern matches an object whose type conforms to the pattern's type through variance, as
  the checker's subtyping does: a `List[Dog]` held as an `Any` matches `List[Animal]`.
- The built-in types belong to the core library's module, `avin`; an impl adding methods to one is
  written there.
- Functions and methods may be overloaded by parameter types. A call takes the most specific
  overload that accepts its arguments, chosen at compile time; an ambiguous call is an error.
  Same-named declarations with the same parameter types are an error.
- [Overloads](/spec/declarations/#functions) are the functions of one name in one scope. A block's
  local functions of a name hide every outer function of that name, whatever their parameter types,
  and a program's own top-level functions hide the prelude's of the same name.
- Choosing an overload tries integer literals and lambdas against each candidate's parameter; every
  other argument is checked once on its own, and only its type is tried. A literal fits any
  parameter that can hold it.
- An integer literal prefers its own type, `int` (or `long` for a literal too large for `int`), when
  that is all that separates the overloads: `f(1)` with `f(int)` and `f(long)`, or with `f(int)` and
  `f(real)`, calls `f(int)`. This applies only where no overload is the most specific, and only to an
  overload that is also the most specific by the arguments that are not literals; `f(1)` with
  `f(long)` and `f(real)` stays ambiguous, and a generic `f[T](x: T)` loses to `f(x: long)` by
  specificity before any literal rule applies.
- A method named without a call, `account.deposit`, is a function value bound to its receiver: a
  closure holding `account`, of the method's type without the receiver. Inside the class, a bare
  method name is the same closure over `self`.
- Settled 2026-11-03: a method value evaluates its receiver once, where the value is made. Through a
  trait-typed or open-class receiver it calls the implementation the object's class has, through
  the object's method table at each call; the object is fixed, so this is the implementation it had
  when the value was made. The class's type arguments come from the receiver's type; a method's own
  are written (`box.pair[bool]`) or decided by the expected function type, and one left undecided
  is an error. A method value takes every parameter, by-name ones still by name, and private
  methods are values only where they could be called. Two values of one method on one object are
  two closures, so `===` tells them apart.
- Settled 2026-11-03: `loop` is a statement that repeats its indented body (or `loop do body` on one
  line) until a `break` or `return` leaves it. `loop` is a keyword.
- Settled 2026-11-03: `do … while` tests at the end, so its body runs at least once. The body is an
  indented block closed by a line starting `while test`, or one statement written `do stmt while
  test`; an optional `else` runs when the test fails. `break` carries the loop's value as in
  `while`, `continue` runs the test, and the test cannot read names the body declared.
- `string` is a built-in type, not a class: it is final, has its own representation, gets its
  methods from the library through `impl string`, conforms to traits through `impl`, and is matched
  by the type pattern `s: string` like any other type.
- Settled 2026-11-03: the library's [string methods](/spec/types/#string) are `byteLength`,
  `charCount`, `isEmpty`, `nonEmpty`, `startsWith`, `endsWith`, `contains`, `slice`, `toUpper`,
  `toLower`, `trim`, `trimStart`, `trimEnd`, `repeat` and `replace`. A slice traps at its call
  when its offsets are out of order or outside the string, or either falls inside a character;
  `repeat` traps on a negative count. Casing is the simple mapping, one character for one; trimming
  removes Unicode `White_Space`; `replace` replaces every occurrence, left to right and not
  overlapping, and an empty pattern occurs nowhere. Methods yielding a collection or iterator
  (`split`, `chars`, `bytes`) wait for the collections.
- Settled 2026-11-03: a built-in type's impl in the core library may declare a method as an
  `@intrinsic`, whose type takes the receiver first.
- Settled 2026-11-03: the string methods answering with `Option` are `indexOf` (with an optional
  starting offset), `lastIndexOf`, `toInt`, `toLong` and `toReal`. Offsets are bytes. A number is read
  whole: `toInt` and `toLong` take an optional `+` or `-` and ASCII digits, so `"+3"` is 3 and
  `"-0"` is 0, and refuse white space, separators and overflow (`"-9223372036854775808"` is a
  `long`); `toReal` is correctly rounded, takes decimal only, and refuses white space, `nan`, `inf`,
  hexadecimal and a value too large to be finite. A refusal is `None`, never a trap.
- A generic overload is compared by the same rule as any other: `f(x: int)` is more specific than
  `f[T](x: T)`, which accepts an `int` while `f(int)` does not accept a `T`.
- Whether a parameter is by-name does not distinguish overloads. An argument some candidate takes by
  name is delayed; an overload that takes it strictly evaluates it once, at the call.
- An overloaded function used as a value needs an expected function type, which chooses the
  overload. `main` cannot be overloaded.
- Methods overload by the same rules, among the methods of a name the receiver's static type has,
  inherited ones included. A method is identified by its name and parameter types: an override, a
  trait implementation and a method table position each match by both, so a subclass overrides one
  overload and inherits the others. `override` matching no inherited parameter types is an error.
- A clause is a line of the function's name and arguments followed by `=`, a guard or indented
  guards; a line of the same name followed by `->` or nothing is another declaration, so two
  abstract overloads may follow each other.
- A class's parameter list is its one constructor. Class parameters are private immutable fields
  (`val` makes one public and read-only, `var` public and mutable), and the statements in the class
  body are the constructor's code, run top to bottom when an instance is created.
- Inside a class, a local, a parameter or a local function hides a member of the same name, and a
  member hides a top-level declaration; `self.name` always reaches the member. In the class body,
  a class parameter's name is its field, so it reads what an assignment to a `var` stored.
- A private member is private to its class, not to one instance: the class's constructor and
  methods, with the lambdas and local functions in them, may use it on any instance of the class.
- A class body cannot `return`. A field declared in the body without a type has its initializer's.
- [Initialization](/spec/declarations/#initialization) is checked at compile time. Through the last
  field's initializer, a class or object body uses `self` only to read a field already set and to
  assign a field: a method call in any spelling (bare name, `self.m()`, `super.m()`), passing,
  storing or returning `self`, a method value of `self`, and a lambda or local function using a
  field or `self` are errors.
  An `open` or `abstract` class's body runs before its subclasses' fields are set, so the limits
  hold for the whole of it, and it never calls a method a subclass may override.
- An [object](/spec/declarations/#object) is made once, before `main`; its name is a value of its own
  type and cannot be called. It shares its name only with its companion class, whose constructor
  the name is in call position, while the bare name and `Name.member` are the companion.
- Module-level `val`s, `var`s and `object`s initialize before `main`, each after the values its
  initializer reads directly or through the functions it calls, the rest in declaration order; a
  cycle is a compile error naming the chain.
- Every function declaration parameter has an explicit type.
- A [default argument](/spec/declarations/#default-and-named-arguments) is evaluated at each call
  that omits it, in the declaring function's scope, and may use the parameters before it. Named
  arguments follow the positional ones, in any order; the arguments written at a call are evaluated
  in the order written, then the omitted ones' defaults in parameter order. Names and defaults belong
  to a declaration: a function value is called by position. A variant takes its fields by name too.
  The default of a by-name parameter is delayed like any argument to it.
- Settled 2026-11-03: methods and class constructors take defaults as functions do. A method's
  default sees `self`, the receiver's members and the parameters before it; a constructor's sees the
  parameters before it and no `self`. Defaults belong to the method that introduces the parameter:
  an override, and a method implementing a trait's, takes them and may not declare its own, so a
  call's defaults are the same whichever class's method runs. A trait method's defaults apply through
  the trait's type. A method of one required parameter and defaults may be called infix.
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
- Settled 2026-11-03: `List[+T]` is the core library's `data` type with variants `Cons(first, rest)`
  and `Nil`. `::` and list patterns always mean the core library's `List`, whatever `List`, `Cons`
  or `Nil` name where they are written. Every `List` method loops rather than recursing, so no
  length deepens the stack, and lists compare and hash by contents and order lexicographically when
  their elements are ordered.
- Sequences are matched with the brackets that build them: `[]` matches any empty sequence and
  `[a, b]` any sequence of exactly two elements.
- `Buffer` is the only growable mutable sequence, and `Deque` the mutable sequence for both ends.
  There is no mutable list type: every collection has a builder, through one library trait, and
  `List`'s builder appends in constant time and yields an immutable `List`.
- `..` and `..<` expressions construct iterable `Range` values.
- An array is made with every element set, by `Array.fill(n, value)`, `Array.tabulate(n, f)` or
  `Array.empty()`; Avin has no null, so no array holds an unset element, and `Array[T]()` is an error.
  `xs(i)` calls `xs.apply(i)`, `xs(i) = v` calls `xs.update(i, v)`, and the compound, `:=` and `++`
  forms evaluate `xs` and `i` once. A call with untyped arguments before `=` is an assignment, so a
  local function's declaration types its parameters or has none.
- A range's step is set with `by`; a step of zero traps, and iteration never steps past the limit or
  the type's largest or smallest value.
- `for` is comprehension sugar over `map`, `flatMap`, filtering and `foreach`.
- The collection hierarchy is rooted at `Iterable`, and `Iterator` is itself `Iterable`.
- `print` is an ordinary library function. It renders with `Display`, separates its arguments with
  `, ` and ends with a newline. `print(debug(x))` selects `Debug`.
- `debug(x)` yields `x`'s `Debug` form as a string. `Writer` writes strings (with `writeChar`,
  `writeInt`, `writeField` and `writeSpaces` beside `write`); `FormatSpec` is `width`, `precision`
  and `left`, as in Sysl; `toString()` is a default body of `Display`.
- A built-in type conforms to a trait through an impl and implements every method of the trait,
  since a trait's default bodies are shared by objects and a built-in value is not one.
- A variant's fields take defaults, each a function of the fields before it, as a constructor's
  parameters do.
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
  compound forms that read every place first. Its places are `var` locals and `var` fields, each
  field's object evaluated before anything else.
- Inside brackets whose commas separate items (call arguments, tuple and collection literals), a
  `return` or `break` value stops at the comma, so `f(a = return 1, b = 2)` has two arguments; a
  parenthesised `return 1, 2` is two tuple items, and `return (1, 2)` returns a tuple. An indented
  block inside the brackets starts a line again, and a one-line lambda body never takes a comma list.
- Actors are built into the language: `actor` declares one.
- State machines are built into the language: `machine` declares one, and `state` declares each of
  its states.
- Slate's `dal` is not part of Avin.
- `Any` holds a value of any type. A value converts to it at a single site (an argument, a binding,
  an assignment, a field, a result, a tuple part written out, a branch expected to yield one), never
  inside a container. A string, a tuple and a `data` value need no box, being objects whose header
  names their type; numbers, `char`, `bool` and `unit` are boxed.
- A type pattern `x: T` matches a value whose run-time type is `T`, a subclass of it or a class
  conforming to it, with type arguments compared exactly. One that can never match is an error; one
  covers only where the matched value's type already conforms to `T`. In a clause a type pattern
  naming the parameter's own type is an error.
- Every type has `==` and a hash. Built-in types, strings, tuples and `data` compare and hash by
  value; class instances, `object`s and closures by identity, which always agree, until the class
  implements or derives `Eq`. `derives Eq, Hash` gives both from the same fields; implementing one
  of `==` and the hash without the other is an error. Keeping a hand-written pair consistent is the
  programmer's responsibility.
- `==` between two values whose static types share no common type is a compile error. On `Any` and
  on trait-typed values `==` is decided at run time through the type's entries: values of different
  run-time types are unequal. `===` stays defined only on reference types, and not on `Any`.
- Settled 2026-11-03: the operator traits are the core library's `Eq` (`equals(other: Self) ->
  bool`), `Hash` (`hash() -> long`), `Ord: Eq` (`compare(other: Self) -> int`, with `less`,
  `lessOrEqual`, `greater` and `greaterOrEqual` defaulting from it), and `Add`, `Sub`, `Mul`, `Div`,
  `Rem` (`add`, `subtract`, `multiply`, `divide`, `remainder`, each `(other: Self) -> Self`) and `Neg`
  (`negate() -> Self`). Every type conforms to `Eq` and `Hash` implicitly; `==` on a type parameter
  still needs a bound that is or requires `Eq`. The built-in types implement the traits their
  operators have, in the core library, and an operator on a built-in type is the machine's
  operation, in generic instances too.
- Settled 2026-11-03: an impl of a trait may bound the type's parameters further (`impl[T: Ord] Ord
  for Box[T]`), making the type conform only for type arguments meeting them; for now only on data
  types and traits that are only bounds.
- Settled 2026-11-03: `derives` takes `Eq`, `Hash`, `Ord`, `Display` and `Debug`. A class derives
  from its constructor parameters, plain, `val` and `var` alike, and not from fields its body
  declares; a data type from each variant's fields. A class derives `Eq` and `Hash` together, and
  `Ord` only with an `Eq` by the same fields, derived or its own; a data type has both already, so
  deriving them there changes nothing. `Display` writes `Name(a, b)`, each field in its own `Display`
  form, and a variant without fields as its name; `Debug` writes the same shape with each field's
  `Debug`, so strings inside are quoted. A width pads the whole. Deriving a trait the type also
  implements, or one a field's type does not implement, is an error naming the field. A generic data
  type's derived `Ord` holds for the type arguments that are ordered; deriving `Display` or `Debug`
  on a generic type, and `Ord` on a generic class, needs the parameters bounded by the trait where
  the type declares them.
- `x.equals(y)` and `x.hash()` may be called on a value of any type parameter, bounded or not.

**Project and tooling**

- A project lists packages; a package has an `avin.hocon` manifest; a module is a directory.
- A program's root is a directory, and each directory under it is a module named by its path from
  the root (`a/b` is `a.b`). The root directory's own files are the root module, which has no name
  and holds `main`; they take no header. Every other file begins with `module a.b`, naming the module
  its directory makes it part of. A program of one file is compiled alone, and its header, if any,
  names its module.
- A module's files share one namespace: what one declares, every other sees by its bare name, its
  `private` declarations included. Another module's declarations are reached only by importing them,
  per file: `import a.b.X`, `import a.b.{X, y as z}`. There is no wildcard import. An unknown
  import, an import of a private
  declaration and an import of a name the file's module declares or another import already binds are
  errors; an unused import is not.
- A file sees its own module's declarations, then its imports, then the standard library's public
  declarations in module `avin`, which need no import; the nearer hides the farther. Program modules
  may not be `avin` or under it.
- Modules may import one another in a cycle; module-level values initialize in the order their uses
  need across the whole program.
- A class and its companion object are in one module, in any of its files.
- One `avin` command does everything: build, run, test, deps and config.
- Testing follows Sysl's model: `@test`, `@tests`, setup and teardown, and the built-in runner.
  Every feature ships with tests.
- `avin build file.avin` writes `./file` by default; `-o` overrides it. A failed build leaves
  existing output untouched.
- The collector is Sysl's `sh.sysl.gc`, shared with Slate. Its API may change if Slate changes with
  it.
- The compiler's untyped AST, typed HIR, lowered IR and LLVM output are kept as distinct stages.
- A class and its companion object use each other's private members, in both directions.
- An initializer's dependencies count every lambda it makes as if it were called, and every function
  it calls in full, whatever branch is taken, so a cycle that no run could follow is still rejected.
- An impl for particular type arguments of a generic type (`impl Show for Box[int]`) is not allowed:
  an impl covers every instance of its type. Allowing one needs a rule for its overlap with a generic
  impl of the same trait.
- The singleton declaration keeps the keyword `object`.
- A program's binding shims are one Sysl package that depends on the runtime's, built into one
  archive the program links in place of the runtime's, so all of a program's Sysl code shares one
  copy of Sysl's library. The archive is cached by a hash of the shims, the Sysl version and the
  runtime's source.
- A binding is a top-level function. Across the boundary a `bool` is a byte and a `char` its code
  point; `string` is copied each way.
- `Option[+T]` and `Result[+T, +E]` are `data` types of the core library, written in Avin, with
  their methods in an `impl`. Those methods are not variance positions, so `getOrElse(default: -> T)`
  takes `T` in a parameter and needs no lower bound: on an `Option[Dog]` seen as an `Option[Animal]`,
  the default is an `Animal`. See [errors](/spec/errors/#option-and-result).
- `?` returns from the function or lambda it is written in, so that one's result type must be
  declared or, for a lambda, known where it is written. It cannot leave a delayed argument or a class
  body.

**Deferred**

- Public C interoperability. Until then, C is reached through Sysl.
- Higher-kinded, path-dependent, union and intersection types; implicit parameters; macros.
- Async and await.
- Partial-function literal syntax, which the user has explicitly left undecided.

## Proposed

- A module is not named in code: `import a.b` is an error, and `b.X` does not reach `a.b`'s `X`.
  Every use of another module's declaration goes through an import of that declaration. The
  alternative is qualified access through an imported module name, `import a.b` then `b.X`.

- An infix method call binds more loosely than arithmetic, bitwise, shift, cons and range operators
  and more tightly than comparisons, `is`, `&&` and `||`, and groups to the left, so
  `xs contains x && ok` is `(xs contains x) && ok`.

- A literal pattern does not match an `Any` (`5 -> …` on an `Any` is an error); `n: int if n == 5`
  says it.
- A tuple's type at run time is the type it was made with: `(animal, 1)` with `animal: Animal` is an
  `(Animal, int)` whatever its class, and a type pattern `(Dog, int)` does not match it. A tuple made
  as `(Dog, int)` matches `(Animal, int)` too.
- After its own last field, an `open` or `abstract` class's body may call a method that cannot be
  overridden (a private one) when that method is safe: it uses `self` only to read and assign
  fields and to call other safe methods, so it cannot reach a subclass's field before the subclass
  sets it. The settled rule read literally forbids every method call in such a body, since the
  subclass's fields are not yet set; this allows the calls that provably observe nothing unset.
- The thread model ([threads](/spec/threads/)): `avin.threads.spawn(body)` starts a closure on a
  new thread with an 8 MiB stack and `join()` gives back its result; a structured `scope` joins
  every thread started in it; a trap in any thread stops the program, and the program ends when
  `main` returns. The heap, module-level values and captured `var`s are shared. The library offers
  `Mutex[T]` owning its value, `Condition`, sequentially consistent `Atomic[T]`, `Channel[T]` and
  `Once`, and every wait marks its thread safe for the collector. A data race is not undefined
  behaviour: heap fields and module-level variables are accessed with LLVM's `unordered` ordering,
  so a racing read sees some value written to that word, and a constructor ends with a release
  fence, so no thread sees an object without its header and constructor-set fields. Actors are
  scheduled onto a pool of these threads.
- A string's byte offsets and lengths are `int`, and a string holds at most the largest `int`
  (2,147,483,647) bytes; making a longer one stops the program as running out of memory does. The
  alternative is `long` offsets, which every caller would then narrow to index with an `int`.
- A trait's `override` may also give an abstract method of a trait it requires a body, not only
  replace a default: `trait Square: Shape` with `override area() -> int = side() * side()` makes
  `area` concrete for every class conforming to `Square`.
- An iterator is `hasNext()` and `next()`, and `next()` on an exhausted iterator traps. The
  alternative, `next() -> Option[A]`, would make an `Option` per element.
- A `for` without `yield` traverses its source through `iterator()` (a range written in place is
  counted and an array indexed) rather than calling `foreach` with the body as a closure. For the
  library's collections the two do the same thing; through the iterator, `break`, `continue` and
  `return` leave the body directly, with no jump out of a closure. A type with `foreach` and no
  iterator is then not a `for` source.
- `Array.fill(n, value)` evaluates `value` once and shares it between the elements; per-element
  values come from `Array.tabulate`. The alternative is a by-name `value`, evaluated per element.

## Open

- Resource cleanup: a `defer` construct or a library `use`.
- How actors work: how messages are declared and sent, mailboxes and replies, how actors are
  scheduled onto threads, what an actor may share with others on the heap, and failure and
  supervision.
- How state machines work: events and transitions, data held by the machine and by each state,
  entry and exit actions, checking that every event is handled, and whether an actor's behaviour
  can be a machine.
- Whether `actor`, `machine` and `state` are reserved everywhere or only where a declaration
  starts; `state` is a common name for a variable or field.
