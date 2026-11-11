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
  store, a use as a value of a known type). If nothing settles it, it is `Nothing` where the call
  names it only where a value is read out (settled 2026-11-04, below), and an error at the call
  otherwise.
- The compiler is written in Sysl. Self-hosting is optional.
- **The standard library is written in Avin.** Sysl is reached through
  [bindings written in Avin](/spec/sysl-bindings/), which the compiler turns into shims that Sysl
  checks. The runtime is written in Sysl.
- The domain is `avinlang.dev`. Compiler modules use `dev.avinlang.compiler`, and the standard
  library uses the reserved root `avin`. A library the project publishes outside the core library is
  a package in its own repository of the `avin-lang` organization, with modules under
  `dev.avinlang`: parser combinators are `github.com/avin-lang/parsing`, module
  `dev.avinlang.parsing`, written as literate Avin.
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
  same result or a type seen as it with no conversion (covariant results; see the Proposed entry
  on override results below). An override cannot be
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
- Traits may hold state (settled, user, 2026-11-04). A trait declares a field with a value,
  `val name: T = expr` or `var name: T = expr`, and every class and object conforming to it stores
  one of its own. Through the class's type it is the class's field; through the trait, or a type
  parameter the trait bounds, it is read and assigned through method table entries the compiler
  fills with a method that loads or stores the class's field. Construction runs the superclass
  first, then stores the class's parameters, then computes the fields of the traits the superclass
  does not already conform to, a trait's required traits before it and each trait once, in the order
  of the `:` list, then runs the class body. A trait's field's value may read the fields initialized
  before it and uses `self` for nothing else. Only a class or an object conforms to a trait holding
  state, or requiring one that does: an impl of it for a `data` type, a built-in type or a tuple is an
  error. A class declaring a field of a trait field's name, or conforming to two traits that declare
  fields of one name, is an error.
- A trait may re-declare a method of a trait it requires without a body, marked `override`, with a
  narrower result (settled, user, 2026-11-04): `trait SortedMap[K, +V]: Map[K, V]` re-declares
  `override updated[W >: V](key: K, value: W) -> SortedMap[K, W]`, so a call through the sub-trait
  yields the sub-trait. The re-declaration takes the same parameters, type parameters and lower
  bounds and yields a result conforming to the overridden one, and shares its position in the method
  table, so one implementation serves both traits and must yield the narrowest result. Re-declaring a
  method that has a default makes it abstract again in the sub-trait: its implementers give their own.
  [Declarations](/spec/declarations/#trait).
- An `object` may extend an `open` or `abstract` class, constructed from the top down as a subclass
  is, overriding its methods and conforming to it; it implements every abstract member it inherits.
- A trait's method, or an overridable class method, may have type parameters of its own and be
  called dynamically. The whole program is known, so each list of its own type arguments that a call
  through a table uses gets a slot of its own in the table of every type implementing the method,
  filled with that type's implementation instantiated at those arguments; such a call also costs one
  virtual call.
- An `impl` adds methods and conformances, never state. Written in a class's or an object's module,
  its methods are the class's own; for a `data` or built-in type, or written elsewhere, they are
  called directly. Inside them the receiver is
  implicit, as in a class, with a one-variant `data` type's fields in scope by bare name.
- The methods of `impl Trait for Type` implement the trait's methods without `override`, and each is
  a method of the trait or of one it requires. A `data` type's method implementing a trait method is
  declared in an impl of that trait.
- A `data` type conforming to a trait gives every variant's descriptor the same method table, at the
  trait positions classes use, so a trait-typed call costs the same whatever the value is.
- A generic impl of a trait writes the type's parameters, in order, as its own:
  `impl[T] Show for Box[T]`. An impl without `for` may also name a type built from its parameters
  (settled 2026-11-04, below).
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
- Settled 2026-11-04 (user): [polymorphic recursion](/spec/generics/#polymorphic-recursion), a
  cycle of generic code whose type arguments grow on every round, is a compile-time error naming
  the cycle, through functions, methods, local functions, lambdas and calls through traits and
  bounds alike; recursion whose type arguments do not grow is legal. Checking never runs without
  end on it.
- [Variance](/spec/generics/#variance) applies only where both type arguments are reference types,
  so it never converts: `List[string]` is a `List[Any]`, `List[int]` is not. Tuple parts and
  function results are covariant and function parameters contravariant on the same terms.
- A variance error is reported in private members as in public ones; there is no escape for a
  private `var`. The methods an impl adds to a `data` type are not variance positions; the traits
  its impls name are.
- A type pattern matches an object whose type conforms to the pattern's type through variance, as
  the checker's subtyping does: a `List[Dog]` held as an `Any` matches `List[Animal]`.
- Settled by the user 2026-11-04: [lower bounds](/spec/generics/#lower-bounds) on a function's or a
  method's own type parameters, written first: `[W >: T]`, `[W >: T: Display]`. A lower bound is a
  covariant position, so a covariant trait declares `put[W >: V](key: K, value: W) -> Bag[K, W]` as
  a virtual method; impl methods that are not variance positions are no longer the only way a
  covariant type takes its parameter in. A value of the bound is one of the parameter with no
  conversion, so a type argument is the bound or a reference type it is seen as. A call infers the
  join of the bound and the arguments, `Any` where they are references with no nearer common type;
  a written or expected type argument is held to the bound (the expected type's part is refined,
  proposed below). An override writes its method's lower
  bound or none. A type's and an impl's parameters take none.
- The built-in types belong to the core library's module, `avin`; a conformance of one is written
  there or in the trait's module. An impl that only adds methods may be written in any module
  (settled 2026-11-04, below).
- Functions and methods may be overloaded by parameter types. A call takes the most specific
  overload that accepts its arguments, chosen at compile time; an ambiguous call is an error.
  Same-named declarations with the same parameter types are an error.
- [Overloads](/spec/declarations/#functions) are the functions of one name in one scope. A block's
  local functions of a name hide every outer function of that name, whatever their parameter types,
  and a program's own top-level functions hide the prelude's of the same name.
- A local value (`val`, `var` or a pattern binding) and a local function of the same block may not
  share a name, whichever is written first (settled, user, 2026-11-04). In a nested block, a lambda
  or as a parameter of the enclosing function, a local may hide an outer local function like any
  outer name (settled, user, 2026-11-04).
- Generic local functions ([declarations](/spec/declarations/#functions)): a local function's type
  parameters follow the enclosing ones it shares, which its calls never write, so `pair[int](1)`
  writes only `pair`'s own. A local function's type parameter may not take the name of one already
  in scope rather than hiding it. Written type arguments and those an expected type gives a generic
  function used as a value are held to its bounds, as a call's are (settled, user, 2026-11-04).
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
  `charCount`, `isEmpty`, `nonEmpty`, `startsWith`, `endsWith`, `contains`, `slice`,
  `toUpperCase`, `toLowerCase`, `trim`, `strip`, `stripLeading`, `stripTrailing`, `repeat`,
  `replace` and `padTo` beside `padStart` and `padEnd`, with the reference library's names (user,
  2026-11-04), and, with the reference's names and behaviour (user, 2026-11-04), `stripPrefix`,
  `stripSuffix`, `capitalize` (only a lower-case first character changes), `stripMargin` with the
  bar or a given margin character (the characters up to U+0020 and the margin removed from each
  line's start, the terminators kept), and `head`, `last`, `headOption` and `lastOption` of the
  characters, `head` and `last` trapping on the empty string. A slice traps at its call when its offsets are out of order or outside the string, or
  either falls inside a character; `repeat` traps on a negative count. Casing is the simple mapping,
  one character for one; stripping removes Unicode `White_Space`, and `trim`, as the reference's,
  the characters up to U+0020; `replace` replaces every occurrence, left to right and not
  overlapping, and an empty pattern occurs nowhere. Methods yielding a collection or iterator
  (`split`, `chars`, `bytes`) wait for the collections.
- Settled 2026-11-03: a built-in type's impl in the core library may declare a method as an
  `@intrinsic`, whose type takes the receiver first.
- Settled 2026-11-03, the searches revised 2026-11-04 by the user: `indexOf` (with an optional
  starting offset), `lastIndexOf` (with an optional last offset), `indexWhere` (with an optional
  starting offset) and `lastIndexWhere` answer a byte offset, -1 when there is none, as the reference
  library's do, and so do a sequence's `indexOf`, `indexWhere`, `lastIndexOf` and `lastIndexWhere`,
  each with the same optional bound; a string's `indexOf` and `lastIndexOf` take a `char` as well
  as a string. Offsets are bytes. A number is read
  whole: `toInt` and `toLong` take an optional `+` or `-` and ASCII digits, so `"+3"` is 3 and
  `"-0"` is 0, and refuse white space, separators and overflow (`"-9223372036854775808"` is a
  `long`); `toReal` is correctly rounded, takes decimal only, and refuses white space, `nan`, `inf`,
  hexadecimal and a value too large to be finite. Revised 2026-11-04 by the user to the reference
  library's: `toInt`, `toLong` and `toReal` answer the number and trap on a refusal with
  `For input string: "…"`, and `toIntOption`, `toLongOption` and `toRealOption` answer `None` for it.
- Settled 2026-11-04: the core library follows the reference language's library in names and
  behaviour unless it genuinely improves on it. `Option` has `isDefined`, `isEmpty` and `nonEmpty`
  (not `isSome`/`isNone`), `get` (trapping on `None`) and `filterNot`; there is no `orNull`, since
  there is no null. `char` has `isDigit`, `isLetter`, `isLetterOrDigit`, `isWhitespace`,
  `isSpaceChar`, `isUpper`, `isLower`, `isControl`, `toUpper`, `toLower` and `asDigit`,
  classifying by the Unicode Character Database. `isWhitespace` is true for the space separators
  except the non-breaking ones (U+00A0, U+2007, U+202F) and for U+0009 to U+000D and U+001C to
  U+001F, so it differs from `strip`'s `White_Space`; `isSpaceChar` is the categories `Zs`, `Zl` and `Zp`.
  `asDigit` is the digit's value in radix 36 (Unicode decimal digits give their value, ASCII and
  fullwidth letters give 10 to 35) and is -1 for any other character, never a trap.
- Settled by the user 2026-11-04 ([collections](/spec/collections/#maps-and-sets)): `Map[K, +V]` and
  `Set[A]` are traits, as in the reference library. The hash-based collections are `HashMap` and
  `HashSet`; `TreeMap` and `TreeSet` conform to `SortedMap[K, +V]` and `SortedSet[A]`, which require
  `Map` and `Set` and re-declare the updating methods with the sorted result. Map and set literals,
  `Map(…)`, `Set(…)`, `Map.empty`, `Set.empty` and the two builders make the hash-based ones, typed as
  the traits; `SortedMap` and `SortedSet` companions, and a literal where one is expected, make tree
  ones. A method putting a value in takes `[W >: V]`. An update through a trait answers the same
  kind; `map`, `collect` and `flatMap` through the trait answer a hash-based collection. Maps and sets
  of different kinds with equal contents are equal, through the traits and `Any`, and hash alike. A
  map has no `mkString` of its own: it joins its entries as pairs, `(a, 1)`, through `Iterable`'s.
  The ordered operations take the reference library's names (user, 2026-11-04): `headOption` and
  `lastOption`, `head`, `last`, `firstKey` and `lastKey` (trapping when empty), `minAfter(k)` (the
  least at least `k`), `maxBefore(k)` (the greatest less than `k`), `rangeFrom` and `rangeTo`
  (inclusive), `rangeUntil` (exclusive), `range` (from inclusive, until exclusive), `iteratorFrom`
  (and a map's `keysIteratorFrom` and `valuesIteratorFrom`), and `reverseIterator`, which the
  reference's sorted collections lack and which is kept; the tree is balanced by weight, each side
  at most three times the other.
- Settled by the user 2026-11-04: the elementary functions are a module, `avin.math` ([system library](/spec/system-library/#avin-math)),
  with the reference library's names, called as `math.sqrt(x)` after `import avin.math` or imported
  by name: `Pi` and `E`; `sqrt`, `cbrt`, `exp`, `expm1`, `log` (natural), `log10`, `log1p`, `pow`,
  `hypot`; `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2(y, x)`, `sinh`, `cosh`, `tanh`;
  `floor`, `ceil`, `rint` (ties to even), `abs`, `max`, `min` (NaN when either is, `-0.0` below
  `0.0`), `signum`, `toRadians` and `toDegrees`. All take and give `real`, follow IEEE 754 and
  never trap: `sqrt(-1.0)` is NaN, `log(0.0)` negative infinity. Not methods on `real`, and no
  integer overloads of `abs`, `max` and `min` yet; the reference's `round`, which answers a `long`,
  is left out until real-to-integer rounding is decided with the conversions. Bindings: one
  runtime function per function, over the methods of Sysl's `Float` on `real`, since a binding names
  a function and these are methods.
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
- A `for` with `yield` is comprehension sugar over `map`, `flatMap` and filtering; a `for` without
  `yield` loops over the source's iterator.
- The collection hierarchy is rooted at `Iterable`, and `Iterator` is itself `Iterable`.
- `print` and `println` are ordinary library functions (settled 2026-11-04). Both take any values, shown as
  interpolation shows an `Any` (below), and separate several with `, `, which is not a tuple; `println` ends with a newline and `print`
  does not. `print()` writes nothing and `println()` writes a bare newline; `eprint` and `eprintln` are
  the same for standard error. `println(debug(x))` selects `Debug`.
- `debug(x)` yields `x`'s `Debug` form as a string. `Writer` writes strings (with `writeChar`,
  `writeInt`, `writeField` and `writeSpaces` beside `write`); `FormatSpec` is `width`, `precision`
  and `left`; `toString()` is a default body of `Display`.
- Settled 2026-11-04 (user): a value of `Any` or of any trait is shown by the type it has at run time.
  `Any` and every trait meet `Display` and `Debug` whatever they declare, as every type meets `Eq`:
  such a value meets those bounds, has `display`, `debug` and `toString()` and is interpolated, and
  renders through its own type's implementation, found through its type record, so `Seq[int]` prints
  as the vector it holds. It does not convert to `Display` or `Debug` as a value, so a value of either
  trait holds only objects that implement it, and is still shown through its method table. A type with no implementation of its own renders as its name (`Cat`,
  `Box[int]`; a function value `<function>`), never an address. Static types keep their rule: a
  class or data type needs its own `Display` to meet the bound or be interpolated, a type parameter
  its bound's. A trait takes no impl of `Display` or `Debug`: its values are shown by their own types,
  which keeps one rule.
- Settled 2026-11-04 (user): `print`, `println`, `eprint` and `eprintln` are each ONE ordinary
  library function taking `values: Any*`, with no other overloads, and the compiler treats them as
  it treats any function: none, one or several values, several separated by `, `. `println(1, pet,
  xs)` with `pet: Pet` and `xs: Seq[int]`, a function value and a class with no `Display` all print,
  each shown as that `Any` is in an interpolated string (by its type's own `Display`, or its name). A
  sequence is one value; only `xs*` spreads it, as for any repeated parameter, and then only a
  sequence of `Any` or of references. As a value `println` is a `(Vector[Any]) -> unit`. **The cost
  is stated plainly: a program that prints through them boxes every number, `char`, `bool` and `unit`
  argument, makes a vector of the arguments, and gives every type record its rendering entry**, since
  any value may reach `Any`'s rendering; a string or an object is passed as it is. A program that
  wants none of that writes to `StandardOutput` through a `Display` bound.
- A built-in type conforms to a trait through an impl and implements every method of the trait,
  since a trait's default bodies are shared by objects and a built-in value is not one.
- A variant's fields take defaults, each a function of the fields before it, as a constructor's
  parameters do.
- Avin has Sysl's full string interpolation and formatting.
- An interpolated string is a method call on `StringContext`: `p"a $x b"` is
  `StringContext(["a ", " b"]).p(x)`. `s`, `f` and `raw` are library methods, and a program defines
  its own interpolator by implementing a trait for `StringContext`.
- A user-defined interpolator is visible where it is imported (settled by the user, 2026-11-04): in
  the module that declares it, in a file importing that module whole, and in a file importing one of
  its declarations by name. `s`, `f` and `raw` need no import. A module that merely imports another
  does not pass its interpolators on, and using one that is not in scope is an error naming the
  module to import.
- Interpolated literals follow Scala's lexing (settled 2026-11-04). A backslash and a `"` or
  `\` after it are skipped as a pair, so `raw"a\"b"` holds `a\"b`, `raw"a\"` is unterminated and
  `raw"a\\"` ends, holding two backslashes; `$"` is a double quote and `$$` a dollar sign in every interpolator. The lexer reads
  no escape: a part reaches `StringContext` as written, and only `s` and `f` process escapes, so
  `p"\d"` is fine and `s"\d"` is an error. A backslash is otherwise an ordinary character
  (`raw"C:\dir"`, `\$name` still interpolates).
- Nested helper functions see their enclosing scope. Closures are full closures, and `return` inside a
  lambda is local to the lambda.
- `match` is a value-producing expression with full destructuring.
- A tuple needs no parentheses where it fills the rest of a line: `= a, b`, `return a, b`, a
  block's line. Bindings take tuples apart (`val q, r = divmod(n, d)`), and `a, b = b, a` is a
  multiple assignment: a statement that makes the whole right side before storing anything, with
  compound forms that read every place first. Its places are `var` locals, `var` fields and module-level
  `var`s, each field's object evaluated before anything else.
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
  run-time types are unequal. `===` stays defined only on reference types; where it meets an `Any`,
  a trait or a type parameter, see the identity entry settled on 2026-11-05.
- Settled 2026-11-03: the operator traits are the core library's `Eq` (`equals(other: Self) ->
  bool`), `Hash` (`hash() -> long`), `Ord: Eq` (`compare(other: Self) -> int`, with `less`,
  `lessOrEqual`, `greater` and `greaterOrEqual` defaulting from it), and `Add`, `Sub`, `Mul`, `Div`,
  `Rem` (`add`, `subtract`, `multiply`, `divide`, `remainder`, each `(other: Self) -> Self`) and `Neg`
  (`negate() -> Self`). Every type conforms to `Eq` and `Hash` implicitly; `==` on a type parameter
  still needs a bound that is or requires `Eq`. The built-in types implement the traits their
  operators have, in the core library, and an operator on a built-in type is the machine's
  operation, in generic instances too.
- Settled 2026-11-03: an impl of a trait may bound the type's parameters further (`impl[T: Ord] Ord
  for Box[T]`), making the type conform only for type arguments meeting them; on data types and
  classes, for any trait.
- Settled 2026-11-03: a conformance through such an impl to a trait that is also a type holds
  wherever one is decided: at conversions (as a covariant type argument too), in type patterns, and
  for calls of the trait's methods, which an instance missing the bounds does not have.
- Settled 2026-11-03: a generic type's derived `Ord`, `Display` and `Debug` are impls bounding each
  parameter by the trait, so they hold where the type arguments implement it.
- Settled 2026-11-03: arrays compare and hash by contents (`===` stays identity); arrays and lists
  display as `[1, 2, 3]`, their `Debug` as `["a", "b"]`, each only when the elements do.
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
- Settled 2026-11-03: every data type has `Display` and `Debug` without `derives`, where its fields
  have them, conditional on its type arguments as a derived one is; `derives Display, Debug` on a
  data type means the same. An impl of the type's own replaces the automatic one, with no conflict.
  A field without the trait leaves the type without it, which is an error only where a use needs it,
  naming the field. A program that never shows a value compiles none of this code. What `derives`
  and the automatic impls name are the core library's traits, whatever the module declares or
  imports under the same names.
- `x.equals(y)` and `x.hash()` may be called on a value of any type parameter, bounded or not.
- Settled 2026-11-03: a type argument a call's arguments decide is the
  [join](/spec/generics/#inference) of the types they give it where a value is read out, as two
  branches' types are joined; the first argument does not decide it alone, so `cat :: dogs` is a
  `List[Animal]` with nothing expected. An expected type decides it first; an invariant or
  contravariant use decides it for good. Two classes with no common superclass join to the one trait
  they share that every other trait they share is a parent of; sharing several unrelated traits is
  no join, an error naming them, rather than a choice among them. The same holds for branches.
- Settled 2026-11-03: the names an [`is` pattern](/spec/operators-and-control-flow/#is) binds are in
  scope where its test is known to have matched: the `then` branch of an `if`, the body of a
  `while`, the right operand of `&&`, and for `is not` the `else` branches and the right operand of
  `||`, with `!` swapping the two. After an `if` whose one branch never finishes, the rest of the
  block sees the other branch's names. `a is P(v) || b is P(v)` binds no `v` for the whole. A
  condition binds a name once, and its names may shadow the enclosing block's.
- Settled by the user 2026-11-09: **data updates are written `with { … }` with field paths**
  ([Changing fields](/spec/declarations/#changing-fields-with)): `p with { pos.x = 3, name = "a" }`
  is a new value of `p`'s type, each named field replaced and every other one `p`'s; a path rebuilds
  only the values along it, a shared prefix once. Paths go through data fields only; a method is
  never named; a path named twice, or a field beside a path inside it, is an error; an index is a
  syntax error suggesting the whole field; values are read against the leaf field's type; the base
  is evaluated first, then the values left to right; an unknown field is reported with the nearest
  one. `with` is a keyword only after an operand and before `{`, binding as a postfix tail.
- Settled by the user 2026-11-09 ("union types are great"): **limited union types**
  ([union types](/spec/unions/)). (1) A union `A | B` is written only in annotations: parameters,
  results, `val`s, fields, type arguments and aliases (`type Num = int | long`). (2) It is never
  inferred: the branches of an `if` or a `match` join as before. (3) It compiles to an anonymous
  tagged sum. (4) `match` takes it apart by type pattern, exhaustively: a member left uncovered is an
  error, and `_` is allowed. (5) `?` combines distinct error types into a union: a function returning
  `Result[T, IoError | ParseError]` may `?` a `Result[_, IoError]` and a `Result[_, ParseError]`.
  Intersection types stay deferred. The further rules are under [Proposed](#proposed).

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
- Testing is built in: `@test`, `@tests`, setup and teardown, and the built-in runner.
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
  point; `string` and `Array[byte]` are copied each way.
- A data type mirrors a Sysl enum or struct with `@sysl("module.Name")` on its declaration. An
  enum's variants are matched by name and their fields by position; a struct is mirrored by one
  variant named as the type, whose fields are matched by name. A mirror has no type parameters and
  never holds itself. An integer field takes any Sysl integer, converted with a check in each
  direction, so a Sysl `usize` is mirrored by a `long`.
- Settled by the user 2026-11-03: **Sysl collections cross the binding boundary as copies.** A Sysl
  slice or `Buf` of a boundary type in a result becomes a fresh Avin `Vector[T]` (or a fresh
  `Array[T]`, where the binding declares one); an Avin `Array[T]` or `Vector[T]` parameter of a
  boundary type is copied into a Sysl slice. Collections nest, inside one another, `Option`,
  `Result` and mirrors' fields. `Array[byte]` stays a bulk copy to and from `[]u8`. No other Avin
  collection crosses.
- Settled by the user 2026-11-04: **A Sysl function may fill an Avin array** ([Sysl
  bindings](/spec/sysl-bindings/#filled-arrays)). An `Array` of `byte`, an integer type, `real`,
  `bool` or `char` that is a binding's own parameter (a handle method's too) is copied into a `[]T`
  the Sysl function may write, and copied back into the array element by element when it returns;
  the shim keeps the array as a root of the thread's stack from before the call until the copy
  back, which runs after the result is made. The representation is a copy, not a view of the
  array's own elements: the collector never moves an object, but a Sysl slice of memory it does
  not own has no owner, so a Sysl function that kept it would read freed memory once the call
  returned; a copy keeps Avin memory out of Sysl. Every other array crossing into Sysl (of other
  elements, or inside an `Option`, a `Result`, a collection or a mirror's field) goes in as a
  `[]const T`, so a Sysl type that could write into it is refused at the binding instead of
  silently losing the writes. A `Vector` stays a `[]T` copy: it never changes, so nothing is lost.
- `Option[+T]` and `Result[+T, +E]` are `data` types of the core library, written in Avin, with
  their methods in an `impl`. See [errors](/spec/errors/#option-and-result).
- Settled by the user 2026-11-04 ("follow the reference library"): `Option`, `Either` and `Result`
  take lower bounds where the reference library's methods have them, so a value taken in may be
  wider than the one held and the result widens to it: `getOrElse[B >: T](default: -> B) -> B`,
  `orElse[B >: T](other: -> Option[B])`, `contains[T1 >: T](x: T1)` and
  `zip[T1 >: T, U](other: Option[U]) -> Option[(T1, U)]` on `Option`; `getOrElse[R1 >: R]`,
  `orElse[L1 >: L, R1 >: R]`, `contains[R1 >: R]`, `flatMap[L1 >: L, R2]` and
  `filterOrElse[L1 >: L]` on `Either`; `getOrElse[U >: T]`, `flatMap[U, F >: E]` and
  `orElse[U >: T, F]` on `Result`. On an `Option[Cat]`, `getOrElse(dog)` is an `Animal`. This
  replaces the earlier reading that the impl's parameters, not being variance positions, needed no
  bound: they still are not, but only a lower bound widens the result. `==` compares a value of a
  type parameter with one of a parameter it is the lower bound of. `joinLeft` and `joinRight` keep
  their shapes, `Either[Either[C, R], R]` and `Either[L, Either[L, C]]`, since a method takes one
  lower bound per parameter and the reference's evidence form has no counterpart.
- Settled by the user 2026-11-04 ("follow the reference library"): an unknown type argument still
  open at the end of its body, that the call's parameters and result name only where a value is read
  out, is `Nothing`: `None` alone is an `Option[Nothing]`, `Left(1)` an `Either[int, Nothing]`,
  `Ok(1)` a `Result[int, Nothing]`, `[]` and `Vector.empty()` a `Vector[Nothing]`. A later use in the
  body still settles it first. A covariant `Nothing` argument is any type, a value type's too, since
  no value has it: an `Option[Nothing]` is an `Option[int]`, in a declared type, an argument and a
  type pattern. An unknown named anywhere else (an invariant or contravariant argument, a lambda's
  parameter) or nowhere stays an error asking for the type: `Buffer()`, `Map.empty()`, `{}`. A
  `Buffer[Nothing]` could hold nothing, and a module's `val b = Buffer()` would only fail later, at
  the first store, so the error stays where the type is missing.
- Settled by the user 2026-11-04: the core library has `Either[+L, +R]`, with `Left(value)` and
  `Right(value)`, because the reference library has it. It follows that library's `Either` in names
  and meaning and is right-biased: `isLeft`, `isRight`, `fold`, `swap`, `map`, `flatMap`,
  `getOrElse`, `orElse`, `contains`, `exists`, `forall`, `filterOrElse`, `toOption`, `toList`,
  `toVector`, `foreach`, `iterator`; `merge` on `Either[A, A]`, `joinLeft` and `joinRight` on the
  nested shapes; `Either.cond(test, right, left)`. `Option` gains `toRight(left)` and
  `toLeft(right)`. Defaults are by name and take lower bounds as `Option`'s do (above). There is
  no left projection (`swap` serves), no `?` on an `Either`, and no conversion between `Either` and
  `Result`, which stays the type for failure. See [errors](/spec/errors/#either).
- `?` returns from the function or lambda it is written in, so that one's result type must be
  declared or, for a lambda, known where it is written. It cannot leave a delayed argument or a class
  body.
- Builders are the trait `Builder[-A, +C]` (`addOne`, `addAll`, `result`, and, as the reference
  library's, `clear`, which every builder defines and which readies a spent one for a new
  collection, `sizeHint`, a hint that changes no result, and `knownSize`, -1 where a builder does not
  know it cheaply); a collection's companion
  object makes one with `newBuilder[T]()` (the reference library's names, user 2026-11-04), and a sequence literal is built through the expected
  collection's, so a program's own class with such a companion is written as a literal too. A `List`
  literal is built from the back with `::`, since `List` is a data type.
- A collection literal compared with `==`, `!=` or an ordering takes the other operand's type.
- A trap inside the core library, a `fail` or a failed check such as an overflow alike, reports the
  place where the program last entered the library: the program's own call, however many library
  calls deep the trap is and whether the program reached the library directly, through a method
  table or through a function value. No annotation asks for it and no function of the program pays
  for it. See [errors](/spec/errors/).

Settled by the user on 2026-11-03:

- A mirror's fields are written in Avin's lowerCamelCase; the compiler matches each to the Sysl
  field of the same words in snake_case.
- `fail(message)` is public in the prelude: it traps with the message at the caller's place and has
  type `Nothing`.
- The methods an `impl` adds to a final class whose fields are all immutable are not variance
  positions, as a `data` type's are not: such an instance holds nothing a method could store a value
  into. A class with a `var` field keeps every method a position, and so does an `open` or
  `abstract` class (settled 2026-11-05, below).
- A one-variant `data` type may have a companion `object`, as a class may: the name in call position
  is the constructor and `Name.member` the companion (`Map.empty`, `Map.newBuilder`, `List.newBuilder`).
- An `impl` may add methods for some type arguments only: `impl[T: Display] List[T]` gives
  `mkString` to lists of displayable elements, and calling it on any other list is an error.
- The integers are one family: a closed core trait `Integer`, whose members the compiler decides (every
  `iN`), lets the core library implement a trait once for every width (`impl[T: Integer] Display for
  T`). Such an impl over a bare type parameter is allowed only for closed core traits, in the core
  library.
- Settled by the user 2026-11-03: any run of the operator characters `+ - * / % < > = ! & | ^ ~ : ?`
  that is not one of the language's operators names a method, declared like any method and called
  infix (`xs ++ ys`) or dotted (`xs.++(ys)`). The language's operators, including the yielding
  compound forms, are not method names: a type has them through their traits, and a method named
  `+` is an error saying to implement `Add`. A symbol call groups left, except that a name ending in
  `:` groups right with its receiver on the right (`x +: xs` is `xs.+:(x)`, `x` evaluated first).
  The level it binds at, and the bitwise operators a type may define, are settled on 2026-11-04.
  There are no prefix or postfix symbol methods; `++` and `--` update when no operand follows on the
  line. `x op= y` calls the method `op=` where `x`'s type has one, and otherwise is the language's
  compound assignment or, for a symbol method `op`, `x = x op y`. The library uses symbols:
  `Vector`'s `++`, `:+` and `+:`, `List`'s and `Map`'s `++`, and a buffer's `+=` and `++=` beside
  `addOne` and `addAll`.
- A module may be named in code through an import of the module itself: `import a.b`, then `b.X`
  reaches `a.b`'s public `X`, alongside imports of single declarations.
- An infix method call binds more loosely than arithmetic, bitwise, shift, cons and range operators
  and more tightly than comparisons, `is`, `&&` and `||`, and groups to the left, so
  `xs contains x && ok` is `(xs contains x) && ok`.
- A literal pattern does not match an `Any` (`5 -> …` on an `Any` is an error); `n: int if n == 5`
  says it.
- A tuple's type at run time is the type it was made with: `(animal, 1)` with `animal: Animal` is an
  `(Animal, int)` whatever its class, and a type pattern `(Dog, int)` does not match it.
- After its own last field, an `open` or `abstract` class's body may call a private method that is
  safe: it uses `self` only to read and assign fields and to call other safe methods.
- A string's byte offsets and lengths are `int`, and a string holds at most the largest `int` bytes.
- A trait's `override` may give an abstract method of a trait it requires a body, not only replace a
  default.
- An iterator is `hasNext()` and `next()`, and `next()` on an exhausted iterator traps.
- A `for` without `yield` traverses its source through `iterator()` (a range written in place is
  counted and an array indexed); `break`, `continue` and `return` leave the body directly. A type
  with `foreach` and no iterator is not a `for` source.
- `Array.fill(n, value)` evaluates `value` once and shares it between the elements; per-element
  values come from `Array.tabulate`.
- The hash-based maps and sets iterate in an order the program cannot rely on: the immutable ones in
  an order fixed by the keys' hashes alone, so equal maps iterate alike, and the mutable ones in
  table order.
- Maps and sets display in the shape of their literals: `{a: 1, b: 2}`, `{1, 2, 3}`, and `{"a": 1}`
  with each part's `Debug`. Mutable maps and sets compare and hash by contents, as arrays do.
- Every `data` type may have a companion, not only one-variant ones: an `object` named after a data
  type of its module is its companion. A variant of the type's own name stays the constructor in call
  position; a type with no such variant (`List`) has nothing of its name to call. An object cannot
  share its name with a variant of another type, nor with a fieldless variant of its own type's name.
  A `newBuilder` returning a `Builder` of its collection takes its type arguments from the type a
  literal expects, so their bounds are checked at the call.
- A collection literal builds whatever collection is expected through its companion's `newBuilder`: a
  sequence literal a sequence, a set literal `{a, b}` a set and a map literal `{k: v}` a map, giving
  the builder each entry as a `(key, value)` pair; with no expected collection they are a `Vector`, a
  `Set` and a `Map`.
- The arithmetic traits carry their identities: `Add` has `zero` and `Mul` has `one`, so `sum` and
  `product` of an empty collection are those values rather than an `Option`. They are written once,
  by impls on `Iterable` (`impl[T: Add] Iterable[T]`, `impl[T: Mul] Iterable[T]`).
- `Seq[+T]` is a core trait over `Iterable[T]` with two abstract methods, `length()` and
  `apply(index)`; its other methods (`lift`, `isEmpty`, `head`, `headOption`, `last`, `lastOption`,
  the folds, `foreach`, `exists`, `forall`, `find`, `indexWhere`, `count`, `toVector`, `toList`, an
  iterator and `reverseIterator`) are default bodies written once over those two. `head`,
  `headOption`, `last` and `lastOption` are `Iterable`'s own, written over `iterator()` for every
  collection (user, 2026-11-04, as the reference library has them), and a sequence and a sorted
  collection override them to reach the end at once. `lift(i)` is the element as an `Option`. `List` is a `Seq`
  and overrides every one that would index into it, so none takes more than linear time on a list.
  `contains` and `indexOf`, which take an element, and `mkString`, `max` and `min`, which need
  something of the elements, are added to `Seq` by impls (`impl[T] Seq[T]`, `impl[T: Display]
  Seq[T]`, `impl[T: Ord] Seq[T]`), written once for every sequence; `contains` and `indexOf` are
  `exists` and `indexWhere`, so on a list they walk it once.
- A sequence pattern on a sequence other than a list asks `length()` once and then reads, with
  `apply`, each element whose pattern is not `_`, in order, stopping at the first that fails.
- `Deque[T]` is a ring over an array, doubling when full as a buffer does, its spare places zero
  bytes. `removeHead()` and `removeLast()` return the element and trap on an empty deque, as
  `Buffer.remove` traps outside the buffer; `isEmpty()` is the test before them. Settled 2026-11-04
  by the user: a buffer and a deque take the reference library's names for adding, `addOne`,
  `append`, `prepend`, `addAll`, `appendAll`, `prependAll`, `insert` and `insertAll`, and a deque's
  ends are `prepend`, `append`, `removeHead` and `removeLast`.
- A trait may declare `static` members, which belong to the implementing type rather than to a
  value: `static zero() -> Self` in `Add`, `static one() -> Self` in `Mul`. Every impl gives them,
  and they are called on a type, `int.zero()`, or on a bounded type parameter, `T.zero()`. `sum()`
  and `product()` on the collections are written with them. The details of
  [static members](/spec/declarations/#static-members): `static` is a keyword, written after
  `private` and before or after `override`; a static member is a trait's, an implementation of one,
  or a built-in type's own (below), and a class or `data` type keeps what belongs to the type in its
  companion object; in a trait it has no default body, and no trait
  overrides one; an implementation is `static` exactly where the trait's member is, an abstract class
  giving it too; its body has no `self`; a trait's default body cannot call one on `Self`; a trait
  with one is a bound and not a type; and a local, parameter, member or module value of a type's
  name hides the type as a receiver.
- Settled by the user 2026-11-04: a built-in type declares
  [static members of its own](/spec/declarations/#static-members-of-built-in-types), as `static`
  members of an impl of the type without a trait (`impl string` with
  `static fromUtf8(bytes: Array[byte]) -> Result[string, Utf8Error]`), for `string`, `char`,
  `bool`, `real`, `byte` and each integer type, since a built-in type has no companion object. The
  core library declares the built-in types' own; a program adds them in an extension, as it adds
  methods, seen where the module is imported and private to it where marked so. They are called on
  the type's name, never on a value, and their bodies have no `self`. A name is a static member or
  a method of the type's values, never both; static members of one name overload by parameter
  types; `wrap` and the numeric limits (`MaxValue`, `MinValue`, `real`'s `NaN` and the rest) are the
  language's and cannot be redeclared. A generic impl, an impl adding to a trait and an impl of a
  class or `data` type declare no static member of their own. Any static member named on a known
  type without a call (`string.fromUtf8Lossy`, `int.zero`, `Point.zero`) is a function value of its
  parameters; an overloaded one takes the expected function type's overload, and without one is an
  error. The first use is `string.fromUtf8` and `string.fromUtf8Lossy`
  ([strings from bytes](/spec/types/#strings-from-bytes)), bound to Sysl's UTF-8 decoders, with
  `Utf8Error(offset: long, truncated: bool)` mirroring Sysl's error.
- `i1` has no `1`: `i1.one()` is a compile error, and `one()` reached at `i1` through a generic
  traps with "i1 has no 1" at the place the program called, so a generic that passes its bounds
  compiles for every type meeting them.
- An `impl` may add methods to a trait (`impl[T] Seq[T]`, `impl[T: Display] Seq[T]`). Its methods
  reach the value only through the trait's own methods, which are variance-checked, so, like the
  methods an impl adds to a `data` type, they are not variance positions: `contains(element: T)`,
  `indexOf`, `mkString`, `max` and `min` are written once on `Seq`, and generic code over `Seq[T]`
  has them. A call is compiled for the receiver's static type, so the trait's methods the added one
  calls are that type's, called directly, and go through the method table only where the receiver
  is seen as the trait. Lookup takes the receiver's type's own methods first: an added method with
  the name and parameter types of one the type has is passed over, so a type keeps a faster version
  of its own where its static type is known. Two impls of different traits adding a method of one
  name and parameter types to a type conforming to both make a call of it on that type an error; an
  impl may not add a method the trait declares.
- The form of a `for` with `yield` ([for comprehensions](/spec/collections/#generators-guards-and-yield)):
  generators `pattern in source` separated by commas, each followed by any number of `if` guards,
  then `yield` and the value on the rest of the line or in an indented block; or the generators one
  to a line in a block after `for`, a line starting with `if` guarding the generator above, with
  `yield` on the line after the block. `yield` is a keyword. A `for` without `yield` keeps one
  generator and no guards.
- A guard is `withFilter`, a lazy view each collection provides (`Option`'s is its `filter`); a type
  of the program's own needs `withFilter` for a guard, with no fallback to `filter`. `flatMap` on
  the library's collections takes any `Iterable` from its function; a `Range` maps to a `Vector`;
  a `Map`'s entries reach a comprehension as pairs, through `map` and `flatMap` overloads taking
  a function of one pair, and yield a `Map`.
- A generator's pattern must match every element, as in a `for` loop, rather than filtering out the
  elements it does not match. `return`, `break`, `continue` and `?` cannot leave a comprehension's
  guards or yielded value.

Settled by the user on 2026-11-04:

- **`From[E]` is one generic trait, implemented once per source error type** by the type the
  errors convert to: `impl From[IoError] for ConfigError` beside `impl From[ParseError] for
  ConfigError`. A type may implement one generic trait at several type arguments where the trait's
  members are all static and it requires no other trait, as `From`'s one member,
  `static from(value: E) -> Self`, is; any other trait is implemented once. `?` on a `Result[U, E]`
  in a function yielding `Result[T, F]` passes the error on unchanged where `E` is an `F`, and
  otherwise through `F.from`: at `From[E]`, or else at the one conformance `From[X]` with `E` an `X`
  that is more specific than every other such. Neither is an error naming `From[E]`; several equally
  specific ones are an error naming them. On a type parameter the conformances are its bounds',
  `F: From[IoError] + From[ParseError]`. See [errors](/spec/errors/) and
  [declarations](/spec/declarations/#impl).
- **One formatting style, applied by `avin fmt [--check] [<path>...]` and the language server's
  `textDocument/formatting`.** Each block level is indented two spaces; a literate file keeps its
  four-column code-block margin and indents two a level inside it, and only its program is
  formatted. Spaces go around binary operators (ranges included, `0 ..< n`), `=` and `->`, after
  `,` and `:`, and nowhere inside brackets; a prefix operator stays against its operand. At most
  one blank line in a row, none at either end of the file, and one between two top-level
  declarations unless both are one line; a declaration's comments and annotations stay with it.
  Comments are kept exactly, a trailing one a space after the code. A line longer than 120
  characters has a list broken one item per line, a call's or a literal's items two columns in and
  a parameter list's four, the closing bracket on its own line; a list the source wrote one item
  per line stays so; a binding's value with no list to break is broken before its loosest operators
  as continuation lines, though only after the last `then`, `elif` or `else` of a one-line `if`
  (a line after an `if`'s test is its block, so a test and an earlier branch stay whole). No trailing commas are added. The formatter keeps every token and
  refuses a file that does not parse; its result parses to the same syntax tree.

- **`compareAndSet` on a value without identity** — a data value, tuple, string, `Option` or `Any` —
  **compares with `==`; on scalars and references it compares the word.** A class instance (even of
  a class that derives `Eq`) and a function value keep identity. See
  [threads](/spec/threads/).

- **`Vector[+T]` is a persistent, radix-balanced tree of arrays of 32**, with a prefix and a suffix
  beside a balanced middle at every level, and stays the default sequence (`[1, 2]`, `[]T`,
  `main(args)`). Indexing and `updated` take one array per level, at most six; `:+` and `+:` take
  amortized constant time; slicing, `take`, `drop` and their kin take time in the depth and share
  every array they do not cut; `++` takes time in the shorter operand. A vector's arrays are
  exactly sized and never written once they are in a tree. The one-array immutable sequence
  remains as **`ArraySeq[+T]`**, for elements built once and then read, with no level between an
  index and its element; every changed `ArraySeq` is a copy. Every collection has `toArraySeq()`.
  See [collections](/spec/collections/#vector).

- **An impl that only adds methods may be written in any module** (a relaxed impl); in another module
  than its type's or trait's it is an [extension](/spec/declarations/#extensions): `impl string`,
  `impl[T: Ord] Seq[T]`, `impl[T] Option[Option[T]]`, `impl Box` for another module's class. Its
  methods are seen in its own module and in every file importing that module, whole or any
  declaration of it, with no further step; a file importing neither does not see them, and the
  core library's are seen everywhere. A type's own methods are found first: an extension method a
  type's own or inherited method of the same name and parameter types always hides is an error at
  its declaration, and one added to a trait is passed over for a type that has such a method. Two
  seen extensions of different modules adding one name and parameter types to a receiver make the
  call an error naming both modules. An extension's `private` methods are private to its module. Its
  methods reach no private member of another module's type, are in no method table and are not
  overridden; `self` is the receiver and the type's public fields and methods are in scope by bare
  name. Infix calls, method values and calls through a bound see extensions as dotted calls do,
  a bound's where the generic function is written. Conformances, `impl Trait for Type`, stay in the
  module of the type or of the trait.

- **Sequences, sets and maps are functions, by subtyping.** `Seq[+T]` is a subtype of `(int) -> T`,
  through its `apply(index)`; `Set[A]`, immutable and mutable, of `(A) -> bool`; `Map[K, V]`,
  immutable and mutable, of `(K) -> V`, whose `apply` traps on a missing key as `m(k)` does. A
  collection is passed, stored and called where its function type is expected with no conversion,
  and variance holds through it: a `Vector[Dog]` is an `(int) -> Animal`. For a set `s`, `s(x)` is
  legal: `true` when `x` is an element of `s` and `false` otherwise, the set's `apply(x) -> bool`,
  which is `contains`.
- A class or trait names a function type among its parents after `:` (`trait Seq[+T]: Iterable[T],
  (int) -> T`), and a data type or class through an impl with no bounds of its own (`impl[K, V] (K)
  -> V for Map[K, V]`), in the type's own module. The type then has an `apply` taking exactly that
  type's parameters and yielding its result or a subtype; `apply` implements no trait method, so it
  is not marked `override` for it. A type is at most one function type, counting those of its
  superclass and its traits, and its parameters keep their variance there. A built-in type is no
  function.
- A function type with one parameter is written without parentheses: `int -> string`,
  `Option[T] -> bool`. `->` in a type groups to the right, so `int -> int -> int` is
  `int -> (int -> int)`; `(A) -> B` is the same type as `A -> B`, `(A, B) -> C` takes two
  parameters, a function taking one tuple is `((A, B)) -> C`, and a function taking a function is
  `(int -> int) -> int`. A function type needs no parentheses in a parameter, field, result,
  type argument, tuple part or `val` annotation, nor as a supertype; a type pattern in a `match`
  arm still writes it in parentheses, since the arm's own `->` follows it. Error messages print
  every function type with a parameter list, `(int) -> int`.
- `main` is `main()` or `main(args: Vector[string])`, the arguments after the program's name, the
  same as `avin.process.args()`. The parameter is exactly `Vector[string]` (a name for that type
  is the same type; `Seq[string]` and `Array[string]` are errors), it has no default, and any other
  parameter list is an error naming the two forms. The result rules are unchanged. A program built
  for its tests does not run `main`, so tests see no arguments.
- A `match` with nothing before it is a [function literal](/spec/operators-and-control-flow/#match-functions)
  of one value, whose arms are ordinary `match` arms: `xs.collect(match Some(n) -> n)`, one arm on
  the `match` line, or an indented block of them, inside parentheses or after an infix method's
  name. Its parameter type comes from the expected type, as a lambda's does, and with none it is an
  error. Where `A -> B` is expected its arms must cover every `A`; where `PartialFunction[A, B]` is
  expected they need not. `PartialFunction[-A, +B]` is a core-library trait with `isDefinedAt`, a
  call that traps at the program's call site outside its domain, `lift`, `orElse` and
  `applyOrElse`; a literal tests its patterns and guards once per call, `lift` or `applyOrElse`.
  Every collection with `map` has `collect`, every `Iterable` has `collectFirst`, and `Option` has
  `collect`. A `PartialFunction[A, B]` is a subtype of `A -> B`, through its `apply`, so it passes
  wherever one is expected and traps there outside its domain.
- `[]T` in a type position means exactly `Vector[T]`: `xs: []int`, `[][]int`, `Option[[]string]`,
  `main(args: []string)`. Its element is a name or a parenthesized type, so `[]int -> int` is a
  function from a vector and `[](int -> int)` a vector of functions. A type pattern takes it
  without parentheses, `v: []int -> body`. There is no fixed-length `[N]T` (an error); error
  messages print `Vector[int]`.
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
- Details of starting and joining: `spawn` gives back a `Thread[T]` whose `join()` any thread may
  call, any number of times and at once; one waits for the thread and the others for that wait. A
  thread's handle is collected like any object once nothing reaches it; a thread never joined keeps
  its stack until the program ends. `s.spawn` is called by the thread running the scope's body
  while the scope is open, and anything else traps, so the scope sees every thread it has to join;
  a thread the scope started starts more through a scope of its own. When several threads trap at
  once, or one traps while another calls `exit`, only the first stops the program: its output is
  flushed once and its reason is the one reported.
- [Repeated parameters](/spec/declarations/#repeated-parameters): `values: T*` is the last
  parameter, at most one, named, with no default and not by-name; inside the function it is a
  `Vector[T]`. A call gives it every positional argument from its position on, each checked
  against `T` (so generic `T` is inferred from them), or splices one `Iterable[T]` with `xs*` as
  its only argument, written last: a `Vector[T]` is passed as it is, any other sequence copied.
  `f(1, xs*)` into the one parameter, `xs*` anywhere else, and naming the repeated parameter are
  errors. A function with one, used as a value, has the type `(…, Vector[T]) -> R`. A method
  overriding or implementing another repeats the same parameter or neither does; `main` takes
  none; `data` fields cannot be repeated. A method whose only required parameter precedes a
  repeated one is called infix with one argument.
- An impl without `for` adds methods to any type built from its parameters
  ([declarations](/spec/declarations/#impl)): `impl[T] Option[Option[T]]`,
  `impl[A, B] Pair[A, List[B]]`, `impl[T] Seq[Seq[T]]`, `impl[K, V] Iterable[(K, V)]`. Every
  parameter must appear in the type. A call matches the receiver's static type against it,
  seeing a reference through its traits and superclasses only at covariant parts, and holds the
  learned parameters to the impl's bounds; a receiver that does not match is an error saying what
  it would need to be. Two impls that both match a receiver and add a method of one name and
  parameter types make the call an error, as does such a method clashing with the type's own. An
  impl of a trait still names its type's parameters in order, so conformances never overlap. The
  core library adds `flatten` to `Option[Option[T]]`, `Result[Result[T, E], E]` and `Seq[Seq[T]]`
  (a `Vector[T]`), `toMap()` to `Iterable[(K, V)]` (keys bounded as a map's) and `unzip()` to
  `Seq[(A, B)]`.
- A heap field or module variable wider than 64 bits is accessed as untorn 64-bit words: a racing
  read of it may combine words of different writes ([threads](/spec/threads/#the-memory-model)).
- Details of [synchronization](/spec/threads/#synchronization): `Mutex[T].with` hands its body the
  value itself, and an immutable value is read with `get`, replaced with `set` and changed with
  `update(f)` under the lock. A mutex is not reentrant, and a thread taking one it already holds
  traps rather than waiting for ever; no other deadlock is detected. A `Condition` is made by its
  mutex's `condition()`, so it is waited for with that mutex only, and `wait()` from a thread not
  holding it traps. `Atomic[T]` holds any type: a value of one word lock-free, an integer wider
  than 64 bits under one runtime lock; `compareAndSet` compares identity for a reference and the
  bits for a real; `add` and `sub` give back the new value and trap on overflow before storing, and
  `update(f)` retries `f` until its compare-and-set succeeds. `Channel[T](capacity = 0)` is
  unbounded at zero and a negative capacity traps; a send to a closed channel traps, closing twice
  does nothing, and a channel is `Iterable`, receiving until it is closed and drained. `Once.run`
  makes every other caller wait for the running body; a body running its own `Once` again traps.
- [Trailing blocks](/spec/operators-and-control-flow/#trailing-blocks): an indented block on the
  lines after a call is the call's last argument, given to the callee's last parameter. A by-name
  parameter takes the block as its delayed expression (`retry(3)` + block); a function parameter
  takes a lambda whose parameters end the call's line, one name or a parenthesized list after
  the call's parentheses (`withFile(path) f ->`, `zipWith(a, b) (x, y) ->`). A callee with no
  other arguments is written without parentheses (`scope s ->`, `s.spawn` + block). It applies
  wherever an expression ends a line (`val r = retry(3)` + block, a method call, a match arm's
  body), and never in the test of `if`, `elif`, `while` or `do … while`, a `for` source or guard,
  or a class's parents, where the block is the form's body. A local function's header keeps its
  meaning: `name()` or `name(x: T)` before a block declares one. A block whose only line is a name
  and an operand stays the infix-continuation error. A block for a last parameter that is neither
  by-name nor a function, already given, or absent is an error.
- [Futures](/spec/threads/#futures) in `avin.threads`: `Future(body)`, the companion's
  `apply[T](body: -> T)`, runs a by-name body on a shared pool and gives back a `Future[T]` at once;
  `await()` waits safe for the collector, `await(timeoutMillis: long)` gives an `Option`, and times
  are milliseconds as a `long`, as in a condition's `waitFor(timeoutMillis) -> bool`. `map`,
  `flatMap`, `zip`, `onComplete`, `Future.all` (a `Vector` in the order given), `Future.first`
  (trapping on no futures) and `Future.successful` never wait, and every function given to one
  runs on a pool thread. A future carries no failure: a computation that can fail is a
  `Future[Result[T, E]]`, which also has `mapOk` and `flatMapOk`, and a trap in a body stops the
  program. `Promise[T]()` has a `future` field; `complete(x)` a second time traps, and
  `tryComplete(x) -> bool` completes only where nothing has. The pool starts workers as work comes,
  up to one per processor (`sysl.platform.cpu_count`), never joins them, and starts one more in the
  place of each worker waiting in `await` while work is queued, so nested waits never deadlock it;
  `await` inside a body is allowed. The pool is a module's `lazy val`, so a program that makes no
  future never makes it.
- [`lazy val`](/spec/declarations/#initialization), at module level and in class and object bodies
  (each instance its own), is computed on its first read, exactly once, and kept; never read, never
  computed. `lazy` is a modifier only before `val`, an ordinary name elsewhere; `lazy var` and
  `lazy` on parameters, class parameters, data fields and locals are errors. A module-level one
  takes no place in the eager initialization order, its readers depending on what its initializer
  reads; an initializer that needs its own value, directly or through other lazy values, traps at
  that read, naming the value. A field's initializer may use every field and method; the class
  body reads the field only where it could call a method. Concurrent first reads run the
  initializer once, the others waiting safe for the collector; the value is published with a
  release store paired with the readers' acquire load, a stored value read with one atomic load
  and a branch.
- [A companion's `apply`](/spec/declarations/) overloads the constructor its name has in call
  position. Where an object declares `apply`, `Name(…)` chooses among the class's constructor (or
  the data type's variant of its name) and every `apply` by the ordinary static overloading rules:
  most specific by parameter types, with by-name parameters, defaults, repeated parameters, generic
  inference and trailing blocks as for any call (`Future(fetch(url))`, `Future` + block). Type
  arguments written at the call are the chosen candidate's: the class's or data type's for the
  constructor, the method's for an `apply`. An `apply` with the constructor's parameter types, up to
  type parameter names, is an error at its declaration. A name with no constructor in call position
  (a data type with no variant of its name, an abstract class, a plain object) calls its `apply`
  alone, and the library's `List`, `Set` and `Map` companions take their elements that way:
  `List(1, 2, 3)`, `Set(…)`, `Map((k, v), …)`. Where a function type is expected, the name is the
  constructor or `apply` that type chooses, as for an overloaded function value; elsewhere the bare
  name stays the object. This amends the earlier rule that the name in call position is only the
  constructor and that an object cannot be called.

- **A method named by a symbol binds at the level of the language's operator its first character
  belongs to**, on Avin's own table, tightest first: a name starting with any character that starts
  no operator below (`~`, `?`) binds above multiplication; `*`, `/`, `%`, `<<` and `>>` names with
  the multiplicative operators and shifts; `+` and `-` names with the additive operators; `&`, `^`
  and `|` names with those bitwise operators, `&` tightest; `:` names with `::`; then ranges and
  methods called by a word (`a max b`); `<`, `>`, `=` and `!` names with the comparisons; and a name
  ending in `=` that is not a comparison (`+=`, `++=`, `|=`) is an assignment. A name ending in `:`
  still groups right with its receiver on the right, and left- and right-grouping operators at one
  level, symbol, word or the language's own, do not mix without parentheses (`1 :: xs :+ 2` is an
  error; `1 +: xs :+ 2` is `(1 +: xs) :+ 2`). So `xs ++ ys :+ z` is `(xs ++ ys) :+ z`, and the
  combinators `a ~ b ^^ f | c ~ d` read `((a ~ b) ^^ f) | (c ~ d)`. **`&`, `^`, `|` and `~`
  between two operands are free method names** for every type but the integers and bytes, whose
  bitwise operators stay the machine's and cannot be redefined, by an extension either; `a | b` on
  such a type calls its method `|`, and `x |= y` calls `|=` where the type has one and is otherwise
  `x = x | y`. Prefix `~` stays the integers' and bytes' complement. `+ - * / %` and the
  comparisons still come only through their traits, and `&&`, `||`, `!`, `===`, `!==`, `::`, `=`,
  `:=`, `->`, `?`, `..` and `..<` stay reserved. This replaces the earlier rule that every symbol
  call binds at the level of `a max b`. See [methods named by
  symbols](/spec/operators-and-control-flow/#methods-named-by-symbols).

- **The shifts `<<` and `>>` are free method names on the same terms as `&`, `^` and `|`** (user,
  2026-11-04), so a parser combinator library can spell sequencing-then-map `p >> f`. On every type
  but the integers and bytes, `a << b` and `a >> b` call the methods of those names, at the shifts'
  own level, that of multiplication (`a ++ b >> f` is `a ++ (b >> f)`, `a >> f | b` is
  `(a >> f) | b`); `x <<= y` calls `<<=` where the type has one and is otherwise `x = x << y`. The
  integers' and bytes' shifts stay the machine's and cannot be redefined, by an extension either,
  and the yielding forms `<<:=` and `>>:=` stay reserved. Avin has no `>>>` operator, so `>>>` was
  already a free name. This amends the rule above. See [methods named by
  symbols](/spec/operators-and-control-flow/#methods-named-by-symbols).

- **The arithmetic operators `+ - * / %` and the orderings `< <= > >=` are free method names on
  data types, classes and traits, with any operand and result types** (user, 2026-11-04), beside
  the operator traits, which stay for generic code: `Instant + Duration -> Instant`,
  `Instant - Instant -> Duration`, `Vec * real`. Such methods overload by the operand's type like
  any other. `a + b` calls `a`'s method `+` where `a`'s type (a data type, a class, or a type
  parameter through its bounds) has one taking one argument, at the built-in operator's own
  precedence; where it has none, the operator is the core trait's (`Add`'s `add`), as before. The
  method is just a method: on a type with both, the operator calls the method and code bounded by
  `Add` calls `add`. Prefix `-a` calls a method `-` taking no argument (`-() -> Vec`), and otherwise
  is `Neg`'s `negate`. `a < b` alone calls the method `<`, whatever it yields; a comparison by a
  method does not chain, and `a < b < c` through one is an error saying to join the comparisons with
  `&&`. `x += y` calls `+=` where the type has one and is otherwise `x = x + y`, the method `+`
  included. The built-in types keep their operators: a method of one of these names on `int`,
  `real`, `byte`, `char`, `string`, `bool` or a tuple, by an extension or a trait implementation, is
  an error. `==` and `!=` stay `Eq`'s, and the wrapping, saturating and yielding forms stay
  reserved. `avin.time` spells its reading arithmetic with them. This amends the two rules above.
  See [methods named by symbols](/spec/operators-and-control-flow/#methods-named-by-symbols).

- **`not` is a soft keyword: a keyword only directly after `is`** (user, 2026-11-04), so the parsing
  library can name its negative lookahead `not(p)`. After `is` it is always the keyword, so `x is
  not(y)` negates the pattern `(y)`; everywhere else it is an ordinary name, and a function, method,
  parameter or local may be called `not`. See [is](/spec/operators-and-control-flow/#is).

- **A method named by a symbol with `~` anywhere in its name binds at the tightest symbol level**,
  whatever its first character: `~`, `~>`, `<~`, `~!`, `<~>` and `^~` bind together there and group
  left, so `num | symbol("(") ~> expr <~ symbol(")")` is `num | ((symbol("(") ~> expr) <~
  symbol(")"))`, `"[" ~> items <~ "]" ^^ f` is `(("[" ~> items) <~ "]") ^^ f`, and `a <~ b ~ c` is
  `(a <~ b) ~ c`. Every other name keeps the first-character rule; a `~` name ending in `=` (`~=`)
  is still a compound assignment, and prefix `~` is unchanged. This amends the rule above, under
  which `<~` bound with the comparisons.

- **A line that starts with a binary operator, indented deeper than the line where its expression
  began, continues that expression**, with the operator's own precedence: the line break means
  nothing. Every binary operator of the language, every method named by a symbol, `~`, and `++` or
  `--` with an operand after it start one; a word, `!`, `.`, `?`, a bracket and the assignments do
  not. It applies wherever an expression with a complete operand ends a line (a binding's value, an
  assignment, a `return` value, an infix call's argument, a `match` arm's or a lambda's body, an
  expression statement) and ends at the first line back at the depth the expression began on; a
  line inside it that does not start with an operator is an error. Blank lines and comments between
  continuation lines are skipped. After an operand `-x` and `+x` there are binary; after the test of
  `if`, `elif` or `while`, a `for` source, a class's parents, and after anything that is not a
  complete operand (`=`, `->`, `else`, `do`, a signature), an indented line is a block's line, so
  `if c` over `-1` is a block yielding `-1`. An indented line starting with an operator after a call
  is a continuation, not the call's trailing block. See [continuation
  lines](/spec/operators-and-control-flow/#continuation-lines).

- **A `.lavin` file is literate source: a Markdown document whose lines indented four columns are
  the program**, and the name alone decides it (an `.avin` file never reads this way). It is
  accepted wherever an `.avin` file is: a program of one file, a file of any module of a directory
  program or of the library, `avin test`, and the language server. The program's own indentation
  starts after the four columns; prose between two blocks ends nothing and reads as blank lines; a
  fenced block (` ``` ` or `~~~`) is an illustration and is not compiled; an indented block under a
  list item is prose. Prose is blanked, not removed, so every position reported is the document's
  own line and column. A tab in a line's indentation and a fence that is never closed are refused
  where they are. `avin tangle` prints the program with the prose stripped, line for line; `avin
  weave` renders the document as one HTML page, the prose as GitHub-flavoured Markdown and the
  program coloured by the compiler's own lexer. See [literate
  source](/spec/declarations/#literate-source).
- **Derivation through shapes** ([derivation](/spec/derivation/)): `derives` takes any trait whose
  module declares a `derive X for T` template, written once like an impl. Every data type and class
  (its constructor parameters) has a shape, made once before `main`: variants, fields, names,
  positions, defaults and annotations. Inside a template, `T.walk(value, visitor)` and `T.walkPair(a,
  b, visitor)` call the visitor's `field[F: Bound]` hook once per field at that field's type, and
  `T.build(builder)` makes a value from fields supplied in any order, using defaults and the
  builder's `missing` for absent ones. The template is checked once; the visitor's hook is found for
  each deriving type, and that each field meets its bound is reported at the `derives` entry naming
  the field. Annotations are values of types implementing `Annotation`, written `@JsonKey("name")`
  on a type, variant or field. The user resolved the proposal's open questions:
  - `Shaped` is a bound only a template's `T` has; a program cannot write it elsewhere.
  - The built-in `Display`, `Debug`, `Ord` and class `Eq` and `Hash` derivations are to be
    re-expressed as core-library templates. A data type's own `==` and hash stay the language's.
  - A misused annotation is not rejected while compiling; it fails when a value is decoded.
  - `build` is refused for open and abstract classes, an error at the `derives` entry; `walk` is
    allowed.
  - Every type parameter of a generic deriving type is bounded by the derived trait.

- Packages beyond the manifest, settled by the user (2026-11-04) as proposed:
  - **Features** ([features](/spec/packages/#features)), Sysl's model with the module as the unit
    gated: a feature block has `doc`, `default`, `modules`, `enables` and `requires`, and
    `name = [x]` is its short form; optional dependencies, `"dep:x"`, `"dep/feature"`, users'
    `features`, `default_features = false` and `-x`, unification by union and the
    `--features`/`--no-default-features`/`--all-features` flags are Sysl's. Where Sysl gates lines
    with `#if feature_x`, Avin has no conditional compilation: a feature lists whole **modules**
    (with those under them), read only while it is on, and an import of a gated module or of an
    optional dependency's module is allowed only from a module whose own gate implies it, checked
    against the manifest whatever is selected, so features are additive by construction. `avin test`,
    `avin check`, the language server and `avin tidy` turn every feature on. Not carried over: Sysl's
    `choices`, a feature's `if` and `targets`, and options, which are about machines and C
    configuration Avin's manifest does not describe.
  - **Members of a repository** ([members](/spec/packages/#members-of-a-repository)), Sysl's:
    `{ git, member = "dir", version | rev }`, the member a package named `<coordinate>//<dir>` for
    selection and `avin.sum`, every member at one version one clone whose hash each member's line
    carries, a fetched member's `workspace = true` and in-repository `path` siblings read as their
    own coordinates at the member's version, and `avin add <coordinate>//<dir>@<version>`. Its modules
    keep their own names, as every dependency's do, so Avin needs no `package.repository` key.
  - **Vendoring** ([vendoring](/spec/packages/#vendoring)), Sysl's: `avin vendor` copies the
    resolution with every feature on into `vendor/` at the project (workspace) root in the cache's
    layout, each copy hashed against its `avin.sum` line first, `.git` and path dependencies left out;
    a `vendor/` present is the cache, and the root's `vendor/` is never a module. Two choices of
    Avin's own: a package missing from `vendor/` is **refused** rather than fetched into it, so a
    vendored build never touches the network; and `avin vendor` **removes** packages the resolution
    no longer reads, as `avin tidy` does for lines.
  - **The standard library's modules** ([standard library](/spec/packages/#the-standard-library-s-modules)):
    the `avin` tree shipped with the compiler (`share/avin/library`, or `AVIN_ROOT/library`),
    versioned with it, never a dependency and never in `avin.sum`; `avin` is the prelude, seen
    without an import, and every module under it is imported by name like any module. A
    compilation reads the prelude and then only the library modules something imports,
    transitively, so the library can grow without every program parsing all of it. Domain libraries
    are packages (as settled for `dev.avinlang`). The standard library has no features.
- **Integer limits and narrowing** (user, 2026-11-04). `MinValue` and `MaxValue` on every integer type
  (`int.MaxValue`, `long.MinValue`, `i8.MaxValue`, any `iN`), on `byte` and on `char`, and on `real`
  `MaxValue`, `MinValue`, `MinPositiveValue`, `PositiveInfinity`, `NegativeInfinity` and `NaN`, all
  compile-time constants. Narrowing integer conversions (`i8(x)`, `int(l)`, `T(n)`, and a byte or char to
  a narrower integer) trap when the value does not fit; real-to-integer still saturates. The wrapping
  form, `i8.wrap(x)` (spelling proposed below), keeps the low bits. A type parameter bounded by `Integer` has the limits too, `T.MaxValue` and
  `T.MinValue`, each instance's own ([generics](/spec/generics/)).
- Settled by the user 2026-11-04: Nested data types ([generics](/spec/generics/#polymorphic-recursion)): a data type naming itself
  at a larger instance in its fields (`Deeper(inner: Tree[Nest[T]])` in `Tree[T]`) is rejected at
  the type as polymorphic recursion, even where no code compares, hashes or shows it, since every
  data type's structural equality, hash and `Display` would need a copy for every depth. A class
  naming itself so in a field is not rejected for that alone; only code that grows with it is.
- Settled by the user 2026-11-04: Which growth counts ([generics](/spec/generics/#polymorphic-recursion)): growth is measured as
  nesting depth, and a cycle is rejected when a round of it nests its type arguments deeper in
  total. A call through a bound on a value of a type parameter takes that type's arguments apart
  (a `Box[U]` reaching `Box`'s method at `U` is one level less); an implementation reached through
  a method table whose static type names every type parameter of the receiver's class (an
  `Iterator[(A, int)]` for an `IndexedIterator[A]`) takes them apart from the receiver's, while one
  reached through a trait that leaves some unnamed (a trait with no type parameters) continues the
  cycle from wherever the class's larger instance was made.
- Settled by the user 2026-11-04: Literal shapes ([collections](/spec/collections/#literals)): each of the library's collections is
  built only by the literal of its own shape: every `Seq` by `[a, b]`, `Set`, `TreeSet` and the
  mutable `Set` by `{a, b}`, and `Map`, `TreeMap` and the mutable `Map` by `{k: v}`, so
  `val s: Set[int] = [1, 2]` is an error naming the literal to write; `{}` is the empty set or the
  empty map, whichever is expected. A program's own collection with a builder is built by any
  literal whose elements its builder takes, a map literal only by a builder of pairs. A map literal's
  keys agree on one type and its values on another, each as `if` branches do. The mutable `Set` and
  `Map` gain companions with `empty` and `newBuilder`.
- Settled by the user 2026-11-04: Packages ([packages](/spec/packages/)), modelled on Sysl's package system:
  - **The manifest**: `package { name, version, description }`, `dependencies { … }` and `workspace
    = [ … ]`, and no other key; an unknown key is an error, not a warning. A name is a lowercase
    letter then lowercase letters, digits, `_` and `-`; a version is `major.minor.patch` with no
    leading zeros. A manifest is optional: a directory or file with none above it is a program as
    before.
  - **Dependencies**: `{ git = "host/owner/name", version = "x.y.z" }` read at the tag `vx.y.z`
    and cloned from `https://`; `{ path = "../dir" }`, relative to the manifest naming it, and in a
    fetched package naming another package of its repository at its version (refused when it leaves
    the repository); `{ workspace = true }`. A coordinate with `https://` or `.git` is refused.
    Features and members of a repository are settled above.
  - **Members, as built**: a fetched package's in-repository `path` and `workspace = true`
    dependencies are read as the repository's own packages at its version whether the package is a
    member or the repository's root package, since the root of a workspace with a `package` block is
    its first member. A member's directory may hold no `.`, `..` or `*`. The clone is filed once per
    repository and version; a repository whose root has no manifest is still filed when a member of
    it is what was asked for.
  - **Vendoring, as built**: `avin tidy` and `avin add` read the machine's cache, fetching into it,
    whether or not the project has a `vendor/`, so a dependency is added to a vendored project and
    then copied in by `avin vendor`. `avin vendor` checks every cache copy before writing anything, so
    a refusal leaves `vendor/` as it was (and a project with none still has none); a vendored copy is
    left alone only when its recorded hash and its files both match its line, so an edited copy is
    replaced; directories left empty by a removal are removed; and it ends `vendored N packages into
    vendor/`, the packages being `avin.sum`'s lines (a member and its repository's root package at
    one version are two packages and one copy). The cache's `.hash` beside a vendored entry is trusted
    by a build, as the cache's is: a build does not re-hash `vendor/`.
  - **`rev` pins**: `{ git = "…", rev = "<hex>" }`, seven to forty lowercase hexadecimal digits,
    refused with a `version` or without `git`. The repository is cloned whole and the commit
    resolved to its full id, fetched by that id when no branch or tag reaches it (only a full id
    can); the cache entry is `@r<full id>` and the `avin.sum` line `<coordinate> r<full id>
    sha256:…`, so an abbreviation and the full id are one package. A cached commit an abbreviation
    begins is found without the network. A package pinned at two commits, or at a commit and a
    release, anywhere in the graph is refused naming both: a commit is not chosen among, since
    none is higher. `avin add <coordinate>@<rev>` writes the full id.
  - **What a dependency brings in**: its modules, under their own names; not its root directory's
    files (its `main`) nor its `tests` directory, so a program can be depended on and `avin test`
    runs only the package's own tests. One module in two packages is refused. A directory holding its
    own manifest is another package and never a module of the tree above it.
  - **Version selection**: the highest version any manifest in the graph asks for, over every member
    of a workspace.
  - **The cache** is the machine's, `<cache>/avin/pkg/<coordinate>/@v<version>/`, the clone's
    `.git` removed and the tree's hash written beside it. Builds fetch; the language server does not.
  - **`avin.sum`**: `<coordinate> v<version> sha256:<hex>` per fetched package and version read,
    one file per project (the workspace root's); the hash is Sysl's tree hash (SHA-256 of a sorted
    listing of each file's SHA-256 and path, `.git` and the root's `.avin/` left out). A mismatch is
    refused; a build only adds lines; `avin tidy [--check]` rewrites it to what the whole resolution
    reads.
  - **Workspaces**: Sysl's: members listed by directory with `*` within a name, a root with a
    `package` block a member, a root without one taking no dependencies and refused as a program.
  - **Finding the package**: every command walks up from the path it is given to the nearest
    manifest with a `package` block, a workspace root without one being passed over. Sysl looks only
    beside the path; walking up is what lets a file, a test directory or an editor's open file name
    its package.
  - **`avin new <dir> [--lib]`** names the package by the directory, writes a module of that name
    (`-` written `_`), `main.avin` unless `--lib`, a test in `tests/` and a `.gitignore` of `.avin/`;
    the names `avin` and `tests` are refused. `avin build` names the executable after the package.
  - **`avin add <coordinate>[@<version>] [<package>]`** records a git dependency: the version is the
    one given, or the highest release tag the host lists (`v` and three numbers; no release is an
    error naming the way to give one). The package is fetched first and listed under the name its own
    manifest gives it; a dependency already listed, under that coordinate or that name, has its entry
    replaced in place, to a lower version too. The edit is made in the manifest's text, so comments
    and layout stay: the entry goes last in the `dependencies` block, a block being made at the end
    of the file when there is none. The result is resolved and `avin.sum` gains its lines (a
    version that is no longer chosen keeps its line until `avin tidy`); where fetching or resolving
    fails, the manifest is put back as it was. It adds only git dependencies: a path dependency is
    written by hand.
- Settled by the user 2026-11-04: Repeated parameters beside overloads and constructors
  ([declarations](/spec/declarations/#repeated-parameters)): an overload with a repeated parameter
  is a candidate only where no overload without one accepts the call, so `pick(x: long)` takes
  `pick(1)` beside `pick(xs: int*)`; among those with one, the arguments are compared by the
  element type. A class's constructor may end with a repeated parameter, whose field is a
  `Vector[T]` (`class Path(val parts: string*)`), and a subclass passes it on with `: Path(parts*)`.
  The core library gains `max(first, rest*)` and `min(first, rest*)` on `T: Ord`, answering the
  greatest or least of one value or more, the first of equal ones.
- Settled by the user 2026-11-04: Option, Result and string methods ([errors](/spec/errors/), [types](/spec/types/#string)):
  `Option` gains `exists`, `forall`, `contains` (on `T: Eq`, which every type meets), `toList`,
  `toVector` and `zip`; `Result` gains `exists`, `forall`, `toList` and `orElse(f)`, where `f`
  takes the error and answers a `Result` that may have another error type. `forall` is true of
  `None` and of an `Err`. `string` gains `chars`, `count`, `split`, `lines`, `isBlank`, `reverse`,
  `padStart`, `padEnd`, `equalsIgnoreCase`, `take`, `drop`, `takeRight` and `dropRight`, which count
  characters where the older methods count bytes. `split` keeps every empty piece and traps on an
  empty separator (no piece boundary is defined there). `lines` ends a line at `\n` or `\r\n`; a
  terminator at the end starts no further line, the empty string has no lines, and a lone `\r`
  stays in its line. `take`, `drop`, `takeRight`, `dropRight` and the padding widths clamp rather
  than trap.
- Settled by the user 2026-11-04: The methods every `Iterable` has ([collections](/spec/collections/#methods-every-iterable-has)):
  folds, `reduce` (trapping when empty) and `reduceOption`, searches, `count`, `minBy`/`maxBy` and
  `min`/`max` (trapping when empty) with `minByOption`/`maxByOption` and `minOption`/`maxOption`
  (answering `Option`), the reference library's names, `mkString`, `partition`, `groupBy`, `groupMap`,
  `groupMapReduce`, the conversions and `zipWithIndex` are written once on `Iterable` and replace
  the copies `Seq` and `Set` had. A collection they make is a `Vector`, whatever the receiver:
  `partition` answers `(Vector[T], Vector[T])`, `groupBy` a `Map[K, Vector[T]]`, `zipWithIndex` a
  `Vector[(T, int)]`, so a set's or a map's partition keeps every element, equal results included,
  and a list's comes back indexed. A type's own method of the same signature still answers where the
  type is known (`List.toList()` is the list itself). `Iterator` has lazy `map`, `filter`, `take`,
  `drop`, `takeWhile`, `dropWhile`, `zip` and `zipWithIndex` answering iterators; its
  `zipWithIndex` is the trait's own method and so is found before `Iterable`'s. To compile these, a
  method table holds a method at a position only for the types a call through that position can
  reach, by the receiver's static type, so a trait method that makes an instance at a larger type
  argument (`zipWithIndex` on an `Iterator[(T, int)]`) is not compiled for every type it makes.
- Settled by the user 2026-11-04: Set and map methods ([collections](/spec/collections/#maps-and-sets)): an immutable `Set` has
  `++` and `--` taking any `Iterable[A]` beside `union`, `intersect`, `diff` and `subsetOf`, which
  keep their names and their `Set` parameter; `+` and `-` stay reserved for `Add` and `Sub`, so one
  element goes in with `incl` and out with `excl`, the reference library's names. An immutable `Map`'s `++` takes any `Iterable[(K, V)]` and `--` any
  `Iterable[K]`. `Map.keys()` and `values()` answer a `Vector` in iteration order, so the two
  line up by index, rather than one-shot iterators; `keysIterator()` and `valuesIterator()` are the
  lazy forms, and `keySet()` answers a `Set`. A mutable map's `keys()` and `values()` are likewise
  vector snapshots and its `keySet()` an immutable `Set` snapshot. `transform` is the new-map form
  taking the key and the value (`mapValues` takes the value alone) on both maps, and a mutable map
  has no in-place `transform`. Everything that builds a collection from a mutable set or map
  (`map`, `flatMap`, `union`, `intersect`, `diff`, `mapValues`, `transform`, `filterKeys`) answers a
  new mutable one; `filterInPlace(p)` and `updateWith(k, f)` change in place, `updateWith` answering the
  new binding as an `Option`. `+=`, `-=`, `++=` and `--=` are methods on the mutable collections
  (a map's `+=` takes a `(key, value)` pair), so they work on a `val`; removing an absent element
  or key is not an error.
- Settled by the user 2026-11-04: Sorting ([collections](/spec/collections/#sorting)): every `Seq` has `sorted()` (elements `Ord`),
  `sortBy[K: Ord](key)` and `sortWith(compare: (T, T) -> int)`, answering a `Vector`; `List`
  has its own three answering a `List`. `Array`, `Buffer` and `Deque` sort in place with
  `sortInPlace()`, `sortInPlaceBy(key)` and `sortInPlaceWith(compare)`, the reference library's
  names. Every sort is a stable merge sort, O(n log n) comparisons in the worst case with one
  scratch copy of the elements; `sortBy` and `sortInPlaceBy` compute `key` once per element. `avin.fs.listDir` answers its names sorted by code point rather than in the file
  system's order.
- Settled by the user 2026-11-04: The methods every `Seq` has ([collections](/spec/collections/#methods-every-seq-has)): `map`,
  `flatMap`, `collect`, `filter`, `filterNot`, `withFilter`, `take`, `drop`, `takeWhile`,
  `dropWhile`, `takeRight`, `dropRight`, `slice`, `splitAt`, `span`, `tail`, `init`, `reverse`,
  `++`, `:+`, `+:`, `updated`, `padTo`, `indices`, `zip`, `zipAll`, `scanLeft`, `distinct`,
  `distinctBy`, `grouped`, `sliding`, `lastIndexOf`, `lastIndexWhere`, `startsWith`, `endsWith`,
  `sameElements`, `intersect` and `diff` are written once on `Seq`, answering a `Vector` (`indices`
  a `Range`, `grouped` and `sliding` an `Iterator[Vector[T]]`). `List`, `Vector` and `ArraySeq`
  keep or gain their own versions answering their own kind. The mutable `Array`, `Buffer` and
  `Deque` answer the element-wise `map`, `flatMap`, `collect`, `filter` and `filterNot` with a new
  collection of their own kind and the structural methods with a `Vector`, so a structural method
  never shares or aliases a mutable receiver; `Array.slice` stays an `Array`. A count is clamped to
  `0 .. length()`; `slice` traps unless its indices are in order within the sequence, as it does on
  strings and arrays. `intersect` and `diff` are multiset operations: each occurrence in the
  argument matches the first unmatched equal element, and the receiver's order is kept. `Range` is
  a `Seq[int]` with constant-time `length`, `apply` and `contains`, and so an `(int) -> int`.
  Overload resolution accepts an argument of a subtype of a parameter's type, as a single
  function's call does, and compares candidates by the same conformance, so a type's own
  `++(other: List[T])` and `Seq`'s `++(other: Iterable[T])` are overloads of which the first is
  more specific.
- Settled by the user 2026-11-04: **Two sequences are equal when they have the same elements in order, whichever kinds they are**:
  a `Vector`, `List`, `ArraySeq`, `Array`, `Buffer`, `Deque` and `Range` of equal elements compare
  equal with `==` and hash alike, with one comparison and one hash written once on `Seq` for them
  all. `==` between two sequence types needs the same element type on both sides (a `Vector[int]`
  and a `List[string]` are a compile-time error, as any two types with no common type are); a
  value typed as `Seq[T]` compares with any sequence. A class that implements `Eq` itself keeps its
  own equality between two of its instances. Settled (user, 2026-11-04): through `Any` and
  trait-typed values sequences of equal elements in order are equal whatever their class, with
  hashes that agree (`val a: Any = [1, 2]; a == List(1, 2)` is `true`); the elements are compared
  as `Any` values are, so `[1] == ["a"]` held as `Any` is `false`. An `Array` is a `Seq` here and
  compares by contents, as it already did. The mutable `Array`, `Buffer` and `Deque` compare
  as they are when the comparison runs, so a hash held across a change is stale, as it was before.
- Settled by the user 2026-11-04: **Ranges of other integer types are `NumericRange[T: Integer]`**.
  `Range` stays the `int` range, and a bare `Range` in a signature means it. `a..b` and `a..<b`
  written as values make a `NumericRange[T]` where the bounds are integers of any type other than
  `int` (`long`, `i8`, `i16`, …); an integer literal bound takes its type from the other bound, and
  bounds of two types are an error. `by` takes a step of the same type, `T`. It is a `Seq[T]`
  with the `Range` rules: a zero step traps, a range is empty when its start lies past its limit in
  the step's direction, and iteration stops without stepping past the limit, so it never passes
  either end of `T`. `get`, `apply`, `head`, `contains` and `isEmpty` are answered from the bounds
  and never wrap, at any width; `length()` traps beyond `int`, and `last()` and the other
  questions that count the values trap for a range whose ends are more than the largest `long`
  apart (the smallest `long` to the largest), which only a range of `long`s can be. `for` over a range
  written in place already counted any integer type, and still does.
- Settled by the user 2026-11-04: Value definitions in a `for` with `yield` ([for comprehensions](/spec/collections/#generators-guards-and-yield)):
  `pattern = value` after a generator, each with its own `if` guards, written with no `val`, and
  never first. It is evaluated once per element that reaches it and binds in what follows. Where no
  guard follows, it is a `val` in the function passed to its generator's source; where a guard
  follows, it and the definitions before it after the same generator pair each element with its
  value through the source's `map`, so it is evaluated for every element before that guard tests
  any, and over a set or map the pair has to be hashable. A definition may not rebind a name its
  generator or an earlier definition after it binds; a later generator may.
- Settled by the user 2026-11-04: Comprehensions over the mutable collections ([for comprehensions](/spec/collections/#generators-guards-and-yield)):
  a mutable `Set` and `Map` have `withFilter`, a lazy view as the immutable ones have, and a
  mutable `Map` a `map` over its entries as pairs, so the first source's kind decides the result as
  elsewhere: a new mutable set or map, as an `Array`, a `Buffer` or a `Deque` gives a new one of its
  own kind, the source unchanged.
- Settled by the user 2026-11-04: `avin.process` ([system library](/spec/system-library/)): `args() -> Vector[string]` holds the
  arguments after the program's name, and `programName()` gives the name separately; `env(name)` is
  an `Option[string]`; `exit(code: int) -> Nothing` flushes standard output first, as a trap does.
  Arguments are checked as UTF-8 at startup. A test run by `avin test` sees no arguments. `main`
  may take the same vector: `main(args: Vector[string])` (settled above).
- Settled by the user 2026-11-04: `avin.fs` and `avin.io` ([system library](/spec/system-library/)): `IoError`, shared by every
  module that does input or output, lives in `avin.io` as a plain `data` type with the variants
  `NotFound`, `PermissionDenied`, `AlreadyExists`, `IsADirectory`, `NotADirectory`,
  `DirectoryNotEmpty`, `InvalidUtf8(offset: long)` and `Other(text: string)`, and a `message()`
  sentence. `avin.fs` reads and writes whole files (`readText`, `readBytes`, `writeText`,
  `appendText`, `writeBytes`, `appendBytes`), asks `exists`, `isDir` and `isFile`, and makes and
  removes (`createDir`, `createDirAll`, `createTempDir`, `remove`, `removeDir`, `removeDirAll`),
  each failure an `IoError` in a `Result`. Writing replaces a file and appending creates one;
  `createDirAll` accepts a directory already there and `removeDirAll` a path already gone.
  `listDir(dir) -> Result[Vector[string], IoError]` answers the entries' names, without the
  directory and without `.` and `..`, sorted by code point, so two listings of an unchanged
  directory agree.
- Settled by the user 2026-11-04: Standard input and error, in `avin.io` beside `IoError` ([system library](/spec/system-library/)):
  standard input as text through `readLine() -> Option[string]`, `lines() -> Iterator[string]` and
  `readAll() -> string`, sharing one runtime-held buffer. A line ends at `\n` with a `\r` before it
  trimmed; the last line needs no terminator. Ill-formed UTF-8 becomes U+FFFD and a failed read ends
  the input, so reading standard input answers no `IoError`, where `fs.readText` answers
  `InvalidUtf8`. `eprintln(x)` mirrors `println` on standard error and is in the prelude with
  `StandardError`, an unbuffered `Writer`; every write to standard error flushes standard output
  first. A test run by `avin test` reads empty input.
- Settled by the user 2026-11-04: Running and testing (stage 10): `avin run <program> [-O0 | -O2] [-- args]` builds into
  `.avin/build/` in the program's directory, reuses the executable while nothing it was built from
  has changed, and exits with the program's status, or 128 and the signal's number for a trap. Its
  arguments reach the process; reading them is `avin.process`'s, in the system library. A test is a
  top-level `@test` function of the program, in any module, taking nothing and yielding unit, named
  by the function or by `@test("a sentence")`; a program built for its tests need not have `main`.
  `run` is the default command and the current directory the default program: `avin` is `avin run .`,
  `avin <path> ...` is `avin run <path> ...` when the first argument is not a command name, and the
  commands taking a program default it to `.`. A command name is always the command; a path that
  spells one is reached as `./test`. `avin test`'s first argument is the program when it names an
  existing path and the filter otherwise.
  `avin test [<program>] [filter]` builds one executable and runs each test in a process of its own, so
  a trap stops only that test, reporting `ok` or `FAIL` per test grouped by file, the reason and the
  test's output under a failure, and `N passed, M failed`; it exits 1 when any failed. The core
  library's `assert(condition)`, `assert(condition, message)` and `assertEq(actual, expected)` (with
  `Eq` and `Debug` bounds, showing both values' `Debug` forms) trap at the assertion's place.
- Settled by the user 2026-11-04: Test attributes, hooks and `@tests` files ([declarations](/spec/declarations/#tests)), following
  Sysl's model:
  - **A test yields unit or a `Result[unit, E]`** with `E: Display`; an `Err` fails it, reading
    `test answered Err: <error>` at the test's name.
  - **`@test`'s arguments**: a description first, then `should_trap` (alone, or with text the run's
    output must contain), `timeout: N` (whole seconds, 0 to 2147483, 0 for none, in place of the
    run's 60) and `ignore: "reason"` (required and not empty), each at most once. `should_trap` is
    satisfied by a trap, not by a non-zero exit. An ignored test starts no process and runs no hook,
    and the summary adds `, K ignored` when K is not 0.
  - **Hooks** `@setup`, `@teardown`, `@setup_all`, `@teardown_all`, at most one of each per module,
    top-level, taking and yielding nothing. `@setup` runs in the test's process; `@teardown` there
    after a test that returned and in a process of its own after one that trapped or timed out (not
    after a failed `@setup`); the `_all` pair each in a process of their own, around the module's
    selected tests that are not ignored. A failed `@setup` fails its test unrun; a failed
    `@setup_all` is a failure of its own and fails each of the module's tests unrun, and skips
    `@teardown_all`; a failed `@teardown` or `@teardown_all` is a failure of its own.
  - **`@tests` files** are checked by every command and built only by `avin test`: their
    module-level values are not computed in a program that runs. Only a `@test` function, a hook
    (a lambda in either counting as it) or another `@tests` file may name what one declares; an impl
    in one is for a type or of a trait it declares; `main` is not declared in one.
- Settled by the user 2026-11-04: Format specifiers ([strings and formatting](/spec/strings-and-formatting/#format-specifiers)):
  - **Integers are signed in every base**: `%x`, `%X`, `%o` and `%b` write a negative value as `-`
    and its magnitude's digits (`${-42}%x` is `-2a`), so the text does not depend on the value's
    width, and `+` and the space flag apply to them as to `%d`. There is no `%u`; it is refused
    with a pointer to `%d`.
  - **A flag that changes nothing under its conversion is an error**, as is a precision under `%c`.
    `%s` refuses the numbers (integers, `byte`, `real`), which have conversions of their own; it
    takes every other `Display` value.
  - **`%s` counts characters**: its width and precision are characters, not bytes, as every
    `Display` width is.
  - **Only a `%` right after a hole is special**: there is no `%%` escape; `%` anywhere else, doubled
    or not, is text.
  - **A count written as a hole is an integer of any width**, not a `byte`; a negative width pads on
    the right, a negative precision is none, and a count beyond `int` is the largest `int`
    (`FormatSpec.withWidth`, `withPrecision`).
  - **`StringContext.f`, called directly**, reads its specifiers as it runs: a malformed one, `%?`
    (its arguments are `Display` values) and a count that is not an `int` or a `long` trap; a value
    under a conversion its type does not take renders in its own form.
- Settled by the user 2026-11-04: The core library's own tests ([tests](/spec/declarations/#tests)): they live in `library/tests`,
  as a package's live in its `tests` directory, and are an ordinary program of `@test` functions in
  its root module, which `avin test library/tests` runs and the compiler's gate runs at `-O0` and
  `-O2`. They test the library through its public names, as a program sees it. The directory is no
  part of the core library every program is compiled with, and a `@test` in the core library's own
  modules, like one in a dependency, is never a program's test.
- Settled by the user 2026-11-04: A collection added to itself ([collections](/spec/collections/#buffer)): `b.addAll(b)` and
  `b ++= b` on a `Buffer` or a `Deque` append the elements it held when the call began, once, and
  `s --= s` on a mutable `Set` empties it.
- **Superseded 2026-11-05** by the [dates and times](/spec/time/) entry that follows; kept as it
  was. Settled by the user 2026-11-04: [`avin.time`](/spec/system-library/#avin-time): `Duration`, `Instant` (monotonic) and `WallTime`
  (wall clock, a `Duration` since the 1970 epoch) are data types counting whole microseconds in a
  `long`, overflow trapping; the two clocks are distinct types so their readings never mix. A
  duration implements `Add`, `Sub`, `Neg`, and scales by a `long` with its methods `*` and `/`; a
  reading combines with a duration through its methods `+` and `-`, and two readings of one clock
  through `-` (or `since`), which yields their `Duration`. The
  text forms are a duration's largest-unit form (`1.5ms`) and an ISO 8601 UTC timestamp. Sleeping
  and calendar fields beyond the timestamp text are not part of it yet.
- Settled by the user 2026-11-05: [dates and times](/spec/time/) (`avin.time`) follow slate's
  date/time design with Avin's spellings (types PascalCase, functions lowerCamelCase). Eight types:
  `Instant` (a point on the timeline, microseconds since 1970 UTC, displayed as ISO 8601 with `Z`;
  it replaces `WallTime`), `Duration`, `Date` (a calendar date with no zone; no `Local` prefix
  anywhere), `Time` (a time of day), `DateTime` (a date and a time, no zone), `Zone`, `Zoned` (a
  date-time in a zone, the only zoned type) and `Period` (months and years). There is no monotonic
  `Instant`: `monotonic()` answers a `Duration`, a monotonic reading naming no point in time, and an
  elapsed time is `monotonic() - start`. Makers: `now()`, `monotonic()`, `date(y, m, d)`,
  `time(h, m, s)`, `dateTime(…)`, `zone("America/Toronto")`, `localZone()`, and `utc`, a value
  rather than a call; `epochSeconds`, `epochMillis` and `epochMicros` to and from counts; the
  durations `micros millis seconds minutes hours days weeks`, also as methods (`2.hours()`); the
  periods `months` and `years`. The implementation binds with `@sysl` over Sysl 0.10.0's `sysl.time`
  (`Date`, `Time`, `DateTime`, `YearMonthDay`, `Offset`, `Weekday`, `Resolution`, `Zone` over
  `sysl.time.tzif` with `offset_at` and `resolve`, `Zoned`, `Period`), with the system clock and the
  local zone from `sysl.posix.time`, and no new C. The choices the design makes beyond these are
  under [Proposed](#proposed).
- Settled by the user 2026-11-04: Bindings to Sysl packages ([packages](/spec/packages/#sysl-dependencies),
  [bindings](/spec/sysl-bindings/#sysl-packages)): a manifest names the Sysl packages its bindings
  reach in a block of its own, `sysl-dependencies`, beside `dependencies`, each entry written as an
  Avin dependency is (`git` with `version` or `rev`, or `path`; never `workspace`). The block is
  separate because Sysl, not Avin, resolves, fetches and verifies those packages: they are not
  Avin packages, so they take no part in Avin's version selection, `avin.sum`, `avin add` or
  `avin tidy`. The shim package's manifest lists every Sysl dependency of every package in the
  program, so a library's bindings build in any program that uses it; one name at two releases of
  one coordinate takes the higher, and one name with any other two origins is refused naming both.
  A fetched package may not name a Sysl directory. The shim archive and the cached executable are
  keyed by these entries and by the sources of each Sysl package named by directory. Sysl's other
  dependency keys (`mount`, `features`, `options`) are not offered yet.
- Settled by the user 2026-11-04: The rest of the system library ([system library](/spec/system-library/)), each module following
  the reference standard library's names and shapes where Avin's rules allow:
  - **[The environment and the platform](/spec/system-library/#the-environment-and-the-platform)**
    are functions of `avin.process`, beside `env`, not a module of their own: `environment()` as a
    `Map`, `currentDir`, `homeDir`, `executable`, `processors`, `pid` and `os()`. There is no
    property store and nothing sets a variable; a child's variables are set on its command. Bindings:
    `sysl.fs.current_dir` and `home_dir`, `sysl.env.current_exe`, `sysl.process.pid` (a `u32`, which
    fits a `long`), `sysl.os` through a mirror of `sysl.Os`, and the runtime's processor count.
    `environment()` needs `sysl.env.vars`, which answers a `Buf` of pairs: it waits for tuples as a
    boundary type, a stage-7 addition.
  - **[Commands](/spec/system-library/#commands)** in `avin.process`: a `Command` value, combined
    with `|` (a pipeline), `andThen`, `orElse` and `followedBy`, redirected with `toFile`,
    `appendTo`, `fromFile` and `withInput`, and run by `run()` (the exit status), `output()` (the
    text, any status but success an `Err`), `lines()` (an iterator as output arrives) and `start()`
    (a `Child`). Importing the module adds `run()` and `output()` to `string`. **Operators Avin cannot
    define**: postfix `!` and `!!` (no method is called postfix, and `!` is the language's), and
    anything spelled with `#` (not an operator character), so `#|` is `|`, and `#&&`, `#||`, `###`,
    `#>`, `#>>` and `#<` are words; `run()` is the status, where the reference's `run()` starts the
    process, which here is `start()`. Bindings: `sysl.process.spawn` and `pipe`, with a mirror of
    `Stdio` holding a `sysl.fs.File` handle, `Spawned` a handle waited for `blocking`, `Status`
    mirrored as `ExitStatus` itself, `Var`, `sysl.fs.create`, `append` and `open` for the
    redirections, and `sysl.io.FdReader` for `lines()`, which answers bytes as they arrive.
  - **[`Source`](/spec/system-library/#source)** in `avin.io`: `fromFile` (read whole, every failure
    up front), `fromString`, `stdin()`, and `lines()`, `chars()` and `readAll()`, each consuming. The
    reference's `getLines` and `mkString` are `lines` and `readAll`, the names `avin.io` already uses.
    No new binding: it is written over `fs.readText` and standard input's buffer, with one runtime
    entry taking a character from that buffer for `chars()`. Not blocked.
  - **[`avin.random`](/spec/system-library/#avin-random)**: a `Random` class (`Random(seed)`
    reproducible, `Random()` seeded by the platform) with `nextInt`, `nextLong`, bounded forms,
    `between`, `nextReal`, `nextBoolean`, `nextGaussian`, `nextBytes`, `alphanumeric`, `shuffle` and
    `setSeed`; the companion's methods use one shared, locked generator. PCG32, written in Avin with
    the wrapping operators, so a seed gives the same sequence everywhere. Binding: one runtime
    function for the platform's seed over `sysl.posix.rand.seed_from_os`, since a `u64` above
    `long.MaxValue` traps at a binding's result; it answers the entropy's top 63 bits.
    `nextGaussian` uses `avin.math`'s `sqrt` and `log`.
  - **[`avin.net`](/spec/system-library/#avin-net)**: `TcpStream` and `TcpListener`; `Url`, parsed
    and percent-encoded, written in Avin; an HTTP/1.1 client (`get`, `post`, `send` with a `Request`)
    written in Avin over the TCP stream, a status never an error. `IoError` gains
    `ConnectionRefused`, `ConnectionReset` and `TimedOut`. Bindings: `sysl.net.resolve`, `socket`
    and the `Socket` methods, held as an opaque handle (a `Socket`, and the `Address` it connects
    to, are not boundary types). `Url` needs neither. `https` takes a Sysl TLS package, named in
    the library's `sysl-dependencies` and bound as any package is; opaque handles and bindings to
    Sysl packages beyond the standard library are both in the compiler.
  - **[Concurrency](/spec/system-library/#concurrency)** stays in `avin.threads`; there is no
    separate concurrency module. The reference's futures map onto what is there (`successful`, `all`
    for `sequence`, `first` for `firstCompletedOf`, `await` for waiting with a time limit); added are
    `Future.traverse`, `mapErr` and `orElse` on a future of a `Result` (its `recover` and
    `fallbackTo`), an `Executor` for pools of one's own (its execution context, chosen explicitly),
    `threads.blocking(body)` and `threads.sleep(d)`. There is no `failed` or `filter`, since a future
    carries no failure. Bindings: none beyond a runtime timed wait for `sleep`. Not blocked.
  - **JSON** is a package of its own, not part of the system library.

- Settled by the user 2026-11-04: Lower bounds and the expected type ([generics](/spec/generics/#lower-bounds)): the expected type
  decides a lower-bounded type argument only where it admits the bound; otherwise the argument is
  inferred from the bound and the arguments, and the result is passed as any value is. So
  `println(m.getOrElse(k, "none"), n)` on a `Map[string, string]`, where each argument expects a
  `Display`, takes `W` as `string` rather than reporting `Display` as no supertype of `string`. A
  delayed (by-name) argument widens an open lower-bounded type argument as any other argument does:
  `cats.getOrElse(k, dog)` is an `Animal`. Both keep programs working that called the methods before
  they took `[W >: V]`.
- Settled by the user 2026-11-04: Trait fields ([declarations](/spec/declarations/#trait)): a trait's field declares its type, since
  each conforming class's layout and the trait's `val` are known before any body is checked; it is
  public, as every member of a trait is, and is never `lazy`. A class never overrides a trait's
  field, `val` or `var`: declaring a field of its name is an error rather than an override. A trait
  never redeclares, with a value, a `val` of a trait it requires.
- Settled by the user 2026-11-04: The wrapping conversion is spelled `i8.wrap(x)`, as the wrapping operators are spelled `+%`: the
  integer type's name, `.wrap`, one operand, any integer target (a bounded type parameter included,
  `T.wrap(n)`). The core library needs it to test whether a value fits a narrower width (the formatter
  converts a generic integer to `long` and back).
- Settled by the user 2026-11-04: [`avin.regex`](/spec/regex/): the pattern language and its matching follow the established
  backtracking dialect exactly: leftmost-first alternation, greedy, reluctant and possessive
  repetition, atomic groups, bounded lookbehind (a repetition without an upper bound in one is an
  error), and the same rules for what a repeated group captures; `\d`, `\s`, `\w`, `\b` and the
  POSIX classes are ASCII unless `U` makes them Unicode's, and `i` folds only ASCII without `u`. Matching walks characters (scalar
  values); every offset in the API is a byte offset, and a `PatternError`'s index counts the
  pattern's characters. There is no null: `group(i)`, `group(name)`, `before(i)`, `after(i)` answer
  `Option[string]`, `subgroups()` a `List[Option[string]]`, and `start(i)`/`end(i)` -1 for a group
  that took no part. Members without arguments are methods (`"a+".r()`, `m.start()`). `Regex(p)`
  traps on an invalid pattern with the error's message and `Regex.compile(p)` answers a
  `Result[Regex, PatternError]`; a group number or name the match does not have traps. Scripts,
  blocks, the binary properties beyond the general category (`Alphabetic`, `Uppercase`,
  `Lowercase`, `Ideographic`, `Word`, `Alnum`, emoji), the flag `U`, `\X` and `\b{g}` answer from
  tables generated from the Unicode Character Database, of the version the general categories come
  from; the flag `c` and `\N{…}` are refused with an `Unsupported` error. Replacing and splitting
  follow the same dialect: `replaceAllIn` (a replacement, or a `(Match) -> string` whose result is
  read as one), `replaceFirstIn`, `replaceSomeIn`, `split` answering an `Array[string]` with the
  empty pieces at the end dropped, `Pattern.split(s, limit = 0)`, `Regex.quote` and
  `Regex.quoteReplacement`; `$n` takes digits while they name a group, `${name}` is the pattern's
  own name only, and a reference that cannot be resolved traps with the dialect's message.
- Settled by the user 2026-11-04: Extractor patterns ([control flow](/spec/operators-and-control-flow/#extractor-patterns)):
  `Name(p…)` names a variant wherever one of that name is in scope, and only otherwise a value (an
  object, a companion, a local) whose type has `unapply` or `unapplySeq`; where it has both,
  `unapply` is used. `unapply` answers a `bool`, matched with no patterns (`Even()`), or an
  `Option`, one pattern matching its value whole and several a tuple's parts. `unapplySeq` answers
  an `Option` of a sequence, matched as a sequence pattern; the prefix-and-sequence form is not
  taken. The matched value is passed to the method as it is, with no type test first, so the
  method's parameter accepts the matched value's type. An extractor pattern never counts toward
  exhaustiveness, and since a binding's or a `for`'s pattern must cover every value, extractors
  appear in `match`, `is`, clauses and match functions only.
- Settled by the user 2026-11-04: Rest patterns ([collections](/spec/collections/#sequence-patterns)): `rest*` or `_*`, last in a
  sequence pattern or an `unapplySeq` extractor's patterns, binds what the patterns before it leave:
  on a list the tail, so `[]` and `[head, tail*]` cover every list; on another sequence what
  `drop(n)` answers, a `Vector`, and there too sequence patterns count lengths, so `[]` and
  `[head, tail*]` cover every sequence. `[rest*]` alone is the whole sequence, of its own type.
- Settled by the user 2026-11-04: A regex as an extractor ([regular expressions](/spec/regex/#regex)): `Regex.unapplySeq(s)`
  answers the groups' texts as a `List[string]` where `s` matches as `matches` asks, the whole of it
  or, unanchored, any part. A group that took no part gives the empty string, since a `string` has
  no null. `Groups`, a top-level object of `avin.regex`, takes apart any `Match`'s groups, `None`
  for a pattern with none. There is no `unapplySeq` taking a `Match` on `Regex` itself, which
  would need a match to know the regex that found it.
- Settled by the user 2026-11-04: Sysl handles ([Sysl bindings](/spec/sysl-bindings/#handles)): a `class` declared with `@sysl`
  naming a Sysl type has no parameters, fields or constructor and is made only by a binding that
  returns it, directly or as the value of an `Option` or `Result`. A method without a body is bound
  to the Sysl method of its name's words in snake_case, or to the one its own `@sysl("name")`
  names. Every handle has `close()`, which gives its counted reference back and does nothing a
  second time; a bound method on a closed handle traps at the call with "`Name` is closed"; a
  handle never closed is given back by a finalizer when the collector frees it. A handle compares
  by identity, and has no `Display`.
  Settled by the user 2026-11-04, replacing "a collection of handles does not cross" and "a handle
  crosses back into Sysl only as the receiver of its own methods": a binding's result may also be
  an `Array` or `Vector` of handles, also inside an `Option` or `Result`, made from a Sysl slice or
  `Buf`, each element a handle of its own with its own reference and finalizer. A handle by itself
  goes into Sysl as the receiver of its own methods or as a parameter of any binding or bound
  method; a closed one passed traps at the call as a bound method on one does. A handle inside an
  `Option` or a collection still does not go into Sysl.
  Settled by the user 2026-11-04: **how a handle is passed to a Sysl parameter**. The shim cannot
  see the Sysl parameter's type, so it lets Sysl choose: a trait, always held by a counted
  reference, is passed as the handle's reference, which the parameter shares; any other type is
  passed as the parameter's type asks of a generic helper, by value (a copy), as a pointer to the
  handle's own value, or at a `&T` as the handle's own counted reference, which the parameter
  shares. A `*T` parameter of a type with a destructor is refused by Sysl at the binding. Revised
  on the user's delegation 2026-11-05 (branch `sysl-091`): from Sysl 0.9.1 the helper settles at
  the `&T`, so a type with a destructor needs no arm of its own and the shim tells only a trait
  apart; under 0.9.0 a `&T` parameter of a type without a destructor received a reference to a copy.
  Since Sysl now checks the helper's body against the parameter's type, each handle parameter of a
  binding and each handle field of a mirror has helpers of its own, written among that binding's or
  mirror's lines of the shim, so a handle Sysl does not take there is reported as the binding or the
  mirror not matching, in the handle's and the place's Sysl types, and no diagnostic names the
  shim's functions (the reject corpus checks that none does). Alternatives weighed: one helper per
  handle type, reported at the handle class (which names neither the binding nor the parameter at
  fault); explicit type arguments at each call (the shim cannot see the Sysl parameter's type).
  Settled by the user 2026-11-04: **a handle is never used by two threads at once**, since its
  counted reference's count is not atomic. Each handle records an owner, and every use (a bound
  method, the handle passed as a parameter, `close()`) compares it with the running code's and traps
  with "`Name` belongs to another thread" where they differ. The owner is an execution context the
  runtime can hand over, not an operating system thread: a thread's body, a pool task whichever
  worker runs it, and later an actor, re-pointed to whichever thread runs it, so an actor's handles
  never trap between its messages; the check is one compare with a word of the running thread's
  context, which the runtime sets. Ownership moves: what a thread's or a future's body reaches that
  the caller owns becomes the body's at its start, and the caller's later use traps; when the body
  returns, what it was given and what it returns are in transit, as are a future's value and what a
  callback registered on one reaches, and the first code to use a handle in transit takes it. The
  finalizer never checks: it runs only on an unreachable handle, on the collecting thread while the
  others are stopped.
  Settled by the user 2026-11-04, replacing "no mirror holds a handle": **a Sysl mirror's field may
  be a handle**, by itself (not in an `Option`, a collection or a tuple), so that Sysl's
  `sysl.process.Stdio` reaches Avin with its `ToFile(file: File)` and `FromFile(file: File)`. Into
  Sysl the field is passed exactly as a handle parameter is, its reference shared for a type with a
  destructor and otherwise a copy or a pointer as the Sysl field asks, with the same traps at the
  call: "`Name` is closed" and "`Name` belongs to another thread". Out of Sysl, which needed nothing
  more, each such field becomes a handle of its own, as a handle result does.
- Settled by the user 2026-11-04: **Tuples cross the Sysl boundary** ([Sysl bindings](/spec/sysl-bindings/#boundary-types)).
  A tuple of boundary types is a Sysl tuple of the same arity, in each direction, each part converted
  as it would be alone: an integer part out of Sysl takes any Sysl integer with the range check, and
  tuples nest in one another, in `Option`, `Result`, `Array` and `Vector`, and in a mirror's fields.
  A handle may be a part of a tuple that is a binding's result or the value of its `Option` or
  `Result`, so `accept` answers `Result[(Socket, Address), IoError]`; a tuple holding a handle does
  not go into Sysl or sit in a collection. `avin.process` has `environment()` over `sysl.env.vars`.

- Settled by the user 2026-11-04: **Text blocks under an interpolator that reads no escapes** ([text
  blocks](/spec/strings-and-formatting/#text-blocks)). The parts `raw"""…"""` and a program's own
  interpolators are given have the block's indentation and trailing blanks dropped but no escape
  read, and since joining a line with `\` is an escape, no line joined: the `\` and the line break
  after it reach the interpolator as written. `s` and `f` join lines in the lexer. A backslash
  directly before a quote or a backslash is taken with it under every prefix, so `\"""` does not end
  a `raw"""` block and `\\"""` does, as `raw"…"` does not end at `\"` and does at `\\"`.
- Settled by the user 2026-11-04: **A block's first `"""` closes it** ([text blocks](/spec/strings-and-formatting/#text-blocks)),
  so `""""` is the end of the block and a stray quote; a block ending in a quote writes it `\"`.
- Settled by the user 2026-11-04: **`avin.random`'s details** ([system library](/spec/system-library/#avin-random)), where the
  chapter left them open, so that a seed's numbers are fixed:
  - Every generator uses PCG32's stream 54 (increment 109), the reference implementation's
    demonstration stream, so `Random(42).nextInt()` gives its published first output, `0xa15c02b7`
    (as an `int`, `-1587805513`). Seeding is the reference's: one step from zero, the seed added,
    one step more.
  - `nextLong` is two draws, the first the high half; `nextBoolean` is a draw's top bit; `nextReal`
    is a 64-bit draw's top 53 bits over 2^53; `nextInt(n)` rejects 32-bit draws below `2^32 % n`,
    and `nextLong(n)` and `between` on `long` reject 64-bit draws below `2^64 % n`; `between` on `int`
    is `nextInt` of the span where it fits an `int`, and `nextLong` of it otherwise.
  - `between` on `real` draws `min + nextReal() * (max - min)` again while it rounds up to `max`,
    and traps on a NaN bound and where `max - min` is infinite, besides `min >= max`.
  - `nextGaussian` is the polar method, keeping the second value of each pair for the next call;
    `setSeed` drops it. `nextBytes` takes four bytes from each draw, lowest first; a negative count
    traps. `alphanumeric` draws `nextInt(62)` per character, digits, then upper, then lower case.
- Settled by the user 2026-11-04: [`avin.regex`](/spec/regex/#unicode-properties): `\b{g}` decides a boundary by the grapheme
  cluster rules over the whole text before it, so it is never inside a cluster: not between the
  two regional indicators of a flag, nor inside an emoji joined by a zero width joiner or an Indic
  conjunct. `\X` matches the cluster starting where it stands, one way only, and has no bounded
  length, so a lookbehind may not hold it. A script is named by any of its names in
  `PropertyValueAliases.txt`, ignoring case, and only the scripts `Scripts.txt` lists, with
  `Unknown`, exist; a block by its name in `Blocks.txt`, that name without spaces, or any of its
  names in `PropertyValueAliases.txt`, short ones included, ignoring case.
- Settled by the user 2026-11-04: [blocking bindings](/spec/sysl-bindings/#calls-that-wait): a
  binding whose Sysl call may wait is written `@sysl("module.name", blocking)`, and a handle's
  method bound by its own name `@sysl(blocking)`; `blocking` is a bare word, written once, and
  nothing else follows the name. The shim converts every argument and takes a handle method's own
  share of the Sysl value, then marks the thread safe for the collector, makes the call, marks it
  running again (waiting out a collection in progress) and only then converts the result; bytes a
  handle's method answers are copied into Sysl memory before the thread leaves the safe region. A
  blocking call therefore never reads or writes the Avin heap, and an argument that is a pointer
  into it, such as a buffer Sysl fills, would need its object held by a frame for the whole call.
- Settled by the user 2026-11-04: [`Url`](/spec/system-library/#url) follows RFC 3986 as the
  established library's URI does, beyond the first design: user information; IPv6 hosts in
  brackets, with a zone; `base.resolve(ref)` by section 5.2, dot-segment removal included;
  `withScheme`, `withUserInfo`, `withHost`, `withPort`, `withPath`, `withQuery` and `withFragment`;
  and a trapping `Url(text)` beside `Url.parse(text)`, as `Regex(p)` beside `Regex.compile(p)`.

Actors and state machines ([actors and state machines](/spec/actors/)), settled by the user
2026-11-04 as written. They answer the open questions on how actors and machines work and whether
their words are reserved.

- Settled by the user 2026-11-04: **Messages are checked by `Sendable`**, a trait the compiler decides for each type from its
  structure and that is written as a bound; `ActorRef[-M: Sendable]` makes the check once, where a
  message type is named, never at a send. An actor's parameters and every type `ask` and `adapter`
  carry are sendable too.
- Settled by the user 2026-11-04: **What is sendable**: the scalars, `unit` and `string`; tuples and `data` of sendable fields; the
  immutable collections of sendable elements; a final class or an `object` with no `var` field and
  only sendable fields; a trait declared `: Sendable`, each implementor checked; `ActorRef`,
  `Behaviour`, and `Future`, `Promise`, `Channel` and `Atomic` of a sendable type; `Mutex` of any
  type (the one deliberate door to shared mutable state); `Once`; Sysl handles; `Isolated[T]`. Not:
  arrays, buffers, deques, mutable maps and sets, classes with a `var` or an `open` or `abstract`
  class, machines, function values (their captures are not in their type) and `Any`.
- Settled by the user 2026-11-04: **Ownership transfer.** A Sysl handle in a message is put in transit by the send and taken by its
  first user, the receiver; the sender's later use traps, through the handle's owner check. A mutable
  object graph moves as an `Isolated[T]`, made by `isolate(body)`: the compiler checks the body reads
  only sendable values from around it and that nothing it calls, transitively, reaches module-level
  mutable state; `take()` gets it out once (a second take traps) and `with(f)` works on it in place
  under the same check.
- Settled by the user 2026-11-04: **Declaring an actor**: `actor Name[T](params) receives M`; the body runs once at the actor's start
  and holds its fields; `receive` holds `match` arms over `M`, checked for exhaustiveness; methods
  are private helpers; `self` is the actor's `ActorRef[M]`, and the object holding its state is
  never a value.
- Settled by the user 2026-11-04: **Behaviours and `spawn`**: `Counter(0)` is an immutable `Behaviour[M]`, and
  `spawn(behaviour, onFailure = Escalate, executor = Executor.shared())` starts it and answers an
  `ActorRef[M]`, in `avin.actors`. Code an actor runs starts a child; code elsewhere a top-level actor.
- Settled by the user 2026-11-04: **`tell` and the mailbox**: `tell(message)` never waits and never fails (no `!`, which is
  reserved); the mailbox is unbounded; one sender's messages to one actor arrive in order; a message
  to an ended actor is dropped.
- Settled by the user 2026-11-04: **`ask`** is `ask(make: ActorRef[R] -> M, timeout: Duration) -> Future[Result[R, AskError]]`, the
  timeout required, `AskError` being `TimedOut` or `Ended`; reply addresses are ordinary
  `ActorRef[R]`s; `adapter(f)` makes a reference translating another protocol into the actor's own;
  `pipeToSelf(future, f)` delivers a future's value as a message.
- Settled by the user 2026-11-04: **Supervision**: a trap in an actor fails only that actor, its frames unwound and the trap reported
  with the actor's name, except while it holds a `Mutex`, runs a `Once`, initializes a `lazy val` or
  is inside a Sysl call, where the program stops. `Supervision` is `Escalate` (the default: the
  parent fails, and a top-level actor stops the program, so an unsupervised program behaves as one
  with threads), `Stop`, or `Restart(limit, within)`, re-running the body from the same behaviour
  with the mailbox kept. There is no resuming with the old state.
- Settled by the user 2026-11-04: **Lifecycle**: `stop()` ends the actor after the current message, dropping its mailbox; `stop(child)`
  only for its own children; children end first; a `stopping` block runs at every end and restart;
  `watch(ref, message)` and `watch(ref, f: Ending -> M)` deliver the end into the watcher's own
  mailbox, `Ending` being `Stopped` or `Failed(reason)`; `ref.ended()` is a `Future[Ending]` for code
  outside actors; an actor is not collected while it runs; the program ends when `main` returns.
- Settled by the user 2026-11-04: **Timers**: `after(delay, message)` and `every(interval, message)` answer a cancellable `Timer`,
  queued like any message and cancelled when the actor ends or restarts.
- Settled by the user 2026-11-04: **Scheduling**: actors are tasks on an `Executor` (the shared pool by default); an actor runs on
  one thread at a time, each message's handling happening before the next's; a worker handles at most
  16 messages of one actor before re-queuing it; the worker repoints its context's owner word to the
  actor so the actor's handles never trap; a waiting actor is replaced in the pool as a waiting
  future is; `tell` happens before the handling of its message.
- Settled by the user 2026-11-04: **Machines**: `machine Name receives E` with `state S(params)` blocks; an arm's value is the next
  state, or `stay`; the first state, without parameters, is the start; `enter` and `leave` run on
  every transition (a state yielded again included) and not on `stay`; `receive` holds arms shared by
  every state, tried after the state's own; the compiler checks every state handles every event;
  `handle(event)` answers the new state and `state()` the current one, of the generated `Name.State`
  data type; a machine is not sendable, and handling an event from inside its own handler traps.
- Settled by the user 2026-11-04: **An actor's behaviour is a machine**: an actor's body takes the same `state` blocks, its messages
  being the events, with the same checks; a restart starts it again in its first state.
- Settled by the user 2026-11-04: **Keywords are contextual**: `actor` and `machine` only where a declaration starts, `receives` only
  in their headers, and `state`, `receive`, `enter`, `leave`, `stopping` and `stay` only inside their
  bodies at the start of a line (`stay` as an arm's value), so `state` stays an ordinary name
  elsewhere.

- Settled on the user's delegation 2026-11-05: **Handing a handle over explicitly** ([Sysl bindings](/spec/sysl-bindings/#handles)). The
  spec has no spelling for it. Proposed: `handOff(value: Any)` in `avin.threads`, putting every
  handle `value` reaches that the running code owns in transit, for the next code to use it; and
  `Channel.send` doing the same with what it sends, which it does not do now because handing a value
  over walks it, and a channel of plain values should not pay for that (the compiler would skip
  the walk for a type that cannot reach a handle).
  **Review with the user:** `Channel.send` walking each message to move handle ownership costs every
  channel; the compiler skipping the walk for types that cannot reach a handle is the mitigation.
- Settled on the user's delegation 2026-11-05: [Executors](/spec/threads/#executors) (2026-11-04): `Executor.fixed(n)` keeps at most `n` tasks
  working at once and starts its workers as work comes, as the shared pool does, a worker waiting in
  `await` or `blocking` leaving its place to another; `n` below one traps. After `shutdown()`,
  `ex.future(body)` traps, while the `map`, `flatMap` and `onComplete` of futures already made on
  the executor still run there, on a worker started for them where none is left, so no future made
  before is stranded. `Executor.shared().shutdown()` traps, since every `Future(body)` runs there.
  `ex.isShutdown()` answers whether it was shut down. `Future.successful`, `Future.all`,
  `Future.first`, `Future.traverse` and a promise's future belong to the shared executor.
- Settled on the user's delegation 2026-11-05: [`sleep`](/spec/threads/#futures) (2026-11-04): a worker that sleeps keeps its place in its pool;
  a body that should leave it while it sleeps writes `blocking(sleep(d))`. Sleeping is a runtime
  function bound `blocking` over Sysl's `sleep`, which waits out the whole duration.
- Settled on the user's delegation 2026-11-05: [`orElse` on a future of a `Result`](/spec/threads/#futures) (2026-11-04):
  `orElse[U >: T, F](other: -> Future[Result[U, F]])`, widening the value as `Result.orElse` does.
  `Future` stays invariant, since its cell is a mutable field, so a fallback written in place as
  `Future.successful(Ok(0))` needs its type stated where nothing else decides its error type.
  `Future` stays invariant: exempting object-private fields from the variance check (as the user had
  accepted) proved unsound here, because each set of type arguments has its own code and a method
  run through a wider view could store a value of the wider type in a field the narrower view reads
  (probe on branch private-variance, not landed). Sound alternatives, not taken now: dispatching a
  class with object-private members by the type arguments in its header, or reading type
  arguments from the object at run time. **Review with the user:** this reverses an accepted
  recommendation.
- Settled on the user's delegation 2026-11-05: Weighted choice in [`avin.random`](/spec/system-library/#avin-random), approved in kind by the user
  2026-11-04, the names and details proposed:
  - `choose[T](items: Seq[T]) -> T` is uniform (one `nextInt(items.length())`) and traps on an empty
    sequence. `chooseWeighted(weights: Seq[real]) -> int` answers an index and
    `chooseWeighted[T](pairs: Seq[(T, real)]) -> T` an item, the two overloaded by parameter type.
    Both are on `Random` and on the companion's shared generator.
  - A draw is `nextReal()` times the total of the weights, redrawn while it rounds up to the total,
    and the answer is the first index whose running total is above it, so a zero weight is never
    chosen and a seed replays. The traps: no weights, a weight that is negative or NaN, an infinite
    weight, a total that overflows, a total of zero.
  - `Weighted[T](pairs)` holds the running totals and the items, built once; `sample(rng: Random) -> T`
    draws by binary search, O(log n), and `size()` is the number of items. It draws exactly as
    `chooseWeighted` would from the same generator state. The alias method (O(1) a draw, a
    different mapping of draws to items) was not taken: the cumulative table keeps one draw per
    sample and a mapping a reader can follow.

- Settled on the user's delegation 2026-11-05: `Url`'s details ([system library](/spec/system-library/#url)), where the design left them open:
  - `Url` is a `data` type whose `host` is an `Option[string]`, `None` where there is no authority
    (`mailto:a@b`) and `Some("")` where it is empty (`file:///etc`); an IPv6 host keeps its brackets;
    `userInfo`, `query` and `fragment` are `Option`s like `port`.
  - Components are kept as written, escapes and all; `Url.decode` is the way to their text.
  - `parse` holds the scheme and host in lower case and a default port as `None`, so `==` compares
    the normalized URL and display needs no work of its own. Defaults are 80 for `http` and `ws`,
    443 for `https` and `wss`, and `portOrDefault()` answers an `Option[int]`, `None` for a scheme
    with no default.
  - Escapes that do not decode to UTF-8 make text no URL (`InvalidUtf8`), at `parse` and in each
    `with` method, so `queryParams` keeps the plain `Vector` the design gives it; `Url.decode`
    answers a `Result`.
  - `queryParams` reads `+` as a space and skips empty pieces; `decode` keeps `+`.
  - `UrlError` is `MissingScheme`, `BadScheme`, `BadCharacter`, `BadPercent`, `BadHost`, `BadPort`
    and `InvalidUtf8`, each but the first with the byte offset `at`.
  - A zone is accepted only as RFC 6874 writes it, after `%25`; hosts of a later form (`[v7.x]`)
    are accepted; characters beyond ASCII other than controls and spaces are accepted in any
    component and kept.
  - `resolve(ref: string)` answers a `Result`, the reference being text; `resolve(ref: Url)` takes
    one already read. The `with` methods take components as written and trap on one the URL cannot
    hold; each optional component's method takes a value or an `Option`.
- Settled on the user's delegation 2026-11-05: **`Source` is a trait**, its three readers abstract, so a program can write a source of its own
  (text made as it is read, a socket later) and hand it to any function that takes one; the kinds
  `avin.io` makes are private classes reached only through `Source.fromFile`, `fromString` and
  `stdin`. The alternative is a final class whose kinds are fixed by the library.
- Settled on the user's delegation 2026-11-05: **An override adds no bound to a type parameter of
  its own** ([declarations](/spec/declarations/#inheritance)). Each bound an override, or a method
  implementing a trait's, writes on its own type parameters is implied by the overridden method's
  bounds on that parameter: one of them, a trait or class one of them conforms to, or `Eq` or
  `Hash`, which every type meets; it may keep, weaken or drop bounds. Why: a call through the base
  type passes any type argument the base's bounds admit, so an added bound let a program that
  type-checked run the override at a type without the bound's methods, which trapped or crashed. The
  alternatives were requiring the bounds to be exactly the same, which forbids a weakening that is
  harmless; and checking the bound at each call through the base,
  which a call cannot do, since it does not know which override it reaches.
- Settled on the user's delegation 2026-11-05: **the methods an `impl` adds to an `open` or
  `abstract` class are [variance](/spec/generics/#variance) positions**, whatever its fields; the
  exemption stays for final classes whose fields are all immutable, `data` types and traits. Why:
  such a method is in the class's method table, so a call through a wider view runs the copy for
  the object's own type arguments, and `(b: Box[Animal]).with(cat)` on a `Box[Dog]` answered a
  `Box[Dog]` holding a cat, whose dog's fields read past the object; the core library's `Vector` had
  the hole. So `Vector` takes an element through a lower bound, as maps do:
  `:+[W >: T](element: W) -> Vector[W]`, `+:`, `++`, `updated`, and `toArray[W >: T]()`, and
  `Seq`'s added `:+`, `+:`, `++` and `updated` widen alike, so a vector's own hides them. A vector's
  `iterator()` and `reverseIterator()` are `Iterator[T]`s, and its guard view is the one every
  sequence has. The alternative, not taken: making an exempt impl method non-virtual, compiled for
  the receiver's static type as a trait's added methods are, which keeps `:+(element: T)` but would
  make an added method on an open class behave unlike a method written in its body.
- Settled on the user's delegation 2026-11-05 (built on branch `avin-net`): `avin.net`'s TCP and
  HTTP details ([system library](/spec/system-library/#avin-net)),
  where the chapter left them open:
  - `write`, `writeText`, `setTimeout` and `shutdownOutput` answer `Result[unit, IoError]`, since
    each can fail; `readAll()` is added beside `read` and `readLine`, and `TcpListener` has `close()`.
  - A read, write or `accept` after `close()` answers `Other("the connection is closed")` (or "the
    listener is closed") rather than trapping, as a closed handle's method does.
  - A host that resolves to no address is `Other(s"no address found for $host")`, not `NotFound`,
    whose message speaks of files. A port outside 0 to 65535, and `read(max)` with `max` below one,
    trap at the call, as a `Url`'s `withPort` does with a port it cannot hold.
  - `ConnectionReset` also covers a write to a connection the other end has closed (`EPIPE`).
  - `setTimeout` sets the read and the write limit together, in whole milliseconds rounded up; zero
    or less takes the limit away.
  - `bind` takes the host's first address and lets a port a recently stopped listener used be taken
    again; the backlog is the platform's usual 128.
  - The socket is a handle on `sysl.net`'s own `Socket`, which closes its descriptor when its last
    copy goes (from Sysl 0.9.1; a runtime type wrapping it with a destructor did this before, and
    was removed on branch `sysl-091`); so closing the handle, or the collector freeing a forgotten
    one, closes the socket. The runtime turns `sysl.net`'s errors into the four a connection tells
    apart (`avin.rt.net_failure`), the error crossing as a handle so that its variants do not join
    the module's.
  - The HTTP client: a `Response` also has its `reason`; `text()` reads ill-formed UTF-8 as U+FFFD;
    `Request` is a class with `method`, `url`, `headers`, `body` and `timeout`, the last three
    defaulted; a method or header name that is not a token, and a header value with a line break,
    trap; `Host`, `Content-Length` and `Connection: close` are written unless the request gives
    them; an interim `1xx` is skipped; redirects are 301, 302, 303, 307 and 308, the sixth answered
    as it is.
- Settled on the user's delegation 2026-11-05 (built on branch `sendable`): [`Sendable`'s
  promises](/spec/actors/#what-may-be-sent), where the design left them open:
  - A declaration promises `Sendable` as it conforms to any trait: after `:` in a class's or a
    trait's header, or in an impl for a data type. The promise is checked against the structure,
    outside the standard library, and an `open` or `abstract` class that promises it is sendable,
    each subclass being checked. The standard library's promises are taken as made, since they are
    for types whose structure cannot show it: `Future`, `Promise`, `Channel` and `Atomic` of a
    sendable type (impls bounded by `Sendable`), `Mutex` of any type and `Once` (in their headers),
    and `Vector` of sendable elements. `ArraySeq`, `List`, `Range`, the hash and tree maps and sets,
    `Option`, `Result` and `Either` are sendable by their structure.
  - A generic trait that promises `Sendable` is sendable at sendable type arguments; a type
    implementing it has the parameters it passes to the trait, other than contravariantly, taken as
    sendable where it is checked, and is sendable at sendable arguments for them. The immutable `Map`
    and `Set` traits promise it, so `HashMap[K, V]` is sendable where `K` and `V` are, and a type
    implementing either is held to it.
  - `Sendable` is only a bound and a promise; it is not the type of a value.
  - A `lazy val` field is sendable where its type is, since it is initialized by exactly one thread.
    (That rests on the runtime initializing a `lazy val` once under contention, which the tests
    `lazy_threads`, `lazy_threads_many` and `sendable_lazy` pin: threads and futures reading one
    first together, counting a single run of each initializer.)
- Settled on the user's delegation 2026-11-05 (built on branch `mirror-handle-fields`): **a mirror
  holding a handle goes into Sysl wherever any mirror does** ([Sysl
  bindings](/spec/sysl-bindings/#mirrors)) — a parameter by itself, or inside an `Option`, a
  `Result`, a collection, a tuple or another mirror — since the shim's conversion of the mirror
  checks and passes each handle field wherever it is called, though a bare handle in an `Option` or
  a collection still does not go into Sysl. The alternative is a mirror holding a handle only by
  itself, as a handle is.
- Settled on the user's delegation 2026-11-05 (built on branch `tailrec`): [tail
  calls](/spec/declarations/#tail-calls), the choices the request left open:
  - **Every self-call in tail position is a loop, with or without `@tailrec`**, at `-O0` as at
    `-O2`, so a program's stack use does not depend on its optimization level; the annotation only
    adds the proof. Alternative: loop only where `@tailrec` asks, leaving the rest to LLVM at `-O2`
    — rejected, since the same program would overflow at `-O0` and not at `-O2`.
  - **The right operand of `&&` and `||` is in tail position**, as in the reference language:
    `n == 0 || (ok(n) && f(n - 1))` is a loop. Alternative: only `if`/`match` branches — simpler to
    explain, but it refuses a common shape of predicate.
  - **A call on another receiver of the method's own type is a loop**, the receiver becoming
    `self` for the next pass, as the reference language does for a method that cannot be
    overridden; a call on a receiver of the class at other type arguments is another instance and
    `@tailrec` refuses it. Alternative: only calls on `self` — it would refuse list-walking methods
    such as `next.length(acc + 1)`.
  - **A private method of an open or abstract class may be `@tailrec`**, since nothing overrides
    it; a public one may not. There is no `final` modifier for a single method, so making a public
    method of an open class a loop means making it private or the class final.
  - **`@tailrec` is accepted on top-level functions, methods and local functions**, and is the one
    annotation a local function takes; a lambda cannot be annotated. It refuses a function with no
    recursive call, as the reference language does, rather than accepting it silently.
  - **A function calling itself is made a loop; mutual recursion is left to general tail calls**,
    which need every caller and callee to agree on a calling convention and to unlink the
    shadow-stack frame before the call, and have their own entry (below).
  - The language has no `try`, `defer` or `finally`, so there is no pending cleanup to take a call
    out of tail position; when such a construct is added, a call inside it is not in tail position.
- Settled on the user's delegation 2026-11-05: **`Iterable`'s transformations**
  ([collections](/spec/collections/#operations-without-higher-kinded-types)), following the
  reference collection library (branch `coll-followups`). `map`, `flatMap`, `filter`, `withFilter`,
  `collect` and `zip` are added to every `Iterable` by `impl[T] Iterable[T]`, each returning an
  `Iterable` (a `Vector` underneath) of the results in iteration order, so a program's own class
  conforming to `Iterable` has them; the library's collections keep their own of the same name and
  parameter types, of their own kind. To let `Seq`'s, `Set`'s and the other traits' versions stand,
  an impl adding to a trait is passed over where an impl adding the same name and parameter types to
  a trait that requires it applies ([declarations](/spec/declarations/#impl)); impls on unrelated
  traits still clash. Why: the item asked for the members the chapter already claimed, and the
  impl is how every other `Iterable` operation is written. Alternatives: methods with bodies
  declared in the trait, dispatched through the object's table, so generic code would get the
  concrete collection back at run time; that would make every collection's `map` an `override`,
  rule out the bounded versions (`TreeSet.map[B: Ord]` cannot override an unbounded `map[B]`),
  and dispatch every call. The chapter's earlier claim that generic code gets the concrete
  collection back is withdrawn. **Review with the user**: a `Map` receiver still offers only its
  pair-making `map`, so `m.map(e -> e.0)` is rejected rather than answering an `Iterable`; making
  it fall back needs overload resolution that checks a lambda's body against each candidate.
- Settled on the user's delegation 2026-11-05: **`rest*` binds what `drop(n)` answers**
  ([collections](/spec/collections/#sequence-patterns)), branch `coll-followups`. One rule for every `n`,
  `n = 0` included: `[rest*]` binds `drop(0)`, the list itself on a list and a `Vector` on a range,
  an array, a buffer or a deque, where it used to bind the scrutinee itself at its own type. Why:
  one rule the reader can predict, and a binding that never aliases a mutable array or buffer
  through a pattern that elsewhere copies. Alternative: `[rest*]` as the scrutinee, as before, which
  made the binding's type depend on whether any element came before `rest*`.
- Settled on the user's delegation 2026-11-05: **An empty collection literal joins to its siblings'
  type** ([collections](/spec/collections/#literals)), branch `coll-followups`. Among the elements of
  a literal, the keys or values of a map literal, the branches of an `if` and the arms of a `match`,
  an empty literal (`[]`, `{}`, or a literal holding one anywhere) is checked after the others and
  against the type they agree on, where nothing useful is expected (nothing, or `Any`), and where
  that type is a collection its literal shape can build. So `[{1: [2]}, {}]` and
  `if c then {1: [2]} else {}` are maps. Why: `{}` alone defaults to a set, and an invariant `Set`
  never joins a `Map`, so the sibling has to settle it before it defaults; the reference language
  gets the same result from inference. Alternatives: a join rule turning `Set[_]` into the other
  side's map after the fact (the literal's builder is already chosen by then), or asking for an
  annotation, as before.
- Settled on the user's delegation 2026-11-05: **A range displays as written**
  ([collections](/spec/collections/#ranges)), branch `coll-followups`: `1..5`, `0..<10`,
  `0..<10 by 2`, the bounds as given, the step only where it is not 1; `Debug` the same, and a
  `NumericRange` the same. Why: it reads back as the expression that made it and costs nothing to
  render. Alternative: the elements, `[1, 2, 3, 4, 5]`, as a vector shows, which hides that a range
  is a progression and is unbounded in size.
- Settled on the user's delegation 2026-11-05: **Precision under `%s` and `%?` is the formatter's**
  ([strings and formatting](/spec/strings-and-formatting/#rendering)), branch `coll-followups`: the
  value is rendered with no width and no precision through `writeDisplayed` or `writeDebugged`, and
  its first N code points fill the field, for every type; renderers see a precision only under the
  other conversions. Why: the chapter promised it for every value and only strings honoured it; cut
  once, no renderer can get it wrong. Alternative: every renderer reading the precision, which each
  collection and data type would have to implement.
- Settled on the user's delegation 2026-11-05 (built on branch `regex-hardening`): [`avin.regex`'s
  limits on matching](/spec/regex/#matching), backtracking with leftmost-first results kept:
  - **The matcher keeps its own backtrack stack**, an array of frames, so a repeated group over a
    long input takes no deeper a call stack however many repetitions it makes; only a part matched
    on its own (a repetition's item matched one way, an atomic group, a lookaround's body) takes a
    call, so the call depth follows the pattern's nesting, never the input. Alternative: a
    linear-time engine — rejected by the request, as back references, lookarounds and
    leftmost-first captures need backtracking.
  - **Each attempt at a start has a step budget, and spending it traps**, the message naming the
    pattern and the input's length and saying to make a repetition possessive or atomic. A step is
    an operation run or a backtrack frame popped; the budget is 1000 for each character of the
    input (plus one) times each operation of the program, never fewer than 10,000,000, about a
    tenth of a second of steps and far beyond what a pattern that backtracks only as far as its
    input asks takes. The budget is per attempt, not per search, so a search is never stopped for
    the number of starts it tries. Alternatives: answering a `Result` from every search — rejected,
    as it would make every `findFirstIn` and `matches` a fallible call for what is a defect of the
    pattern, which the trap's message says how to fix; no limit (the earlier behaviour) — rejected,
    as one bad pattern hangs its program.
  - **`withStepLimit(n)` sets an attempt's budget** (`n` above 0, or it traps), for a program that
    wants a tighter bound on untrusted text or a looser one for a pattern known to be slow; the
    trap's message then gives the limit. Alternative: a global setting — rejected, as the budget
    belongs to the pattern.
  - Review with the user: whether a search should also have a non-trapping form for untrusted
    patterns (a `Result`-answering `tryFindFirstIn`, say), and whether the default budget's
    constants are the right ones. A greedy, unbounded repetition of a group in a pattern without a
    back reference already remembers where a further repetition failed, so the textbook
    `(a+)+$`, `(a|a)*b` and `(a|aa)+$` finish in polynomial time and do not trap; their reluctant,
    bounded and back-referencing forms are the ones that do.
- Settled on the user's delegation 2026-11-05: **[Guaranteed tail calls](/spec/declarations/#tail-calls)**
  (branch `tailcalls`). Every call in tail position of a function of the program, by name, through
  a method table or of a function value, replaces the caller's stack frame, at `-O0` as at `-O2`,
  so mutual recursion, state machines of functions and continuation passing run in constant stack.
  A self-call is still a jump within the function, which is cheaper. `@tailrec` stays about
  self-recursion.
  - Every compiled function has LLVM's `tailcc` convention, and a tail call is `musttail call
    tailcc` followed at once by its `ret`. The C convention is kept only where code outside the
    program calls in: `main`, a C-convention wrapper for each function whose address a Sysl
    binding's shim is given, and the closure runner the runtime calls to start a thread.
  - Calls through method tables and of function values are tail calls too, since every address a
    table, a type record or a closure holds is a function of the program with its convention. A call
    the whole-program pass guards by the receiver's descriptor returns from each of its branches.
  - The caller gives its shadow-stack frame back just before the call, as a return does. The
    arguments are then roots of no frame until the callee links its own, its first act, and nothing
    between the two allocates, polls or stores, so no collection can see the gap.
  - A call stays an ordinary one where an argument may point into the caller's own stack (a local
    function given the address of a local it captures), where the core library calls the program's
    code (it restores the caller site after), where the result is converted, and in a class's
    constructor, which publishes its object after its body. A lambda or local function value that
    captures a local of its enclosing function is entered through an entry that keeps the local on
    its own stack, so that entry's call adds one frame per call; capturing only parameters and values
    adds none.
  - **Only a function whose arguments all travel in registers has `tailcc`**: the context and at
    most five integers, bools or references, at most eight reals, and no integer wider than `long`
    as a parameter or the result. The rest keep the C convention and make and receive ordinary
    calls. Under `tailcc` the callee pops the arguments passed on the stack, and LLVM's unoptimized
    code generation reloads spilled values from the wrong place after such a call (seen as crashes
    and wrong results at `-O0`); with every argument in a register there is nothing to pop. The
    limits are those of the smallest target (x86-64: six integer registers, eight vector ones), so a
    program's stack use is the same everywhere. **Review with the user**: a function with six or more
    such parameters gets no tail call; lifting the limit means passing the extra arguments in a
    block the caller's caller owns, or a fixed LLVM.
  - Alternatives weighed: merging each group of mutually recursive functions into one function that
    switches on which one is running (no convention change, but only for calls by name, and code
    size grows with the group); a trampoline in the library (no compiler work, but every tail call
    pays an allocation and an indirect return, and the program must be written for it).
  - **Review with the user.** Error backtraces lose the frames a tail call replaced; the user may
    prefer this only at -O2.

- Settled on the user's delegation 2026-11-05 (built on branch `process-commands`): **How the parts of a command share its settings** ([commands](/spec/system-library/#commands)).
  Proposed: a directory and variables set on a pipeline or a sequence are every part's, a part's own
  directory replacing it and its own variables added after (a name set twice has the last value).
  A time limit on a command made of parts bounds the whole, from when it starts: each program is
  given what is left, a part not yet started when it runs out is `TimedOut` without being started,
  and of two nested limits the earlier ends it. Output redirected on a pipeline is its last stage's
  and input its first's; on a sequence the file is opened once and shared, so the parts write one
  after the other and the second reads on where the first stopped. A part's own redirection replaces
  the enclosing one, whose file is still opened (and so emptied by `toFile`), as a shell's is.
  Why: each reads as a shell's `( … )` with the setting outside it, which is what a reader expects.
  Alternatives: a limit given to each program afresh (a sequence of slow parts would never end), and
  a file opened per part (each part of a sequence would empty what the one before wrote).
- Settled on the user's delegation 2026-11-05 (built on branch `process-commands`): **Input given as text, and output captured, go through files** ([commands](/spec/system-library/#commands)).
  Proposed: `withInput(text)` writes the text to a temporary file, opened and unlinked before the
  program starts, and `output()` has the output written to a temporary file read once the command
  has ended. Why: a pipe the program feeds or drains while it waits is a deadlock once either side
  outgrows the pipe's buffer, and a file never fills. The alternative is pipes served by a thread.
  `output()` decodes the bytes as `lines()` does, each ill-formed run replaced with U+FFFD, rather than
  failing with `Io(InvalidUtf8)`; the alternative is the failure, which would lose the whole output
  over one byte. **Review with the user**: the decoding.
- Settled on the user's delegation 2026-11-06 (branch `command-clear-env`): **A command's environment: `clearEnv`, `withoutEnv`, `withEnvironment`** ([commands](/spec/system-library/#commands)).
  A command keeps its environment changes as an ordered list (set, remove, clear), applied in the order called; on a joined command the command's list comes before each part's own. `clearEnv()` starts from an empty environment, `withoutEnv(name)` drops one inherited variable (an unset name is no error), `withEnvironment(map)` is `clearEnv()` then each entry. With only sets, the running program's environment is left to Sysl's `inherit_env`; once a list clears or removes, the whole environment is built from `environment()` and handed over with `inherit_env = false`. A bare program name after a clear is looked for on the `PATH` the command was given, or the system's default path where none, never the running program's: that is Sysl's `spawn` rule, kept so a command never finds a program through a variable it was told not to have. Variables whose name or value is not UTF-8 are not passed once a command removes or clears (`environment()` passes them over). WHY: a daemon must start jobs from the submitter's environment, not its own. ALTERNATIVES: a single `withEnvironment` only (cannot remove one variable cheaply); a settings pair of a base flag and a removed set (loses the order of calls, so `clearEnv().withEnv(x)` and `withEnv(x).clearEnv()` could not differ); looking the program up on the parent's `PATH` (a cleared command would still find programs by a variable it lacks). Review with the user: the `PATH` lookup rule.
- Settled on the user's delegation 2026-11-05 (built on branch `process-commands`), with one change: **`kill()` and a started sequence** ([commands](/spec/system-library/#commands)).
  Proposed: `Child.kill()` sends signal 15 to every program of the command still running and starts
  no part not yet started; `waitFor()` then answers `Signalled(15)` for a program it stopped. A
  started sequence starts its next part when `waitFor()` or `isAlive()` finds the part before it has
  ended, on the code that started it, which owns the processes; `lines()` over a command holding a
  sequence moves it on with a thread of its own, since its reader is waiting on the output. A
  pipeline holding a sequence is polled, its processes asked without waiting at growing intervals up
  to 16 ms, while a pipeline of programs alone is waited for without polling. Alternatives: signal 9,
  which a program cannot clean up after, or `kill(signal: int = 15)`; every started command on a
  thread of its own, whose processes the starting code could then not stop, since a handle belongs
  to one thread. Settled: signal 15 as proposed. Changed: a started sequence must move on by itself, since a sequence that
  stalls until its starter asks is a surprise; a thread of its own will own the processes and drive
  the sequence, and the `Child` will ask it to wait or to kill through a channel, which keeps each
  handle on one thread. Until that is built the sequence moves on only when asked, as described.
  **Review with the user**: the signal.
- Settled on the user's delegation 2026-11-05 (built on branch `core-followups`): **`==` and `!=` on
  `Any` are decided at run time**, as [equality](/spec/equality-and-memory/#equality) says: values of
  one type compare as that type does, by value or by identity, and values of different types, a
  reference and a value among them, are unequal. The sentence in
  [Any and Nothing](/spec/types/#any-and-nothing) saying an `Any` had no equality was stale and is
  corrected. Rationale: every value has `==` and a hash, so any value can be a map key, a `Map[Any,
  V]`'s among them, and the compiler already compared `Any`s through each value's own entries.
  Alternative weighed: no `==` on `Any`, so that every comparison goes through a type pattern first,
  which breaks "every value can be a key".
- Settled on the user's delegation 2026-11-05 (built on branch `core-followups`): **`===` is defined
  only where its answer is**: at least one side's static type is a reference type that cannot hold a
  value type (a class, an `object`, an array, a buffer, a function type, or a type parameter bounded
  by a class). The other side may be the same type, a sub- or superclass, a trait the class conforms
  to, an `Any`, or a type parameter with no class bound; an `Any` or such a parameter is compared as
  an `Any`, and a boxed value there is never the object, so the answer is `false`. Two sides that
  may each hold a value type (traits, `Any`s, type parameters with no class bound) are an error,
  "`===` is undefined on Shape and Shape, which may hold values with no identity; compare with `==`,
  or give one side a class type"; so is a value type on either side. See
  [identity](/spec/equality-and-memory/#identity). Rationale: a boxed or data value may be one
  object or two at the compiler's choice, so trait-to-trait identity answered arbitrarily
  (`f(Box(2), Box(2))` true and `f(3, 3)` false for `f(a: Shape, b: Shape) = a === b`), while a
  class instance is never a boxed value, so mixed class and trait is well defined. Alternatives
  weighed: rejecting any trait or `Any` operand, which needlessly breaks the library's
  `elements === self` (a buffer, deque or set given itself as an `Iterable`); and keeping
  trait-to-trait identity, which is unsound. A class with a trait it does not conform to is rejected
  even when the class is open and a subclass might conform. **Review with the user**: whether an
  open class and a trait it does not itself conform to should compare, since a subclass may be both.
- Settled on the user's delegation 2026-11-05 (built on branch `core-followups`): **a tuple already
  made converts to a wider tuple only where each part already is what the wider tuple holds**: a part
  that is an object (a string, a `data` value, a tuple, an instance of a class) is its own `Any`, and
  an instance of a subclass is one of its superclass, so `(string, int)` is a `(Any, int)`; a part
  that would need a box (a number, a `bool`, a `char`) does not convert, so `(int, int)` is not a
  `(Any, int)`. A tuple written out where the wider one is expected boxes its part there. See
  [Any and Nothing](/spec/types/#any-and-nothing). Rationale: a conversion that changes no bits is
  free, while a boxed part changes the tuple's layout and would mean copying the tuple behind a plain
  assignment. Alternatives weighed: no conversion of a tuple once made (what the chapter said, and
  needlessly strict for parts that already are objects); converting every part by copying, which
  hides an allocation.
- Settled on the user's delegation 2026-11-05 (built on branch `core-followups`): **two classes with
  no common superclass that share several unrelated most specific traits do not join, and the error
  names the traits**: "the branches of this `if` yield Duck and Goose, which share Swims and Flies but
  no single most specific type; write the type you want", and the same ending for `match` arms,
  clauses, collection and map literals and a loop's `break`s, as
  [inference](/spec/generics/#inference) says. Rationale: the bare "yield Duck and Goose" did not
  say why two classes that plainly share traits fail to agree. Alternative weighed: choosing the
  first trait declared, which makes the type depend on declaration order.
- Settled on the user's delegation 2026-11-05 (built on branch `core-followups`): **a tuple type
  pattern tests the type the tuple was made with**, as [Any and Nothing](/spec/types/#any-and-nothing)
  says, not the classes of its parts: `t: (Animal, int)` holding a `Dog` does not match
  `_: (Dog, int)`, and a tuple made as a `(Dog, int)` does wherever it is held. Each part is tested
  by a tuple pattern with a type pattern inside it, `(d: Dog, n)`, and
  [type patterns](/spec/operators-and-control-flow/) no longer say a tuple is "taken down part by
  part". Rationale: a tuple's run-time type is one type, fixed when it is made, so the test agrees
  with the static type it was made at. Alternative weighed: testing part by part, which would make a
  tuple's run-time type depend on what it holds rather than on how it was made.
- Settled on the user's delegation 2026-11-05 (built on branch `option-ord`): [`Option[T]` is an
  `Ord` when `T` is](/spec/errors/#option-and-result), `None` before every `Some` and `Some`s as their
  values, following the reference library's ordering of options. It makes `derives Ord` work on a
  data type with an `Option` field and lets options sort and have `max`/`min`. Alternative: no order
  on options, leaving a field of one underivable — rejected as the cost falls on every record with
  an optional field. (Tuples already had `Ord`; `Either` and `Result` follow in the next entry.)
- Settled on the user's delegation 2026-11-05 (built on branch `sysl-handles`): **a handle may name a
  Sysl trait, and its trait object is kept in a cell** ([Sysl bindings](/spec/sysl-bindings/#handles-on-traits)).
  Every handle's object stays two words, the first naming the Sysl value and the second its owner;
  where the reference is a trait object, two words itself, the first word points at a cell the shim
  allocates holding it, freed when the reference is given back. The shim tells the cases apart while
  Sysl compiles it, by whether a reference to the type is wider than a pointer, and treats a trait
  as always held by a counted reference. Rationale: a struct handle is unchanged and reached as
  directly as before, the compiled code's owner and closed checks read the same two words for every
  handle, and the compiler need not know which Sysl types are traits, which only Sysl does.
  Alternatives weighed: a handle object of three words for a trait, which needs the compiler to know
  the Sysl type's kind before Sysl checks the shim, and moves the owner word of one kind of handle;
  every handle boxed, which costs a struct handle an allocation and a load for nothing.
- Settled on the user's delegation 2026-11-05 (built on branch `sysl-handles`): **an `Array` or a
  `Vector` of handles that is a parameter goes into Sysl** ([Sysl bindings](/spec/sysl-bindings/#collections-of-handles)),
  replacing "a handle inside a collection does not go into Sysl": as a slice of shares of the
  handles' Sysl values, each element checked (closed, another owner) as a handle passed by itself is,
  then made the elements the Sysl parameter takes; a vector as a `[]T`, an array as a `[]const T`,
  as for every collection, and a `Buf` parameter not reached. Only the collection itself is the
  parameter: not nested, not in an `Option`, not a `List`, not a callback's answer, not a mirror's
  field. Rationale: a user interface library's containers take `[]&View`; taking shares first means
  a call that waits reads no Avin object, and what Sysl keeps it keeps by its own count. A slice or
  `Buf` of handles out of Sysl already became a vector of handles, so nothing changed there.
  Alternatives weighed: rooting the Avin collection for the call instead of taking shares, which
  leaves a pointer element reading a value the collector may free once the call returns and Sysl
  keeps it; allowing `List` and nested collections, which nothing yet needs.
  **Review with the user:** whether an `Array` of handles should reach a writable `[]&T`, since a
  handle collection's elements written by Sysl could never reach the array anyway; today it is
  refused as for every other array, so such a parameter is bound with a `Vector`.
- Settled on the user's delegation 2026-11-05 (built on branch `sysl-handles`): **a handle may name an
  instance of a generic Sysl type**, `@sysl("module.Signal[string]")`, struct or trait
  ([Sysl bindings](/spec/sysl-bindings/#handles-on-generic-instances)), its methods bound at that
  instance. Each type argument is written as Avin writes it and is a boundary type that is one Sysl
  type: an integer of 8, 16, 32 or 64 bits, `byte` (spelled `u8` for Sysl), `real`, `char`, `bool`,
  `unit`, `string`, or an `Option`, a `Result` or a tuple of these; the compiler checks the
  arguments' shape, Sysl that the type takes them. The shim imports the generic type and spells the
  instance wherever it names the handle's type, since Sysl gives no alias to a trait's instance.
  Rationale: `syslui` hands out `&Signal[T]`, and one class per instance needs no generic classes in
  bindings. Alternatives weighed: mirrors and handles as type arguments, which have Sysl spellings of
  their own but would make an annotation's text name Avin types; fully generic bindings, one class
  for every `T`, which stay specified and unbuilt. A `&Signal[T]` parameter shares the handle's
  own signal (from Sysl 0.9.1), so a Sysl function setting a signal is seen by the handle
  (`tests/programs/run/sysl_generic_handle_sharing`, `tests/programs/run/ui_signal_widgets`).
- Settled on the user's delegation 2026-11-05 (built on branch `tuple-ord`): [`Either[L, R]` and `Result[T, E]` are `Ord`](/spec/errors/#either)
  when both sides are. `Left` orders before every `Right`, following the reference library's
  ordering of eithers; `Result` is ordered as `Either` with `Err` in the place of `Left`, so every
  `Err` before every `Ok`. Rationale: both types put the failure on the same side (`Left` and
  `Err` are the reason, `Right` and `Ok` the value), so the one rule "failure before success" holds
  for both, and `Err` agrees with `Left` under `toRight`/`toLeft` conversions. Alternatives weighed:
  `Ok` before `Err`, matching the declaration order of `Result`'s variants (which is what
  `derives Ord` would give a data type, and what a reader of `data Result` might expect) but
  disagreeing with `Either`; and no order on `Result`, leaving a record with a result field
  underivable. Review with the user: the `Err`-first choice is the one a reader of the declaration
  could find surprising. Tuples needed no change: their `Ord`, lexicographic by part at every
  arity, was already written by the compiler for each arity and is now covered by tests.

- Settled on the user's delegation 2026-11-05 (built on branch `sysl-callbacks`): [passing Avin functions to
  Sysl](/spec/sysl-bindings/#callbacks), the choices the request left open:
  - **The closure's lifetime is Sysl's**: the shim's closure captures a counted Sysl value with a
    destructor that holds a root of the collector, so the root is given back when Sysl drops its
    last reference to the closure. Why: Sysl closures already carry captured state and drop it, so
    nothing has to be released by hand and nothing leaks while Sysl's own ownership is right.
    Alternatives: an explicit `release` the library calls (leaks whenever it forgets, and frees a
    function Sysl may still call when it releases early); a root for the program's whole run
    (leaks every callback ever given).
  - **A callback belongs to the code that gave it by default; `@threaded` on a binding's parameter
    lets any thread call it.** The default compares owners, as a handle's use does, and traps on any
    other caller. Why: a callback's captures are ordinary Avin objects, shared without
    synchronization, so a call from another thread is a data race unless the function is checked;
    most UI and event callbacks run on the thread that registered them. Alternatives: a flag on the
    whole binding, `@sysl("name", threaded)` (coarser: one binding may take two callbacks); reading
    `&sync Fn` from the Sysl signature (Avin does not parse Sysl types; Sysl's check already refuses
    the unmarked case, and the refusal is reworded to name the mark). **Review with the user**: the
    spelling `@threaded`, and whether the default should be the owner (as built) or the OS thread.
  - **What a `@threaded` parameter is given is checked by its captures**, since a function value is
    never `Sendable`: a top-level function, or a lambda, local function or method value whose every
    capture is a `val` or a parameter of a sendable type. A function value held in a variable or a
    parameter is refused (its type does not say what it captures), and a binding with a `@threaded`
    parameter cannot be used as a value, which would hide the call from the check. Alternatives:
    making function types carry a sendability mark (`Sendable` function types; a larger language
    change); no check (unsound).
  - **A thread Sysl started itself is attached for a `@threaded` callback's call** (its own context,
    a fresh owner, the stack guard installed) and detached after; a callback without the mark called
    there traps without a place, since the thread never entered from Avin. Why: an audio or I/O
    thread of a C library is the common case, and attaching is cheap and safe since the thread holds
    no Avin references between calls. Alternative: always trap (makes such libraries unusable).
  - **A callback made while its thread is in a blocking call makes the thread running for the call
    and safe again after**; the place a trap in the library reports (the context's caller site) is
    restored when the call ends.
  - Settled on the user's delegation 2026-11-05 (the director): **a binding with a `@threaded`
    parameter is `blocking` whether or not it says so**, since a Sysl call that may wait for another
    thread to call back would otherwise hold up that thread's first collection for ever. Why: the
    deadlock cannot be detected, and the cost is two stores and two loads per call. Alternative:
    documenting that such a binding is declared `blocking` (built first; a forgotten word hangs the
    program). A same-thread callback's binding still blocks only where it says so.
  - **A trap inside a callback stops the program at the callback's own place.** Avin traps report a
    place rather than a backtrace, so "with the Avin backtrace" is the trap's place; there is no
    unwinding through Sysl frames, which would need Sysl to be unwind-safe.
  - **The program defines one C-convention entry per callback parameter** (a bool as a byte), which
    the shim declares `extern` and calls with the closure; the shim itself is generic in the Sysl
    types of the closure's arguments, so a callback's integer parameter takes any Sysl integer with
    the check a result has. Alternative: the shim calling the closure's code directly through a
    function pointer (ties the shim to Avin's internal convention for `bool`).
  - The runtime offers `avin.rt.callbacks_held`, the number of functions Sysl holds, for tests and
    for libraries that want to check they release what they register.
- Settled on the user's delegation 2026-11-05 (built on branch `sendable-spawn`): **`Sendable` where values cross threads
  today** ([what is shared](/spec/threads/#what-is-shared)). The actors chapter's build order has no
  step for it; it applies the rule of `isolate`'s body, less the whole-program half, to threads and
  futures. Review with the user: it adds two annotations users can write.
  - The check is two marks on a parameter, `@sendable` and `@isolated`, which any function may
    write. What is passed for `@isolated` may use only sendable values from around it: a lambda, a
    by-name argument, a local function or a method value by what it captures (its nested closures
    and the local functions it calls included), any other expression by the locals and parameters it
    reads; what it makes itself is its own. `@sendable` adds that a closure captures no `var`, whose
    box it would share with the code that made it. A marked parameter holding a function may be
    passed on for one with the same mark or a weaker one; an override may not add a mark or
    strengthen one. *Why*: the library's thread entries are ordinary Avin functions, so a user's
    wrapper of `spawn` needs the same spelling the library uses, and a mark on the parameter checks
    every call where the closure is written and its captures are known. *Alternatives*: a list of
    library functions known to the compiler (no user wrapper could pass a body on); a function type
    that says it is sendable (`() -> T` that is `Sendable`), which would also let a sendable function
    be stored and sent, but makes function types carry a flag through unification and subtyping
    (later taken up as `@sendable` function types, under Proposed); no check, leaving the data race the
    memory model already makes harmless (the user's direction is that threads share nothing mutable
    by default).
  - The bodies of `spawn`, `s.spawn`, `Future(body)` and `ex.future(body)`, the functions of a
    future's `map`, `flatMap` and `onComplete`, and `orElse`'s by-name argument are `@sendable`.
    Their results are bounded by `Sendable`, as are `Future.successful`'s value, a `Promise`'s and
    the results of `mapOk` and `mapErr`, so every future's value is sendable however it was made.
    *Alternative*: bounding `Future[T]` itself, which would make every generic function naming a
    future repeat the bound.
  - A `Mutex` is sendable of any value, so its value is `@isolating` where it is made (a buffer the
    caller keeps cannot be guarded), `with` is `with[R: Sendable](@isolating body: (T) -> R) -> R`
    (the value cannot be returned or stored outside), and `get`, `set` and `update` exist only for a
    sendable value. A `with` body may assign the caller's `var`s of sendable types, since it runs at
    once on the calling thread. *Why*: without it, `Mutex(b)` or `m.with(v -> v)` hands the guarded
    buffer to code that changes it without the lock, which is the shared mutable state the mutex is
    the one door to. *Alternatives*: `Mutex` of any value unchecked, as written before; checking only
    `with`'s result. The cost: a body cannot give back a fresh mutable value (`nextBytes` on the
    shared random generator copies through a vector), and a generic body over an unbounded `T`
    cannot take its argument in (the shared generator's `shuffle`, `choose` and `chooseWeighted`
    now draw under the lock and pick outside it, with the same draws). Review with the user.
  - The standard library promises `Sendable` for `Thread[T]` of a sendable `T`, `Executor`, a
    mutex's `Condition`, and `TcpStream` and `TcpListener`, which hold a Sysl handle and are handed
    over with it. A `Scope` is not sendable, so the trap on starting a scope's thread from another
    thread is reached only through module-level state.
  - Tests that shared mutable data across threads were changed to share it safely: a scope's
    threads write `Atomic` slots rather than an array; a flag and a once-initialized value became
    `Atomic`s; a vector built in a `var` is copied into a `val`; a test's own byte array is made by
    each thread; a trait sent through futures promises `Sendable`; the run test of the scope trap
    became a rejection test; a test's `Once` counter became an `Atomic`; a test helper passing a
    function to its threads marks it `@sendable`.
  - `Command.lines()` on a command holding a sequence starts the command on its driver thread, so
    the mutable job belongs to that thread alone, and learns through a promise whether it started;
    before, the job was made on the calling thread and handed to the driver. A started `Child` and
    a lines iterator are mutable and not sendable; a thread starts and reads its own commands.
    Moving one would need `Isolated` or a promise like `TcpStream`'s; a started `Child` has since
    been given the promise ([proposed](#proposed)), and a lines iterator has not.
- Settled on the user's delegation 2026-11-05 (built on branch `fix-lone-value`): **A member call on the name of a data type whose variant of its own name has no fields**
  (`Lone.m(…)` with `data Lone()`; branch `fix-lone-value`). The name is both the type and a value
  of it. Proposed: where the type has a static member named `m`, the call is the type's and chooses
  among its static members alone; otherwise it is the value's method, its trait methods and
  impl-added methods included. A method value `Lone.m` follows the same order. Why: a static member
  can only be reached through the type's name, while the value's method is always reachable through
  a value, so giving the static the name loses nothing; and it keeps `Name.m(…)` with a static
  member meaning what it means on every other type. Alternatives weighed: the value always wins
  (makes a fieldless type's static members unreachable by name); choosing among the static members
  and the value's methods together by parameter types (a call on a type name would then mix two
  kinds of receiver in one overload set, which nothing else in the language does); refusing a
  static member and a value method of one name on such a type (an extra rule for a rare case). The
  companion object needs no rule: it cannot share the name of a fieldless variant
  ([declarations](/spec/declarations/#object)).
- Settled on the user's delegation 2026-11-05 (built on branch `option-ord-fix`): the [polymorphic recursion](/spec/generics/#polymorphic-recursion)
  check counts a cycle through a method an impl with bounds of its own adds only along the paths
  whose type argument's outermost type may meet the bounds, that type's own arguments taken as any
  type; a type argument whose outermost type the check does not know (a tuple, a function, a part
  taken apart from a larger type) counts as one that may. Why: the check must never miss a real
  cycle, and the outermost type is what the flow already tracks per path; `impl[T: Ord] Ord for
  Option[T]` otherwise made every program loading the threads library fail. Alternatives weighed:
  connecting the impl's methods from the parameter as a whole (what it did: false cycles through
  types that never meet the bounds); tracking type arguments deeper than the outermost type (exact
  for nested shapes such as a `Box[Box[Plain[T]]]`, which is still counted, at the cost of a much
  larger flow graph). Review with the user if a nested shape turns up in real code.
- Settled on the user's delegation 2026-11-05 (built on branch `option-ord-fix`): an ordering, `<`, `<=`, `>`, `>=`, settles an unknown in one
  operand's type from the other's, so `Some(1) < None` compares two `Option[int]`s, as `==` already
  did. Why: the two operands of an ordering are one type, so the other operand is the only evidence
  and the obvious one. Alternative: requiring `None[int]`, which `==` does not.

- Settled on the user's delegation 2026-11-05 (built on branch `qualified-variants`): **Qualified variant access, `Type.Variant`** (branch `qualified-variants`, 2026-11-05). Decided:
  `T.V` and `m.T.V` name data type `T`'s variant `V`, beside the bare `V`, in expressions and
  patterns; the variant comes before any other member of that name, and a companion object
  declaring a method or a field named after one of its type's variants is refused where it is
  declared. A capitalized `T.Name` that is no variant, companion member or static member is reported
  with the type's variants. Why: the qualified form is meant to be unambiguous, and refusing the
  companion member keeps it so, at the cost of a rule that only bites on a member spelled like a
  variant (members are lowerCamelCase, variants PascalCase). Alternatives weighed: the companion's
  member winning (`Shape.Circle` would then not always be the variant, defeating the point);
  choosing between them by call shape (a value and a call of one name meaning different things);
  allowing both silently with the variant winning (the companion member unreachable by name). A
  static trait member spelled like a variant is not refused, since the trait and the type are
  declared apart: the variant wins in `T.V`, and the member stays reachable through a type
  parameter. Type arguments are written once, on the type or on the variant, not both. Review with
  the user: whether the "capitalized name" test for reporting a missing variant should instead be
  any name that resolves to nothing on a data type.
- Settled on the user's delegation 2026-11-05 (built on branch `thread-ownership`): **Every operation on a connection or a listener checks its socket's owner first** (branch
  `thread-ownership`). A `TcpStream` keeps the bytes it read ahead in Avin fields, so a read served
  from them alone never reached the socket, and the code that had handed the connection over read
  on from them without trapping; a closed connection or listener answered an error, and a
  listener's `port()` its field, from any thread. Now `read`, `readLine`, `readAll`, `write`,
  `writeText`, `setTimeout`, `shutdownOutput` and `close` on a connection, and `port`, `accept` and
  `close` on a listener, first call a runtime method that does nothing on the open socket, or the
  socket's `close()`, which does nothing on a closed one: both check the owner, so the previous
  owner traps at every use, open or closed, and the new owner reads the bytes read ahead, which
  move with the connection. The owner check comes before a read's size check. Why: the promise is
  that a connection belongs to one thread at a time, and a race on the read-ahead fields is exactly
  what it rules out. Alternatives weighed: dropping the read-ahead on hand-over (loses bytes the
  other end sent); a check in the compiler for every field of a class holding a handle (a language
  rule for what one library type needs); an Avin owner field compared by hand (it would not move
  when the runtime hands over, which moves only handles).
- Settled on the user's delegation 2026-11-05 (built on branch `thread-ownership`): **A started `Child` promises `Sendable`, and moves between threads as a connection does**
  (branch `thread-ownership`). It holds a runtime token, a handle holding nothing but its owner,
  and `waitFor`, `isAlive` and `kill` use it first, so handing a child to a thread's or a future's
  body makes it the body's, the code that handed it over traps at any use while the body runs
  (`` `Child` belongs to another thread ``), and it comes back to the next user when the body
  returns. The body may wait for it, stop it and start a sequence's later part. Why a token: a
  child's own handles come and go (a program that has ended gives its handle back, a part not yet
  started has none, nor has one whose time ran out before it began), so none of them can carry the
  promise. Alternatives weighed: `Isolated` (not built yet, and a child is a resource handed over
  whole, as a connection is, not an object graph being built); keeping it not sendable, so that a
  thread starts and waits for its own commands (what it was; a server that starts a command and
  hands it to a waiter could not). A lines iterator stays not sendable: it is consumed where it is
  made, and the thread reading a command's output can start the command itself.
- Settled on the user's delegation 2026-11-05 (built on branch `thread-ownership`): **A handle class's `@sysl` may name it for its traps: `@sysl("avin.rt.Token", named: "Child")`**
  (branch `thread-ownership`; the director decided that the old owner's trap names the type the user
  holds). Every trap a handle raises, belonging to another thread and being closed, in compiled code
  and in the shim, uses that name rather than the class's; the name must be a type's (a capital
  first, letters, digits and `_`), and nothing else about the handle changes. Why: the trap is read
  by the code that misused the value, which knows the `Child`, not the private token class inside
  it. Alternatives weighed: a token class of the same name in a module of its own (two types named
  `Child`, which misleads a reader of the library); the trap text computed by the runtime from a
  name the token holds (the check is inline, one compare in compiled code, and its text is fixed
  when the program is compiled); naming the trap after the class whose method made the call (a
  rule no reader would guess). `TcpStream` and `TcpListener` each keep a handle class of their
  own over the one Sysl type `sysl.net.Socket`, `named:` for the type, so their traps name
  the type the user holds (branch `socket-trap-names`); two handle classes may share one Sysl
  type. The other library handles (`File`, `FdReader`, `Spawned`, `Address`) are held only inside
  private library values whose own checks come first, so none is named further.
- Settled on the user's delegation 2026-11-05 (built on branch `module-var-threads`): **a thread's body reaches no
  module-level mutable state** ([what is shared](/spec/threads/#what-is-shared)). What is passed
  for a `@sendable` parameter, directly or through every function it reaches, reads and writes no
  module-level `var` and reads no module-level value, `lazy val` or `object` whose type is not
  sendable. Reached means: the function a call names; for a call through a method table or a bound,
  every method of that name in that slot of any type's table; for an operator on a value that is not
  a scalar, every implementation of the trait method it calls; every closure and function value
  made along the way; and the code a pattern runs (an extractor's method, a sequence's length). The
  error names the module-level item and the functions it is reached through, shortest path first.
  *Why*: a module-level value is reached by name, not captured, so the capture rule of `@sendable`
  never sees it, and a body that bumps a module-level counter races with `main`. Avin compiles the
  whole program, so what a call can reach is known, as `isolate`'s check of its body already
  assumes. *Alternatives*: no check (the memory model makes the race harmless to the collector, but
  the threads chapter promises that threads share nothing mutable but the synchronized types);
  rejecting only what the body names itself (misses every call, so it closes nothing); per-thread
  copies of module-level `var`s (a different meaning for one declaration depending on which thread
  reads it); and treating each module-level `var` as an implicit `Atomic` (hides a lock in every
  read and does not make a read-modify-write atomic).
  - The whole argument counts, not only the closure: `spawn(makeBody())` is checked through
    `makeBody` and every closure it makes, since what runs on the thread is whatever it gives back.
    The cost is a rejection where a call evaluated at once on the calling thread reads a `var` to
    choose the body. *Alternative*: following only closures and function values, which misses a
    body returned by a call.
  - A call through a trait method or an operator counts every implementation in the program, since
    which one a value has is not known where the body is checked. The cost is a rejection where one
    implementation reads a module-level `var` and the body never meets a value of that type.
    *Alternative*: following the receiver's static type only where the class is final, which is
    finer but has to agree with devirtualization.
  - What is passed for an `@isolated` parameter is not held to this, since it runs at once on the
    calling thread. A `Mutex`'s value and `with` body were `@isolated`; they are now `@isolating`,
    which is held to it (see the `mutex-isolating` entry below).
  - Printing stays legal with no promise: `print`, `println`, `eprintln` and the `StandardOutput`
    and `StandardError` objects reach the runtime's streams, which keep no state in Avin, and the
    objects hold no field. The standard library's own module-level state is held to the rule like
    a program's: the futures' worker registry, guarded by its lock, promises `Sendable`, as do the
    regex library's Unicode run tables (arrays filled before the table is made and never written
    after), and so does
    the `Annotation` trait, since a type's shape is one module-level value every thread may read
    (an annotation type holding a mutable field is now rejected where it implements the trait). A
    collection's companion object (`Array`, `Buffer`, `Deque`, the mutable `Map` and `Set`) is
    sendable like any object with no field; only the collections themselves are mutable.
    Review with the user: whether the `Annotation` promise is wanted, and whether the trait and
    operator over-approximation rejects too much in practice.

- Settled on the user's delegation 2026-11-05 (built on branch `lambda-untupling`): **A lambda takes a tuple apart by its parameters** ([tuples](/spec/types/#tuples); branch
  `lambda-untupling`). Where a function of one parameter whose type is an *n*-tuple (*n* ≥ 2) is
  expected, a lambda of exactly *n* parameters binds the tuple's elements in order, each typed from
  its element; a declared parameter type must fit its element (a wider one is accepted, as for any
  lambda parameter), and an element the expected type leaves unknown takes its declared type. `_`
  binds nothing; an inner tuple is one element. It applies only where the lambda's parameter count
  does not fit the expected function directly, and among overloads a candidate that needs it is
  chosen only where none takes the lambda as written (`Map.map((k, v) -> …)` stays the
  two-parameter overload). A count fitting neither shape is an error naming both. Why:
  `xs.zip(ys).forall((x, y) -> x == y)` is the natural spelling and was rejected, leaving only
  `match (x, y) -> …`, a pattern where two names suffice; the reference language takes tuples apart
  the same way. Alternatives weighed: keeping the `match` function as the only way (no new rule,
  but the common case pays for a pattern); allowing a pattern in any lambda parameter position,
  `((a, b), c) -> …` (more general, but a second pattern syntax beside the `match` function, which
  already covers nested and refutable shapes).

- Settled on the user's delegation 2026-11-05 (built on branch `iterable-size`): **every `Iterable` has `size()`, `knownSize()` and `sizeCompare(n)`** ([collections](/spec/collections/)), following the reference library: `size()` counts by iterating unless a collection overrides it with its own count, `knownSize()` is the count where it is known without iterating and -1 otherwise (a `List` other than `Nil`, an iterator, a view), and `sizeCompare(n)` answers -1, 0 or 1, reading at most `n` elements when the count is unknown. Sequences have `size()` and `length()`, equal. They are overridable trait members rather than `impl` methods, so a call through `Iterable[T]` reaches the collection's own count. Consequence: a class implementing `Iterable` that declares its own `size`, `knownSize` or `sizeCompare` must mark it `override`. Alternatives weighed: an arbitrary-sign `sizeCompare` (the reference library's contract, but -1/0/1 is what every caller tests); `length()` on every `Iterable` (left to sequences, whose length is their defining measure); `sizeIs` (needs special syntax). Review with the user: the `override` requirement on existing user classes.

- Settled on the user's delegation 2026-11-05 (built on branch `ui-binding`): **A callback's integer answer goes into whatever integer the closure Sysl takes answers, with a
  check** ([Sysl bindings](/spec/sysl-bindings/#callbacks)). An Avin function answering a `long` or
  an `int` reaches a Sysl closure answering any integer, `u32`, `u64` and `usize` among them, and
  an answer the Sysl type cannot hold stops the program where the binding Sysl is running in was
  called, naming the type by sign and width (`value out of range for u32`). Why: Avin has no
  unsigned integers (settled 2026-11-02), and syslui asks for a colour by `&Fn() -> u32`; the
  answer is the mirror image of an [integer result](/spec/sysl-bindings/#integer-results), which
  already takes any Sysl integer, so one rule covers both directions of a value Sysl reads, and a
  narrower signed answer is the same case. A closure answering a generic Sysl function's type
  parameter names no integer, and answers the binding's own type; the compiler learns that from
  Sysl's refusal to infer one and builds the shims once more. Alternatives weighed: an `int`
  reinterpreted as the `u32`'s bits (a colour past `0x7FFFFFFF` written negative, and a reader
  never sure which); refusing the binding (no window could be given a colour); converting only
  into unsigned types (two rules for one shape). Built on `ui-binding`.
- Settled on the user's delegation 2026-11-05 (built on branch `ui-binding`): **The user interface binding is the package `ui`, in `packages/ui` of this repository, rather
  than a module of the standard library** ([User interfaces](/spec/ui/)). Why: no package may
  declare a module under `avin`, the standard library carries no Sysl dependencies, and a program
  that does not draw should neither build syslui nor link SDL3; a domain library is a package of
  its own ([packages](/spec/packages/#the-standard-librarys-modules)). Alternatives weighed: `avin.ui`
  in the standard library (needs the library to name Sysl packages and every program to link SDL3);
  a repository of its own (premature while the binding and the language move together). Built on
  `ui-binding`. Review with the user: the name `avin.ui` was the plan's, and only the standard
  library may hold it.
- Settled on the user's delegation 2026-11-05 (built on branch `ui-binding`): **The binding is thin and keeps syslui's shapes** ([User interfaces](/spec/ui/)). Each syslui
  function is bound under its Avin name with syslui's defaults restated; a view is one handle class
  on the trait; one signal class for each instance a control takes (`IntSignal`, `RealSignal`,
  `BoolSignal`, `StringSignal`); `slider` is one overload for `int` and one for `real`; `Role`,
  `Variant`, `TextStyle`, `MissKind`, `Missed` and `Rect` are mirrors. The package's one function of
  its own is `tap(v, key, at)`, the draw, snapshot, press and release a test of an interface makes
  every time. Why: the component and signal layer, which is Avin's, decides the shapes an
  application writes; this layer only has to reach syslui. Alternatives weighed: a generic signal
  class (Avin binds a handle at one instance); wrapping every binding in a private one (no name to
  change yet). Built on `ui-binding`.
- Settled on the user's delegation 2026-11-05 (built on branch `ui-binding`): **`run` is `blocking`, and no ui callback is `@threaded`** ([User interfaces](/spec/ui/#the-application)).
  The frame loop waits for events for most of its life, so the thread is safe for the collector
  meanwhile, and syslui-sdl calls the tree, the ground and every control's callback on the thread
  that called `run`. Alternatives weighed: an unmarked `run`, which would hold up every other
  thread's collections for the life of the window. Built on `ui-binding`.
- Settled on the user's delegation 2026-11-05 (built on branch `unicode-identifiers`): **Names in any script** (branch `unicode-identifiers`, 2026-11-05; [names](/spec/declarations/#names)).
  A name starts with a character of Unicode's `XID_Start` or `_` and continues with `XID_Continue`
  (UAX #31, the Unicode identifier rule), at the Unicode version the compiler's tables were cut
  from (17.0.0). Why: domain vocabulary is written in the programmer's own language, and the XID
  properties are the ones designed to be stable under normalization and across Unicode versions.
  Alternatives weighed: ASCII only (what the lexer had; refuses `größe` for no reason a reader
  could guess); any non-ASCII character that is not a space (admits emoji, arrows and operators
  of other scripts, so `a→b` would be one name); General_Category letters, marks and digits
  (Sysl's rule; close to XID but not stable under normalization, and admits pattern characters
  such as U+2E2F).
- Settled on the user's delegation 2026-11-05 (built on branch `unicode-identifiers`): **A name in a normal form other than NFC is an error, with its NFC spelling** (branch
  `unicode-identifiers`). Why: every name stays byte for byte the text written, so spans, rename,
  lookup and emitted symbols need no second spelling, and two spellings that look identical can
  never be two names, which is what normalizing would also guarantee. Alternatives weighed:
  normalizing to NFC silently (friendlier to an input method that writes decomposed text, but
  every name then has a spelling the source does not contain); no normalization (Sysl's choice:
  `café` and `cafe` with U+0301 are two names that look the same); NFKC (folds `ﬁ` into `fi` and
  full-width letters into ASCII, which changes what a name visibly is). A module's directory name
  is held to the same rule. Review with the user: whether silent normalization is preferred.
- Settled on the user's delegation 2026-11-05 (built on branch `unicode-identifiers`): **Capitalized means an uppercase or titlecase first letter (`Lu`, `Lt`); a caseless letter is
  not capitalized** (branch `unicode-identifiers`). Every place the compiler asks whether a name
  is capitalized (a missing variant written `T.V`, a scalar type's `MaxValue`-style constants, a
  missing type in an import of a library module, a handle's `named:`, the highlighting of types)
  uses this test, so `Ärger`, `Δelta` and `ǅemal` read as type-like names and `名前` as a value's.
  A bare name in a pattern is a variant when one of that name is in scope and a binding otherwise,
  whatever its case, so a caseless name is a binding unless it names a variant. Why: a caseless
  script has no way to write a capital, and treating its names as lowercase keeps them usable for
  the commonest names (values, functions); its types are still declared and used by name.
  Alternatives weighed: caseless counting as capitalized (every CJK local would read as a
  constructor-like name); refusing caseless names where a capital is expected (a CJK program could
  not name a type). Review with the user: this is the choice most likely to matter to a program
  written wholly in a caseless script.
- Settled on the user's delegation 2026-11-05 (built on branch `unicode-identifiers`): **Look-alike names in different scripts are different names, with no warning** (branch
  `unicode-identifiers`): Latin `a` and Cyrillic `а` are two names. Why: confusable detection
  (UTS #39) needs data the compiler does not carry and warns on legitimate mixed-script code.
  Alternative: a mixed-script warning per name. Review with the user if spoofed names in shared
  packages become a concern.
- Settled on the user's delegation 2026-11-05 (built on branch `unicode-identifiers`): **A character above ASCII that begins no token is reported with its code point and stepped over
  whole** (branch `unicode-identifiers`): an emoji is "not a character a name or an operator is
  spelled with", and a digit or mark of another script where a name begins "cannot begin a name",
  each naming the character and its `U+` code point. Why: the bytes of one character used to be read as separate
  symbols, which the parser then reported one at a time.

- Settled on the user's delegation 2026-11-05 (built on branch `sendable-functions`): **`@sendable`
  function types**, answering the open question whether a function value can be sent, with a type
  that says what it captures ([function types](/spec/types/#function-types), [what is
  shared](/spec/threads/#what-is-shared)).
  - `@sendable (A) -> B` is a function type whose values capture only sendable values and no `var`
    and reach no module-level mutable state; it is `Sendable`, and a plain function type is not.
    Only a function type may carry the mark.
  - A lambda, a function reference or a method value written where a `@sendable` function type is
    expected is made as one, and is checked there by exactly the checks what is passed for a
    `@sendable` parameter gets: its captures, and the whole-program reach of module-level state.
    The error names the type (`` a `@sendable (int) -> int` may use only sendable values … ``). Any
    other value of a plain function type is not one, and the mismatch says why.
  - `@sendable (A) -> B` is a subtype of `(A) -> B` with no conversion: in covariant positions
    (a vector's element, a result, an override's result) too, not through an invariant one (an
    array's element). Two branches of the two types agree on the plain one.
  - A parameter marked `@sendable` that takes a function holds a value of the `@sendable` function
    type inside the function, so the mark and the type are one notion seen from the two sides; the
    parameter's declared type is unchanged, so an override that drops the mark still overrides.
    Passing a value already of a `@sendable` type to `spawn` or any marked parameter is accepted as
    it is.
  - Data and classes whose fields are `@sendable` functions are sendable by the existing structural
    rules, at every type argument of a generic one (a `Parser[T]` holding a
    `@sendable (Input) -> Option[T]`), so module-level parser grammars, `lazy val`s among them, pass
    the module-state check and run on spawned threads.
  - Two overloads that differ only in whether a function parameter is `@sendable` are one overload
    (refused as declared twice), as two differing only in a by-name parameter are, since one lambda
    may be passed for either and overload resolution runs before the sendability check.
  - An override's parameter types match its method's exactly, the mark included; its result may be
    the `@sendable` subtype.
  - *Why*: the parsing library's grammars are module-level values holding functions, and could not
    be used from a thread at all; the `@sendable` parameter mark already decides exactly what a
    function value may capture and reach, so the same check at the point a value is made lets the
    type carry the guarantee through storage, fields and module-level values, soundly, and the
    spelling reuses the mark readers already know.
  - *Alternatives*: inferring capture sendability into every function type with no mark (every
    lambda's type would depend on its body, unification and overloads would see two function types
    per signature, and an API could not promise it); effect-style capture sets in the type (precise,
    but a large addition to the type system for one property); leaving functions never sendable
    (sound, but module-level grammars, handler tables and task lists stay unusable from threads).
  - **Review with the user**: the spelling `@sendable (A) -> B` (an annotation in type position, the
    only one), against a keyword or a `Sendable`-bounded spelling; and whether two overloads
    differing only in the mark should be refused or resolved to the plain one for a lambda.

- Settled on the user's delegation 2026-11-05 (built on branch `state-machines`): **State machines, the choices [the chapter](/spec/actors/#state-machines) leaves open** (branch
  `state-machines`, 2026-11-05). A machine is read into its `Door.State` data type and a final class
  holding the current state, so it needs no runtime. Decided:
  - **Order inside a transition.** The arm runs first; then the current state's `leave`, in which
    `state()` still answers the state being left; then the new state is current and its `enter`
    runs, in which `state()` answers it; `handle` answers the state after `enter`. Why: an arm
    computes where to go, `leave` cleans up the old state's data, which it can still read, and
    `enter` sees the machine as it will be. Alternatives: switching the state before `leave` (it
    would then read data no longer current); running `leave` before the arm (an arm yielding `stay`
    would have left).
  - **A state is a variant of its module**, as every data type's variant is: `Locked(code)` and
    `door.state() is Locked(_)` name it bare. Why: it follows from `Door.State` being an ordinary
    data type matched by bare names, as the chapter writes. Alternatives: scoping state names to
    their machine, written bare only inside it and `Door.State.Locked` outside (the chapter's own
    `door.state() is Locked(_)` would no longer read). Amended (Proposed, branch `shared-variants`,
    below): two machines of one module may now share a state's name, and a state an event's
    variant, each bare use decided by the type expected there, as for every shared variant name;
    until then a state's name had to be one no other variant of the module had.
  - **`Door.State` takes all of the machine's type parameters, with their bounds**, so a state of a
    generic machine may hold values of them: `Cell.State[string]`. Why: the smallest rule that lets
    any state hold any of them. Alternative: only the parameters the states use (the type's
    arguments would then depend on which states mention which parameter).
  - **`stay` is the value of an arm where it is the arm's value**: the arm's whole body, the last
    line of a block, and each branch of an `if` or a `match` that is itself in such a place.
    Elsewhere `stay` is an ordinary name. Why: the chapter makes `stay` a keyword only as an arm's
    value. Alternative: anywhere in an arm's body (it would then be a value that could be stored,
    with a meaning only at the end).
  - **The trap on handling from inside a handler names the machine and is reported at its
    declaration**: ``avin: trap: `Relay` handles an event from inside one of its own arms, `enter`
    or `leave` blocks at file:line:col``. The first state's `enter`, run as the machine is made, is
    such a block too. Why: the check is in the code the declaration makes. Alternative: reporting
    the `handle` call that re-entered, as a library trap reports the program's call (it needs the
    machine's code to count as the library's for its traps, which its arms are not).
  - **The coverage error lists every case left out**: each variant of the event type not covered in
    full, written as its first uncovered value (`Lock(_)`, `Both(true, None)`), followed by "where
    the guard is false" when a guarded arm could have taken it; for an event type without variants,
    one value (`-1`). Why: it is the chapter's own message, extended to the shapes it does not show.
  - **A machine takes no parents and no `derives`** in its header in this step, and a state and the
    `receive` block take `else` arms as a `match` does. Alternative: parents after the parameters, as
    a class's; nothing in the chapter needs them yet.
  - **Handling an event from the machine's own body**, before the first state's `enter` has run, is
    allowed: the machine is in its first state then. Alternative: trapping as from inside a handler.
  - **Not sendable, said as such**: a machine passed where a sendable value is needed is reported as
    "a machine, a mutable object that moves between its states", not by its hidden state field.
- Settled on the user's delegation 2026-11-05 (built on branch `ui-int-params`): **A binding's integer parameter goes into whatever integer the Sysl
  parameter is, with a check** ([Sysl bindings](/spec/sysl-bindings/#integer-parameters)). An Avin
  `int`, `long`, `i8` or `i16` parameter reaches a Sysl parameter of any concrete integer type, `u8`
  to `u64`, `usize` and the signed widths, converted in the shim; a value the Sysl type cannot hold
  stops the program where the binding was called (`value out of range for u32`). A handle's method
  converts the same way, and a blocking call converts with its thread safe for the collector,
  making it running again before stopping the program. Why: syslui's colour modifiers take a `u32`
  parameter, Avin has no unsigned integers (settled 2026-11-02), and the rule is the mirror image of
  the integer result and the callback answer, so all three directions of an integer follow one
  rule. Consequence: a `long` now reaches an `i32` parameter where its value fits, which was
  rejected before. Alternatives weighed: converting only into unsigned types (two rules for one
  shape); an `int` reinterpreted as the `u32`'s bits (a colour past `0x7FFFFFFF` written negative).
- Settled on the user's delegation 2026-11-05 (built on branch `ui-int-params`): **An integer parameter is passed as the Avin type
  itself where Sysl cannot infer a type for its conversion**: a parameter whose Sysl type is a type
  parameter of a generic function (`same[T](x: T)`), and every integer parameter of a binding whose
  call left any type argument uninferred (syslui's `slider[T](value: &Signal[T], lo: T, hi: T)`,
  where the integer bounds decide `T`). The compiler learns this from Sysl's refusal and builds the
  shims once more, as it does for a callback's answer. Why: a conversion needs a target type, and a
  generic parameter has none until its arguments say. Alternatives weighed: converting the
  parameters a generic call can still infer and passing only the stalled ones as they are (needs a
  build per round of refusals); refusing such bindings (`slider` would be unbindable). Review with
  the user: a binding to a Sysl function overloaded only by integer width now reaches the overload
  Sysl picks for an uninferred integer argument, rather than the one matching the Avin type
  exactly; no binding does this today.
- Settled on the user's delegation 2026-11-05 (built on branch `ui-int-params`): **An integer an `Option`, a `Result`, a tuple or a
  collection parameter holds is not converted**, and the binding is rejected with a sentence saying
  so. Why: the conversion functions for these shapes are shared between bindings and spelled with
  the Avin type's Sysl spelling; making each generic in its integers would make every nested
  parameter an inference problem, for no binding that needs it yet. Alternative: converting them as
  results are converted (the shape is there, the need is not).
- Settled on the user's delegation 2026-11-05 (built on branch `ui-int-params`): **A colour in `ui` is an `int` holding
  `0xRRGGBB`**, as syslui draws it, with `rgb(r, g, b)`, `mix`, `lighten` and `darken`
  ([User interfaces](/spec/ui/#colours)). Why: syslui has no alpha, so a `0xRRGGBBAA` would carry a
  channel nobody reads; every colour fits an `int`, which is what a hex literal is by default; and a
  binding's parameter cannot take an Avin data type, so a `Color` type would need a wrapper around
  every modifier. Alternatives weighed: a `long` (what `app`'s `ground` answers, but wider than any
  colour); a `Color` data type with `rgb`/`rgba` (a wrapper per modifier, and an alpha syslui
  ignores). Review with the user: `app`'s `ground` still answers a `long`; it could become an `int`
  to match.
- Settled on the user's delegation 2026-11-05 (built on branch `ui-int-params`): **A value narrower than 32 bits crosses the
  boundary widened as the C convention says**: Avin marks a shim's `i8` and `i16` parameters and a
  callback's narrow signed answer `signext`, and a byte or bool `zeroext`, which Sysl relies on. This
  fixes a bug: a negative `i8` or `i16` computed at run time reached Sysl as a large positive value
  (`-5` as `65531`). Alternatives weighed: passing every narrow integer as an `i32` and narrowing in
  the shim (changes the type a generic Sysl parameter infers).

- Settled on the user's delegation 2026-11-05 (built on branch `sendable-params`): **a `@sendable` parameter is sendable
  inside its function, whatever its type** ([threads](/spec/threads/#what-is-shared)). Found by the
  parsing library, whose combinators take a `@sendable f: PartialFunction[char, U]` and could neither
  capture it in a `@sendable` lambda nor pass it on.
  - Inside the function the parameter is sendable: a `@sendable` lambda captures it and it is passed
    on for another `@sendable` parameter without a check. *Why*: what is passed for it was checked
    where it was passed, as a function-typed parameter's already was.
  - What is passed for one that does not take a function is itself sendable: a `match` function
    written in place (the core library's `MatchPartialFunction` of two lambdas, checked by what the
    arms capture and the module-level state they reach), a value of a sendable type, a parameter so
    marked, or a class's marked `val` parameter. Any other value is refused, where before a fresh
    object (`what it makes itself is its own`) passed. *Why*: the value is now shared by whatever the
    function hands it to, so a fresh object with a `var` field would be shared between threads.
  - The mark belongs to the parameter, not to its type (the sound minimum): a marked value copied
    into a local, stored in a data field typed as the trait, combined (`f.orElse(g)`), or passed for
    an unmarked parameter bounded by `Sendable`, is a value of the trait's type again, which is not
    sendable. A class's `val` parameter marked `@sendable` is a field whose value is sendable, so it
    leaves the class sendable by its structure; a `var` one does not. *Alternatives*: a
    `@sendable` mark on trait types (`@sendable PartialFunction[A, B]`), which would carry the
    property through locals and fields, or marked data fields; both are new type-system surface, and
    can be added later without breaking what this allows. **Review with the user**: whether
    `@sendable` should become a mark on trait types, as it is on function types.
  - A lambda or a `match` function passed for a type argument bounded by `Sendable`
    (`^^^[U: Sendable](value: U)`) is made as a `@sendable` function and checked as one where it is
    made; where the expected type decides that argument as a plain function type, it is taken as its
    `@sendable` form, which conforms to the plain one. *Why*: a plain function type is never
    sendable, so the call could not succeed otherwise. *Alternative*: requiring the type written,
    `^^^[@sendable (int, int) -> int](…)`, which is noisy for every operator parser. A `match`
    function where a `PartialFunction` is expected is not changed by the bound: it is a
    `PartialFunction`, which does not promise `Sendable`.

- Settled on the user's delegation 2026-11-05 (built on branch `mixed-lambda-inference`): **the lambdas and `match` functions of a call are
  checked in turn, each the first in the call whose parameter types are known**
  ([generics](/spec/generics/#inference)): declared by it, or decided by the arguments checked so
  far; where none is, the first one left is checked and reports what it cannot infer. Found as a
  bug: they were checked strictly left to right after the other arguments, so
  `both(match n -> n + 1, (x: int) -> x * 10)` for `both[A, B](g: (A) -> B, f: (A) -> B)` failed
  with "cannot infer the type of the value this `match` matches", as did an untyped lambda written
  before the typed one, while the same arguments swapped were accepted. *Why*: the smallest rule
  under which argument order does not matter for what can be inferred, and every call accepted
  before is checked in the same order as before (each lambda it checked was already the first
  ready one). *Alternatives*: keep left to right (order-sensitive, as the bug shows); check the
  lambdas with declared parameters first, then the rest left to right (misses chains, where a
  `match` function's type comes from an untyped lambda's result); or solve all of them together
  as constraints, checking each body against partly unknown parameters (a larger change, with
  diagnostics harder to place). **Review with the user**: the order rule is visible in which calls infer, so a later
  constraint solver would be a language change.
- Settled on the user's delegation 2026-11-05 (built on branch `sendable-followups`): **three follow-ups to `@sendable` parameters**
  ([threads](/spec/threads/#what-is-shared), [callbacks](/spec/sysl-bindings/#callbacks)). They
  amend the `sendable-params` entry above where it says a combination, and a `match` function where
  a `PartialFunction` is expected, are not sendable.
  - A binding's `@threaded` parameter takes a value of a `@sendable` function type and a parameter
    marked `@sendable` as they are, and a lambda passed there may capture a marked parameter. A
    plain function value, an unmarked or `@isolated` parameter, and an unmarked parameter captured
    stay refused. *Why*: each was checked where it was made or passed to be what any thread may
    run, which is all the `@threaded` check asks. *Alternative*: none sound is narrower; requiring
    a lambda around it (`x -> f(x)`) is the contortion this removes.
  - **`f.orElse(g)` is sendable where `f` and `g` are**, by a promise the standard library writes on
    the method, `@sendable_result`: a call of a function so marked is sendable where its receiver
    and every argument are a `match` function written in place, a value of a sendable type, a
    marked parameter or class field, or another such call; chains of `orElse` follow. The promise
    is refused outside the standard library, since nothing checks it against the body, and a call
    that names the method statically reaches no override. A combination held in a local is a
    `PartialFunction` again, as a marked parameter copied into one is. Only `orElse` makes the
    promise: `lift` answers a function whose captures a program's own `PartialFunction`
    implementation decides, and `applyOrElse` answers a value, not a partial function.
    *Alternatives weighed*: a general rule that any call whose parts are sendable is sendable
    (unsound: the call may answer a fresh object with a `var`, or reach module-level state); a
    conditional `Sendable` impl for `OrElsePartialFunction` (the result's static type is the trait,
    which no impl of the class reaches); hard-coding `PartialFunction.orElse` in the checker (the
    annotation says the same and names its users); a `@sendable` mark on trait types, under review
    in the entry above, which would subsume this. **Review with the user**: whether a program's own
    combinators should be able to make the promise, checked against the body (a body that only
    builds an object of a final class from its parameters).
  - **A `match` function where a `PartialFunction` is expected, passed for a type argument bounded
    by `Sendable`, meets the bound**, as a lambda there does: its two lambdas are made as
    `@sendable` functions and checked where they are made, the one running its arms holding every
    pattern, guard and body, so a fault is reported once. `PartialFunction` itself does not promise
    `Sendable`, so the bound is waived at that call only where every value the call gives the
    callee of the type argument is such a function: each parameter whose type could give one (any
    position but what a function passed takes, a data type's or a class's argument counted whatever
    its variance) is of exactly that type and given such a function. A written type argument is
    held to the bound as written, as `take[(int) -> int](…)` is. *Why*: the callee obtains values
    of its type argument only from its arguments, each of which was checked. *Alternatives
    weighed*: a sendable form of the `PartialFunction` type (the trait-type mark under review
    above); making `MatchPartialFunction` promise `Sendable` (it holds two plain function fields,
    which are not sendable, and a `match` function's static type is the trait anyway).

- Settled on the user's delegation 2026-11-05 (built on branch `ui-components`): **Interface state is a generic Avin class
  `State[T]`, with `get`, `set` and `update(f)`, and a set is one write: an `invalidate()` for a
  state held in Avin, or a write of its signal for one a control has linked**
  ([components](/spec/ui/#components-and-state)). Why: the state lives in Avin, so it holds any
  type; making the set the frame loop's write means nothing else has to be called after one, and a
  set is counted exactly as a signal's is. Alternatives weighed: syslui's own signals for every
  state (only four instances exist, so a list or a data value could not be state); a set that skips
  the write when the value is unchanged (needs `Eq` on every `T`, and a function or a class has
  none).
- Settled on the user's delegation 2026-11-05 (built on branch `ui-components`): **A control over a state reads and writes the
  state's own signal, made once by `State.signal(s)`, which is from then on where the value lives**,
  rather than a second copy kept in step. Why: one place for the value means a control's write is
  the state's value at once, with no build in between, and no write can echo back into the other
  side, so there is no loop to break. Alternatives weighed: a bridge copying both ways at each
  build, comparing against the last value it saw (stale between a tap and the next build, and a
  conflict to resolve when both change); a callback from syslui on each write (not in the binding).
  The controls taking a `State` are overloads of the binding's own (`switch`, `checkbox`, `radio`,
  `radioGroup`, both `slider`s, `textField`, `textArea`, `tabs`, `scroll`).
- Settled on the user's delegation 2026-11-05 (built on branch `ui-components`): **Components are functions returning `View`, written
  `component c -> …` or `keyed(key) c -> …`; what a component keeps across builds is asked of its
  `Scope` by order (`c.state(init)`, `c.remember(make)`, `c.derived(f)`), and a component is found
  again by its place among its parent's unkeyed components, or by its key.** A build is a call of
  the function `root(body)` answers; a component the build of its parent leaves out is dropped with
  everything inside it. Why: a component stays a plain function of its arguments, so props are
  ordinary parameters and need no class; reified type tests make the positional slots sound (a slot
  asked for as another type stops the program rather than being misread); the scope is a parameter,
  so a request outside a component cannot be written by mistake. Alternatives weighed: a class per
  component with fields as state (props would have to be copied into an instance kept from an
  earlier build, and every component would need a class and a table of instances by type); a
  state machine per component (fits a component with modes, not one holding a value); a scope
  passed from parent to child explicitly (every call of a component would carry it). Review with
  the user: the shape of the API, and whether keys should be any `Hash` value rather than a
  `string` or an `int`. **Review with the user**: the component API's shape, and whether keys should be any `Hash` rather than a string or an int.
- Settled on the user's delegation 2026-11-05 (built on branch `ui-components`): **The mistakes the order and the keys can make stop
  the program**: a component built with no root building, two components under one parent with one
  key, a slot asked for as another type than the last build's, a scope asked after its build, and a
  root built inside its own build. Why: each would otherwise give one component another's state
  silently. Alternatives weighed: starting the slot or the component afresh (hides the mistake), a
  warning (Avin has no runtime warning channel).
- Settled on the user's delegation 2026-11-05 (built on branch `ui-components`): **`derived(f)` computes again only after something
  has been written since its last computation, by the counter `writes()` answers**, rather than by
  the states it read. Why: every state, signal and `invalidate()` moves the counter, so it is never
  stale where the frame loop would rebuild, and it needs no record of what `f` read. Alternatives
  weighed: tracking the states read during `f` (finer, but a hidden global and a cost on every
  `get`), computing on every call (no use as a value of its own).
- Settled on the user's delegation 2026-11-05 (built on branch `ui-components`): **The interface's state belongs to the thread running
  it, and the type system keeps it there**: a `State`, a `Derived` and a `Scope` are mutable, so not
  sendable, and a closure for another thread cannot capture one. There is no `post` or `runOnUi` yet.
  Why: the frame loop's write counter and syslui's signals are not synchronized, so the only sound
  cross-thread path is a queue the frame loop drains, and nothing in the binding wakes the loop from
  another thread. Alternatives weighed: a sendable `post(@sendable f)` queuing a change (its
  function could not capture the state it is meant to change, so it would need a sendable handle
  to a state, a design of its own). Review with the user: a worker reporting to the interface
  needs that handle and a way to wake the frame loop. **Review with the user**: the path from another thread, which needs a sendable handle and a way to wake the frame loop.
- Settled on the user's delegation 2026-11-05 (built on branch `ui-components`): **An example program is one package, so the to-do
  example is `examples/ui-todo` beside `examples/ui`**, since the corpus builds each directory of
  `examples` as one program with one `main`.

- Settled on the user's delegation 2026-11-05 (built on branch `actor-runtime`): **A behaviour's library form is
  `Behaviour[M](name: string, start: @sendable (ActorRef[M]) -> (M) -> unit)`** ([declaring an
  actor](/spec/actors/#declaring-an-actor)). The start runs once on the actor's first turn, is
  given the actor's own reference (`self`), makes the actor's state in its locals and gives back the
  handler of each message; `actor` declarations will lower to exactly this, the name being the
  declaration's. Why: it is the smallest value that holds "the declaration and its arguments" and
  can be started again for a restart, and making the start a `@sendable` function puts the existing
  capture and module-level-state checks on everything an actor's body reaches, with no new check.
  Alternatives weighed: a trait with a `receive` method implemented by a class per actor (the state
  would be an object the program sees, which the spec rules out); a constructor hidden from
  programs (then the runtime could not be tested before declarations exist, and a program could not
  build an actor from a function).
- Settled on the user's delegation 2026-11-05 (built on branch `actor-runtime`): **`Executor.execute(task)`** runs a task on
  the executor's workers with no future, as an owner of its own with nothing handed to it, and runs
  after `shutdown` as a future's functions do ([executors](/spec/threads/#executors)). Why: an
  actor's turns are queued on its executor again and again, and `avin.actors` cannot reach the
  pool's module-private queue; the path `future` takes hands over by walking everything the task
  reaches, which for an actor is its whole state and mailbox at every turn. Alternatives weighed:
  putting `avin.actors` inside `avin.threads` (the spec names the module); walking at each turn
  (cost grows with the actor's state); a compiler-known hook visible only to the library (new
  machinery for one call). **Review with the user:** it adds a public method whose owner rule
  (handles the giver owns trap in the task) differs from `future`'s.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-runtime`): **Scheduling details**
  ([scheduling](/spec/actors/#scheduling)). An actor's number is one sequence for the whole
  program, from one in the order actors are spawned, not one per declaration. `spawn` hands the Sysl
  handles its behaviour reaches to the actor, as a thread's `spawn` does with its body. The start
  runs in the first turn and is not one of the 16 messages; a turn ends early when the mailbox is
  empty; an actor on an executor that has been shut down goes on handling what it is told, so
  `tell` never fails. Why: each is the smallest rule that keeps `tell` from waiting or failing and
  keeps a handle usable by whoever runs the actor. Alternatives weighed: numbers per declaration
  (`Counter#1`, `Logger#1`; needs a registry of names); dropping messages to an actor whose executor
  is shut down (a silent loss the spec does not ask for).
- Settled on the user's delegation 2026-11-05 (built on branch `actor-declarations`): **An `actor` declaration is read into
  a function and a hidden class** ([declaring an actor](/spec/actors/#declaring-an-actor)).
  `actor Counter(start: int) receives M` is the function `Counter(start: int) -> Behaviour[M]`,
  with the parameters' defaults, and a private class holding the actor's reference, the parameters
  and the body's fields and methods, whose name (`actor Counter`) no program can write. The start
  of the behaviour makes one object of that class, which runs the body, and gives back the call of
  its `receive` match; inside the body `self` reads the reference. Why: it lowers onto the library's
  `Behaviour` with no runtime support, as the chapter says a declaration is, and the class gives the
  body's methods the implicit receiver and its fields their life across messages for free, while
  the object never becomes a value the program sees. Alternatives weighed: the body's fields as the
  start lambda's locals and its methods as local functions (no implicit receiver, and forward
  references between methods break); a runtime-known actor class (machinery the runtime does not
  need).
- Settled on the user's delegation 2026-11-05 (built on branch `actor-declarations`): **What an actor's parameters may be.**
  Each parameter's type is held to `Sendable` where the actor is declared, not where the start
  captures it, so the error names the actor and the parameter; one the program marks `@sendable`
  is exempt, as a marked parameter of a function is sendable inside it whatever its type. A `var`
  parameter is a field the actor may change, each start beginning again from the argument; a
  repeated parameter is allowed; a by-name one is refused. An argument is checked only by its type,
  so `Worker(s"$label$made")` computed from an actor's own `var` is accepted. Why: the chapter says
  the parameters follow a class's rules and must be sendable; this is the reading that adds no rule
  of its own. Alternatives weighed: marking every parameter `@sendable` (then an argument computed
  from a `var` is refused, since what is passed for a marked parameter is checked by what it reads);
  refusing `val` and `var` parameters (no gain: nothing outside the actor reaches its fields).
- Settled on the user's delegation 2026-11-05 (built on branch `actor-declarations`): **An actor's diagnostics name the
  actor.** A message type that is not sendable is reported once where it is written after
  `receives` (`` `Buffer[int]` does not conform to `Sendable`, so the actor `Buffers` cannot receive
  it ``), not once for the behaviour and once for the reference; a `receive` that does not cover the
  message type says `` actor `Partial` does not handle `C` `` and how a message is ignored; module-level
  state reached from the body, an arm or a method is `` the actor `Arm` may run on another thread ``,
  the path naming the actor's methods by their own names; a method value carrying the state is `the
  state of the actor`. Why: the program wrote an actor, not a behaviour and a lambda. Alternatives
  weighed: the generic messages of the `@sendable` start (they name a function type the program
  never wrote). **Review with the user**: the hidden class is spelled `actor Counter`, with a
  space, so it reads well where a diagnostic names it.

- Settled on the user's delegation 2026-11-05 (built on branch `actor-lifecycle`): **An actor's lifecycle is the class `ActorContext[M]`
  its state extends** ([lifecycle](/spec/actors/#lifecycle)). `stop`, `watch`, `unwatch`, `after`,
  `every`, `children`, `parent` and `onStopping` are methods of `avin.actors.ActorContext[M]`, made
  from the actor's reference; the hidden class of an `actor` declaration extends it, so the body,
  arms and methods call them as their own, and a behaviour written as a library value makes one in
  its start, `ActorContext(me)`. Each finds its actor through the runtime's table of running actors
  by the owner of the code running, and traps where that is not the actor (made by `main`, or
  called from a future). A field or method of an actor with the name and parameters of one of these
  is refused (`` `stop` is one of every actor's own methods ``), and the body may call them before
  its own fields are initialized, since they reach only the context's. The same table gives `spawn`
  the actor whose code is running, its new child's parent. *Why*: the chapter writes `stop()` and
  `after(…)` bare inside an actor, typed by the actor's message type, and inheritance gives exactly
  that with no new lookup rule, while the library form keeps working with no declaration.
  *Alternatives weighed*: changing a behaviour's start to take the context instead of the reference
  (every library-form behaviour changes, and `me` is no longer a reference); free functions finding
  the running actor (`after` could not be typed by the actor's messages); names the checker
  resolves specially inside actors (a lookup rule of its own, and nothing for the library form).
  **Review with the user**: the public name `ActorContext` and `onStopping`, and that an actor's
  own names now include `children` and `parent` (a test's field `children` was renamed).
- Settled on the user's delegation 2026-11-05 (built on branch `actor-lifecycle`): **Stopping, the cases the chapter leaves open.**
  `stop(self)` is `stop()`; stopping an actor that has ended does nothing, whoever started it (there
  is no actor left to protect, and the end may have raced the call); an actor asked to stop before
  its first turn still runs its body first, so its `stopping` block always has state to run on; an
  actor started while its parent is ending is stopped at once and its parent does not wait for it; a
  `stopping` block is given to the context after the body's statements, and several `onStopping`
  functions run in the order given; `children()` omits a child once it has ended, before its end is
  told to watchers. *Why*: each is the reading that never traps on a race and never leaves a parent
  waiting for ever. *Alternatives weighed*: `stop(self)` trapping as "not a child"; stopping an
  ended non-child trapping (needs a record of every child that ever ended); a child started from
  `stopping` made a top-level actor instead.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-lifecycle`): **Watching, the cases the chapter leaves open.**
  Watching an actor again replaces the earlier watch, so its end is told once; an actor watching
  itself is told nothing; `watch(ref, f)` takes a `@sendable` `f`, run on the pool where the end is
  known, its result queued; `unwatch` and the replacing watch drop a message already queued and not
  yet handled. *Why*: one message per watched end is what a program counting ends needs, and the
  dropped message is what makes `unwatch` mean something under a race. *Alternatives weighed*: a
  message per `watch` call; running `f` in the watcher's turn (the mailbox would hold functions
  rather than messages); watching itself trapping.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-lifecycle`): **Timers.** One thread, started with the first
  timer, keeps every actor's timers and sleeps until the earliest is due; timers fire in the order
  they fall due, those due together in the order they were set; a delay of zero or less is due at
  once; a repeating timer is set again an interval after it fires (it does not catch up after a
  stall), and an interval of zero or less traps; `cancel()` works from any thread, since a `Timer`
  is sendable, and drops a message of it queued and not yet handled. `Timer` has only `cancel()`.
  *Why*: a thread per timer would hold a worker per pending timer, and dropping queued messages
  makes a cancelled timer's count exact without timing. *Alternatives weighed*: a fixed rate that
  catches up; `every(zero)` sending as fast as it is handled; an `isActive()` query (left out until a
  program needs it).
- Settled on the user's delegation 2026-11-05 (built on branch `actor-lifecycle`): **An actor's state is never captured by a `@sendable`
  function**, whatever its fields: `Future(stop())` in an arm, or a body reading a `val` field, is
  refused with `` it uses the state of the actor `Escaper`, which only the actor's own code reaches;
  copy what it needs into a `val` first ``. Before, only a state with a `var` was refused. *Why*: the
  context's methods act for the actor that is running, so a state carried to another thread is never
  right to use there, and the static refusal says so where the runtime would trap. *Alternatives
  weighed*: keeping the structural rule and relying on the trap. **Review with the user**: it
  tightens a settled rule.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-lifecycle`): **The expected type bounds a contravariant type
  argument of a call's result only from below** ([generics](/spec/generics/#inference)). A call
  whose result is `Sink[T]` for a contravariant `T` takes `T` from its arguments, and from the
  expected type only where they leave it open, so `watch(spawn(Worker()), …)` and `val r:
  ActorRef[Nothing] = spawn(Quiet())` check. Before, the expected `ActorRef[Nothing]` made `M`
  `Nothing` and the argument was refused. *Why*: `Sink[A]` is a `Sink[B]` for every `B` below `A`,
  so the expected argument says nothing about `T` beyond a lower bound. *Alternatives weighed*:
  recording a lower bound on the variable (the checker's unknowns hold one type, not bounds).

- Settled on the user's delegation 2026-11-05 (built on branch `actor-ask`): **`Ended` means "not delivered", decided at the send.** An ask completes with `Err(Ended)` exactly
  where the asked actor was ending or had ended when the question was sent, so the message was
  dropped. A question that was queued and then dropped because the actor ended before handling it
  times out. Why: the chapter defines `Ended` as "the message could not be delivered", and an actor
  may legitimately forward the reply reference and end, the reply still to come from another actor;
  watching the asked actor's end for every ask would also hold a callback per ask on long-lived
  actors. Alternative: complete with `Ended` as soon as the asked actor ends before a reply (quicker
  failure for a question dropped from a mailbox, wrong for forwarded replies).
- Settled on the user's delegation 2026-11-05 (built on branch `actor-ask`): **An ask's reply reference** is an ordinary `ActorRef[R]` shown as `Reply#n`, numbered from one
  apart from the actors (so asks never disturb the actors' numbering), unequal to every other
  reference; its `ended()` completes (`Stopped`) once the ask has its answer or has timed out, and
  an ask *of* a used reply reference answers `Ended`. Why: it is a one-shot address, so it ends
  when it can accept nothing more. Alternatives: no identity of its own (sharing the asker's, which
  `main` and threads do not have), or numbering it among the actors.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-ask`): **The timeout is set once the question is delivered**; a timeout of zero or less times out at
  once unless the reply was already there. Alternative: trap on a timeout not more than zero as
  `every` does on its interval (a zero timeout is a legitimate "only if it is already answered").
- Settled on the user's delegation 2026-11-05 (built on branch `actor-ask`): **`adapter(f)`'s `f` runs where the message is sent**, so it is `@sendable`, and the adapter
  refers to the adapting actor: equal to its `self`, the same hash and display, the same `ended()`;
  a message sent to it once the actor has ended is dropped, and an ask of it answers `Ended`. Why:
  the mailbox holds the actor's own message type, so translating at the send keeps it so, and the
  chapter says a reference compares by the actor it refers to. Alternative: translate in the
  actor's own turn, which would let `f` read the actor's state but needs a mailbox of translations.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-ask`): **`pipeToSelf` of a future complete already sends at once**, so its message precedes any the actor
  sends itself after the call, and two such pipes arrive in the order they were made; an
  incomplete one sends from wherever the future completes, so `f` is `@sendable`. `T` carries no
  `Sendable` bound, the value never leaving the future's own handing-over. Alternative: always go
  through `onComplete`, leaving the order of an already-complete pipe and a later `tell` open.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-ask`): **A lambda's parameter of a known shape with an open type argument** (`Promise[T]`,
  `ActorRef[R]`) takes that shape, and the body decides the argument, as `p -> ReadLine(p)` decides
  `T` from `ReadLine`'s parameter; a body that leaves it open is reported at the parameter, once.
  A wholly unknown parameter type is still an error. Why: inference fails otherwise exactly where
  the explicit form succeeds, and `ask(r -> Get(r), …)` is the chapter's idiom. Alternative:
  require the type argument at every request-style call.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **How a trap is contained.** The runtime runs an
  actor's start, messages and `stopping` blocks under a return point (`_setjmp`), and a trap there
  restores the shadow stack, caller site and owner it recorded and returns to the turn
  (`_longjmp`). A count of regions in doubt on each thread (a lock held, which covers `Mutex` and
  `Once`; a `lazy val` initializing; an Avin function Sysl called) makes a trap inside one stop the
  program instead. *Why*: no unwinding tables are needed and the common path costs one `_setjmp`
  per turn. *Alternatives*: unwinding through every frame (needs landing pads in every function),
  or running each actor on a thread of its own (defeats many actors on few threads).
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **`Failed`'s reason is the trap's reason without its
  place** (`Failed(integer overflow)`), the place being in the report. *Alternative*: the whole
  report line, which ties the value to the file layout.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **Every trap in an actor's code is reported naming the
  actor, also where it stops the program** (a mutex held, a `Once`, a `lazy val`, a Sysl call), so
  the report says where the bug ran. *Alternative*: the plain `avin: trap:` line where the program
  stops, as a thread's.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **A stack overflow in an actor stops the program**, as
  in a thread, with `avin: trap: stack overflow`: the stack may run out inside the runtime or the C
  library holding a lock, so nothing can be said about shared state. *Alternative*: contain it with
  `siglongjmp` from the signal handler (unsafe where the fault came from inside a lock).
  **Review with the user**: the chapter lists no exception for it.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **An actor that escalates handles nothing more and
  waits for its parent's failure to stop it** (it ends `Failed`), rather than ending at once; a
  failure escalated to a parent that is already ending or restarting is dropped, since the parent
  is stopping its children anyway. *Why*: a failure escalated to the top then stops the program
  with no `stopping` block run anywhere, deterministically, and the parent's supervision decides
  the whole branch's fate. *Alternative*: the child ends on its own and tells the parent, which
  races the program's stop against the branch's `stopping` blocks.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **A restart also cancels the actor's watches** (its
  timers are cancelled as the chapter says), since the fresh state sets its own; its reference,
  number, parent and watchers stay, and a stop asked for before the restart is honoured after it.
  A restart is reported as `avin: restarting Counter#3`, after the trap's line. *Alternative*:
  keep the watches, whose messages the fresh state may not expect.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **`Restart`'s window counts restarts made within the
  last `within`; a limit less than zero or a window not more than zero traps in `spawn`**, as an
  `every` interval not more than zero does. *Alternative*: treat them as zero (a zero window would
  silently mean "restart for ever").
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **A trap in a `stopping` block ends the actor `Failed`**
  with that reason, or keeps the first reason where a trap had failed it already, asks no
  supervision (so a block that traps during a restart ends the actor), and runs no later
  `onStopping` function. *Alternative*: end it `Stopped` where it was stopping (hides the bug from
  its watchers).
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **A `pipeToSelf` function and a watch's function run in
  the receiving actor's own turn**, when it takes the message, rather than wherever the future or
  the end completed, so a trap in one fails the actor that asked for it and never an unrelated
  thread or the actor watched. The message of a pipe of a future complete already is still made
  at once, in the actor's code. This revises the earlier pipe decision's "sends from wherever the
  future completes". An adapter's function still runs at the send (its trap is the sender's), as
  settled. *Alternative*: leave them where the value is known, where a trap stops the program from
  a pool worker or fails the watched actor in the middle of ending. **Review with the user**: the
  adapter and the pipe now differ in where their function runs.
- Settled on the user's delegation 2026-11-05 (built on branch `actor-failure`): **`spawn`'s parameters are `(behaviour, onFailure,
  executor)`**, as the chapter writes them, so an executor alone is given by name
  (`executor = ex`). *Alternative*: keep the executor second.

**C bindings**

- Settled by the user 2026-11-05: **public C interoperability is no longer deferred.** The user's
  words, on how to build the MLT library: *"Doing it as a c binding would force c interoperability
  and would provide a good stressor for it. The sysl MLT binding isn't being used."* So Avin binds C
  libraries itself ([C bindings](/spec/c-bindings/)), and `avin-lang/mlt`, MLT 7 bound over them,
  is the stressor that drives the work (roadmap stage 13). Until it is built, C is reached through
  Sysl as before.
- Settled 2026-11-05 on the user's question *"Why should C interop go through Sysl since avin
  generates LLVM IR?"*: **a C binding is a direct call** ([how a C binding is
  compiled](/spec/c-bindings/#how-a-c-binding-is-compiled)). The emitter declares the C symbol in
  the LLVM IR and Avin's own code calls it; clang links the library with the flags `pkg-config`
  answers. What the Sysl shim does for a Sysl binding — converting arguments and results, the
  `enter_safe`/`leave_safe` bracket of a `blocking` call, keeping a handle's reference, callback
  trampolines and attaching a C-owned thread — is done by Avin's lowering and the runtime. *Why*:
  the shim exists so that Sysl checks a binding against a Sysl signature, and a C binding has none,
  so a shim would add a compile step, a cache and a Sysl dependency and check nothing. *Alternative
  weighed*: lowering each C binding into the Sysl shim as an `extern` (the director's first
  recommendation), which would have had Sysl's ABI lowering of aggregates for free but no check
  against C's header, since Sysl does not read headers.

- Settled on the user's delegation 2026-11-05 (built on branch `c-interop-design`): **C bindings** ([chapter](/spec/c-bindings/)).
  Each is the recommendation the chapter is written to:
  - **The binding copies the C prototype as a string**, `@c("char *mlt_properties_get(mlt_properties
    self, const char *name)")`, beside an Avin signature in Avin types, matched by position. *Why*:
    C's `unsigned`, `long`, `const` and typedefs cannot be said in Avin types, the prototype is what a
    reader compares with the header, and clang can check it exactly. Three marker words (`out`,
    `inout`, `userdata`) and `length(p)` say what C's types do not. *Alternatives*: a vocabulary of C
    types in Avin (`CInt`, `CSize`, …), a second type system in safe code; C types implied by the
    Avin types, which cannot spell a `uint32_t` or a typedef.
  - **Every binding is checked against its header, by clang, in a generated C unit**: a
    `_Static_assert` of `__builtin_types_compatible_p` per binding, the kind of each typedef, every
    mirror's layout; the unit also measures typedef widths, enumerators and constants as object-file
    data read without running it (cross builds measure the target). A package with bindings and no
    `headers` is refused. *Why*: it is the one check on the trusted surface that can be automatic.
    *Alternatives*: trusting the declaration as a Sysl `extern` does; parsing headers in the
    compiler (the Sysl project refused that too).
  - **C libraries are named in the manifest**, `c-libraries { mlt { pkg-config, headers, install } }`
    (or `link = [...]`), and every program links the C libraries of all its packages. *Why*: it is
    the shape of `sysl-dependencies`, and a library's requirement travels with it. *Alternative*: a
    module-header `@link`, as Sysl writes it, which cannot carry headers or the install message.
  - **Struct by value goes through clang-compiled glue**, which takes and answers the struct by
    pointer; scalars, pointers and structs by pointer are called directly. *Why*: clang's ABI
    classification is right on every target at once, and the C unit already exists. *Alternatives*:
    implementing AAPCS64 (Darwin first) and then System V x86-64 classification in lowering, a
    per-target body of code whose bugs corrupt arguments silently; refusing by-value structs, which
    MLT's `mlt_color` and `mlt_event_data` need. **Review with the user** if the extra call is ever
    measured to matter.
  - **Handles own one C reference**: a pointer answered is owned unless the binding says
    `borrowed`, which needs a `retain`; `release`/`retain` are a C function or a C expression in
    `self`; `as:` lists the other C types a handle passes as (C's first-member inheritance), each by
    a C conversion that lends; `keeps:` makes the holder keep an Avin object (MLT's profile),
    `consumes:` closes a handle the call takes. *Why*: the sysl MLT binding found exactly these
    shapes, and every one is written once per class or binding. *Alternative*: one Avin handle class
    per C type with explicit upcast bindings, many more declarations for the same check.
  - **C handles keep the single-owner check**; `shared` exempts a thread-safe C object and makes it
    sendable. *Why*: most C objects are not thread-safe. *Alternative*: no check for C handles.
  - **Strings**: `const char *` from a `string` copied with a NUL, a held NUL trapping; a `char *`
    result copied and UTF-8-checked (trapping), `Array[byte]` for unchecked bytes, borrowed unless
    `free:` names the freeing function. *Alternatives*: truncating at a NUL, replacing invalid UTF-8
    with U+FFFD (what standard input does), a `Result` per string result.
  - **Null is `Option`; a null bound as the type itself traps.** A C integer bound as
    `Result[unit, int]` is `Ok` on zero. *Alternative*: a `Result` for null pointers, which would need
    an error the C call never gave.
  - **Out parameters join the result**, as a tuple after the C result or in a status's `Ok`. *Why*:
    Avin has no pointers to pass. *Alternative*: filled one-element arrays, which work today but read
    badly.
  - **Buffers are copied (`bytes:`) or borrowed as an `avin.c.ByteView`** (`view:`), a read-only,
    bounds-checked handle holding its owner, **closed when its owner is next used**. *Why*: MLT may
    replace a frame's image on the next call, so validity is "until the owner changes", and the owner
    check catches every use after that. *Alternatives*: views valid only inside a block
    (`frame.withImage { … }`), which forbids keeping one at all; copy only, which costs a copy per
    frame. Writable views are proposed below, from the Cairo survey.
  - **Callbacks**: one C-convention trampoline per function type, the closure found through the
    `userdata`, the runtime's Sysl-callback record and `callback_enter` reused; `keeps:` holds a
    registered callback on its handle, whose `forget:` unregisters each before `release`; a
    keeping class without `forget` is refused; `@threaded` as for Sysl, a C thread attached per call.
    *Alternative*: a registration object returned to the caller, which the user must keep alive.
  - **Varargs are refused.** A package needing one may carry C. *Why*: an argument's type cannot be
    checked against `...`, and a format string from Avin is a hole. *Alternatives*: allowing calls
    whose extra arguments the prototype fixes. Whether an Avin package may carry C sources of its
    own, compiled into the C unit, is itself proposed: it is the trusted escape hatch for macros with
    arguments and odd shapes, and it widens the trusted surface beyond `@c(`.
  - **No pointer type and no `unsafe` block in Avin; the `@c` declarations (and the glue they
    generate) are the whole trusted surface.** *Why*: soundness in safe code, and one grep finds
    everything to review. *Alternative*: an `unsafe` region with a raw pointer type, which would let
    a binding's pleasant layer be written in Avin but makes every Avin file a possible hole.
  - **`errno`** is not read by a binding yet; an `errno` flag answering `Result[T, int]` from a `-1`
    result is the likely form when a POSIX library needs it.
  - **Review with the user**: the prototype-string form of `@c`, the largest syntax choice here; whether a
    package may carry C sources, which widens the trusted surface beyond the `@c` lines; the two
    parser additions (`val` with no initializer for `@c(const:)`, list-valued annotation
    arguments); and a missing `@threaded` on a callback C calls from its own thread being caught
    only by a trap at run time.

- Settled on the user's delegation 2026-11-05 (built on branch `c-bindings-scalars`): **C bindings of scalars** (stage 13 step 1), the choices
  building it made where the chapter is silent.
  - **A typedef's kind is decided by the Avin type and checked by the C unit** with
    `__builtin_classify_type`: an integer type for an integer, an integer one byte wide (of either
    sign, as a C `char` is) for `byte`, `bool` for `bool`, a floating-point type for `real`, its
    width measured to choose `float` or `double`. *Why*: the checker cannot see the header, and the
    Avin type says what the binding means. *Alternatives*: a kind written in the prototype; measuring
    every typedef's kind and refusing in the driver, which needs an expression valid for any type
    and C has none.
  - **`unsigned char` crosses as `byte` or as an integer type, `char` (and a `char` typedef) only
    as `byte`, `signed char` only as an integer type.** *Why*: the table lists `unsigned char` in
    both rows and `char` only as a byte. *Alternative*: `char` as an integer at its measured sign,
    which makes a program's meaning depend on the machine.
  - **An argument's trap names the C type, `value out of range for u64 (size_t)`; a result's names
    the Avin type, `value out of range for long`**, as a Sysl binding's do. *Alternative*: naming
    both types in both directions.
  - **A result the binding declares `unit` is declared `void` in the IR**, so a discarded pointer or
    integer needs no measurement; a struct result is refused even then, since it may be returned
    through memory. *Alternative*: declaring the measured type and ignoring the value.
  - **The measurements are found by a marker in the object's bytes**, one table of two bytes per
    type (width, sign), rather than by its symbol, so no object format (Mach-O, ELF, COFF) is parsed.
    *Alternative*: reading the symbol table, a reader per format.
  - **The mismatch diagnostic takes its two types from a pointer of the prototype's type set to
    the header's function**, whose clang diagnostic keeps the typedefs' names, and the header from
    the note of a redeclaration as the prototype says it; the `_Static_assert` stays the check, so a
    clang that only warns on the pointer still refuses the binding. *Alternative*: the assertion's
    own text, which clang writes with every typedef expanded.
  - **The toolchain names the `pkg-config` it asks** (`Toolchain.pkg_config`), so a build for
    another machine can ask that machine's, and the corpus asks one that also finds the C fixture in
    `tests/c`, which it builds. *Alternatives*: the environment's `PKG_CONFIG_PATH`, which a test
    cannot set for the compiler it runs in its own process; a package-relative `.pc` path in the
    manifest, which the chapter does not have. **Review with the user**: whether `avin` should read
    `PKG_CONFIG` as autotools does.
  - **A function a header declares `static inline` is not caught by the header check**: C lets the
    unit's redeclaration inherit the internal linkage, so the binding fails at the link, where the
    linker names the missing symbol. *Alternative*: reading the C unit's symbol table for a local
    definition, a reader per object format.

- Settled on the user's delegation 2026-11-05 (built on branch `cairo-survey`): **C bindings, from the Cairo survey** ([chapter](/spec/c-bindings/)). The user asked for `avin-lang/cairo`, Cairo 1.18 bound over the C bindings beside
  `avin-lang/mlt`, ported from the reference binding's surface. Mapping it onto the chapter found these
  lacks; each is the smallest addition that covers it:
  - **An integer bound as `bool`** is `false` for zero, `true` otherwise; a `bool` passes `0`/`1`.
    *Why*: `cairo_bool_t` is an `int`, and so is every pre-C99 library's truth value. *Alternatives*:
    binding it as `int` and comparing in Avin at every call site; a `truth` marker word in the
    prototype, which says what the Avin type already does.
  - **A status that is a C enum binds as `Result[unit, E]`** with `E` its mirror: `Ok` for the zero
    enumerator, `Err(variant)` otherwise; the mirror keeps its zero variant, which a callback answers.
    *Why*: Cairo's `cairo_status_t` is the error type itself, and mapping an `int` back to it by hand
    repeats the enum. *Alternatives*: `Result[unit, int]` and a mapping function; requiring the mirror
    to omit its zero variant, which a callback answering `Success` could not then name.
  - **Latched errors need nothing new**: the status query is an ordinary binding, and the library
    asks it after constructors, before output (`writePng`, `finish`, the PNG stream) and on request,
    answering `Result[_, CairoError]` there and `unit` from the drawing calls. *Why*: Cairo latches
    errors on the object and makes every later call a no-op, so the next query sees every failure.
    *Alternatives*: a per-class `status:` expression the compiler checks after every call, trapping
    or answering `Result` from all ~150 drawing calls, which costs a call each and makes every line of
    drawing code carry a `?`; trapping on any latched error, which turns a bad PNG file into a crash.
    **Review with the user**: whether drawing calls should trap on a latched error in a debug build.
  - **`attach:` ties what a handle keeps to the C object's lifetime**, through the library's
    destroy notification (`cairo_surface_set_user_data`, `mlt_properties_set_data`): the kept
    objects move to a pinned runtime record that C's `destroy` releases; `key:` names the static key
    type the C unit defines per class. *Why*: Cairo contexts and surface patterns reference their
    surface, so the Avin `Surface` can be collected while C still draws into the `ByteBlock` (or font
    file) it was made over — a use after free that `keeps:` on the handle does not prevent. The same
    gap reaches MLT wherever a C object that keeps a pointer (a producer keeping its profile) is held
    by another C object (a playlist) after its Avin handle is collected. *Alternatives*: keeping the
    kept objects for the rest of the program, a leak per surface; refusing `keeps:` on handles C can
    reference, which refuses `create_for_data`; making the library hold every surface in an Avin
    table. **Review with the user**: whether the MLT `keeps:` bindings should move to `attach:` too.
  - **Field accessors take expressions and bounded indices**: `field:` a C lvalue in `self` and the
    accessor's parameters, `bound:` the C expression an index is checked below; an index with no bound
    is refused. *Why*: `cairo_path_t` is a C-owned array of a union, which no mirror may hold, and
    reading it element by element through glue keeps the union out of Avin. *Alternatives*: an
    opaque value per element, which needs by-value glue for every read; a package-carried C file
    flattening the path into doubles, the trusted escape hatch proposed above, wider than one line.
  - **`Option` of a mirror for a nullable `const T *`.** *Why*: `cairo_recording_surface_create`
    takes null for unbounded extents. *Alternative*: two bindings, one passing a sentinel struct.
  - **Upper-case enumerators match an upper-case prefix** (`CAIRO_FORMAT_` → `Argb32`). *Why*:
    most C libraries write enumerators in upper case. *Alternative*: `@c("name")` on every variant.
  - **String and `real` constants**: `@c(const:)` reads a string literal's bytes as a measurement,
    checked as UTF-8. *Why*: Cairo's tag names (`CAIRO_TAG_DEST`) are string macros. *Alternative*:
    copying the literals into Avin, which drifts if the header changes them.
  - **Arrays of mirrors** go to `const T *` converted into C memory for the call; **arrays C
    allocates** are copied out with `array:`, `length:` and `free:` (generalizing `bytes:`). *Why*:
    Cairo's glyph arrays, in (`cairo_show_glyphs`) and out (`cairo_scaled_font_text_to_glyphs`).
    *Alternatives*: a handle on `cairo_glyph_allocate`'s array with indexed writers, a C-shaped API
    in Avin code; writing a converted array back for non-`const` pointers, which no Cairo call needs.
  - **`view` on the result, and writable views**: a bare `view` names the C result as the memory;
    `writable` answers an `avin.c.MutByteView` (a `ByteView` with `update`, `fill`, `write`),
    closed by the owner's next use like any view. *Why*: Cairo's pixel access is
    `cairo_image_surface_get_data`'s result, and the reference program writes pixels and calls
    `mark_dirty`, which, being a use, closes the view so a write after it traps. *Alternatives*:
    copy out, edit, copy back with `create_for_data` (a full copy per edit); a block-scoped
    `withPixels { … }`, which the chapter already weighed against for reading; one `ByteView` type
    with a run-time writable flag, which moves a type error to run time. A `stable` marker for
    owner methods that do not close views (Cairo's `get_width` after `get_data`) was weighed and left
    out: the library reads those first.
  - **`avin.c.ByteBlock`, memory Avin gives C**: fixed-length, zeroed, outside the collected heap,
    never copied at a call, read and written like a `MutByteView`, freed when collected, kept by C
    through `keeps:` (and `attach:`). *Why*: `cairo_image_surface_create_for_data` keeps the pointer
    and writes into it for the surface's life, which neither a copied array nor a heap array (moved
    out of the heap for `blocking` calls) can be. *Alternatives*: pinning an `Array[byte]` in the
    collector, which pins the collector's design to non-moving arrays; leaving `create_for_data`
    unbound (the reference program uses it).
  - **Memory lent to a callback is a view** closed when it returns, `const` a `ByteView`, otherwise a
    `MutByteView`, its length from a `length(p)` beside it; a function-pointer typedef is written out
    in the prototype. *Why*: Cairo's PNG write and read streams pass `(closure, data, length)`.
    *Alternatives*: a copied `Array[byte]` per chunk (fine for writing, impossible for reading, where
    the callback must fill C's buffer).
  - **Steps 8 and 9 of stage 13 swap**: buffers before callbacks, so callbacks can take views.
    *Why*: the PNG stream needs both, and neither MLT step needs the other. *Alternative*: callbacks
    first with copied arrays, the read stream deferred.
  - The survey's three questions, settled with it. **`attach:` for MLT**: only where C keeps
    something Avin owns that C's own reference count does not cover (memory Avin gives C, a callback's
    closure); MLT's playlists, tractors and services take their own reference to what they are given,
    so a counted handle needs nothing more, and the MLT steps are audited for the other cases.
    **Review with the user.** **No trap on a latched error in debug builds**: a program behaves the
    same at every optimization level; the binding checks the status after constructors and before
    output, as the chapter says. **A FreeType binding** (`avin-lang/freetype`) waits until Cairo's
    font-face step needs it; the toy font API and the platform's own font backend come first.

- Settled on the user's delegation 2026-11-05 (built on branch `freetype-design`): **C bindings, from the FreeType survey** ([chapter](/spec/c-bindings/)). The user: *"Yes we'll need a freetype binding to work with cairo."*
  The user (2026-11-05): *"Yes we'll need a freetype binding to work with cairo."* A third stressor,
  `avin-lang/freetype`, binds FreeType 2 (`pkg-config freetype2`) to the reference binding's surface,
  and `avin-lang/cairo` depends on it for Cairo's FreeType font faces (`cairo-ft.h`). This replaces
  the Cairo survey's "a FreeType binding waits until Cairo's font-face step needs it". Mapping the
  reference surface onto the chapter found these lacks; each is the smallest addition that covers it:
  - **A binding may name a handle class of another package** its package depends on: the prototype
    names the class's own C type or an `as:` type; the class's glue is the declaring package's,
    compiled once; the package's bindings are checked after the `headers`, and with the `pkg-config`
    flags, of every package whose handle classes they name, in dependency order; ownership (lent,
    `keeps:`, `consumes:`, `attach:`, owned and `borrowed` results) crosses unchanged. *Why*:
    `cairo_ft_font_face_create_for_ft_face` takes FreeType's `FT_Face`, and a header of one library
    need not include the other's (Cairo's happens to; most do not). *Alternatives*: a third package
    holding only the shared handle class, which the reference binding needs only to break a
    dependency cycle Avin does not have; Cairo declaring its own `FT_Face` handle, two handles with
    two owners and two `release`s on one C object, a double free; trusting each library's header to
    include the other's.
  - **A header may be named by a macro**: an entry of `headers` that is a C identifier is emitted
    as `#include NAME`, defined by an earlier entry, and a macro no earlier header defines is
    reported at the entry. *Why*: FreeType documents its headers by macro (`#include <ft2build.h>`
    then `#include FT_FREETYPE_H`), and Cairo's own `cairo-ft.h` includes it so. *Alternatives*:
    naming the path the macro expands to (`<freetype/freetype.h>`), which the installed FreeType's
    own `ft2build.h` shows and which needs nothing new, but writes into the manifest a layout that
    FreeType's macros exist to keep its users out of; a `prelude` string of raw C lines in the manifest, wider than
    one rule and the start of a C file in HOCON.
  - **A kept handle whose class declares `retain` is held by a C reference**, taken through that
    `retain` when the call succeeds and given back through its `release` after the holder's own
    `release`. *Why*: the collector finalizes the unreachable in address order, so a `Face` and the
    `Library` it keeps, dead in one collection, may be finalized library first; freeing the library
    frees every face still in it, and the face's `FT_Done_Face` then frees one twice. With the
    reference, the library's `release`, `FT_Done_Library`, only drops a count (`FT_Done_FreeType`
    also frees the memory manager whatever the count, so it is no `release`). MLT's `Consumer` keeping its `Playlist`
    (`keeps: "source"`) gains the same order. *Alternatives*: ordered finalization in the collector
    (kept before keeper), which Avin cannot change and which the collector's address-order sweep
    does not offer; holding the Avin object and taking the order on trust, a use after free whenever
    a `release` touches what it keeps; making the library's `release` a no-op while faces exist, a
    count in Avin of what C already counts.
  - **`kept: "name: Class"` lets a holder's `release:`/`retain:` name what it keeps**, as that
    handle's C pointer; the class is made only by bindings keeping exactly one handle of that class,
    and an unused name is refused. *Why*: `FT_Done_MM_Var(library, master)` frees the variation
    descriptor through a library the descriptor does not record; the face it came from publishes it
    (`face->glyph->library`), and the C reference above keeps that face valid until after the
    `release`. *Alternatives*: walking the descriptor into Avin data and freeing it inside one
    binding, which needs a binding that copies a C struct holding arrays of structs (a larger
    addition); a `release` taking a second argument from a per-handle C field the runtime stores,
    the same thing with a less readable spelling; leaving the variation API unbound.
  - **`keeps:` takes a list** (`keeps: ["library", "base"]`). *Why*: `FT_New_Memory_Face` keeps its
    library and its memory. *Alternative*: repeating the key, which HOCON merges rather than lists.
  - **`keeps:` naming an array parameter is refused**, since its C copy lives for the call; memory C
    keeps is a `ByteBlock`. *Why*: `FT_New_Memory_Face` reads its buffer lazily for the face's
    life. *Alternative*: pinning the array, which the `ByteBlock` decision already weighed against.
  - **`length(p)` may mark an `inout` pointer to an integer**: the array's length is written in, C's
    answer is part of the result, checked to be no more than the length; `None` for an `Option` of
    the array passes null and zero. *Why*: `FT_Load_Sfnt_Table` takes `FT_ULong *length`, and is
    called once with no buffer to learn a table's size, then again to fill one. *Alternatives*: an
    `inout` length the caller passes, which lets the length and the array disagree (a read past the
    end); a `ByteBlock` sized by a first call, the same disagreement.
  - **A field accessor may answer a `view`** of a byte-pointer field, `length:` a C expression in
    `self`. *Why*: the glyph bitmap is `face->glyph->bitmap.buffer`, `|pitch| * rows` bytes the next
    load replaces; the view's closing on the face's next use is exactly FreeType's rule.
    *Alternatives*: `bytes:` on a field, a copy of every glyph before the library walks it, which the
    library could still choose; a glue function per field answering the pointer, the same with more
    C. The view's bounds check catches the reference binding's `row * pitch` indexing, which reads
    before the buffer when `pitch` is negative.
  - **Cairo's FreeType font face is an `attach:` class keeping the `Face`.** Cairo keeps the
    `FT_Face` without a reference, documents that it must outlive every font face made from it, and
    asks for it to be given back from `cairo_font_face_set_user_data`'s destructor; contexts and
    scaled fonts reference the font face, so `keeps:` on the Avin handle is not enough. The destroy
    unpins the record and the `Face` is then finalized as usual. *Alternative*: a destroy that calls
    `FT_Done_Face` itself, from whatever thread drops the last reference, while an Avin `Face` may
    still be in use: FreeType's face count is not atomic.
  - **`FT_Error_String` binds as `Option[string]`**, and the library carries its own names for the
    codes, as `@c(const:)` values of `fterrors.h`'s `FT_Err_*`. *Why*: Homebrew's FreeType is built
    without `FT_CONFIG_OPTION_ERROR_STRINGS`, so the function answers null there.
  - **The test font is Kaisei Decol Regular**, from the reference repository, under the SIL Open
    Font License 1.1 (Copyright 2020 The Kaisei Project Authors, no Reserved Font Name), bundled
    with its `OFL.txt` in the package's tests. *Alternative*: a smaller OFL Latin font (4.4 MB is
    large for a test fixture), at the cost of a font the reference program's output was not made
    with. **Review with the user**: the fixture's size.
  - Left out: `cairo_ft_font_options_substitute`, which takes a fontconfig `FcPattern *` and waits on
    a fontconfig binding; `FT_Outline_Decompose` and the other callback APIs, which the reference
    binding does not reach.
  - The survey's two questions, settled with it. **The test font is a small OFL font**, not Kaisei
    Decol Regular (OFL 1.1 and usable, but 4.4 MB): a Latin face of a few hundred kilobytes, its
    licence file beside it in the package's tests, keeps the repository light, and the reference
    binding's outputs that need Kaisei's glyphs are reproduced with the small face's instead.
    **`cairo_ft_font_options_substitute` waits on a fontconfig binding**: it takes a fontconfig
    pattern, and nothing else in `cairo` or `freetype` needs fontconfig. The macro-named headers and
    `FT_Error_String`, which the survey placed in step 2, ride step 3, since step 2 was already being
    built.

- Settled on the user's delegation 2026-11-05 (built on branch `c-bindings-strings`): **C bindings of strings and truth values**
  (stage 13 step 2), the choices building it made where the [chapter](/spec/c-bindings/#strings) is
  silent.
  - **A C string is exactly one pointer to a plain `char`**, `const` when the qualifier comes before
    the star (`const char *`, `char const *`); a qualifier after it (`char *const`) is the pointer's
    own and changes nothing. `signed char *`, `unsigned char *`, `void *` and `char **` are other
    pointers. *Why*: those are the types C's string functions use, and the header check catches any
    other spelling. *Alternative*: taking `unsigned char *` as a string too, which libraries use for
    bytes, not text.
  - **A typedef crossing as a string is checked by the C unit** to be `char *` or `const char *`
    as a result, and `const char *` as a parameter (`__builtin_types_compatible_p`), and is not
    measured. *Why*: the same rule the chapter gives a spelled `char *` parameter, which C may write.
  - **A typedef crossing as `bool` may be a C `bool` or an integer type** (checked with
    `__builtin_classify_type`), and is passed as the integer it measures; a C `bool` typedef is then
    one byte holding 0 or 1, the same registers as an `i1`. An integer keyword crossing as `bool`
    may be any but a plain `char`, which crosses only as `byte`. *Why*: the checker cannot tell a
    `bool` typedef from `cairo_bool_t`, and one measured path serves both. *Alternative*: a marker
    word for a truth value, which the Cairo survey weighed and set aside.
  - **The traps' words**: ``the string passed to `s` of `length` holds a NUL at byte 3`` (the
    chapter's), `` `fixture_invalid` answered a string that is not UTF-8 at byte 2`` (the offset is
    where the first ill-formed sequence starts, as `from_utf8` reports it; overlong forms,
    surrogates, values past U+10FFFF and a cut-off sequence are all ill-formed), and
    `` `fixture_maybe` answered null`` (the chapter's). A parameter declared with a pattern rather
    than a name is named `parameter N`. *Alternative*: naming the binding rather than the C function
    in the result traps; the C function is what answered.
  - **A string argument's copy is freed after the result is converted**, not when the call
    returns, since C may answer a pointer into a string it was passed (`strchr`, `strstr`); the
    copies are made with `malloc` by the runtime (`avin_rt_c_string_in`) and freed by
    `avin_rt_c_string_free`. A non-`blocking` call cannot collect, and the collector never moves an
    object, but the copy is still needed for the NUL. *Alternative*: one buffer per thread reused
    across calls, which a callback calling the same binding again would overwrite.
  - **`free:` accepts a function the headers declare as `void (void *)`, `void (char *)`,
    `void (const char *)` or `void (<the result's type>)`**; anything else, including one answering
    a value, is refused at the binding, as is a name no header declares. It is called only for a
    non-null answer, after the copy is made, and **a binding whose result is `unit` with `free:`
    frees what C answered and discards it**. `free:` on a binding that answers no string is refused
    at check. *Why*: a free function that answers a status is not one a binding can ignore, and a
    library's own free functions are seldom safe on null. *Alternative*: refusing `free:` on a
    `unit` result, which would leave no way to call such a function without leaking.
  - **An `Array[byte]` result may be an `Option` too**, null being `None`, and a null bound as a
    bare `Array[byte]` traps as a bare `string` does. **An `Array[byte]` parameter is refused** until
    [arrays](/spec/c-bindings/#arrays-and-out-parameters) are built. *Alternative*: an empty array
    for null, which hides the difference C makes.

  **Review with the user**: whether `free:` on a `unit` result should stay legal.

- Settled on the user's delegation 2026-11-05 (built on branch `c-bindings-handles`): **C handles** (stage 13 step 3), the choices
  building them made where the [chapter](/spec/c-bindings/#handles) is silent.
  - **`shared` is a bare word of the class's `@c`**, `@c(type: …, release: …, shared)`. *Why*: the
    `@c(` lines are the whole trusted surface, and a thread-safety claim is one of them; no class
    modifier says it today. *Alternative*: a `shared class` modifier, a keyword for one use.
  - **A handle's method is bound by a prototype taking the receiver first, named `self`**, of the
    class's C type (or that type `const`); the method's parameters follow by position. *Why*: the
    chapter's every example writes it so, and the name makes the receiver visible in the C text.
    *Alternative*: the receiver at any position named `self`, which no surveyed library needs.
  - **What keeps is the handle the binding answers, else a method's receiver**, and a method
    answering a handle may name `self` in `keeps:` to have the new handle keep the receiver.
    Nothing is kept unless the call succeeded: a handle answered (not null), or a zero status.
    *Why*: a library object made by a method of another usually depends on it (a face's variation
    descriptor). *Alternative*: the receiver always keeps for a method, which leaves the made
    handle unprotected.
  - **`consumes:` closes the handle passed whatever the call answers, and may name `self`**; the
    consumed handle keeps what it kept until it is collected or closed. *Why*: C functions that take
    a reference take it unconditionally, and giving back what the consumed object keeps while C
    still holds the object would be a use after free. *Alternatives*: closing only on success,
    which most taking functions do not distinguish; releasing what it keeps at once.
  - **A holder keeping a handle by a C reference also takes one to each C object that handle so
    holds**, after it, given back after it. *Why*: found under the collector's stress in the
    fixture: a variation descriptor keeps a face, the face's own handle is collected first and gives
    back its reference to the library, and the library is freed under a face the descriptor's
    `release` reaches through. *Alternatives*: holding the kept handle as the Avin object as well,
    which orders nothing within one collection; refusing chains of kept handles, which FreeType's
    API is.
  - **The class `kept:` names declares `retain` and no `kept:` of its own**, and is found in the
    holder's module or by its module and name (`freetype.Face`); `retain:` sees the name only where
    it takes a `borrowed` result's reference, as the pointer of the handle the binding keeps; a
    holder keeping a handle whose class has `kept:` holds it as the Avin object. *Why*: the name is
    a pointer only while a C reference holds it, and a class with `kept:` cannot be given back from
    its pointer alone. *Alternative*: storing every kept name in the C reference's record, which
    then has to hold references of its own, the same chain again.
  - **A status's code is any integer type**, converted with the result's range check
    (`Result[unit, long]` for `long long`; a `long long` code past `int` traps for
    `Result[unit, int]`). *Alternative*: `int` only, which loses a `long` status's code.
  - **The C unit includes a package's headers after those of every package whose handle classes
    its bindings name**, and otherwise in the order the libraries are listed, the program's own
    first. *Why*: that is the only order the header check depends on, and it is read from the
    bindings themselves. *Alternative*: a topological order of the whole package graph, which
    reorders headers no binding needs reordered.

  **Review with the user**: `shared` as a word of `@c` rather than a modifier, and the chain of C
  references a kept handle brings with it.

- Settled on the user's delegation 2026-11-05 (built on branch `c-bindings-fields`): **`as:` conversions and field accessors** (stage 13
  step 4), the choices building them made where the [chapter](/spec/c-bindings/#handles-and-c-s-inheritance)
  is silent.
  - **An `as:` entry's right side is a C function's name or an expression in `self`**, as
    `release:`'s is, and `as:` is one string or a list. The C unit checks with
    `__builtin_types_compatible_p` that what it makes is the entry's type. *Why*: one rule for every
    piece of glue, and C would otherwise convert a wrong pointer silently (the C unit compiles with
    warnings off). *Alternative*: an expression only, which makes MLT's every entry repeat `(self)`.
  - **A conversion is made after the closed and owner checks, from the handle's own pointer, and
    passes null as null**; what a binding `keeps:` or `consumes:` is the handle itself, held by its
    class's own `retain`, never the converted pointer. *Why*: the converted pointer is lent and may
    point into the object, so a reference taken through another class's glue would be the wrong
    count. *Alternative*: holding the converted pointer, which has no `release` of its own.
  - **A field is a member path read as `self->` followed by it**; its type is named in the C unit by
    `__typeof__` with the indices replaced by zero, so it is measured and checked without being
    read. *Why*: every surveyed accessor (MLT's profile, FreeType's face and glyph slot, Cairo's
    path) is a member path, and an unevaluated operand needs no index. *Alternative*: a full C
    expression in `self`, which nothing surveyed needs and which makes `self->` noise in each.
  - **Only scalars are written**; a string or handle field is read only. *Why*: a string or a
    reference stored into a C struct needs an owner that frees or gives it back, which no surveyed
    library asks Avin to be. *Alternative*: writing strings copied with `malloc` and never freed.
  - **Indices are `int` or `long`, passed as `long long`, one `bound:` each, in order**, a list for
    more than one; a bound may use `self` and the indices. The check is `0 <= index < bound`, made
    in Avin before the glue reads, trapping with "index `i` of `Class.method` out of range". *Why*:
    a two-dimensional field needs a bound per index, and the trap names the index that was out of
    range. *Alternative*: one bound for all indices.
  - **A handle read from a field is lent, without `borrowed` written**, and its class must declare
    `retain` and no `kept:`. *Why*: a struct's pointer is never the reader's to give back.
    *Alternative*: requiring `borrowed`, a word that could only ever be written one way.

  **Review with the user**: writers of scalars only.

- Settled on the user's delegation 2026-11-05: **[Dates and times](/spec/time/)** (branch
`datetime-design`), the choices the design makes beyond the user's settled entry. Where the slate design leaves a name open, the names follow the
widely used date/time library family of Avin's reference standard library ecosystem (`atZone`,
`plusDays`, `withYear`, `toInstant`, `isBefore`, `toMinutes`), except where a slate or a simpler
spelling is better, as each entry says.

- **Representation and ranges.** Every type but `Zone` is a `data` type whose fields are its
  representation (`Duration(micros)`, `Instant(epochMicros)`, `Date(epochDay: int)`,
  `Time(microsOfDay)`, `DateTime(date, time)`, `Zoned(dateTime, offset, zone)`,
  `Period(months, days)`); the makers check and the constructors take fields as given. A `Duration`
  and an `Instant` span a whole `long`, a `Date` a whole `int` of days, and what leaves a range traps.
  *Why*: data types are unboxed values, and every field value of `Duration`, `Instant`, `Date` and
  `Period` is meaningful, so those constructors are total. *Alternatives*: one common range for every
  type (±290,000 years), cleaner to state, but enforceable only with constructors the module keeps
  to itself, which Avin does not have; classes with private state, boxed and with identity. A
  `Time` or `Zoned` built through its constructor with fields no maker would give is the cost.
  **Review with the user**: a module-private constructor would close it.
- **Days and weeks are exact durations; a calendar day is `plusDays(n)` or `Period(0, n)`.** The
  settled list makes `days` and `weeks` durations, as Sysl 0.10.0's are, so `Period` holds months
  and days (Sysl's `Period(months, days)`), `months` and `years` make it, and `Date + Duration` does
  not compile (`date.plusDays(1)`). On a `Zoned`, `z + days(1)` is 24 hours and `z.plusDays(1)` the
  same wall clock a day later. *Why*: the user's list and Sysl agree, and the exact/calendar line
  stays in the types. *Alternatives*: slate's implementation, where `days` and `weeks` make periods
  so `zoned + days(1)` keeps the wall clock, which contradicts the settled list. **Review with the
  user**: the two spellings answer differently across a daylight-saving change.
- **Names where slate leaves them open, and where slate's are kept.** `t.atZone(z)`, `d.atTime(t)`,
  `t.atDate(d)`, `d.atStartOfDay()`, `zt.toInstant()`, `plusYears`/`plusMonths`/`plusWeeks`/
  `plusDays` (no `minus` forms: a negative count), `withYear`/`withMonth`/`withDay`/`withHour`…,
  `isBefore`/`isAfter`/`isSameInstant`, a duration's `toHours()` (whole) and `toHoursPart()`, a
  date's `daysUntil`/`monthsUntil`/`until`. Kept from slate as better: `weekday()` (shorter, and the
  name of its type) over `dayOfWeek`, `day()` and so `withDay` over `dayOfMonth`, `onOrAfter(w)` and
  `onOrBefore(w)` over adjuster objects, `parseTimestamp`, the `epoch*` makers. Slate's single `at`
  is spelt `atZone`/`atTime`/`atDate`, since one overloaded word hides which conversion is meant and
  `atZone` names what can fail. `startOf("month")` becomes `startOfMonth()` and its siblings, so a
  date's `startOfHour` is a compile error rather than a run-time one. *Alternatives*: slate's
  `d.minutes()` for whole minutes (the same word makes and measures), `dayOfMonth`.
- **Choosing an instant for a skipped or repeated reading.** `dt.atZone(z)` answers
  `Result[Zoned, ZoneTimeError]` (`Skipped`, `Repeated`), and `atZoneEarlier(z)`/`atZoneLater(z)`,
  Sysl's `at_earlier`/`at_later`, always answer: the earlier or later instant of a repeated hour, and
  in a skipped hour both push the reading past the gap by its length. A period on a `Zoned`, a
  `with…` and `startOfDay` push past a gap and, in a repeated hour, keep the offset the reading had
  where it is one of the two and otherwise take the earlier. *Why*: what the binding gives, and the
  established rule. *Alternatives*: slate's string choice (`"earlier" | "later" | "after"`), a
  `Pick` enum argument.
- **A `Zoned`'s equality and order.** `==` needs the same instant in the same zone; `Ord` orders by
  instant, then wall reading, then zone name, so it agrees with `==` and a `TreeMap` keeps two
  readings of one instant; `isBefore`, `isAfter` and `isSameInstant` compare instants alone.
  *Alternatives*: slate's, where `<` compares only the instant and so is false both ways for two
  zones' readings of one instant, an `Ord` that disagrees with `==`. **Review with the user**: `<`
  between two zones' readings of one instant is decided by the zone names.
- **Text.** ISO 8601 with `T`, the seconds always written, the fraction only when there is one and
  without trailing zeros (slate's), years outside `0`–`9999` signed; a fixed zone's `Zoned` writes no
  bracketed name. A duration keeps the form the user settled on 2026-11-04, its largest unit of `s`,
  `ms` and `us` (`90.5s`, `1.5ms`); a period writes as `1y 2mo 3d`. *Why*: one shape per column; the
  duration form is the user's own decision, which a delegated choice does not overturn.
  *Alternatives*: Sysl's renderers (a space for `T`, seconds dropped when zero), today's `WallTime`
  form with six fraction digits always, slate's `1h 30m` duration form (the design agent's
  proposal). **Review with the user**: whether durations should move to slate's `1h 30m` form.
- **Parsing.** `parseDate`, `parseTime`, `parseDateTime`, `parseTimestamp` and `parseZoned`, each a
  `Result` with `TimeParseError` (`BadShape(text, at)`, `OutOfRange(text, field)`,
  `Trailing(text, at)`, `UnknownZone(text, name)`, `at` a byte offset), every display read back,
  ASCII digits only, a timestamp's offset required. *Why*: Sysl's error kinds, with the text a
  message must quote. *Alternatives*: a message string only (slate); reading by pattern, left out
  until wanted.
- **Patterns.** `format(pattern)` with slate's pattern language (`YYYY`, `MMMM`, `WWWW`, `h12`, `a`,
  `f`…, `z`…, `|literal|`, `||` a bar), English names and no locales; an element the value lacks, a
  malformed run and an unclosed bar trap, the pattern being program text. `Instant` has no `format`.
  *Alternatives*: a compiled pattern answering a `Result`, as `Regex.compile` does, for patterns read
  from outside.
- **Zones.** `zone(name)` answers `Result[Zone, ZoneError]` (`UnknownZone`, `NotAZoneName` decided
  before any file is read, `Malformed`); `localZone()` always answers, `utc` where the host names no
  zone it can read; `fixedZone(offset: Duration)` takes whole minutes within ±23:59 and traps
  otherwise. An offset is a `Duration` and Sysl's `Offset` stays behind the binding.
  `isDaylightSavingAt` and `abbreviationAt` read the zone's table through `sysl.time.tzif`.
  *Alternatives*: an `Offset` type of its own (a ninth public type), `localZone()` as a `Result`.
- **The wall clock reads microseconds** as the host gives them. *Alternatives*: whole milliseconds,
  slate's choice for agreement with a back end Avin does not have.
- **Smaller additions**: `isoWeek()` and `isoWeekYear()`; `endOfMonth()`; `Date - Date` a `Period` of
  days; `d / e` between durations a `long`; a `Period` and a `Zone` `Eq` and `Hash` but not `Ord`; a
  `Time` with no arithmetic; `Weekday` ordered Monday to Sunday with `isoNumber()` and `plus(n)`;
  `months` and `years` as methods on `int` as the durations are on `int` and `long`; `3 * d` does not
  compile, the built-in types' operators gaining no methods.
- **Delays.** Every delay in the library is a `Duration`: `Future.await` and `Condition.waitFor` gain
  overloads taking one beside their milliseconds; a command's deadline is a `monotonic()` reading;
  actor timers keep the runtime's monotonic microseconds and their `Duration` parameters.
  *Alternatives*: leaving the millisecond forms alone.

- Settled on the user's delegation 2026-11-05: **Data types of one module may share a variant's
  name** (branch `shared-variants`).
  Replaces the rule that a variant's name is unique across a module's files, which kept a C or Sysl
  binding from mirroring two enums with one variant name (`Slant.Normal`, `Weight.Normal`;
  `sysl.process`'s `Stdio.Inherit` and `Group.Inherit`) and two machines from sharing `Idle`.
  **Review with the user.** The parts:
  - **The expected type decides a bare use.** A shared name is the variant of the type expected
    where it is written: a parameter's, an annotated binding's, a result's, a field's, a
    collection's element, and in a pattern the matched value's, so nested patterns decide each name
    by its field's type. Why: it is how the qualified form's work is already done everywhere a type
    is known, and it keeps bare names, Avin's usual form, working in all the places a shared name is
    actually used. Alternatives: refusing every bare use of a shared name (mirrors would read
    `Weight.Normal` everywhere); picking the first declared type (silent, order-dependent);
    deciding also by shape, a call meaning the variant with fields (a second rule for a reader to
    know; not built).
  - **Where nothing decides it, an error names the types and the qualified forms**: "`Normal` is a
    variant of `Slant` and `Weight`, and nothing here expects one of those types: write
    `Slant.Normal` or `Weight.Normal`". An expected type that is none of them is not a decision, and
    `Any` or a trait expected is not either. Reached through a module, the forms keep the module:
    `fonts.Slant.Normal`. Why: the qualified form always works, so the fix is the message.
  - **A comparison's other operand decides**, as it does a number literal's type: `w == Normal`,
    `Normal != w`. A shared name, bare or called, is checked after the other operands, with their
    type expected. Why: comparing with a variant is the commonest use outside a `match`.
    Alternative: requiring `is Normal` or `Weight.Normal` there.
  - **Coverage, derived impls and shapes are per type** and unchanged: a `match` over `Weight` is
    covered by `Weight`'s own variants; `Display`/`Debug` write the name; `Eq`/`Hash`/`Ord`, `walk`
    and `build` use the type's own declaration order; layouts and mirrors are keyed by type, and a
    mirrored variant crosses into Sysl as its own enum's.
  - **A shared name is still one name of the module**: a function, module-level value or object of
    that name is refused ("`Normal` is already a variant of `Slant` and `Weight`"), a companion object
    may not have a name another type's variant has, and the name is not a type. Within one type a
    variant's name stays its own. Why: functions and values are not chosen by expected type, so the
    clash would stay unresolvable. Alternative: letting a function or value hide the variants.
  - **Imports: `import m.Normal` is allowed** where `m`'s types share `Normal`; the imported name is
    shared in the importing file as in `m`, and decided the same way. Why: it costs no new rule and a
    file importing one of the names for its own type's use reads naturally. Alternative weighed:
    refusing the import and asking for the type (`import m.Weight`, then `Weight.Normal`), which the
    brief leaned to; refused because the bare form then could not be written at all in that file.
  - **Visibility**: a shared name is seen from another module where any of its types is public, and
    a private type's variant is left out there, so where only one type is public its variant needs
    no deciding (`signals.Stop`). Where all are private the name is private to its module.
  - Machines: two machines' states, and a state and an event's variant, may share a name, by the same
    rule (amending the settled machine entry above). An arm's result is its own `State`, `handle`'s
    argument the event type, so machine code needs nothing written.

- Settled on the user's delegation 2026-11-05 (built on branch `actor-state`): **`state` blocks in actors** (stage 12 step 8), the
  choices building it made where the [chapter](/spec/actors/#an-actors-states) is silent.
  - **Only a transition runs `leave`**: an actor that stops, fails or restarts runs its `stopping`
    block, which reads `state()`, and not the current state's `leave`. *Why*: the chapter defines
    `leave` as running when the actor moves out of a state into another, and `stopping` is already
    the one block an ending runs; running both would run two cleanups for one ending, in an order
    the program cannot see. *Alternative*: running the current state's `leave` before `stopping`,
    which suits a state holding a resource (the chapter's `Ready(socket)`) but runs code written
    for a transition when none happens.
  - **A trap in `enter` or `leave` fails the actor** as a trap in an arm does, and a trap in a
    `leave` leaves the actor in the state it was leaving; one in an `enter` leaves it in the state
    entered. The first state's `enter` runs as the last statement of the body, so a restart runs it
    again, and a trap there fails the actor as it starts. *Why*: the transition's steps run in the
    order the chapter gives, inside the turn, and supervision already decides what a trap in a turn
    means. *Alternative*: rolling the state back on a trap, which needs a copy of the state before
    every transition.
  - **An actor's `State` type and its states are private to its module**, as the class holding its
    state is, and `state()` is one of the actor's own methods, not its reference's. The states are
    still names of the module, and share a name with another type's variant there, a machine's
    states included, by the rule for shared variant names above. *Why*: nothing outside an actor reaches its state. *Alternative*: a public `State` type
    another module could match on, which nothing could hand it.
  - **An actor with states needs no `receive` block**, as a machine needs none; without states it
    still needs one. A shared arm of an actor with states yields a state, as a machine's does.
  - **No reentrancy trap**: only the runtime hands an actor a message, one turn at a time, so the
    actor needs no busy flag of the kind a machine's `handle` keeps.
  - **A sendable function reading a stateful actor's fields is refused with the advice to copy into
    a `val`**, not to make a field a `val`, which cannot make an actor that moves between states
    sendable.

  **Review with the user**: the first choice; whether an ending should run the current state's
  `leave` is the one a program holding a resource in a state notices.

- Settled on the user's delegation 2026-11-05: **A variant pattern writes no type arguments**
  (branch `qualified-variant-targs`). In an
  expression a generic type's variant takes its type arguments on the type, `Heap[int].Leaf`, or on
  the variant, `Heap.Leaf[int]` and `Heap.Node[int](…)` (settled by the declarations chapter's
  `Option.Some[long](9)`; only the fieldless form had been missing, and is now built). In a pattern
  neither form is written: `case Heap.Leaf[int]`, `case Heap[int].Node(…)` and `case Some[int](x)`
  are reported, *"a pattern takes its type arguments from the value it matches; write `Heap.Leaf`"*.
  Why: the scrutinee's type already decides them, so a written list could only repeat it or be an
  error, and a test of the instance is a type pattern's job (`x: Box[int]`). Alternatives weighed:
  accepting both spellings in patterns with the arguments required to equal the scrutinee's (more
  symmetric with expressions, but adds a rule that only restates the type); accepting them and
  ignoring them (silently wrong where they differ). Revisit if a pattern on an `Any` scrutinee
  should match a variant at one instance, which would give written arguments a meaning.

- Settled on the user's delegation 2026-11-05: **C enum mirrors, constants, statuses and out
  parameters, as built** (branch `c-bindings-enums`,
  [chapter](/spec/c-bindings/#enums-and-constants)). The chapter left these open:
  - **An enumerator's words and case.** A word starts at each upper-case letter of the variant's
    name, a digit and `_` staying with the word before them (`Yuv420p` one word, `ColorDodge` two,
    `Rgb16_565` → `RGB16_565`); they are upper case where the prefix has an upper-case letter and no
    lower-case one, and lower case otherwise, a prefix with no letter or none at all included.
    *Why*: it is the case rule the chapter already states, made total. *Alternatives*: a `case:`
    word on the mirror; matching the header's enumerators case-insensitively, which the C unit cannot
    list. A name starting with a digit after the prefix (`CAIRO_PDF_VERSION_1_4`) is named with the
    variant's own `@c`, and the C unit's refusal of the derived name says so.
  - **A mirror crosses only where the prototype writes its C type** (or that type `const`), not at
    a plain `int` that happens to carry it. *Why*: the binding then checks that the C function means
    this enum. *Alternative*: any integer position, which loses that check.
    **Review with the user**: some C APIs take an enum's values as `int`.
  - **Duplicated values**: a variant crosses as its enumerator's value, and a value C answers is the
    first variant in declaration order holding it. *Alternative*: refusing a mirror listing two
    enumerators of one value, which refuses aliases a header keeps for compatibility.
  - **A status mirror is `Ok` for zero whether or not it lists a zero variant.** *Why*: zero is
    success by the binding's form, not by the mirror. *Alternative*: requiring the zero variant.
  - **Constants**: an integer type takes any integer constant expression the headers define,
    enumerators, characters and a `static const` clang folds among them; `real` takes a floating or
    an integer constant; `string` only a string literal, its bytes checked as UTF-8. A value its Avin
    type cannot hold is refused at the constant with its value. `bool`, `char` and `byte` constants are
    refused. *Why*: the three kinds the chapter names, each checked where it is read. *Alternatives*:
    narrowing silently; a `bool` constant from an integer (left for when a library needs one).
  - **`out` and `inout` are written before the parameter's type** (`out int *x`), the parameter a
    pointer to what is answered, never to `const`; a parameter may still be named `out`. A non-`void`
    C result is left out where the binding's result is the out values alone (`int f(out int *a)`
    bound as `-> int` answers the out value), and is the first part where the result is a tuple of
    one more. *Why*: it is the chapter's "a C result bound as `unit` leaves the tuple without it"
    without a `unit` part to write. *Alternative*: requiring a tuple whose first part is `unit`.
  - **An out slot starts as zero or null**, so a C function that writes nothing answers zero,
    `None`, or stops where the type has no null. **A handle is never `inout`**: whether C kept the
    reference it was passed or replaced it cannot be said. *Alternative*: passing it in and treating
    what comes back as a new owned handle, which double-frees where C answers the same pointer.
  - **An `inout` string** is passed as a NUL-terminated copy through `const char **` and read back
    where C pointed it, before the copy is freed, so C may move it into the copy; a string C answers
    through an out pointer is left to C, as a borrowed result is.
  - **What `keeps:` keeps with out handles**: the holder is the handle the C result is, else the
    first handle answered through an out pointer, else the receiver; a status keeps only when it is
    zero, and a `None` handle keeps nothing. `borrowed` applies to every handle the binding answers.
- Settled on the user's delegation 2026-11-05 (branch `time-instant`): **A `Duration` given to a millisecond wait is
  rounded up to whole milliseconds.** `Future.await(d)` and `Condition.waitFor(d)` pass the runtime's
  millisecond wait `ceil(d / 1ms)`, so `await(micros(1))` waits a millisecond; a duration of zero or
  less asks without waiting, as a count that is not positive does. *Why*: a timed wait should never
  end before the time it was given, the rule `setTimeout` already follows for a socket.
  *Alternatives*: rounding down, which turns every duration under a millisecond into no wait at all;
  a microsecond wait in the runtime, which the condition variable beneath does not offer.
- Settled on the user's delegation 2026-11-05 (branch `time-instant`): **`toMicrosPart()` is the part below the second**
  (`micros % 1_000_000`), as the chapter's list goes from `toSecondsPart()` straight to it, with no
  `toMillisPart()`. *Why*: the four parts then cover a duration with nothing left over (hours below
  the day, minutes, seconds, microseconds). *Alternatives*: the part below the millisecond, which
  would need a `toMillisPart()` beside it to be complete.

- Settled on the user's delegation 2026-11-05: **An expected type reaches a core library variant
  that a nearer one hides** (branch
  `none-shadowing`). A module's own variant `None` (a C mirror's `ImageFormat.None`) hid `Option`'s
  throughout the module, so `return None` from a `-> Option[T]` function, `val x: Option[int] =
  None` and a `None` pattern on an option were all refused. Now, where the type expected at a bare
  variant name (every position of the shared-variant rule above: parameter, annotated binding,
  result, field, collection element, comparison operand, pattern) is a data type that none of the
  nearer variants of that name belongs to, but the core library's variant of the name does, that
  variant is meant: `None`, `Some`, `Ok`, `Err`, `Left`, `Right` and any later prelude variant.
  **Where nothing decides it the nearer declaration wins**, as the scoping order always said, so
  `val f = None` in that module is still `ImageFormat.None`; an expected type that is neither (an
  `int`, `Any`, a trait) also leaves the nearer one, reported against what was expected. Only the
  core library's variants are reached this way: a file may not import a name its module declares,
  so an imported variant is never the hidden one, and an imported `None` hides `Option`'s exactly as
  a module's own does. **Review with the user**: it makes programs valid that were refused, and
  changes no valid program's meaning (a use the rule now gives to `Option` was a type error before).
  *Why*: the rule for shared names is the one a reader already knows, and a C mirror's
  `None` is common enough that writing `Option.None` in every option-returning function of a
  binding module would be a tax on exactly the code the bindings chapter shows. The undecided case
  keeps the nearer one because every such use was valid before and meant the module's variant;
  making it an ambiguity error would break `val f = None` and `f == None` in existing mirror
  modules, and would make the module's own name second-class in its own module. *Alternatives*:
  an ambiguity error naming `ImageFormat.None` and `Option.None` wherever nothing decides (no
  silent pick, but breaks valid programs and asks for a qualified form on the module's own type);
  leaving shadowing as it was and requiring `Option.None` (the status quo the bug report was
  about); refusing a variant named like a core library variant (would rename C enumerators).

- Settled on the user's delegation 2026-11-05: **The language server's `workspace/symbol` and
  `textDocument/foldingRange`** (branch
  `lsp-symbols-folding`). Workspace symbols are read off each file's parse, for every program an
  open document belongs to, its unopened files included and its dependencies' left out; a name
  matches a query holding its characters in order (a subsequence), ASCII letters in either case; at
  most 500 symbols are listed, in file order, for any query. A symbol's location is its name, its
  container its module, or its type for a method, a variant or a machine's state; an actor is listed
  once, as its class, and compiler-written members (names with a `$`) are not listed. Folding is by
  whole lines, from the lexer's blocks (the line before an indent to the last line of code before the
  matching dedent), multi-line tokens, comment runs, the import run and, in a literate file, runs of
  program text; two regions of the same lines are listed once, the block's kind winning. WHY: the
  parse is there while the program has mistakes, as the outline's is; a subsequence is what editors'
  symbol search does; 500 is more than a person reads and small enough to answer per keystroke; the
  lexer's indent and dedent tokens already know comments, text blocks and bracketed continuations,
  which a reading of indentation alone would not. ALTERNATIVES: symbols from the checker's index
  (absent while the program has a syntax error, and it holds no declarations of its own); prefix or
  fuzzy-scored matching ranked by score (fine to add later; the protocol leaves ranking to the
  client); Unicode case folding (needs a case table the compiler does not have); no cap, or a cap
  only for the empty query; folding by indentation alone. Review with the user: whether ranking by
  match quality is wanted, and whether a machine's compiler-written `state()`/`handle()` should be
  listed (they are, at the machine's name).

- Settled on the user's delegation 2026-11-05: **An arm no value can reach is an error** (branch
  `unreachable-arms`). An arm of a `match`, a `match` function, an actor's `receive` or a machine
  state, and a clause of a function, is an error when the arms before it without a guard already
  match every value it would: after `_`, after a binding, after a pattern covering the whole type,
  or covered by several earlier arms together. The message names the earlier arm that alone covers
  it, by pattern and line, *"this arm is never reached: the arm `_` on line 3 already matches every
  value it would"*, or says the arms before it do together. Where the covering arm binds a name
  spelled as a type or a variant is (an upper-case first letter), it adds *"`Live` binds a new
  value there, since no variant `Live` is in scope"*. A guard never hides the arms after it; a
  clause hides the ones after it only when one of its bodies has no guard; a clause body after a body
  with no guard is an error too. Reachability is decided on the coverage matrix, so it knows what
  coverage knows (variants, `true`/`false`, literals, ranges, comparisons, `n + k`, strings by
  equality, sequences by length, nesting); an extractor or a type test is taken to match every value
  in the arm judged and none in the arms above it, so an arm is called unreachable only when it is,
  whatever the extractors answer. A machine state's own arms are judged against each other; the
  shared `receive` arms, which a state may override by handling an event itself, are judged against
  each other once. A match whose patterns already failed to check is not judged. WHY: an arm
  `Live -> …` meant as a variant that was not in scope bound a new name, matched every phase, and
  hid the arms after it: the actor runtime hung and spun at 900% CPU with nothing reported. A dead
  arm is always a mistake, either in the arm or in an arm above it, and it is found by a check the
  compiler already makes. ALTERNATIVES: a warning (Avin has no warnings channel; every diagnostic is
  an error, and a warning shown only sometimes is the one that is missed); an error only after an
  irrefutable arm (catches the bug that prompted this, but not `Some(_)` before `Some(0)`, which is
  as dead); leaving it as it was. Review with the user: whether two identical extractor arms
  (`Even() -> …` twice) should count as unreachable, since an extractor is a function that could
  answer differently each time.

- Settled on the user's delegation 2026-11-05: **C arrays as pointers and `blocking` calls, as
  built** (branch `c-bindings-arrays`, stage 13
  step 6, [chapter](/spec/c-bindings/#arrays-and-out-parameters)). The chapter left these open:
  - **An `Array[int]` or `Array[long]` goes only to a pointer to a *signed* integer of its size;
    an `Array[byte]` to any one-byte integer; an `Array[real]` to any eight-byte floating-point
    type**, typedefs included, all checked by the C unit (`__builtin_classify_type`, `sizeof` and
    `(T)-1 < (T)0`). *Why*: "the element types must be the same size and kind, and nothing is
    converted" — an unsigned element read as an `int` would turn its upper half negative, which Avin
    has no unsigned type to hold, while a byte is an octet of no sign. *Alternatives*: any integer
    of the size (silently reinterpreting unsigned elements); only the C types the chapter lists
    by name (which refuses `FT_Fixed *`, `aa_octet *` and every typedef). **Review with the user**:
    an unsigned element array (`uint32_t *` pixels) needs a binding per element or a buffer.
  - **`length(data)` is written first, before the parameter's type and before `inout`**
    (`length(buffer) inout FT_ULong *length`), as `out` and `inout` are; a parameter may still be
    named `length`. It marks an integer of C's keywords (not `char`, not an enumeration) or a
    typedef, which the C unit checks to be an integer. *Alternative*: after the type, which reads as
    part of it.
  - **An `Option` of an array is accepted wherever an array is**, not only beside an `inout` length:
    `None` passes null and a length of zero to any binding. *Why*: C that takes null for "no data"
    is common, and the rule is the chapter's for `inout`. *Alternative*: only beside an `inout`
    length.
  - **An array's length crosses as an `int` converted to the length's C type**, so an array too
    long for an `unsigned char` length stops the program at the call, naming the length's type and
    the array ("value out of range for u8 (unsigned char), the length of `p`"). **A length C writes
    back through `inout` is read as an out value, then compared with the array's length**, the
    program stopping where it is greater; `None` bounds nothing, since it asks C the length it
    needs.
  - **Every array position crosses as a pointer in place unless the call is `blocking`**: the
    collector never moves an object and no collection runs while a call that is not `blocking` holds
    the thread, so nothing is copied; a `blocking` call is passed a copy off the heap (an empty
    array's a byte long, so C is never passed null for it), copied back only where the pointer is not
    `const`, and freed once the thread is running again. *Alternative*: copying for every call,
    which costs the common case for nothing.
  - **A `blocking` call keeps each handle it is passed alive across the wait** by using its object
    after `leave_safe`, so the collector, running while the call waits, cannot give the C pointer
    back under C. *Alternative*: relying on the caller holding it, which a temporary argument does
    not.
  - **Arrays of mirrors, strings, handles, `bool` and the narrower integers are refused** with the
    four element types the chapter lists; arrays of mirrors arrive with struct mirrors (step 7).
- Settled on the user's delegation 2026-11-05 (built on branch `sysl-generic-bindings`): **generic bindings are bounded by `Boundary`**
  ([Sysl bindings](/spec/sysl-bindings/#generics)). A binding's type parameters, a generic handle
  class's, and a bound method's own are each bounded by `Boundary`, a core trait the compiler decides
  from a type's structure as it decides `Sendable`: the integers of 8, 16, 32 and 64 bits, `byte`,
  `real`, `char`, `bool`, `unit`, `string`, `@sysl` mirrors, and `Option`s, `Result`s and tuples of
  these, and a type parameter bounded by it. It is only a bound: not a type, and no impl, trait or
  derivation declares it. Each call's type arguments are held to it as to any bound, a generic
  function passes its own type parameter on only where that one is bounded by it too, and every
  instantiation gets a shim of its own that Sysl checks, so a Sysl bound the instance does not meet is
  reported at the binding in Sysl's words; an Avin bound beside it (`[T: Boundary + Ord]`) is Avin's to
  check, with no mapping between Sysl's traits and Avin's. A generic handle class,
  `@sysl("m.Signal") class Signal[T: Boundary]`, names the generic type without arguments, its type
  parameters invariant and the Sysl type's in order; each instance the program uses is a handle of its
  own on the Sysl type's instance, with its own finalizer, spelled by the shim from the instance's
  types, so a mirror may be a type argument (the annotation-text instances keep their narrower rule).
  Rationale: the declaration can be checked once and every instantiation is then sound by the bound
  machinery that already exists, including through a library's generic wrappers, with no
  instantiation-time diagnostics in lowering (which reports none); "one Sysl type" is what a handle's
  instance needs and what keeps a type argument's Sysl type the same in both directions.
  Alternatives weighed: unbounded type parameters checked per call at concrete types only (refuses
  every generic wrapper, or needs a closed-world pass in the checker that does not exist); an
  implicit bound, never written (hides why a wrapper's `T` is refused); admitting collections (their
  Sysl type is a slice going in and a slice or a `Buf` coming out, so a `T` a Sysl function stores
  would not be one type) and handles (one goes into Sysl only by itself, so `Option[T]` at a handle
  could not cross in); a mapping from Sysl traits to Avin bounds (no table exists, and Sysl checks its
  own bounds anyway). **Review with the user:** the name `Boundary`, and whether handles should become
  type arguments of generic handles (`Signal[View]`), which needs the shim to pass a handle inside a
  generic Sysl value.
- Settled on the user's delegation 2026-11-05 (built on branch `sysl-generic-bindings`): **a callback closure's parameters are written
  out where Sysl cannot infer them**. A Sysl closure taking the function's own type parameter
  (`applied[T, U](x: T, f: &Fn(T) -> U)`) left the shim's lambda `(q0) -> …` untyped, which Sysl
  refuses, for generic and non-generic bindings alike; the shim now writes each parameter as the Avin
  type's Sysl spelling on that diagnostic, and is rebuilt for as long as each build names something
  more to write out (the lambda's types first, the integer answer after), not once.
  Rationale: the same retry the shim already made for integer answers and parameters, extended.
  Alternatives weighed: always typing the lambda (wrong where Sysl hands a collection as a `Buf`
  rather than a slice, which only inference gets right); refusing such bindings.
- Settled on the user's delegation 2026-11-05 (branch `actor-isolation`): **A send hands a message's handles to the receiving actor,
  not into transit** ([actors](/spec/actors/#handing-over-ownership)). The settled ownership
  decision says a send puts a message's handles in transit for their first user, the receiver, and
  that the sender's later use traps. `ActorRef.tell` knows the actor the message goes to, so the
  handles the sender owns are handed straight to that actor's owner as the message enters its
  mailbox. Why: in transit, a sender that uses a handle before the receiver does would take it
  back, and the trap the decision promises would depend on a race; handed to the actor, the
  sender's next use always traps. A message dropped because the actor has ended hands nothing over,
  so its handles stay the sender's. A reply to an `ask` completes a future, which already puts its
  handles in transit for the waiting code. Alternatives weighed: in transit as written (racy, as
  above); handing over when the receiver takes the message off its mailbox (too late: the sender
  could have used the handle meanwhile).
  **Review with the user:** it refines the wording of a decision the user settled.
- Settled on the user's delegation 2026-11-05 (branch `actor-isolation`): **Handing a value over stops at what is shared between
  owners.** The runtime's walk follows every object a value reaches, so a message holding an
  actor's reference walked into that actor's state and handed its handles away (an actor that
  spawned a child capturing its own reference lost its connections to the child). The standard
  library marks the types shared between owners `@keeps_owners` — `ActorRef`, `Timer`, `Future`,
  `Promise`, `Executor`, `Channel`, `Mutex`, `Condition`, `Once`, `Atomic`, `Thread`, `Scope` — and
  their type records carry the mark, at which every walk (a send, `spawn`, a thread's or a future's
  body, completing a future) stops: their handles keep their owners, as a channel's values already
  do. `Behaviour` is walked, since its start's captures are what the new actor owns. The mark is the
  standard library's alone. Alternatives weighed: a walk the compiler writes for each type (exact,
  but a collection's nodes are untyped to it); a marker trait instead of an annotation (visible to
  every program, which could then hide its own handles from a send).
- Settled on the user's delegation 2026-11-05 (branch `actor-isolation`): **Which messages a send walks.** A send walks its message
  where its type may hold a handle: a handle; a tuple, data type or class with a field that may; a
  generic type of the standard library, its collections and `Isolated` among them, at a type
  argument that may, or that keeps a function (`Behaviour`); a function value; and `Any`, a trait,
  or an open or abstract class, which may hold anything. A `@keeps_owners` type and the scalars and
  strings never are. Decided per type where the program is compiled (the actors' intrinsic
  `holds_handles`), so a message of numbers, strings, actors' references or `Vector[int]` costs no
  walk. Alternatives weighed: walking every message (pays for every send); a standard library
  generic type judged by its fields too (a `Vector[int]`'s nodes hold `Any`, so every vector would be
  walked).
- Settled on the user's delegation 2026-11-05 (branch `actor-isolation`): **`@isolating`, the mark of `isolate`'s body and `with`'s
  function** ([what is shared](/spec/threads/#what-is-shared)). The rule of `isolate`'s body is
  `@isolated`'s (only sendable values from around it) and the whole-program check that nothing it
  runs reaches module-level mutable state; `@isolated` deliberately leaves out the second half for a
  mutex's body. A third mark between `@isolated` and `@sendable` carries both, and a program's own
  function that isolates what its caller passes writes it too: a parameter so marked is passed on
  for `@isolating` or `@isolated`, and a `@sendable` one for `@isolating`, but not the other way. It
  allows capturing a `var` of the code around it, since the body runs at once. Alternatives weighed:
  holding every `@isolated` argument to the module-level check (a mutex's body was thought to have
  no need of it, which the `mutex-isolating` entry below corrects);
  an annotation only the standard library may write (a program could then not wrap `isolate`).
  **Review with the user:** it adds a third annotation users can write, beside `@sendable` and
  `@isolated` (a mutex's value and body take it too, below).
- Settled on the user's delegation 2026-11-05 (branch `actor-isolation`): **`with` holds an `Isolated`'s objects while its function
  runs.** Two `with`s of one `Isolated` on two threads would change one graph at once, and a `take`
  from inside `f` would hand the objects out while `f` still works on them. So `with` moves the
  `Isolated` from held to in use by one atomic compare-and-set and back when `f` returns; a `take` or
  `with` meanwhile traps with `` `Isolated` is in use by `with` ``, and a `with` after a `take` traps
  as a second `take` does (`` `Isolated` was already taken ``). A trap inside `f` leaves it in use.
  Alternatives weighed: a lock that makes a second `with` wait (an actor holding it would stall a
  worker, and a `with` from inside `f` would deadlock); no check (a data race).
- Settled on the user's delegation 2026-11-05 (branch `actor-isolation`): **Where `Isolated` lives.** `Isolated`, `isolate` are in
  `avin.actors` (the chapter that defines them), imported as `import avin.actors.{Isolated,
  isolate}`; a thread passes one through a channel the same way. `Isolated(objects)` may be called
  directly, its parameter marked `@isolating`, as `Mutex(value)`'s is. Alternatives
  weighed: the core module (imported everywhere, though only concurrent code wants it);
  `avin.threads` (the actors chapter names it).
- Settled on the user's delegation 2026-11-05 (branch `mutex-isolating`; found on
  `actor-isolation`): **a mutex's value and its `with` body are `@isolating`**
  ([synchronization](/spec/threads/#synchronization)). Under `@isolated`, `m.with(b -> cache = b)`
  on the main thread stored the buffer in a module-level `var`, so the main thread could change it
  later without the lock while a worker changed it under the lock; `Mutex(make())`, where `make`
  keeps the buffer it returns in a module-level `var`, did the same at construction. Both
  parameters now take the whole-program check that nothing they run reaches module-level mutable
  state. The cost: a body may not read a module-level `var` (or a module-level value of a type that
  is not sendable) even where it stores nothing; it reads a local copied before the `with` instead.
  Module-level `val`s of sendable types, another mutex among them, stay readable, so nested `with`s
  on two module-level mutexes are legal. Alternatives weighed: a narrower rule refusing only a flow
  of the guarded value into module state while still allowing reads of module `var`s (an escape
  analysis through every call, field, collection and closure; the store can go through a module
  `val` of a mutable collection or an object's method, so the rule is the same whole-program reach
  with flow tracking on top, harder to make sound and to explain in one diagnostic); leaving the
  constructor `@isolated` (keeps the same hole open through the value's maker). **Review with the
  user:** a `with` body that only reads a module-level `var` counter is now refused.

- Settled on the user's delegation 2026-11-05 (built on branch `c-bindings-callbacks`, stage 13 step 9,
  [chapter](/spec/c-bindings/#callbacks)): **C callbacks, as built.** The chapter left these open:
  - **A function pointer finds its `userdata` by name**: the function pointer's own prototype marks
    exactly one parameter `userdata`, and the binding marks one of the same name, which carries it
    (`void (*listener)(userdata void *data, …)` beside `userdata void *data`); each of the binding's
    carries exactly one function, and a parameter taking a function is named. *Why*: MLT and Cairo
    already use one name for both, and a name says unambiguously which `void *` goes with which
    function when a binding takes two. *Alternatives*: by position (the n-th `userdata` carries the
    n-th function; silent when C's order differs), or one function per binding (MLT and Cairo need
    no more, but GLib-style APIs pass a `destroy` beside the callback).
  - **One trampoline per binding parameter, not per C function type.** *Why*: two bindings of one
    C type may cross it as different Avin types (`int` and `long`, a mirror and an integer), so the
    conversions differ; the trampoline is a few instructions and the function it runs is the
    conversion. *Alternative*: one per C type with the conversions chosen at run time.
  - **A callback answers a scalar, a truth value or an enum mirror, and `unit` exactly where C's
    function answers `void`**; a string, a handle or a status as its answer is refused. *Why*: a
    string or a handle answered to C would have to outlive the call with nothing to free it.
    *Alternative*: answer a string copied into memory C must free, which no C API this stage binds
    expects.
  - **What C calls a function with crosses as a binding's result does**, a string copied and
    checked (or an `Array[byte]`, or `None` for null), a handle lent through its class's `retain` —
    a handle class without `retain` is refused there — and a value no Avin type holds stops the
    program where the binding C is running was called, as the answer's does, naming the function.
  - **`forget:` is a C function's name, called `name(self, userdata)`, or a C expression in `self`
    and `userdata`**, run in the handle's glue for each callback it keeps, after its pointer is taken
    and before `release`; a handle that keeps a handle never takes the other's callbacks.
  - **A kept function's record roots it until the handle keeping it is given back.** *Why*: the
    record must outlive any call C makes, even one racing the collector on a thread of C's own.
    *Consequence*: a kept function that captures its own holder keeps both alive until `close()`.
    *Alternative*: trace the function from the handle, which frees such a cycle but lets a C thread
    call a function the collector has just freed. **Review with the user**: whether that cycle is
    worth a weak record.
  - **In a program that passes C a function, every call of a C library function may run the
    collector**, and is planned as one that does: any C call may call back a function C keeps. The
    C unit's glue never does. *Alternative*: a `calls-back` word on the bindings that may, which is
    cheaper and trusted.
  - **libc's `qsort` and `bsearch` take no `userdata`, so they cannot be bound**; the corpus binds a
    C function of its own that calls `qsort_r` and `bsearch` through one.

- Settled on the user's delegation 2026-11-05: on branch `c-bindings-structs` (roadmap stage 13
  step 7), **C struct mirrors, opaque values and arrays C answers.**
  - **A mirror's layout is measured, and checked against the header's order.** The C unit measures
    each field's offset, width and sign and the struct's size, and asserts the fields come in the
    header's order, each where C's alignment puts it after the one before, none after the last;
    lowering reads and writes each field at its measured offset and width, with the traps of an
    argument and a result. *Why*: a field narrower or wider than its Avin type (`uint8_t` as `int`,
    `float` as `real`, `unsigned long` as `long`) crosses as a scalar parameter does, and the layout is
    the target's. *Alternatives*: a layout implied by the Avin types alone and asserted (cannot spell
    a `float` or a `short` field); matching fields by name in any order (hides a reordered header).
  - **Every field is named, which a positional initializer checks.** The unit initializes the struct
    by position with one zero per mirrored field under `-Wmissing-field-initializers` made an error,
    so a field hidden in the padding between two others is found. For that the unit no longer passes
    `-w`; it ignores every warning with `#pragma clang diagnostic ignored "-Weverything"` instead.
    *Alternative*: the layout checks alone, which miss a small field in padding.
  - **A mirror has one variant named as the type**, as a Sysl struct mirror does, and its fields are
    integer types, `byte`, `bool`, `real`, `string`, **`Option[string]`** (null as `None`), enum
    mirrors and other struct mirrors. *Why*: FreeType's and Cairo's structs carry nullable names.
    *Alternative*: `string` only, a stop on null.
  - **Strings in a mirror passed in are copies freed after the call; strings C writes into a mirror
    are copied out and left to C**, as a borrowed string result is. *Alternative*: a `free:` per
    field, which no surveyed struct needs.
  - **Every out value stays in the result, the array's length too**, where `array:` names the array.
    *Why*: one rule for out values. *Alternative*: dropping the out value `length:` names.
  - **An array C answers that is null** is `None` for an `Option`, an empty array where its length is
    zero, and a stop otherwise; a negative length, or one past an `int`, stops the program; the
    array is freed by `free:` (a function or an expression in the array's name, optional for memory
    C keeps) after it is copied, and unread where a status fails. **Review with the user**: whether
    `free:` should be required with `array:`.
  - **Opaque values cross by value only**: a parameter, a result and a method's `self`, never through
    a pointer, `inout` or in an `Option`; each object is its own, compared by identity, and
    sendable, since it is an immutable copy. *Alternative*: also `out` and `const T *`, which no
    surveyed binding needs.
- Settled by the user 2026-11-06: **concurrency that waits is written as straight-line blocking
  code, run on lightweight threads; Avin has no `async` or `await` keywords.** A lightweight thread
  is a runtime-scheduled thread with a stack of its own; a few operating system threads, about one
  per core, run whichever are ready. A call that would wait (a socket read, `Future.await`, a
  channel, `sleep`, a lock) parks only its lightweight thread: the runtime records what it waits
  on (a socket through the runtime's poller over kqueue or epoll, a future, a timer), and runs another on the
  same operating system thread. `spawn`, the futures' pool and actors' turns come to run on
  lightweight threads, so the APIs that exist today keep their meaning and become cheap; a server
  holds one lightweight thread or one actor per connection, and a connection that waits holds no
  operating system thread. Concurrency stays explicit: `Future(body)` to start work and `await()`
  on it, `Future.all` and `Future.first`, channels and `scope`.
  *Why*: no division of functions into those that may wait and those that may not, so one library
  serves every caller and any function at any depth may wait; an ordinary call allocates nothing;
  existing code needs no change. *Alternatives*: stackless `async` functions compiled to state
  machines (smaller per waiting task, in the hundreds of bytes, but every function on the path to
  a wait marked `async`, libraries split in two, and an actor's state open to change at each
  `await`); `async`/`await` as sugar over lightweight threads (`async def` a `Future`, `await e`
  `e.await()`), declined as a second spelling of what `Future` already says.
  *Consequences*: a stack per lightweight thread, reserved large and committed as touched (at
  least a page, 16 KB on arm64, per lightweight thread), with moving parked frames to the heap a
  later reduction; the collector scans parked stacks; a C call that blocks holds its operating
  system thread, as `blocking` already accounts for.
- Settled on the user's delegation 2026-11-06: **lightweight threads are time-sliced at
  safepoints.** A lightweight thread that has run for its slice, 10 ms to begin with and tunable,
  yields at its next safepoint poll and goes to the back of the ready queue, so threads that never
  wait share a core instead of starving the others. A timer in the runtime raises the yield request
  on the carrier running it; compiled code already polls a flag on every back edge of its control
  flow and before every call that may recur, for the collector, and the same poll answers it, so a
  thread that does not overrun pays nothing more. A wait still switches at once, and an actor's
  turn still ends after 16 messages. A long C call cannot be interrupted; `blocking` lets another
  carrier run in its place. Built with the scheduler core.
  *Why*: fairness for computation-heavy threads at no cost to the rest, from a poll that exists
  already, with every interruption at a point where the roots are exact.
  *Alternatives*: switching only at waits (a long computation holds its carrier until it ends);
  interrupting at any instruction by a signal (needs exact roots at every instruction, which the
  shadow stacks give only at safepoints).
- Settled on the user's delegation 2026-11-06: **The allocation chunk belongs to the carrier, a runtime record per operating system thread, and
  a context reaches it through a pointer** (branch `lwt-carrier-chunk`). `Context.carrier` points at
  the `Carrier` the context runs on, which holds the `gc.Chunk`; allocation reads
  `ctx.carrier.chunk`, one load more than before, and compiled code never touches the chunk, so
  lowering is unchanged. Attaching a thread makes its carrier (a record that keeps itself alive)
  and links it into a list of carriers beside the list of contexts, both under the heap lock;
  detaching retires the chunk, unlinks the carrier and frees it, and detaching twice changes
  nothing. A collection retires the chunk of every carrier rather than of every context. A
  context's `carrier` is stored only while the context is safe, by one store, so a later
  scheduler moves a lightweight thread between carriers without moving a chunk.
  *Why*: thousands of parked lightweight threads must hold no chunk each, and the chunk is a
  property of the thread that is running, not of the code it runs. *Alternatives*: the chunk kept
  in the context and handed over at each switch (a copy of the chunk's six words per switch, and
  every parked context still sized for one); a thread-local carrier found through the platform's
  thread-local storage (no pointer in the context, but a thread-local access on every allocation,
  dearer than one load); the carrier embedded in the thread's own storage beside its context (no
  separate record, but a carrier would then die with the context it was made beside, which is
  wrong once contexts outnumber carriers).
- Settled on the user's delegation 2026-11-06 (branch `lwt-scheduler`): **the scheduler is one ready queue under one lock, run by
  workers started one at a time as threads become ready, up to one per processor.** A thread made
  ready goes to the back of the queue; an idle worker sleeps on a condition and is woken for it, and
  where none is idle and fewer workers than processors are counted, another is started. A worker is
  an operating system thread with a carrier of its own that runs no compiled code itself: it switches
  to a lightweight thread and back, and after the switch back does what the thread asked (let a
  guard go, re-queue it, give its stack back). *Why*: the simplest correct queue, with every wake in
  the order threads became ready, and the lock is held for a few instructions. *Alternatives*: a
  queue per worker with stealing (less contention on many cores; more code, and no ordering
  between workers), left for when a measurement asks for it; starting every worker at once (simpler
  counting, but a program with one thread would start one per processor).
- Settled on the user's delegation 2026-11-06 (branch `lwt-scheduler`): **a lightweight thread's stack keeps `spawn`'s 8 MiB default,
  reserved through `sysl.fiber` and committed as touched, with 64 KiB at the least; the stacks of
  ended threads are kept, 64 at most, for threads asking about the same size.** *Why*: the default
  costs address space only, so every program that recursed deeply in a thread before still does,
  and a reused stack saves a mapping and its guard per short thread. *Alternatives*: a smaller
  default such as 1 MiB (less address space at a hundred thousand threads, but deep recursion that
  fitted in a thread would overflow); no reuse (a map and an unmap per thread). Review with the
  user: the default is the knob most likely to want a different value once servers hold many
  thousands of threads.
- Settled on the user's delegation 2026-11-06 (branch `lwt-scheduler`): **`main` stays on the program's first operating system thread
  and is not a worker; its waits block that thread.** Every wait has two forms: a lightweight thread
  parks, and any other thread (the main thread, a thread attached for a callback) sleeps on one
  shared condition until its own waiter is woken. *Why*: the main thread stays free for the `ui`
  package, which must run on it, and a program whose `main` never waits keeps its whole processor
  count of workers. *Alternatives*: `main` as a lightweight thread run by a worker, with the first
  operating system thread one of the workers (one kind of wait, but the `ui` package would then need
  a way to pin a thread to it, which is the open main-thread question).
- Settled on the user's delegation 2026-11-06 (branch `lwt-scheduler`): **time-slicing answers at the existing poll by a second bit of
  its flag.** `requested` holds `Stop` (1), raised by a collection, and `Yield` (2), raised by the
  monitor, a runtime thread that ticks every millisecond while a worker is busy, when a worker's
  thread has run 10 ms while another is ready; it also sets that worker's own `yield_wanted`. A poll
  still loads the word and branches on zero; `avin_rt_safepoint` waits out a `Stop` and then, on a
  worker whose flag is set, yields to the back of the queue, and lowers `Yield` once no worker wants
  it. Any lightweight thread yields at any poll, inside a lock or a callback too. *Why*: compiled
  code is unchanged and pays nothing while nothing overruns; while one does, the others' polls take
  the slow call for at most a tick. *Alternatives*: a flag per carrier read through the context (no
  spurious slow calls, but two loads per poll and a compiler change); not yielding inside a lock
  (the lock's waiters park anyway, so it buys nothing and lets one thread hold a worker).
- Settled on the user's delegation 2026-11-06 (branch `lwt-scheduler`): **a blocking call is noticed, not announced: `enter_safe`
  marks the carrier blocked and counts the call, and the monitor releases a worker that has stayed
  blocked across a whole tick while threads are ready.** A released worker is not counted against
  the target, and another is woken from the spares or started; when the call returns the thread
  runs on, and its worker rejoins the count at its next switch, or rests as a spare where the count
  is full. This generalizes the pools' `blocking` compensation to every blocking binding, `sleep`
  and a wait for a `lazy val`. *Why*: a call that returns within a tick costs two stores and starts
  nothing, and the generated shims and every binding stay as they are. *Alternatives*: compensating
  at every blocking call (a thread start or wake per call, however short); a pool of threads for
  blocking calls only (every such call a hand-off between threads).
- Settled on the user's delegation 2026-11-06 (branch `lwt-scheduler`): **every lock and condition parks: `Mutex`, `Condition` and
  `Once` as well as channels, futures, promises and joins.** The runtime's one lock and one
  condition, which all of them are built on, keep a queue of waiters; a lock let go goes straight to
  its oldest waiter. The brief planned `Mutex` and `Condition` for the next step, keeping them
  blocking until then. *Why*: channels, futures' cells and the pools are built on the very lock and
  condition `Mutex` uses, so keeping `Mutex` blocking meant a second, temporary implementation of
  both; and a `Mutex` whose holder parks can then never hold every worker. *Alternatives*: a parking
  lock and condition beside the blocking ones, used by channels and futures only, until the next
  step (two implementations, one to remove). Review with the user: it moves part of the next step
  into this one.
- Settled on the user's delegation 2026-11-06 (branch `lwt-scheduler`): **a context waits as the one waiter it holds**, since a thread
  waits on one thing at a time; a timed wait of a lightweight thread is a timer the monitor fires,
  added by the worker after the thread has switched away and taken off by the thread when it runs
  again. *Why*: no waiter is ever made per wait, and no waiter is reachable once its wait has
  answered. *Alternatives*: a waiter in the waiting function's frame (the same, but its address is
  a local's, which Sysl's escape check refuses to queue); a heap waiter per wait (an allocation per
  wait).
- Settled on the user's delegation 2026-11-06 (branch `c-bindings-buffers`): **views and blocks are one object layout, the runtime's
  `CMemory`, with their operations as intrinsics of `avin.c`.** `ByteView`, `MutByteView` (a
  subclass) and `ByteBlock` (a subclass of `MutByteView`) are library classes the compiler lays out as
  the address, owner, length, closed flag, the two reasons they trap with and the lender; each
  operation is a call of the runtime that checks the code running, the closed flag and the bounds, and
  stops the program at the caller. *Why*: a class holding an address in an Avin field could be made by
  its public constructor with any address, since Avin has no private constructors; a layout only the
  compiler and the runtime fill keeps every address one C lent or Avin allocated. *Alternatives*: a
  Sysl handle per view (the view could not keep its lender, an Avin object, alive); inline MIR for
  every operation (more compiler, and a closed view's reason is a string the runtime reports anyway).
- Settled on the user's delegation 2026-11-06 (branch `c-bindings-buffers`): **a view's lengths and indices are `int`s**, and a C length
  past the greatest `int`, or negative, stops the program at the call. *Why*: arrays' are `int`s, so a
  copy, a slice and an index mix with them without conversions; 2 GiB is more than a frame or a glyph.
  *Alternatives*: `long` lengths (every index written `long(i)`); `long` lengths with `int` indices
  (a view longer than any index reaches). Review with the user: a library lending more than 2 GiB at
  once would need `long`.
- Settled on the user's delegation 2026-11-06 (branch `c-bindings-buffers`): **null lends `None` in an `Option` whatever the length, and
  otherwise an empty view where the length is zero and a stop where it is not**, as arrays C answers
  are. *Why*: FreeType's empty glyph is null with no rows, and a frame with no image may answer null
  with a size of zero. *Alternative*: null always a stop outside an `Option`.
- Settled on the user's delegation 2026-11-06 (branch `c-bindings-buffers`): **a view belongs to the code owning its lender, and is lent
  only by a method of a handle that is not `shared`.** A view checks its lender's owner at each use, so
  handing the lender over (a thread's body capturing the view or its frame) takes the view with it and
  any other thread traps. *Why*: closing a view is a write the lender's next use makes, and reading
  memory the lender's next call may free; both must happen on one owner at a time, which a `shared`
  handle does not have. *Alternatives*: an owner of the view's own (a view handed over without its
  lender would race the lender's next use); `shared` views closed under a lock (a lock per byte read).
- Settled on the user's delegation 2026-11-06 (branch `c-bindings-buffers`): **a failed status lends nothing and copies nothing, but is a
  use of its receiver**, which closes the view lent before it. *Why*: the call ran, and may have freed
  the memory. *Alternative*: closing only on success (a stale view after a failed conversion).
- Settled on the user's delegation 2026-11-06 (branch `c-bindings-buffers`): **the record `attach:` gives C belongs to one handle object**:
  later keeps through the same handle add to it, the handle lets go of it when its reference goes back,
  and C holds it until `destroy`; on an `attach:` class every kept object, handles too, is held as its
  Avin object, so a kept handle's own C reference lives as long as the record. `key:` may be left out
  (a `char` of the class's own keys the record), and `attach:` with `shared` is refused. *Why*: the
  record's slot is reused only once neither the handle nor C holds it, so a handle never updates
  another's record; holding kept handles as objects keeps their references alive past the holder's
  `close()`. *Alternatives*: one record per C object, found through the library's own getter (no
  getter is named, and `attach:` would need a second expression); C references for kept handles (given
  back with the holder's handle, while C still uses them). Review with the user: two Avin handles on
  one C object each attach under the same key, so the second replaces the first's record, and C's
  destroy of the first lets go of what the first kept while the first handle still keeps it.
- Settled on the user's delegation 2026-11-06 (branch `c-bindings-buffers`): **a `ByteBlock`'s own constructor makes a block of no bytes,
  passed to C as null**, and `ByteBlock(length)` is the companion's `apply`. *Why*: a class has a
  constructor, and the empty block is harmless where every index is out of range. *Alternative*: an
  abstract class with a factory (a block could not be named as the type `ByteBlock(n)` makes).
- Settled on the user's delegation 2026-11-06 (branch `c-bindings-buffers`): **`bytes:` is `array:` for an `Array[byte]`**, with `free:`
  where C allocated the bytes. *Why*: the spec's copy was the one-element case of the generalized
  `array:`; one path copies both. *Alternative*: `bytes:` with no `free:` (a leak per call for a library
  that allocates).
- Settled on the user's delegation 2026-11-06 (branch `lock-turn-leak`): **a join answers only once
  the ended thread keeps nothing alive** ([threads](/spec/threads/)).
  An ending lightweight thread detaches its context (its frames, which hold its closure and its
  joint, stop being roots) and then marks its joint done and wakes its joiners, both under the heap
  lock, so no collection runs between the two and the joint outlives its last use even when nothing
  else reaches it. *Why*: before, the joint was marked done first, so a collection right after a join
  could still find the closure and the joint through the ended thread's frame; a program saw its
  memory freed one collection late, and a runtime test asserting an empty heap after a join failed
  under load. *Alternatives*: keep the old order and promise only that an ended thread's objects are
  collected eventually (weaker, and visible to any program measuring its heap); or clear the frame's
  slots before marking the joint done (the joint, then reachable from nothing for a thread nobody
  joins, could be collected while the ending thread still writes to it).

- Settled on the user's delegation 2026-11-06 (branch `lwt-timers`): **`sleep` parks a lightweight thread on a timer, and the
  monitor thread fires the timers.** `sleep` in a lightweight thread parks it with no guard and a
  timer at its deadline, as a timed wait parks, so its worker runs other threads; `main` and other
  threads that are not lightweight block, safe for the collector. The monitor, which already wakes
  once a tick while any worker is busy and sleeps until the soonest timer while none is, fires the
  timers that are due. *Why*: the monitor exists, already fires timed waits and already sleeps until
  the soonest one, so sleeps need no thread of their own and no new wake-up path; a sleep is never
  more than a tick late while workers are busy. *Alternatives weighed*: the poller's timeout (the
  poller of sockets, step 4, is not built yet; once it is, its wait could take the soonest timer as
  its timeout and the monitor could hand timers over, which this choice does not preclude); a timer
  thread of its own (one more operating system thread doing what the monitor does); keeping `sleep`
  a blocking call that the monitor compensates for (a worker per sleeper, which is what the
  lightweight threads are for). Review with the user only if sleep precision under 1 ms matters.

- Settled on the user's delegation 2026-11-06 (branch `lwt-timers`): **the timers are a binary heap of waiters keyed by deadline, ties
  broken by the order they were set, each waiter keeping its place in the heap.** Adding, firing and
  cancelling a timer each cost O(log n) under the timer lock, and reading the soonest is O(1);
  threads whose time runs out together are made ready soonest first, and two due at the same
  microsecond in the order they began waiting. A timed wait that its event ends first takes its own
  timer off when it runs again (its place makes that one step, not a search); the timer and the
  event race on the waiter's state, and only the one that changes it from pending makes the thread
  ready, so a wait answers exactly once and leaves no timer behind. *Why*: the linear list cost a
  scan per cancel and per tick, which ten thousand sleepers turn into a hundred million steps a
  second; a heap of the waiters themselves needs no allocation per timer beyond the heap's array.
  *Alternatives weighed*: a hierarchical timing wheel (O(1) add and cancel, but a tick-quantized
  deadline and more machinery than a heap that is already fast at this scale); a sorted list (O(n)
  insertion); a heap without places (cancel would search, O(n)).

- Settled on the user's delegation 2026-11-06 (branch `unix-sockets`): Unix domain sockets in `avin.net`: `UnixStream` and
  `UnixListener` mirror `TcpStream` and `TcpListener`, sharing the read-ahead code (`read`,
  `readLine`, `readAll`) through one private class both hold; `UnixListener.bind(path)` fails with
  `AlreadyExists` where anything is at the path, and the separate `bindReplacing(path)` removes a
  socket file nobody listens on (a probing connection is refused) before binding, leaving a live
  listener's path and any non-socket file alone; `close()` leaves the socket file; a path the
  platform cannot carry (103 bytes on macOS, 107 on Linux) is an `Err` with the platform's
  message, and a connect to a path with nothing at it is `NotFound`. Waits are the TCP ones: blocking
  bindings that park the lightweight thread's carrier as TCP's do, with no poller integration.
  WHY: a daemon and its command-line client need a local socket with the API they already know from
  TCP; silently deleting an existing path could remove another program's live socket or an unrelated
  file, so the safe `bind` refuses and the replacing form is explicit. Alternatives weighed: `bind`
  always removing a stale file (a surprise on a path that is a live socket or not a socket at all);
  `close()` removing the file (wrong for a listener handed between threads, and a second program may
  already have replaced it); one shared `Stream` trait over TCP and Unix (no use yet, and the
  language's traits would add a dispatch to every read). Review with the user: `bindReplacing`'s probe
  connection is seen by a live listener as an immediately closed connection.

**Open files in `avin.fs`** (branch `fs-file`, for the job manager's log following and event log; the
[system library](/spec/system-library/#open-files) chapter has the surface). Each choice below the
spec did not settle, each settled on the user's delegation 2026-11-06. Review with the user: the first two (a `File` kept to one thread, and no seek from the end).

- Settled on the user's delegation 2026-11-06: **the type is `File`, opened by `fs.open`, `create`, `append`, `openUpdate` and
  `createUpdate`.** WHY: the five modes cover every combination a program asks for, named for what
  happens to what is already there, as Sysl names them, and the whole-file functions keep their names.
  ALTERNATIVES: an `OpenMode` argument (one function, but a mode a program can get wrong at runtime
  and a longer call for the common case); only read, create and append (no way to overwrite part of a
  file).
- Settled on the user's delegation 2026-11-06: **a `File` is not `Sendable`.** WHY: its Sysl handle belongs to the thread that opened it,
  and a program that wants another thread to use a file opens it there; `TcpStream`'s hand-over
  machinery buys nothing for a file a program can reopen. ALTERNATIVES: make it `Sendable` with the
  owner check a `TcpStream` has. Review with the user if a file must move between actors.
- Settled on the user's delegation 2026-11-06: **seeking is from the start only, plus `seekToEnd()`; a negative offset is an `Err`, a
  seek past the end is allowed.** WHY: following a log needs "start at the end", and a tail needs
  absolute offsets, which `size()` gives; offsets back from the end are `size() - n`, with no second
  vocabulary. ALTERNATIVES: a `Whence` argument; a signed offset meaning from the end.
- Settled on the user's delegation 2026-11-06: **`readLine` answers an unterminated last line; `readCompleteLine` does not, and leaves
  the partial line unread.** WHY: a file followed while another program writes it shows half a line
  at some moment, and a follower must not act on it; a file read to its end wants the last line even
  with no `\n`. ALTERNATIVES: one `readLine` with the first meaning (torn lines when following), or
  the second (the last line of a file with no final newline lost).
- Settled on the user's delegation 2026-11-06: **`read(max)` and the line calls share one buffer of the `File`'s own, and `position()`
  is the logical position (the file's, minus what is buffered and unread).** WHY: lines need a
  buffer, and `read`, `seek` and `write` have to agree with it. A `write` or `seek` gives the
  buffered bytes back first. ALTERNATIVES: separate byte and line modes (a program mixing them gets
  surprises).
- Settled on the user's delegation 2026-11-06: **a read at the end is not sticky.** WHY: following a growing file is the point; the next
  read asks the file again. ALTERNATIVES: a latched end that `seek` clears (Sysl's own flag is not
  used).
- Settled on the user's delegation 2026-11-06: **misuse is an `Err`, not a trap: a call on a closed file, a read of a write-only file,
  a write to a read-only one, a negative seek offset; a negative `max` or `count` or tail size traps**
  like `TcpStream.read`'s, being a program mistake at the call. WHY: the first group depends on the
  state the program's files are in at runtime. The `Err` for those is `Other` with a plain sentence,
  as `TcpStream`'s closed connection is. ALTERNATIVES: a dedicated `IoError` variant for a closed file
  (more to match on for a rare case).
- Settled on the user's delegation 2026-11-06: **`readTail(file, maxBytes)` answers text, begins at a whole character, and replaces
  invalid bytes with U+FFFD; `readTailLines(file, count)` answers lines.** WHY: the caller wants to
  print or search the end of a log, where a cut character is garbage and an invalid byte elsewhere
  should not refuse the whole tail. A start inside a UTF-8 sequence skips the rest of that sequence
  (continuation bytes), never reading earlier bytes to complete it, so exactly the bytes asked for
  or fewer come back. ALTERNATIVES: back up to include the whole character (more than `maxBytes`);
  answer bytes and leave decoding to the caller; `InvalidUtf8` for a cut start.
- Settled on the user's delegation 2026-11-06: **opening a directory for reading or updating is `IsADirectory`.** WHY: the platform
  opens a directory for reading and fails only at the first read, which would show up as an
  unexplained failed read. ALTERNATIVES: leave it to the read.
- Settled on the user's delegation 2026-11-06: **the calls do not park a lightweight thread.** WHY: the whole-file `avin.fs` calls do
  not either, and a regular file's read does not wait for a peer. ALTERNATIVES: bind them
  `blocking` (needed only for files on a stalled network mount).

- Settled on the user's delegation 2026-11-06 (branch `json`): **JSON is the package `json`, in `packages/json` of this repository,
  module `json`, rather than `avin.json` in the standard library** ([JSON](/spec/json/)). Why: the
  system library chapter already says JSON is a package of its own; the package mechanism reads a
  path dependency and a git dependency on a directory of a repository (`//packages/json`), as the
  `ui` package is reached; and a domain library outside the standard library keeps the compiler's
  library small. Alternatives weighed: `avin.json` in the standard library (contradicts the system
  library chapter, and every compiler release would carry it); a repository `avin-lang/json` of its
  own (premature while the derivation templates it is to carry are still being built in this one).
  Review with the user: whether JSON should after all ship with the compiler, since marshal and most
  services need it.
- Settled on the user's delegation 2026-11-06 (branch `json`): **a JSON number is a `Long` when the text writes a whole number that
  fits a `long`, and a `Real` otherwise; a `Real` is always written with a fraction or an
  exponent.** Why: identifiers, counts and sizes stay exact past 2^53; writing `1.0` rather than `1`
  makes `parse(render(j)) == j` for every finite value, kind included. `-0` is the `Long` 0 (a whole
  number, and a `long` has no minus zero); `-0.0` is the `Real` minus zero. A whole number beyond a
  `long` is a `Real`, rounded. The `Real` form follows the usual shortest-digits rule, written out
  from 1e-6 up to 1e21 and with an exponent outside. Alternatives weighed: one `Num(real)` (loses
  integers beyond 2^53); keeping a number's text (`Num(text)`, exact for any size but every reader
  parses again); a third `Big(digits)` variant for integers beyond a `long` (exact, at the cost of a
  case every match must handle). Review with the user: the variant names `Long`, `Real`, `Str`,
  `Arr`, `Obj` (chosen because `Array` is a built-in type's name and `String`/`Object` read as
  types), and the companion's `obj`/`arr`/`str`/`num`.
- Settled on the user's delegation 2026-11-06 (branch `json`): **a number too large to be a finite `real` is an error, and a NaN or an
  infinity is written `null`.** Why: a `Real` holding an infinity cannot be written back as JSON, so
  accepting `1e400` would break the round trip; writing `null` keeps `render` total, as most writers
  do. Alternatives weighed: reading `1e400` as an infinity; trapping when writing a non-finite value.
- Settled on the user's delegation 2026-11-06 (branch `json`): **an object keeps every member, a key written twice included, and `get`
  and `apply` answer the last.** Why: reading is lossless and the order is the document's, which a
  log or a configuration file shown back to a person needs; the last-wins lookup is what most
  readers do. Lookup is a scan from the end, which is fast for the objects a protocol carries.
  Alternatives weighed: refusing a duplicate key (strict, but RFC 8259 allows it); the first one
  winning; members in a `Map` (fast lookup, loses order or needs an ordered map).
- Settled on the user's delegation 2026-11-06 (branch `json`): **arrays and objects nest at most 512 deep unless `parse` is given
  another limit, a scalar being 0 deep.** Why: the reader is recursive, and a hostile document of a
  million `[` would otherwise end in a stack overflow trap; 512 is far beyond any real document and
  far inside a lightweight thread's 8 MiB stack. Alternatives weighed: no limit; an iterative reader
  with no limit (more code, and the values built are still recursive to write); 1000 or 10000.
- Settled on the user's delegation 2026-11-06 (branch `json`): **a lone surrogate escape is an error.** Why: a `string` holds only
  characters, and U+D800 is not one. Alternatives weighed: replacing it with U+FFFD (silent loss);
  carrying it as WTF-8 (a string that is not UTF-8).
- Settled on the user's delegation 2026-11-06 (branch `json`): **an error's line ends at a line feed only and its column counts
  characters; it also carries the byte offset.** Why: the line feed is the only line end JSON text
  ordinarily has, a `\r\n` counts the same, and counting characters matches what an editor shows.
  The place is counted out only when an error is made, so reading pays nothing for it.
  Alternatives weighed: a lone carriage return also ending a line; columns in bytes.
- Settled on the user's delegation 2026-11-06 (branch `json`): **the accessors answer `Option`s, `apply` traps, and `asLong` takes a
  `Real` that is a whole number within a `long`.** Why: Options chain with `?` in a function
  answering an `Option` (`j.get("a")?.get("b")?.asInt()`), and `apply` is for a member the program
  knows is there, as a map's is; a producer writing `3.0` for a count is common. Alternatives
  weighed: a missing member answering `Null` (confuses absent with null); a cursor type with its
  own errors (a later layer, which the derivation's `JsonReader` covers).
- Settled on the user's delegation 2026-11-06 (branch `json`): **the corpus runs every package under `packages/` that has a `tests`
  directory, with `avin test` at `-O0` and `-O2`.** Why: a package's tests are what `avin test`
  runs in it, and the repository's own packages must be gated like the core library is. A package
  whose behaviour needs a window (`ui`) has no such directory.

- Settled on the user's delegation 2026-11-06 (branch `process-control`): **A `Child`'s process ids are recorded, and read by
  any thread** ([commands](/spec/system-library/#commands)). `pid()`, `group()`, `signal` and
  `signalGroup` read atomics the starting code writes as each program starts, never the command's
  handles, which belong to one thread, so a job manager can signal a child another thread is waiting
  for. `pid()` is the program started last (a pipeline's last stage, a sequence's latest part, as a
  shell's `$!` is), and `signal` reaches that one program; `kill` remains the call that reaches
  every program and stops a sequence. A command seen to have ended is refused with the platform's
  "No such process" rather than signalled, since its id may have been given again; one that ended
  unseen is a zombie the signal reaches harmlessly. Alternatives: `signal` through the owner, which
  traps on a child another thread waits for (what the marshal survey met); a `signal` reaching every
  program, which needs every program's id kept and the job's handles' state shared; `pid()` as an
  `Option`, rather than `0` for a command whose time ran out before it started a program.
- Settled on the user's delegation 2026-11-06 (branch `process-control`): **One group per command, and `signalGroup` without
  one is an `Err`** ([commands](/spec/system-library/#commands)). `inNewGroup()` puts every program
  of the command in one group its first program leads, the outermost setting deciding; a sequence's
  later part whose group has emptied leads a new one (joining a gone group is refused by the
  platform), and `group()` follows it. `signalGroup` on a command with no group of its own is
  `Err(Other("the command has no group of its own"))`, never a signal to the program's own group,
  and it still signals after the command ended, since a group outlives its leader while anything it
  started is left: the case a job manager kills for. Alternatives: a trap for `signalGroup` without a
  group (a daemon would die of a missed setting); a group per part; refusing the later part.
  **Review with the user**: whether `signalGroup` should trap.
- Settled on the user's delegation 2026-11-06 (branch `process-control`): **Standard error is redirected as output is**
  ([commands](/spec/system-library/#commands)): `errorsToFile`, `errorsAppendTo` and
  `errorsToOutput()`, the last sharing the output's open file (one offset, so neither stream
  truncates or overwrites the other), going into what `output()` and `lines()` capture, and, where
  the output is the program's own, opening `/dev/stdout` to append. On a pipeline every stage's
  errors go to the pipeline's redirection. The redirections apply output first whatever the order
  they were called in. Alternatives: one `bothToFile(file)` (a second name for a pair of calls);
  `Stdio`-like values passed to one `withStreams`; `2>&1` read as the output at the point of the call.
- Settled on the user's delegation 2026-11-06 (branch `process-control`): **Signals are watched in `avin.signal`, and a wait
  looks every 20 ms** ([`avin.signal`](/spec/system-library/#avinsignal)). The platform's facility
  is a counted flag, so `wait()` parks its lightweight thread and looks again every 20 milliseconds,
  the latency bound; `take()` asks once. Constants are `val`s named for what they mean (`Terminate`,
  `Interrupt`, `Hangup`, `User1`...), the user ones numbered by the runtime for the platform; sending
  stays in `avin.process` beside the process ids it takes. Alternatives: the POSIX short names
  (`TERM`), against Avin's naming; a `data Signal` (a closed set, while numbers are open, and
  `ExitStatus.Signalled` speaks `int`); a wake from the handler through the scheduler, which needs a
  runtime handler that is async-signal-safe, for a latency no program has needed yet.

- Settled on the user's delegation 2026-11-06 (branch `nested-literal-typing`): **A number literal nested in an argument takes the
  numbers the other arguments decide** ([generics](/spec/generics/#inference)). An argument that is
  or holds an integer literal (a collection literal, a tuple, or a call other than a conversion
  such as `long(1)`, with such a part) is checked after the arguments that hold none, against the
  type they decided so far, and an expression of literals alone (a literal, or an operation or `if`
  on such expressions) after both, so `assertEq(count(Box(1)), 1_000_000_000_000 + 1)` with a `long`
  result is a `long` whichever is written first. Where the decided type would otherwise be left open
  so a later argument may widen it, the argument is given its shape with the value types and every
  invariant type argument kept, and every class or other reference leaf at a covariant place left
  open. So `assertEq(f(), [1, 2])` with `f() -> Vector[long]` is a
  `Vector[long]`, while `same(dogs, [Cat(2)])` still joins to a `Vector[Animal]`. Why: the chapter
  already says a literal takes the type the other arguments decide, and a literal one level down is
  the same literal to a reader. Two limits, chosen as the smallest rule: of two arguments that
  both hold literals, the first written decides for the second (`same((1, x), (x, 2))` for
  `x: long` is an error, where a part-by-part solution would type it); and choosing an overload
  checks a nested literal once at its own type where the candidates' parameters differ (`kind(Some(1))`
  beside `kind(o: Option[long])` and `kind(o: Option[string])` fits neither), as only a bare literal
  is tried against each candidate. Alternatives: leaving each literal unknown until its use settles
  it, as a type argument is (solves both limits, but makes every literal an inference variable);
  trying each candidate's parameter on a nested literal by its structure (collection, tuple and
  constructor shapes). Review with the user: whether either limit is worth lifting.

- Settled on the user's delegation 2026-11-06 (branch `stdout-flush-isnan`): **standard output gets an explicit `flush()`; reading flushes it; `real` gets `isNaN`, `isInfinite` and `isFinite`.** `flush()` is a prelude function and a method on `StandardOutput` and `StandardError` (a no-op beyond flushing output, as error keeps no buffer); a read of standard input that must wait flushes output first. The classifiers are methods on `real` in `avin`, written in Avin (`self != self`; comparison with the infinities; `self - self == 0.0`); `real.NaN`, `real.PositiveInfinity` and `real.NegativeInfinity` already existed, and `real` is the only floating type. WHY: a drawing program must push a frame without a newline, and a prompt must be seen before the wait; the methods read as `char`'s do, where the `avin.math` functions are not methods. ALTERNATIVES: a `Writer.flush` on the trait (every writer would need a meaning); flushing only on `print` (defeats buffering); `math.isNaN(x)` functions (the spec keeps `avin.math` for elementary functions). Review with the user if a `Writer`-level `flush` is wanted.

- Settled on the user's delegation 2026-11-06 (branch `exit-hooks`): **Exit hooks are `process.onExit(@sendable body: () -> unit) ->
  ExitHook`, with `cancel() -> bool`, run newest first and once each** ([exit
  hooks](/spec/system-library/#exit-hooks)). Why: a library that changes the world outside the
  program (tui's raw mode and alternate screen) must put it back on every ending, and the language
  has no `finally` or `defer`; a hook per change, cancelled when the change is undone normally, is
  the smallest shape, and newest first undoes changes in the reverse of the order they were made.
  Alternatives: one global handler to set (libraries would overwrite each other); a `defer`
  construct (scoped to a frame, so it cannot run on a trap or an `exit` from another thread);
  oldest first.
- Settled on the user's delegation 2026-11-06 (branch `exit-hooks`): **The thread that ends the program runs the hooks**: the main
  thread when `main` returns, the thread calling `exit`, the trapping thread after its report
  (an escalated actor failure and running out of memory included), a test's process as a program.
  The other threads go on running meanwhile; one that traps or calls `exit` waits for the end
  silently, the first ending being the program's. So the body is `@sendable`. Why: a trap on another
  thread cannot hand work to the main thread, which may be blocked or be the one that trapped, and
  stopping every other thread first would stop the threads a hook needs (a future's pool) or leave
  them stopped holding locks. Alternatives: always on the main thread (impossible when it is the
  one waiting or trapping); stopping the other threads first; hooks free to capture anything
  (a data race with the threads still running). **Review with the user**: whether a hook should
  rather be allowed to capture non-sendable state from the main thread for the common case.
- Settled on the user's delegation 2026-11-06 (branch `exit-hooks`): **A hook that traps or calls `exit` never goes on, and the hooks
  after it still run**, on top of its stack rather than after unwinding it (the runtime's frames
  hold counted values a jump past them would leak). The trap is reported as `avin: trap in an exit
  hook:` and the program then ends as a trap ends it, even where `main` returned or `exit` was
  called; an `exit` in a hook keeps the status of the ending that ran the hooks. A trap in a hook
  while a mutex is held stops the program at once, since a later hook may need what the lock
  guards. Why: one bad hook must not keep the others from putting their state back, and a failed
  cleanup must not end with a status that says success. Alternatives: a trap in a hook ends the
  program at once; an `exit` in a hook replaces the status (the last word wins, which a cleanup
  should not have).
- Settled on the user's delegation 2026-11-06 (branch `exit-hooks`): **A hook registered while the hooks run runs too**, at once after
  the one that registered it; `cancel()` answers `false` for a hook that has run or begun. Why: the
  registry is a stack the runner pops, so a hook a cleanup registers is the newest. Alternative:
  ignore registrations once the program is ending (silently drops cleanup a hook asked for).
- Settled on the user's delegation 2026-11-06 (branch `exit-hooks`): **No hook runs on a stack overflow or on an unwatched signal.**
  A stack overflow is reported by a handler on its own small stack where no Avin code can run. A
  signal the program does not watch keeps its platform action; a program that wants its hooks on
  `Terminate`, `Interrupt` or `Hangup` watches it and calls `exit`, as tui already watches them.
  Why: running hooks on an unwatched signal needs a watcher thread in every program with a hook, and
  a library watch of its own would change what the signal does for the program, which
  [`avin.signal`](/spec/system-library/#avinsignal) promises it does not. Alternatives: the runtime
  watches `Terminate`, `Interrupt` and `Hangup` whenever a hook is registered and nothing else
  watches them, ending with `exit(128 + sig)` after the hooks; running hooks from the overflow
  handler on a fresh stack (the thread may have overflowed inside a lock). **Review with the
  user**: the signals, if a program wants cleanup on Ctrl-C without watching for it.

- Settled on the user's delegation 2026-11-06 (branch `generic-joins`): **Two tuples join part by part** ([generics](/spec/generics/#inference)).
  `(Dog, long)` and `(Cat, long)` agree on `(Animal, long)` wherever two types are joined: the
  arguments of one type parameter, the branches of an `if`, the arms of a `match`, the elements of a
  literal, and a tuple inside a covariant argument (`Option`, `Vector`, `List`). Each part joins as two
  types alone do, so a part of an invariant type (`Array[Dog]` beside `Array[Cat]`) or two different
  value types (`long` beside `int`) still have no join, nor have tuples of different sizes. Why: the
  chapter already lists a tuple part among the places a value is read out, and two classes alone join;
  a tuple of them failing to was an omission in the checker's join, which handled classes and generic
  types but not tuples. Alternatives: requiring the tuple type to be written (a contortion the class
  case does not need). Nothing here deserves a conversation.

- Settled on the user's delegation 2026-11-06 (branch `generic-joins`): **A number literal among values that agree on one type takes the
  type the others decide** ([generics](/spec/generics/#inference), [collections](/spec/collections/#literals)).
  Among the elements of a collection literal, the keys or values of a map literal and the branches of
  an `if`, the values are checked in tiers, as a call's arguments are: those holding no literal first,
  then values built around a literal (`Some(2)`, `(2, "b")`, `[2]`), then expressions of literals alone
  (`2`, `2 * 3`, `-5`), then empty literals, each later tier given the shape of what the earlier agree
  on with its value types kept and its classes left open. So `[long(1), 2]`, `[2, long(1)]` and
  `if c then 2 else big` are `long`, and `[1.5, 2]` is `Vector[real]`. A `match` arm takes part only
  where it is an expression of literals alone, since an arm checked after the others would be checked
  outside its pattern's scope, and a value built around a literal may name a pattern's binding.
  Diagnostics for values with no common type still read in written order, at the later value. Why:
  `types.md` already says a literal takes its width from the other operand, and the literal chapter
  says elements agree as an `if`'s branches do; `[long(1), 2]` being an error while `long(1) + 2` is
  not read as a gap to programs. Alternatives: only the collection literal (leaves `if` and `match`
  stricter than the literal that is said to agree as they do); leaving each literal an inference
  variable settled at its use (the alternative weighed for nested literals too, deferred for the
  same reason); deferring every `match` arm that holds a literal by re-entering its pattern's scope
  (more machinery for a rare shape). Review with the user: whether `match` arms such as `Some(2)`
  beside `Some(big)` should also agree.

- Settled on the user's delegation 2026-11-06 (branch `okor-command-message`): **`Option.okOr` / `okOrElse` and `CommandError.message()`**.
  `Option[T]` gains `okOr[E](error: E) -> Result[T, E]` and `okOrElse[E](f: () -> E)`, the `Result`
  counterparts of `toRight`; `Result.ok()` and `err()` already existed. `okOr` takes its argument
  by value and `okOrElse` a function called only for `None`. WHY: the two names say which form
  evaluates; the by-name form already exists as a lambda call, and a by-name `okOr` would leave
  `okOrElse` redundant. Alternatives: a by-name `okOr` only (as `toRight` is; one method, but an
  argument with effects reads as eager and is not), or `toRight`-style names (`toOk`). Review
  with the user if `okOr`'s eagerness should instead match `toRight`'s.
  `CommandError.message() -> string` reads `exited with status N`, `killed by signal N (Name)`
  (`unknown` for a signal `avin.signal` has no name for), `timed out`, and an `Io` error's own
  `IoError.message()`. WHY: one sentence per variant in `IoError.message`'s lower-case style, the
  signal's name beside its number as `avin.signal` names them. Alternatives: the number alone;
  `ExitStatus.message()` as well (not added: nothing needs it yet).

- Settled on the user's delegation 2026-11-06 (branch `sysl-098`): **`Child.detach()`** ([commands](/spec/system-library/#commands)).
  `detach() -> long` lets go of every program of the command still running without stopping it
  (Sysl's `Spawned.detach`) and answers `pid()`, the same `long` the `Child` already answers it as;
  a sequence's later part not yet started is never started, and the sequence's own redirected files
  are closed, the running part holding its copies. Afterwards `waitFor`, `isAlive` and `kill` trap
  (`a detached `Child` is not waited for, asked after or stopped`), while `pid`, `group`, `signal`,
  `signalGroup` and `residentBytes` answer from the ids recorded at each start; a second `detach`
  answers the pid again, and detaching a command already waited for lets go of nothing. Nothing reaps
  a detached program: one that ends while the program runs is a zombie until the program ends.
  WHY: once detached the `Child` can no longer see the command end, so an answer from `waitFor` or
  `isAlive` would be a guess, and a trap is how the library treats a handle used against its state
  (a `Child` used from the wrong thread); keeping the id-based operations lets a program still stop
  what it started. ALTERNATIVES: `waitFor` still waiting by pid (reaping, which makes a detached
  command a waited one again and races a reaper the program may run itself); `Err` from `waitFor`
  and `false` from `isAlive` (an `isAlive` of `false` would be a lie about a running program);
  answering `int` as Sysl does (the `Child` answers every id as a `long`); refusing to detach a
  pipeline or a sequence. **Review with the user**: whether a detached command should be reaped
  by the runtime so it leaves no zombie.
- Settled on the user's delegation 2026-11-06 (branch `sysl-098`): **`totalMemory` and `residentBytes`** ([the environment and the
  platform](/spec/system-library/#the-environment-and-the-platform)). `process.totalMemory() ->
  Result[long, IoError]` and `process.residentBytes(pid: long) -> Result[long, IoError]` over Sysl's
  `total_memory` and `resident_bytes`, the `u64` bound as a `long` (machine memory and a process's
  resident pages are far below 2^63), and `Child.residentBytes()` for the program `pid()` names. A
  process that is not there (Sysl's `NotFound`, which from Sysl 0.9.9 it answers for a reaped child
  and an unused pid alike), a pid of zero or less and one past the largest `int` are
  `Err(NotFound)`; Sysl's `NotPermitted` is `Err(PermissionDenied)`, which macOS answers for pid 1
  and Linux does not (it reads any process's size). A `Child` seen to have ended, or with no program
  started, is `Err(NotFound)` without asking. WHY: the pid is a `long` everywhere else in
  `avin.process` (`pid()`, `signal`); `NotFound` and `PermissionDenied` are the `IoError` cases a
  program tells apart, where Sysl's `NotPermitted` would otherwise read as `Other(text)`.
  ALTERNATIVES: `pid: int` as Sysl takes it; `Option[long]` (loses why); passing Sysl's
  "no such process" through as `Other(...)` as `signal` does.

- Settled on the user's delegation 2026-11-06: **A group of only ended, unreaped processes answers "No such process"** (branch
  `group-signal-eperm`; [commands](/spec/system-library/#commands)). `signalGroup` maps the
  platform's permission refusal to `Err(Other("No such process"))`: on macOS `kill(-pgid)` answers
  EPERM while a group holds only zombies and ESRCH once they are reaped, so the same program saw
  either answer by the timing of the reaping. Every process in a group a command made belongs to the
  program's user, so a refusal can only mean that. WHY: one answer for an empty group, however the
  reaping falls. Alternatives: leave the platform's answer and have callers wait for reaping (no way
  to observe it, and the flake stays); report the permission error only for a group the program did
  not start (not reachable, since `signalGroup` only names groups of its own commands).

- Settled on the user's delegation 2026-11-06 (branch `parallel-codegen`): **A build compiles the program's LLVM text as codegen units
  at once, one clang each, at most eight, and keeps each unit's object in the toolchain's cache by
  its text.** A function only one function calls goes with its caller (kept local, so still inlined
  there: without this a hot single-caller function across a cut ran ~6% slower at `-O2`); other
  functions are placed by a hash of their name, globals with their first user, aliases
  with their global; a local crossing units is promoted to a hidden `avin.cgu:` name; at `-O2` small
  functions (body cost at most 32) and short constants are copied `available_externally` into the
  units that use them. Why: clang -O2 on one 34 MB module was ~10 s of a 15 s rebuild of tui's `top`
  on one core of eighteen; the hash placement keeps an edit to one function inside one unit, so the
  cache recompiles one unit. Eight: on a machine of six performance and twelve efficiency cores more
  units land on the slow cores and repeat more declarations and copies (the same cap Sysl's builds
  measured). Alternatives weighed: units balanced greedily by size (better balance, but every edit
  that shifts a size reshuffles the units and defeats the cache); copying small local functions as
  local copies (cheaper, but a function would have one address per unit); no copies at `-O2` (a
  call across units could never be inlined); no object cache (a one-line edit recompiles every
  unit); one unit per processor without a cap. `AVIN_CODEGEN_UNITS` overrides the count. **Review
  with the user** only if `-O2` run-time performance across units ever shows up in a profile: the
  copy limit is the knob.
- Settled on the user's delegation 2026-11-06 (branch `parallel-codegen`): **The cache of unit objects is trimmed to 1 GiB**, oldest
  first, after a build that compiled a unit, sparing anything used in the last hour; a reused
  object's time is refreshed. The test corpus links without keeping objects. Why: a build of `top`
  keeps ~8 objects of a few MB per level, so 1 GiB holds dozens of builds; the hour keeps a
  concurrent build's objects from being evicted under it. Alternatives: a count of objects; no
  trimming.

- Settled on the user's delegation 2026-11-06 (branch `process-exec`): **`process.exec` and its environment**
  ([becoming another program](/spec/system-library/#becoming-another-program)).
  `exec(program, args) -> Result[Nothing, IoError]` over Sysl 0.9.9's `sysl.process.exec`,
  inheriting the environment, and an overload `exec(program, args, environment: Map[string,
  string])` giving exactly that environment, nothing inherited, a bare name looked for on its `PATH`.
  Why: a `Result` with `Nothing` on its `Ok` side lets `?` pass the failure on and says in the type
  that success never comes back; a whole `Map` matches what `process.environment()` answers, so
  adding to the program's own is `environment().updated(...)`, and a self-upgrading daemon wants the
  environment it was started with. Alternatives weighed: answering a bare `IoError` as Sysl does
  (no `?`); a `Command.exec()` carrying `withEnv`/`clearEnv` edits (a command's directory, time
  limit and redirections have no meaning for an `exec`, and would need refusing); a vector of
  environment edits. Standard output is flushed by the Sysl call (`fflush` of every stream, which
  is where Avin's output is buffered). A `match` over the answer still needs an `Ok(_)` arm, since
  exhaustiveness does not yet treat a variant holding `Nothing` as impossible. **Review with the
  user** whether such arms should be dropped from exhaustiveness.
- Settled on the user's delegation 2026-11-06 (branch `process-exec`): **`waitPid` and `tryWaitPid` answer `Err(NotFound)` for
  anything not there to wait for**: a pid that is not this program's child or was already reaped
  (Sysl's `Other(10)`, `ECHILD`), and a pid of zero or less or past the largest process id (Sysl's
  `Other(22)` for the first, refused before the call for the second). Why: it matches
  `residentBytes`, where a process that is not there, a reaped child among them, and a pid of zero
  or less are `NotFound`, and gives a caller one case to match. `waitPid` is bound `blocking`, as
  `Child.waitFor` is, and both answer the same mirrored `ExitStatus`. Alternatives weighed:
  `Other` with a sentence (unmatchable by kind); a trap for a pid of zero or less (a pid read back
  from saved state is data, not a programming error).
- Settled on the user's delegation 2026-11-07 (branch `fs-rename`): `fs.rename(from, to) -> Result[unit, IoError]` ([system
  library](/spec/system-library/)) **replaces** what is at `to`, in one step, and does not refuse.
  A file replaces a file; a directory replaces an empty directory; a directory that holds entries
  is `DirectoryNotEmpty`; a file onto a directory is `IsADirectory`, a directory onto a file
  `NotADirectory`. Renaming a path to itself succeeds and changes nothing. WHY: the call exists for
  the atomic write (write `state.json.new`, rename it over `state.json`), which is only atomic if a
  reader sees the old file or the new and the replacement never fails because the old one is there;
  replacing is also what the platform's own rename does, so no extra check can race. ALTERNATIVES:
  refuse with `AlreadyExists` when `to` exists (a separate check-then-rename is racy, and the
  atomic write would need a remove first, losing atomicity); two functions, `rename` and
  `renameReplacing`. Across file systems the call fails and moves nothing (`Other`), never copying.
  Review with the user if a refuse-by-default `rename` is preferred.
- Settled on the user's delegation 2026-11-07: **A `Zone` holds its rules in Avin until Sysl's own zone can be held** (branch `local-time`).
  A zone is a final class of a name and its rules: one fixed offset, or the bytes of the host
  database's file for it as an immutable `Vector[byte]`. Each question (`offsetAt`,
  `isDaylightSavingAt`, `abbreviationAt`) passes the bytes to the runtime, which decodes them with
  Sysl 0.9.9's `sysl.time.tzif` and answers. Why: that release's decoded `tzif.Zone` is a view of the
  bytes it was decoded from, so it cannot be held as a handle past the call that made it; the bytes
  can, and a zone so held is an immutable value any thread may share, as the chapter says, where a
  handle belongs to one thread. Decoding a table walks its header, short beside reading it.
  Alternatives weighed: a handle over the borrowed view (a dangling reference once the shim's copy
  is freed); a runtime table of decoded zones under a lock, handed out by number (global state and a
  lock for what is a pure function of the bytes); waiting for Sysl 0.10.0's owning `Zone` (marshal
  needs the local time now). When Avin moves to Sysl 0.10.0, `Zone` becomes a handle over its zone
  with no change to Avin's API. Review with the user: the roadmap planned the handle from the start.
- Settled on the user's delegation 2026-11-07: **The local zone is the one `TZ` names, else the one `/etc/localtime` is, read from the
  database like any other** (branch `local-time`). `TZ` with a leading `:` dropped is a zone's name,
  a `TZ` that is a path is that file, named for what follows its `zoneinfo/`; with `TZ` unset,
  `/etc/localtime`'s bytes are the table and its link's target after `zoneinfo/` the name
  (`localtime` where it is a copy). An empty `TZ`, one naming no zone, one that is not a zone's
  name, and a file that does not decode give `utc`. Why: one source for the offset, the abbreviation
  and daylight saving, and a local zone equal to `zone(name)` of its own name, so
  `localZone() == zone("America/New_York")?` where that is the host's zone. Alternatives weighed:
  the offset from `localtime_r` (Sysl's `local_offset`) with the abbreviation from the table (two
  sources that could disagree, and a POSIX rule `TZ` such as `EST5EDT` with no file would have an
  offset and no abbreviation); a zone named `local` (never equal to the named zone it is). A POSIX
  rule string with no file of its name in the database is therefore `utc`.
- Settled on the user's delegation 2026-11-07: **Pattern details the chapter left open** (branch `local-time`): `||` is a bar between the bars
  too (`|a||b|` writes `a|b`), as it is outside them, so any text can be quoted; `DDD` pads the day of
  the year to three digits, as `DD` pads the day of the month to two; `YY` is the last two digits of
  the year's magnitude (`01` for year `-1`); `YYYY` outside `0` to `9999` writes the year as the
  display does (`+12345`, `-0001`); `z` and `zz` of UTC are `+00:00` and `+0000`, never `Z`, so a
  pattern writes one shape for every zone (a literal `Z` is written as itself); `f` … `ffffff` cut
  the fraction rather than rounding, so a reading never rounds into the next second; and the whole
  pattern is read before anything is written, so a malformed pattern traps as malformed whatever
  value it is given. Alternatives weighed: `DDD` unpadded (a column of days of the year would not
  line up); `Z` for UTC's `z` (a pattern's output would change shape with the zone).
- **Settled on the user's delegation 2026-11-07 (branch `stop-signals`): `avin.signal` names `Stop`, `Continue`,
  `TerminalStop` and `Child`.** Each is an `int` numbered by the runtime for the platform
  (`avin.rt.job_signal`, over Sysl's `STOP`, `CONT`, `TSTP` and `CHLD`, as `User1` and `User2` are
  over `USR1` and `USR2`), since all four differ between macOS and Linux. **`Stop` cannot be
  watched**: `watch(Stop)` is `Err(Other("Invalid argument"))`, the refusal `Kill` already gets,
  because the platform lets nothing catch it. `Continue`, `TerminalStop` and `Child` can be
  watched like any other signal; a watched `TerminalStop` no longer stops the program, and a
  watched `Child` is raised whenever a program the program started ends, is stopped or continues.
  A status a signal ended also names them (`killed by signal 19 (Stop)`), and now names `User1` and
  `User2`, which it called "unknown". Why: a supervisor pauses a job's process group with `Stop`
  and `Continue`, and needs `Child` to see an end that races a restart; an `Err`, not a trap, is
  the existing treatment of a signal that cannot be watched, and a refusal is a value the caller
  can act on. Alternatives weighed: a trap on `watch(Stop)` (inconsistent with `Kill`, and a
  platform-dependent number would make "this one traps" a hidden fact of the platform); leaving
  `Stop` out of the library and sending a literal number (the number is not portable); spelling
  `Continue` as `Cont` (the library spells every other name out). Review with the user: whether
  `TerminalStop` should be left unwatchable, as a program that traps Ctrl-Z rarely means to.
- Settled on the user's delegation 2026-11-07 (branch `lambda-after-literal`): **A lambda's result takes part in a call's literal tiers**
  ([generics](/spec/generics/#inference)). A lambda whose parameter types are known is checked in
  the tier of the value it yields (the last item of its block; the lowest tier among an `if`'s or a
  `match`'s branches), after the other arguments of that tier, rather than after every argument. So
  `pick(s -> s.toLongOption(), 0)` for `pick[T](read: (string) -> Option[T], absent: T)` takes
  `T = long`, as passing a named function does, and `pick(s -> s.toRealOption(), 0)` takes `real`;
  a lambda yielding `Some(1)` or `1` takes the numbers the other arguments decide. A lambda that
  leaves its body by `return` or `?`, and a `match` function, are still checked last, since such a
  body needs its result type known before it is checked. Why: the settled number-literal decision
  says a literal gives way to a type from a neighbour, and a lambda's result is a neighbour; a
  lambda checked last let the `0` fix `T = int` first, so the lambda and a named function of the
  same type behaved differently (marshal met it). Alternatives: deferring the literal instead of
  advancing the lambda (the same order in effect, but it reaches only literals, not the lambda's own
  literal result); checking a lambda with `?` early against a result type known only in shape (a
  larger change to how `?` finds the type it returns, left for when a program needs
  `pick(s -> Some(s.toLongOption()? + 1), 0)` to be a `long`). Review with the user: whether a
  lambda that uses `?` should also take part, which needs `?` to work from a result known in shape.
- **Settled on the user's delegation 2026-11-07 (branch `hash-digest`): `avin.crypto`, digests over `sysl.crypto`.**
  The module is `avin.crypto`; it has `sha256`, `sha512`, `sha1` and `md5` (each taking a
  `Vector[byte]` or a `string`, hashed as UTF-8, answering a `Vector[byte]`), `hmacSha256` and
  `hmacSha512`, `toHex`, and incremental `Sha256()`, `Sha512()`, `Sha1()` and `Md5()` with `update`
  and `finish`. WHY: marshal hand-wrote FNV-1a to key a build cache because the library had no
  digest, and Sysl already has SHA-2, SHA-1, MD5 and HMAC; a binding cannot disagree with Sysl.
  `avin.crypto` rather than `avin.hash` because the module holds keyed digests, and "hash" in Avin
  already means the trait behind `HashMap`. A digest is a `Vector[byte]`, not a string, so it
  compares, keys and stores as bytes and hex is one call away. Details: `finish()` caches its digest
  and answers it again, and `update` after `finish` traps (a Sysl hasher cannot be finished twice);
  the hashers are Avin classes over private handles because a handle has no constructor; the
  one-shot digests and HMACs go through small `avin.rt` functions (`runtime/avin/rt/digest.sysl`)
  because Sysl's one-shot functions answer a fixed array (`[32]u8`), which no binding result takes,
  and Sysl keeps its streaming HMAC private. ALTERNATIVES: `avin.hash` (above); digests as hex
  `string` (loses the bytes); one-shots through the Sysl hasher handle only (cannot reach HMAC);
  making the boundary accept fixed `[N]u8` results (a compiler change for a handful of functions);
  an HMAC written in Avin over the hashers (a second implementation to keep correct); also offering
  SHA-224, SHA-384, HMAC-SHA-1 and HMAC-MD5 (Sysl has them, nothing asks) and a constant-time
  `verify` (Sysl has one; add it when a program checks tags). Review with the user: whether `sha1`
  and `md5` belong in the standard library at all, and whether the other SHA-2 widths and `verify`
  should be added now.

- **A top-level name declared twice is reported with the first declaration's place, and a clash
  between two files of a module suppresses every other diagnostic.** Settled on the user's
  delegation 2026-11-07. Each duplicate message ends `; first declared at FILE:LINE:COL`. When the
  clash spans files, the program's other errors are dropped before printing, because they are almost
  always cascades of the clash (two `Filter` types in one module gave 48 conformance errors and no
  word of the clash, and the CLI shows five). A clash inside one file keeps its other errors, since
  existing reject cases mix a duplicate with unrelated errors and the duplicate is easy to see there.
  WHY: the clash is the cause and must be the error read first. ALTERNATIVES: suppressing for
  same-file clashes too (hides real unrelated errors in one file); ordering duplicates first without
  suppressing (the cascade still fills the five shown). Review with the user: whether same-file
  clashes should suppress the cascade too.

- **File metadata in `avin.fs`** Settled on the user's delegation 2026-11-07: `metadata(path)` answers
  `Result[Metadata, IoError]` with `Metadata(size: long, modified: Instant, isDir: bool,
  isFile: bool)`, from one `stat`, so the fields describe one entry; `size(path)` and
  `modified(path)` are shorthands for the two fields a program usually wants, and `setModified(path,
  instant)` sets the modification time (and the access time with it). Every call follows symbolic
  links, as `exists`, `isDir` and `isFile` do; a dangling link is `NotFound`. `modified` is an
  `avin.time.Instant`, the type `now()` answers, so a program compares a file's age with
  `now() - modified`-style arithmetic and nothing else. Why: marshal's retention kept `.used`
  sidecar files because a file's age and size could not be read; one `stat` is also one trip, and
  a value is not three answers about three possibly different files. Alternatives weighed:
  separate `size`/`modified` only (three trips for three facts, no directory answer); returning
  Sysl's whole `Meta` (owner, inode and permission bits are not what Avin programs ask yet, and
  they tie the type to POSIX); a `linkMetadata` that does not follow (nothing needs it yet, and
  `avin.fs` offers no way to make a link to ask about); `setModified` omitted (Sysl has
  `set_modified`, so it costs a line, and it lets "last used" be recorded without writing).
  Times are microseconds, the library's resolution.

- Settled on the user's delegation 2026-11-08 (branch `runner-streaming`): **`avin test` pushes its report out as it goes, and on SIGTERM or
  SIGINT writes out what it holds and names the work in progress.** The report text of a run is
  unchanged. The report is pushed to its reader when a test finishes and at least half a second has
  passed since the last push (the first finish always pushes), and at the end of the run, so a log
  read while the run is alive shows every test but the last half second's, and speed is unchanged.
  On SIGTERM or SIGINT the runner pushes the report at once, then writes `avin test: interrupted by
  SIGTERM; in progress:` (or `SIGINT`) and one line for each piece of work still running, in the form
  `run      <file>  <test>  <seconds>s` for a test and `compile  <program>  <seconds>s` for the build of
  the program, stops the test processes it started and exits with 143 (TERM) or 130 (INT), within
  about 20 ms of the signal. A watcher thread reads the signals, so a build blocked in a tool, or a
  compiler pass that is busy, cannot keep the lines from being written. The new option `avin test
  --prefix` puts the file's path and a colon before every result line (`a.avin:   ok    passes  3ms`),
  for runs whose output is interleaved with other runs' in one log; headings and the summary are not
  prefixed. Why: a job stopped at its deadline left a zero-byte log because the report sat in a
  buffer until the run ended, and a hung build named nothing. Alternatives weighed: flushing after
  every line (cost: a write for every line of a many-test run), a prefix always on (changes every
  existing report), a prefix on only when output is not a terminal (the same program would write
  different text by where it was sent), and polling the signal flag between tests (a blocked test or
  build would never reach the poll). Review with the user: the option's name `--prefix`, and the exit
  codes 143 and 130 (the shell's 128 + signal convention).
- Settled on the user's delegation 2026-11-09 (branch `with-update`): **`with` is refused on a data type with several variants**, and a
  path through a field of such a type (`Option` among them) is refused too. Why: variants are
  constructors, not types, so no static type says which variant a value is and which fields it has;
  the language already refuses `s.radius` on a `Shape` for the same reason, and a `match` arm makes
  the new variant directly (`Circle(r) -> Circle(r + 1)`). Alternatives weighed: accepting it where
  every variant has every named field, with the same type, and choosing the variant at run time (a
  hidden `match`), which makes the form's cost and its errors depend on edits to other variants; and
  trapping on a variant without the field, which turns a compile-time error into a run-time one.
  Review with the user: whether the every-variant-has-it form is wanted for enums whose variants
  share a common header.
- Settled on the user's delegation 2026-11-09 (branch `with-update`): **a `with` keeps the base's type arguments**: `Box[int] with
  { v = "s" }` is a type error, not a `Box[string]`. Why: the result is then always the base's type,
  as the desugaring to a copy says, the value of each change is read against a known field type (so
  literals and `None` take it), and a field the change does not name, which may hold a `T`, never
  has to be re-checked at a new type. Alternative weighed: re-inferring the type arguments from the
  changed fields and the kept ones, which is what a call of the variant would do; it is left for an
  explicit construction, `Box("s", b.label)`.
- Settled on the user's delegation 2026-11-09 (branch `with-update`): **`with` is read as the update only when the brace follows it on
  the same line**, as a postfix tail binding tighter than every prefix and infix operator; before
  anything else it is an ordinary name, so `a with b` stays the infix call of a method `with`. Why:
  keywords are contextual, and `Isolated.with` and the library's `shared.with(f)` keep their names.
  The cost: an infix call of a method `with` whose argument is a set or map literal must be written
  `a.with({…})`. Alternative weighed: making `with` a reserved word, which would rename those
  methods.
- Settled on the user's delegation 2026-11-09 (branch `with-update`): **a misspelt field is reported with the nearest field** when it is
  at most two single-byte edits away and fewer edits than the name's length (`nmae` suggests `name`;
  a one-letter name suggests nothing). Alternative weighed: listing every field of the type, which is
  long for wide types.
- Settled by the user 2026-11-09: **placeholder lambdas `_ + 1`** ([placeholder
  lambdas](/spec/types/#placeholder-lambdas); branch `placeholder-lambdas`). Each `_` in a call's
  argument is a parameter of its own, in the order written, and the smallest argument holding it is
  the lambda's body: `xs.map(_ + 1)`, `xs.map(_.name)`, `xs.reduce(_ + _)`, `xs.map(f(_, 2))`. The
  right side of a `val` with a declared function type takes one too. Parameter types come from the
  expected type, as for any lambda. Why: the user's rule that Avin adopts the conveniences that add
  no complexity; the short form reads as what it does and is the usual one for a one-step function.
- Settled on the user's delegation 2026-11-09 (branch `placeholder-lambdas`): **a `_` that is a whole argument belongs to the argument
  around its call**, so `xs.map(f(_, 2))` is `xs.map(x -> f(x, 2))` and `f(_)` is never the identity
  lambda `f(x -> x)`. Where a call with such a `_` is itself no argument and no declared value's
  right side (`f(_, 2)` as a statement), it is an error asking for the lambda to be written out.
  Why: the identity is rarely what `f(_)` means, and `x -> x` is short; reading the whole call as the
  lambda is what makes partial application, `f(_, 2)`, work. Alternative weighed: the identity
  lambda, which makes `f(_, 2)` a call of `f` with a function.
- Settled on the user's delegation 2026-11-09 (branch `placeholder-lambdas`): **`xs.map(f(_))` is an error saying to pass `f`**: a
  placeholder lambda that only calls a function named in lowercase with all its parameters in order
  (`f(_)`, `add(_, _)`) is that function, and functions are already values. A name starting in
  uppercase (`Some(_)`, `Point(_, 0)`) is taken for a constructor and makes the lambda. Why: one
  spelling for one meaning, and the error teaches that a function is passed by name. Alternatives
  weighed: accepting it as `x -> f(x)` (harmless, but two spellings), and deciding by what the name
  resolves to rather than its case, which needs the parser to know the checker's answer; the case
  test follows the settled naming rule. Review with the user: whether the redundant form should
  simply be allowed.
- Settled on the user's delegation 2026-11-09 (branch `placeholder-lambdas`): **where a placeholder is searched for.** Through
  operators of every precedence, member access and method calls, tuple parts, `is`, ranges, `?`,
  `with`'s base, a call's callee and a call's arguments that are `_` alone; never into a lambda, a
  block, `if`, `match`, a collection literal, an interpolation, an assignment or another call's other
  arguments. A `_` in those is an error, not a lambda around the nearer construct. Why: the set is
  the expressions that read as one step on one value, and a `_` inside a nested lambda or a branch
  would make the reader search for which lambda it belongs to. Alternatives weighed: searching
  everything to the argument (makes `xs.map(n -> n + _)` a lambda returning a lambda) and searching
  operators only (rejects `(_, 0)` and `_ is Some(_)`).
- Settled on the user's delegation 2026-11-09 (branch `placeholder-lambdas`): **a declared value takes a placeholder lambda only by its
  written type**: `val f: (int) -> int = _ + 1` and any declared type other than a tuple; an
  undeclared `val f = _ + 1` is an error, as are a function's body, `return` and an assignment's
  right side, whose expected type the parser does not see. Why: the brief's rule, kept syntactic so
  the lambda's extent never depends on type inference. Alternative weighed: any expression with an
  expected function type, which makes the extent of a lambda depend on checking.

- Settled on the user's delegation 2026-11-09 (branch `type-aliases`): **type aliases are transparent**, `type Name[T] =
  Body` ([type](/spec/declarations/#type)): each use is the body with the type arguments
  substituted, and the checker keeps no trace of the alias, so diagnostics name the expansion. Why:
  an alias is shorthand for a long type, and a type a reader can see through needs no conversion and
  no new rules for equality, overloading or patterns. Alternatives weighed: a nominal alias (a new
  type, as `data` makes), which would need conversions everywhere and duplicates what a one-variant
  data type already does; remembering the alias for diagnostics, which needs an alias-carrying type in
  every type the checker builds and is left as a later improvement.
- Settled on the user's delegation 2026-11-09 (branch `type-aliases`): **`type` is a declaration word only before a name**,
  and elsewhere an ordinary name, as `actor`, `machine`, `lazy` and `with` are. Why: no Avin code in
  the library or the tests names anything `type`, but a field called `type` is common in data that
  mirrors JSON or C, and keywords are contextual where they can be. Alternative weighed: a reserved
  word, which would forbid those names for no gain in parsing.
- Settled on the user's delegation 2026-11-09 (branch `type-aliases`): **an alias's type parameters take no variance, no
  bounds and no lower bound; the types its body names hold its type arguments to their own bounds at
  each use.** Why: the alias is its expansion, so its uses are held exactly as the expansion written
  out would be, and a bound on the alias would only repeat the body's (or wrongly differ from it).
  Alternatives weighed: bounds on alias parameters checked against the body, as a data type's are,
  which adds a second statement of the same constraint; no check at all until the expansion is used
  as a value, which would accept `Sorted[Opaque]` in a signature.
- Settled on the user's delegation 2026-11-09 (branch `type-aliases`): **an alias whose body does not use one of its type
  parameters is refused.** Why: `A[int]` and `A[string]` would be one type, which reads as a
  distinction the program does not make. Alternative weighed: allowing it as a phantom parameter, which
  a transparent alias cannot give any meaning. Review with the user: a phantom-type idiom would need
  `data` instead.
- Settled on the user's delegation 2026-11-09 (branch `type-aliases`): **an alias names only a type: it is not a value, a
  constructor, a pattern, an impl's target or a parent in a class's list**, each refused with a
  message naming the expansion, and the name that makes and matches its values where there is one.
  Why: one meaning for an alias's name keeps a call and a pattern reading as what they make and
  match, and the expansion may be a tuple or a function type that has no constructor at all. An
  alias of a function type in a class's parent list is refused with the rest, though the function
  type itself may be written there. Alternatives weighed: allowing an alias of a data type or a class
  as its constructor and in patterns (`P(1, 2)`), which makes an alias's use depend on what it
  expands to; allowing `impl Alias`, which would add methods to a type under a name that does not
  say which. Review with the user: aliases of traits in a parent list may be wanted later.
- Settled on the user's delegation 2026-11-09 (branch `type-aliases`): **an alias's name may be shared with a function or a
  module value, but not with any other type of its module (data type, class, object, alias), a
  built-in type or `Self`.** Why: an alias lives only where types are written, so a function of its
  name is never ambiguous, while two types of one name would be. Alternative weighed: refusing every
  clash, as a class's name clashes with a function's because the class's name is also its
  constructor, which an alias is not.

- Settled on the user's delegation 2026-11-09 (branch `derives-enumerable`): **`Enumerable` is a core library trait derived through an
  ordinary template** (`library/avin/enumerable.avin`), its members `static values() ->
  Vector[Self]`, `ordinal() -> int`, `name() -> string` and `static valueOf(name: string) ->
  Option[Self]`; the compiler's only part is who may derive it. Why: the template mechanism already
  gives every member (`T.shape()`, `T.variantOf`, `T.build`), so nothing new enters the compiler
  but the refusal, and the trait can be a bound (`E: Enumerable`) in generic code. Alternatives
  weighed: members the compiler writes out per type (a constant vector made once, faster
  `values()`, but a second derivation path beside the templates); an `enum` keyword (new syntax for
  what a data type already is).
- Settled on the user's delegation 2026-11-09 (branch `derives-enumerable`): **`values()` is a `Vector`, made anew at each call**.
  Why: `Vector` is the library's default sequence, what a literal `[a, b]` makes; a template has
  no per-type storage to keep one in, and the vector is immutable, so a fresh one is
  indistinguishable from a kept one except in cost (O(variants) per call). Alternatives weighed:
  `Array` (mutable, so it would have to be copied anyway); `List`; an iterator (cannot be indexed
  by `ordinal()`). Review with the user if `values()` turns up in a hot loop: a compiler-kept
  constant would make it free.
- Settled on the user's delegation 2026-11-09 (branch `derives-enumerable`): **`name()` is part of `Enumerable`, separate from
  `Display`**. Why: `Display` can be replaced by an impl, so it cannot be the inverse of
  `valueOf`; `name()` always is, `T.valueOf(x.name()) == Some(x)`. Alternatives weighed: reusing
  `toString()`/`Display` (breaks the round trip once `Display` is customised); a `static
  names()` (redundant with `values().map(v -> v.name())`). A type's own method named `name`
  then clashes, reported as two methods of one name; review with the user if that proves common.
- Settled on the user's delegation 2026-11-09 (branch `derives-enumerable`): **`valueOf` matches the declared name exactly, byte for
  byte, answering `Option`**. Why: exact is predictable and cheap, and a program wanting another
  rule (case-insensitive, trimmed) writes it over `values()` and `name()`. Alternatives weighed: a
  trapping `valueOf` (the library answers input that may be wrong with `Option`, as
  `toIntOption` does); case-insensitive matching (ambiguous where two variants differ only in case).
- Settled on the user's delegation 2026-11-09 (branch `derives-enumerable`): **a generic data type cannot derive `Enumerable`, even
  when every variant has no fields** (`data Tag[A]` with `X | Y`). Why: a derived impl of a
  generic type bounds each parameter by the trait (`impl[A: Enumerable]`), which means nothing for
  a phantom parameter, and `Tag.values()` would need a type argument nobody can infer. A
  parameterless enumeration is what such code wants. Alternative weighed: allowing it with
  unbounded parameters, `Tag[int].values()`, a special case in how templates are applied.
- Settled on the user's delegation 2026-11-09 (branch `derives-enumerable`): **only data types derive it; a class (an object cannot
  write `derives`) is refused even without constructor parameters**. Why: a class instance has
  identity, so `values()` would make a new instance per call, unequal to the last. Alternative
  weighed: allowing a class of no parameters as an enumeration of one.

**Exports between modules** (branch `export-reexports`; [modules](/spec/declarations/#visibility-and-modules))

- Settled on the user's delegation 2026-11-09: **`export` is written exactly as `import` is**: `export a.b.X`, `export a.b.{X, y as
  z}`, renaming with `as`, one top-level line each, at the top of any file of the module. Why: one
  syntax to learn, and every form an import has means the same thing in an export. Alternatives
  weighed: an `export` modifier on an `import` line (`export import a.b.X`), which ties presenting
  a name to bringing it into one file; a list in the package manifest, far from the code it shapes.
- Settled on the user's delegation 2026-11-09: **an exported name is a name of the module, seen by every file of the exporting module
  without an import**, and found after the module's own declarations and before the file's imports.
  Why: a module's files are one namespace, and an export adds to that namespace exactly as a
  declaration would; a module presenting `Point` and writing `centre(p: Point)` beside it should
  not have to import what it already offers. Alternatives weighed: an export visible only to other
  modules, so the exporting file also writes the matching import (two lines per name, and a file of
  the module that forgets the import sees a name its importers see but it does not); an export
  visible in its own file only, as an import is (breaks "a module's files are one namespace").
- Settled on the user's delegation 2026-11-09: **no wildcard export**, as there is no wildcard import. Why: a module's surface stays
  readable from its export lines, and a declaration added to an internal module never silently
  joins the public surface. Alternative weighed: `export a.b.*`, convenient for a facade over one
  module; revisit with wildcard imports if those are ever added.
- Settled on the user's delegation 2026-11-09: **an export names a public declaration of another module, directly or through that
  module's export; private, unknown, whole-module, own-module and root-module exports and a
  `private export` are errors.** Why: an export presents a declaration, never widens one, and the
  root module has no name to be imported by. A whole module is not a declaration: `export a.b`
  would make a module a member of another, which modules are not. Alternatives weighed: exporting
  a whole module as a nested name (`shapes.geometry` reachable as `facade.geometry`), which needs
  module members and is left for later; letting an export make a private declaration public, which
  would make `private` meaningless to the module that wrote it.
- Settled on the user's delegation 2026-11-09: **exports chain, and two modules may export each other's declarations; exports that
  lead round without reaching a declaration are an error at each export of the cycle.** Exports are
  resolved from the source before anything is declared, following each chain with the names it has
  passed, so a cycle stops rather than loops. A link that is wrong (a private or unknown target) is
  reported where it is written, and the exports chained after it are not reported again; an import
  through such a chain reports the name as private to, or missing from, the module it names.
- Settled on the user's delegation 2026-11-09: **an exported name clashes as a declaration does, but two paths to one declaration
  never clash.** An export clashes with a declaration of its module, with another export of the
  name reaching a different declaration, with an import of a different declaration in a file of the
  module, and with a module imported under that name. A module exporting a name and importing the
  same declaration, exporting it from two files, and a file importing it both directly and through
  an export are all accepted. Why: the error exists to stop one name meaning two things; when both
  mean one declaration nothing is ambiguous. Alternative weighed: refusing every duplicate, which
  makes a module that both uses and presents a declaration choose between an import and an export
  for no reason.
- Settled on the user's delegation 2026-11-09: **every kind of top-level declaration can be exported**: data types, aliases (generic
  ones too), classes with their companion objects, objects, traits, machines (with `Door.State`),
  functions (all public overloads of the name, never a private one), module-level `val`, `var` and
  `lazy val`, and variants. A type's variants are not exported with it: each is exported by its own
  name, as each is imported by its own, and `Color.Red` reaches one through an exported type either
  way. Why: an export is a name for a declaration, and every name an import can bring in is one an
  export can present; variants follow imports so the two never differ. Alternative weighed:
  exporting a data type's variants with it, which an import does not do either.
- Settled on the user's delegation 2026-11-09: **a module that exports a declaration of another presents that module's extensions**:
  the exporting module's own files, and every file importing it whole or any name of it, see the
  methods the other module's extensions add; a file importing a declaration through an export sees
  its declaring module's extensions as a direct import would. Why: "importing a module or any of
  its declarations brings in its extensions" should not depend on which path reached the
  declaration, and a facade over a package should carry the package's extensions with it. Only
  modules whose declarations are exported are presented, not modules the exporter merely imports.
  Alternatives weighed: extensions reached only by importing their module directly (a facade could
  not offer a package's extension methods at all); an explicit `export` of an extension (an impl
  has no name to export). Review with the user: this widens which extensions a file sees.
- Settled on the user's delegation 2026-11-09: **`export` is a contextual word**, meaning an export only at the start of a top-level
  line before a name, and an ordinary name everywhere else (a function `export(…)`, a field, a
  parameter, a local). Why: as `type` is; nothing in the library or tests names anything `export`
  today, but the word is a natural method name (`report.export()`), and keeping it free costs one
  token of lookahead. Alternative weighed: a reserved keyword, simpler to lex but taking a useful
  name from every program.
- Settled on the user's delegation 2026-11-09: **a diagnostic and the language server name the declaration an export reaches**: an
  import clash reports `already imported, from \`inner.Widget\``, the declaring module's path, and
  go-to-definition on an exported name (at the export line or any import of it) goes to the
  declaration itself. Completion after `m.` does not yet list a module's exports (see Open).

- Settled on the user's delegation 2026-11-09 (built on branch `regex-budget`, for `marshal grep`): [`avin.regex`'s checked
  searches](/spec/regex/#matching). A spent step budget becomes a failure the caller can handle,
  answering the review question left on the settled limits on matching:
  - **A checked family beside the plain API**: `tryMatches`, `tryFindFirstIn`,
    `tryFindFirstMatchIn`, `tryFindPrefixOf`, `tryFindPrefixMatchOf`, `tryFindAllIn`,
    `tryFindAllMatchIn`, `tryReplaceAllIn` (both forms), `tryReplaceFirstIn`, `tryReplaceSomeIn`,
    `trySplit` and `Pattern.trySplit`, each `Result[T, MatchError]` with `T` what the plain search
    answers. *Why*: a pattern written in the program that backtracks too far is a defect the trap
    explains, and every `findFirstIn` stays a plain `Option`; a program searching outside text or
    patterns (a grep) needs to go on, and says so by its choice of method. It mirrors
    `Regex(p)` beside `Regex.compile(p)` and `Url(text)` beside `Url.parse`. *Alternatives*: every
    search answering a `Result` (breaking, and makes every search fallible for what is almost
    always a defect); a regex-level mode (`checked()`) changing what the plain methods do on
    overflow (the result type cannot change with it, so it would need a sentinel); an
    `error()` query on the iterator after it stops (easy to forget, so a spent budget reads as "no
    more matches").
  - **The gathered forms of every-match searches**: `tryFindAllIn` answers a `Vector`, not an
    iterator, and the error discards the matches found before it; a replacer has been called for
    those by then. *Why*: an iterator of `Result`s, or one whose end might be an error, makes the
    common loop longer and is easy to read wrongly. *Alternatives*: an `Iterator[Result[Match,
    MatchError]]`; giving the matches found so far with the error.
  - **`MatchError(pattern, inputLength, budget)`, a plain `data` type** with `message()` and
    `Display`, as `PatternError` is; `inputLength` counts characters. *Alternatives*: a `RegexError`
    with variants for both kinds of failure (`compile` would change type, breaking); a bare message
    string.
  - **The plain searches still trap**, the message now naming the budget and the checked
    alternative: `… past its budget of N steps; make a repetition possessive or atomic, or search
    with a `try` method to handle it`. The budget stays per regex (`withStepLimit`), its default
    unchanged. *Alternatives*: a per-call limit argument on the `try` methods (doubles every
    signature; `withStepLimit` is cheap to call per search).
  - The matcher learns of a spent budget by a flag every step checks, so a spent budget ends the
    whole search through every nested part; a lookaround's or atomic group's body answering no for
    it is never taken as the body failing.

- Settled on the user's delegation 2026-11-09: **Union types: the rules beyond the user's five points** (branch `unions-design`;
  [union types](/spec/unions/)).
  - **Precedence: `|` binds tighter than `->` and looser than everything else in a type**, so
    `(A) -> B | C` returns a union and a union of functions parenthesizes each. *Why*: a function
    returning one of several results is far more common than a union with a function member, and
    `->` is already the loosest type operator. *Alternatives*: `|` looser than `->` (makes the
    common case need parentheses); refusing the unparenthesized form (noisy).
  - **A union in a type pattern is parenthesized**, `x: (int | string) ->`; a `|` in a pattern is
    always an alternative, and `x: int | string` is reported with the parenthesized form. *Why*: it
    keeps one meaning for `|` in patterns, as function types already need parentheses there.
    *Alternatives*: parsing a type after `:` greedily (makes `_: A | _: B` mean something else).
  - **Normal form: flatten, order irrelevant, a member that converts to another member (by subtyping
    or boxing into a trait or `Any`) absorbed, `Nothing` absorbed, one member is that member. Written
    by hand, a duplicate, absorbed or `Nothing` member is an error naming the type to write; arising
    through an alias or type argument it is absorbed silently.** *Why*: `T | int` at `T = int` and
    `T | Animal` at `T = Dog` must compile, while `Dog | Animal` written out is almost certainly a
    mistake. Absorbing by conversion rather than subtyping alone keeps every injection unambiguous
    (`int | Display` would otherwise leave an `int` two homes). *Alternatives*: keep subsumed members
    distinct (two homes for one value, an ambiguity on every injection); refuse at instantiation too
    (breaks generic code); silently absorb written duplicates (hides mistakes). Review with the user:
    whether written duplicates should be errors or warnings.
  - **Inference carries a union it is given and never forms one**: `val u2 = u` is the union; a
    union and its member join to the union; `T` in `x: T | string` takes the members the argument
    leaves over (`orText(u)` with `u: int | bool | string` gives `T = int | bool`); a member argument
    decides no `T`. *Why*: anything less makes unions unusable in generic code; forming one would be
    the inference the user ruled out. *Alternatives*: refusing to solve `T` against a union (forces
    explicit type arguments everywhere).
  - **Injection at a single site, like boxing**: a member's own type first, else the one member it
    converts to, several an error naming them; a literal takes its default type where that is a
    member, else the one numeric member it fits; `None`/`Ok(x)`/an empty literal go to the one member
    that is an instance of their type. *Why*: mirrors how a value becomes an `Any`, and never guesses.
    *Alternatives*: choosing the first member written (order would then matter, contradicting the
    normal form).
  - **Overloads**: a member type is more specific than a union containing it; a union more specific
    than a type every member converts to; a union argument never chooses among member overloads (an
    error suggesting a `match`). *Why*: overloads are resolved statically (settled 2026-11-02).
    *Alternatives*: generating a dispatch `match` over member overloads (dynamic overloading, which
    the language does not have).
  - **A named function is adapted where written** to a function type whose parameters inject into its
    union parameters (`val f: (int) -> string = describe`); a function value already made is not.
    *Why*: the same single-site rule as `@sendable` references. *Alternatives*: always requiring a
    lambda (a contortion for the common case).
  - **Representation: a union whose members are all types an `Any` holds without a box is laid out
    as an `Any` (one pointer, the header as the tag); one with a scalar member is a tag, a bits slot
    and, if any member is a reference, a separate pointer slot.** Consequently variance through
    containers holds exactly where the layouts agree: `List[Dog] <: List[Dog | Cat]`,
    `Result[T, IoError] <: Result[T, IoError | ParseError]`, but not `List[int] <: List[int | string]`.
    Tag numbering follows the normal form, and the layout is decided before optimization, so -O0 and
    -O2 agree. *Why*: reference unions cost nothing over `Any` and keep `?`'s common case free;
    scalar unions avoid an allocation per value; the separate pointer slot keeps the collector's
    roots exact. *Alternatives*: every union boxed as an `Any` (simplest, but `int | long` allocates);
    every union a tag-and-payload value (no variance through containers, a conversion for every
    `Result` of a wider error); one overlapping payload word (needs tag-aware root scanning).
    Review with the user: the variance consequence is visible to programs.
  - **`match`**: type patterns, supertypes and sub-unions cover members; any other pattern applies to
    the one member it can match (`0` and `n < 0` on an `int | string` reach the int), and one more than
    one member could match is an error; a type pattern naming no member is an error; in generic code
    the `match` is checked as written and, at an instance where members come together, the first arm
    in order wins. *Why*: lets literals and constructors reach their member without a nested
    `match`, without guessing. *Alternatives*: type patterns only, as on an `Any` (verbose).
  - **`is` binds, it does not narrow**: `v` keeps its union type after `if v is _: string`; the
    pattern binds a name. *Why*: the settled `is` scoping (2026-11-03) has no flow-sensitive typing,
    and unions should not introduce it. *Alternatives*: flow narrowing of the tested variable
    (a new, language-wide mechanism). Review with the user.
  - **What a union can do**: `==` and hashing as two `Any`s holding the values; `===` only where every
    member is a class or function type; a union conforms to a trait every member conforms to whose
    methods take `Self` only as receiver (`Display`, `Debug`, `Hash`, a library `JsonEncoder`), plus
    `Eq` and `Sendable` member-wise, never `Ord`; a member call is looked up on the members' join and
    on those traits, ambiguity an error. *Why*: printing, hashing and calling a shared superclass
    method are what a union is used for; `Ord` would need an order between members that order-free
    unions cannot give. *Alternatives*: refusing every member call (forces a `match` just to call a
    method every member has); any method name every member has, structurally (breaks the nominal
    principle, and its result would be an inferred union). Review with the user.
  - **`?` with unions**: (1) `E` converting to `F` (a member, a member's subtype, a union of members)
    passes on unchanged; (2) for a union `F`, the one member implementing `From[E]` converts; (3) for a
    union `E` and a plain `F`, each member converts on its own; (4) otherwise `F.from(e)` as before.
    `?` never adds a member. *Why*: unions replace hand-written wrapper error types without breaking
    the `From` rule existing code relies on. *Alternatives*: a union `F` consulting no `From` (loses
    the bridge from wrapper errors); inferring the error union from the `?`s in the body (the
    inference the user ruled out).
  - **Refused**: `impl` or extension on a union; a union as a parent or as an upper bound (a lower
    bound is allowed); a union across `@sysl`/`@c` declarations and mirrors; a `with { … }` field path
    through a union field; casts, calls and construction of a union. *Why*: each needs a declared
    type or a C/Sysl layout a union does not have. *Alternatives*: shims that pass a tag and payload
    across a binding (no Sysl or C library takes one).
  - **Chapter**: a chapter of its own, `spec/unions.md`, linked from types, generics, errors and
    `match`. *Alternatives*: a section of types.md (too long for one section).

- Settled on the user's delegation 2026-11-09 (branch `unions-core`): **Union types: the choices the core's implementation
  made** ([union types](/spec/unions/)).
  - **Every union is laid out as an `Any` for now, a scalar member in a box.** The chapter's
    tag-and-payload layout for a union with a scalar member needs values of more than one machine
    word in the compiler's intermediate code (parameters, results, fields, array elements, closure
    captures), which nothing else in the language has; the core ships without it. The type rules stay
    the chapter's, so programs do not change when the layout lands: `List[int]` is still no
    `List[int | string]`, and a union of references still nests in covariant containers. *Why*: one
    layout makes every part of the core correct now (exact roots, `-O0` and `-O2` alike, equality,
    hashing and rendering through the `Any` machinery), at the cost of a box per scalar injected.
    *Alternatives*: building the multi-word value first (holds every other part of the item); a single
    tagged word (cannot hold a `long` and a pointer apart, so roots would not be exact). Review with the
    user: the allocation per scalar injection until the layout lands.
  - **A copy of generic code at a union type argument is a copy of its own**, so a type pattern naming
    the parameter tests the union's members, `t: T` at `T = int | bool` taking neither a string nor
    anything else. Through a method called by its table, the type argument is still taken as `Any`.
    *Why*: a function's copies are keyed by their type arguments, and the union is one; a method's
    table slot is shared by every receiver. *Alternatives*: erasing every union type argument to `Any`
    (a `T` arm would take every value).
  - **Members that come together through a type argument are merged when they are the same type, and
    a member a subtype of another is kept, injection choosing the widest.** `T | int` at `T = int` is
    `int`; `T | Animal` at `T = Dog` keeps both members in the type, and a `Puppy` given to it is the
    `Animal`. *Why*: substitution is done where the class table is not at hand; the two forms hold the
    same values and match the same way, and only a message names the difference. *Alternatives*:
    absorbing subtypes at every substitution (needs the declarations at each one).
  - **`x: int | string` in a pattern is recognized as a type pattern followed by bare names that are
    no variants**, and reported with the parenthesized form; any other alternatives are checked as
    written. *Why*: a bare name is a binding, which alternatives refuse anyway, so nothing legal is
    taken from the program. *Alternatives*: deciding it in the parser (cannot tell a variant from a
    type there).
  - **A `|` after a type is a guard, not a union member, where the name after it is followed by an
    operator no type is followed by** (a comparison, arithmetic, `&&`, `||`, `!`, `is`), so
    `f(n: int) -> int | n > 0 = n` is still reported as a guard on a signature. *Why*: keeps the
    existing clause diagnostics. *Alternatives*: refusing `|` after a result type outside brackets.
  - **Messages the chapter left open** are those of [what is refused](/spec/unions/#what-is-refused)
    and these: a pattern several members could match names them and suggests a type pattern; `None`,
    `Ok(…)`, a collection literal or a lambda with several members of its kind names them and asks for
    the type meant; a literal several numeric members fit names them and suggests a conversion, as in
    `byte(3)`; two arguments of one type parameter that agree on no type add that a type argument is
    never inferred as a union, with the union to write; a missing clause names the member, as in
    `size(_: string)`. *Why*: each says what to write. *Alternatives*: the generic messages, which
    name no member.

- Settled on the user's delegation 2026-11-10: **Union types: the operations' choices the chapter left open** (branch
  `unions-ops`; [union types](/spec/unions/)).
  - **`?` converts an error union member by member whatever the function's error type is**, not
    only where it is no union: an `IoError | ParseError` passed on from a function yielding
    `ConfigError | Invalid`, where `ConfigError` implements `From` at each, converts each member by
    rule 2. *Why*: rules 1 and 2 cannot reach this case (no member implements `From` at the union),
    so rule 3 as written would leave it an error with no way to say what to do but `mapErr`, while
    each member converts on its own; the extension only turns an error into the conversion a reader
    expects. *Alternatives*: keep rule 3 for a plain `F` only (refuses an obvious program);
    implement `From` at a union (a union has no `impl`).
  - **A trait with type parameters counts for a union through the one instance every member
    holds**: `Vector[int] | List[int]` has `Seq[int]`'s methods, so `u.size()` and `u.map(…)` work
    although the two join on no type. *Why*: the chapter's rule ("the traits the union conforms to")
    names no restriction to traits without parameters, and the instance is unambiguous when the
    members agree on it. *Alternatives*: only traits without type parameters (refuses the common
    sequence case); every instance any member holds (ambiguous).
  - **A member of the union found through a trait or the join is called, named as a value or read
    as a field with the union seen as that type**, with no conversion, as an `Any` holding its
    member is seen: a field of the join class is read (`p.called` on `Puppy | Kitten`). *Why*: the
    union is laid out as an `Any`, which already is a value of every trait and class its member is;
    the field is the chapter's "looked up where it would be on the members' join". *Alternatives*:
    methods only (a field of the shared superclass would need a `match`).
  - **`===` between a union with a value member and an object of a class is accepted**, answering
    `false` for a value member, as it is between an `Any` and an object; it is refused where neither
    side holds only objects (`Dog | int` against an `Any`). *Why*: the identity chapter's rule for
    `Any`, which the union chapter says the union follows. *Alternatives*: refuse every `===` on a
    union with a value member (stricter than `Any`, with no added safety).
  - **The diagnostics' wording**: a union argument to overloads of its members says the member is
    known only while running and suggests a `match`; a bound a union misses says why (no order;
    the member that does not conform; a trait taking `Self` other than as the receiver); a result
    with a scalar error union names `mapErr(e -> e)`. *Why*: each names the fix. *Alternatives*: the
    general messages alone.
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-sockets`): **a socket wait parks on one poller with a thread of its own, one
  readiness report per wait.** Every socket `avin.net` makes is non-blocking; an operation that would
  have to wait registers the descriptor with the runtime's single `sysl.io.poll` set as `OneShot`,
  under a token naming a slot that holds the waiter, and parks; the poller's own operating system
  thread waits on the set and makes each reported waiter ready, and the operation is tried again. A
  slot is let go under one lock by whichever ends the wait, so a report arriving after a timeout
  finds its slot gone or taken again under a newer token and wakes nobody. *Why*: the monitor sleeps
  on a condition, which a kernel set cannot wake, and a thread blocked in the set's wait needs nothing
  else; a one-shot registration per wait can never deliver a stale wake to a later wait on another
  thing. *Alternatives*: the monitor waiting on the poller with the soonest timer as its timeout
  (the timers then move under the poller, a larger change to settled code, which this choice does not
  preclude); workers polling the set between threads (no extra thread, but a worker with nothing ready
  then sleeps in the kernel instead of on its condition, and wakes need a second path); a level- or
  edge-triggered registration kept for the socket's life (one system call less per wait, but a
  report for a wait that has ended can reach the next one, so every wait in the runtime would have to
  tolerate spurious wakes).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-sockets`): **a connection's timeout bounds each wait, not the whole
  operation.** A write that waits for room several times fails with `TimedOut` only where one wait
  outlasts the limit, as the kernel's per-call socket timeouts did before; the limit is kept in the
  connection rather than in the socket, since a non-blocking socket ignores the kernel's. *Why*: a
  large transfer to a slow but steady peer keeps going, and the meaning of `setTimeout` does not
  change. *Alternatives*: a deadline for the whole `write` or `readAll` (stops a slow transfer that is
  still making progress). Review with the user if a whole-operation deadline is wanted as well.
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-sockets`): **`connect` parks; a Unix `connect` does not.** A TCP connection
  that cannot be made at once waits for the socket to become writable, then reads the socket's
  pending error (`SO_ERROR`) for the outcome, since a second `connect` answers "already connected"
  even for a refused connection on macOS. A Unix connection is made at once by the kernel or refused,
  so it stays a plain call and the socket is made non-blocking after. *Alternatives*: a blocking
  `connect` that the monitor compensates for (holds a worker for a round trip to the peer).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-sockets`): **a connection or listener cannot be closed under a thread parked
  on it.** It belongs to the parked thread, so another thread's `close()` traps as any other use
  from there does, and the wait ends only by the peer, a timeout or readiness. *Why*: the ownership
  rule already decides it, and no wake path for a socket closed elsewhere is needed. *Alternatives*: a
  shared close that wakes the parked thread with an error (needs sockets shared between threads,
  which the ownership rule excludes; cancellation by `scope` is where an outside end of a wait
  belongs).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-ui`): **Other threads reach the interface through inboxes and `post`, run
  by the main thread from one queue** ([User interfaces](/spec/ui/#threads)). `inbox(handle)` is
  made on the main thread; its handler, which may capture `State`, stays there; the `Inbox[T, R]` is
  sendable, with `send(m) -> Future[R]` and `ask(m) -> R`. `post(@sendable work)` queues work with
  no answer. Every message and post waits in one queue, so one thread's order holds across inboxes.
  Why: the interface's state is not sendable and must not become so, so the code that touches it
  has to be made on the main thread and only a sendable message may cross; a future for the answer
  reuses what already parks a lightweight thread. Alternatives weighed: `post` alone (a sendable
  closure cannot reach a `State`, so it could not change the interface); a sendable `State` behind a
  lock (every build would lock, and a write from another thread would race the build); an actor for
  the interface (the frame loop owns the thread, so a mailbox would still need this queue); a queue
  for each inbox (loses one thread's order between two inboxes).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-ui`; moved to `on_frame` on
  branch `binding-f32`, which rests on the Proposed `f32` entry): **The frame loop runs the waiting
  work in syslui-sdl's `on_frame`**, at the start of every frame and before the loop decides whether to build
  the tree again, so work posted before a frame runs in that frame and what it writes is built in it;
  `run` runs what is left once more as it returns. The package's binding of `app` is `named` (branch
  `binding-defaults`, see the Proposed `named` entry), so it passes `on_frame` and leaves `on_event`,
  `font` and `size`, which come before it, to syslui-sdl's own defaults; until then it copied those
  defaults into Avin, since a binding passed its arguments by place. Why: `on_frame` is the hook syslui-sdl
  runs first in a frame for exactly this. Alternatives weighed: a wrapper of `ground`, which `lwt-ui`
  used while no real could cross into an `f32` (run after the tree, so its work was built a frame
  late); `tree` (called only when something was written, so it would never notice a post);
  `on_event` (called only when there are events).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-ui`): **`handlePosted` runs only what was waiting when it was called.**
  Work that sends or posts waits for the next call. Why: work that posts itself again, a common
  animation shape, would otherwise hold a frame for ever. Alternatives weighed: draining until the
  queue is empty (simpler, but unbounded).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-ui`): **An ask on the main thread stops the program; a send or post there
  is queued.** Why: the main thread handles the message, so its own wait would never end; a trap
  names the mistake where a hang would not. Alternatives weighed: handling the queue up to the
  message at once (re-enters a handler that asks, and runs work out of frame order).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-ui`): **Making an inbox, `handlePosted` and `run` are the main thread's
  alone**, refused at compile time from code for another thread because they reach the package's
  module-level state, and checked again at run time with `isMainThread()` (a new function of
  `avin.threads`, answered by the runtime from the calling context). `run` on another thread stops
  the program before any window is opened. Why: a window on a thread other than the process's
  first never shows on macOS, and a handler made on another thread would run on the main thread
  with that thread's values. Alternatives weighed: answering `Err` from `run` (a mistake in the
  program, not a condition it can handle); moving `run` to the main thread silently (the caller's
  wait for its result would be the same hang).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-ui`): **An inbox lasts as long as the program, and work still waiting when
  `main` returns is dropped.** Why: inboxes are made once, as a window's state is, and the program
  ends when `main` returns whatever its threads are doing. Alternatives weighed: closing inboxes
  (needs a lifetime the interface does not have yet); running the queue at exit (runs interface code
  after the interface is gone).
- Settled on the user's delegation 2026-11-10 (built on branch `binding-f32`): **A `real` crosses into and out of any Sysl floating-point type
  that rounds: `f32`, `f16`, `bf16` and `real`.** A binding's own `real` parameter, a mirror's `real`
  field going into Sysl, and a callback's `real` answer are rounded into the Sysl type to the nearest
  value it holds, a tie to the even one, as an IEEE conversion rounds: a value past its largest
  becomes an infinity of its sign, one below half its smallest subnormal a zero of its sign, a NaN
  stays a NaN, and nothing stops the program. A `real` result, a mirror's field coming out, a
  callback's parameter, and a real an `Option`, a `Result`, a tuple or a collection result holds are
  widened exactly. A real inside a parameter's `Option`, tuple or collection is passed as `real`
  only, as an integer there is passed as its declared type. A binding's own real is passed as it is
  first and rounded only where Sysl refuses a `real` there (the shim is built again), so a Sysl
  function overloaded at `real` and `f32` (`sysl.math.ulp`) is called at `real`, unrounded, and a
  generic one takes the real itself. A real is not made from an `f128` (its arithmetic is not
  `sysl.math.Float`; Sysl widens a parameter into one itself), nor from any non-floating type. Why: Avin has one floating type, and a
  graphics or audio library's `f32` (syslui-sdl's `size`, a canvas's coordinates) is what a binding
  meets; rounding is what every conversion between floating widths does and the only one that keeps
  every `f32` value exact on the way back. It mirrors how an integer crosses into any Sysl integer.
  Alternatives weighed: an `f32` type in Avin (a second floating type in every arithmetic rule, for
  the boundary's sake); trapping where the value does not fit (a rounding is not an error, and an
  infinity is a legal `f32`); converting a real held inside a parameter's option or collection too
  (integers are not converted there either, and the copy would be written element by element);
  rounding every real parameter at once (an overload at `f32` would then be chosen over the one at
  `real`, losing precision silently).
  **Review with the user** whether a `real` into an `f32` should trap on overflow to infinity
  rather than round to it.
- Settled on the user's delegation 2026-11-10 (built on branch `binding-defaults`): **A binding marked `named` (`@sysl("f", named)`) passes
  every argument by its parameter's name, so a parameter it leaves out takes the Sysl function's own
  default.** The Avin name is spelled as Sysl spells it, each uppercase letter an underscore and its
  lowercase, as a mirror's fields are (`onFrame` is `on_frame`); the binding lists the parameters it
  passes in any order; the receiver of a handle's method is never named; every parameter of a
  `named` binding has a name. A binding without `named` passes by position, as before. Sysl checks
  the names and reports a missing parameter without a default, or a name it does not declare, with
  its own message at the binding. Why: a binding that had to reach a parameter behind defaulted ones
  copied the Sysl defaults into Avin (the ui package copied syslui-sdl's `font` and `size` and reached
  a private handle for `on_event`'s default), and those copies go stale silently; with names, Sysl
  supplies and evaluates its own defaults at each call, a non-literal one included. One word on the
  annotation, decided per binding, changes no existing binding's shim and needs no knowledge of the
  Sysl signature in the Avin compiler. Alternatives weighed: caller-side omission, where an argument
  an Avin caller leaves out of a binding's call is left out of the Sysl call too (most general, but a
  binding would need a shim for each set of omitted arguments its callers use, and Avin's own
  defaults on a binding would mean two things); a per-parameter mark such as `@named` or a
  `= sysl` default on a positional list (the binding still lists every parameter up to the last one
  it passes, and a mixed call needs the names anyway); naming every binding's arguments always (would
  make every existing binding's parameter names part of its contract with Sysl); copying the
  defaults (the stale copies this replaces). Review with the user: the surface word `named`, and
  whether a caller-side form should come later.
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-scope`): **Cancellation by a scope stops a thread and drops its frames**
  ([cancellation](/spec/threads/#cancellation)). `s.cancel()` cancels the scope's threads, and those
  it starts afterwards (which run nothing); a cancelled thread stops at its next cancellation point
  with its frames dropped and ends with no result: `Thread.result() -> Option[T]` (new) answers
  `None`, and `join` traps. The cancellation points are every wait but a lock's (sleep, join,
  condition, so channels, futures and promises, and a socket's wait for its descriptor) and every
  safepoint poll. A socket's wait is ended inside the runtime's Sysl code, which may not drop its
  own frames (they hold the socket), so it fails the operation and the library stops the thread as
  soon as the operation has returned to it; no caller ever sees that failure. *Why*: Avin has no
  exceptions and a trap stops the program, so a cancellation that made each wait answer an `Err`
  would change the type of every wait in the library and leave a computing thread unreachable; the
  runtime already drops an actor's frames on a trap, and a thread shares nothing mutable outside a
  lock, so dropping its frames loses nothing anyone else can see. *Alternatives weighed*: every wait
  answering an `Err` or `Option` (the direction first sketched; changes every signature, and a loop
  that never waits is never cancelled); a cooperative flag the thread polls (a thread that forgets
  to look is never cancelled, and parked threads still need waking); a trap at the next wait (stops
  the program). Review with the user: `join` trapping on a cancelled thread rather than answering an
  `Option`, which keeps every existing `join` unchanged.
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-scope`): **A thread is not stopped inside a region others may wait on**: a
  lock held, a `lazy val` being initialized, an Avin function Sysl called (the regions a trap already
  treats as in doubt), and the library's running of what waits for a future it completed. It
  finishes the region and stops at its next cancellation point after. A wait on a condition is a
  cancellation point when the lock it lets go is the only region the thread is in; the thread then
  stops without taking the lock again. Waiting for a lock is not a cancellation point. *Why*: a
  mutex is the only door to shared mutable state, so a thread stopped outside every lock leaves
  nothing half changed, and never leaves a lock held; a lock is held briefly, so making its wait a
  point would buy little and race the hand-over of the lock. *Alternatives weighed*: stopping
  anywhere and releasing the locks a thread holds (leaves the state they guard half changed); making
  the release of the last lock itself a cancellation point (cuts library code short after its
  release, as a future's completion is).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-scope`): **Cancellation reaches down through scopes, and only scopes.** A
  cancelled thread stops first at the innermost scope it runs, which cancels its own threads and
  waits for them, and the cancellation then goes on outward; a scope opened while its thread holds a
  lock sets no such stop, so its threads are not cancelled with the thread. Threads started with the
  plain `spawn`, and futures' work, are not reached by any scope's cancellation. *Why*: structured
  joins hold only if a scope never returns before its threads, cancelled or not; unstructured threads
  have no owner to be cancelled by. *Alternatives weighed*: cancelling the thread alone and leaving
  its inner scope's threads running (breaks the scope's promise); a `Thread.cancel()` on any thread
  (unstructured cancellation, which nothing here needs).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-scope`): **A failure does not cancel a scope by itself.** A trap stops the
  program as before, so there is no failing thread to propagate; a thread reports a failure as a
  `Result`, and the scope's body cancels and returns it. `Scope` gains `cancel()` and
  `isCancelled()` only, both used by the thread running the body while the scope is open. *Why*: the
  body already sees every thread's result and decides what a failure means; a `scope` that inspected
  results would need to know their type is a `Result`. *Alternatives weighed*: a `scopeTry` whose
  threads return `Result[T, E]` and whose first `Err` cancels the rest (a second entry point for what
  a channel and a `cancel` express); a sendable canceller a thread could call (the body can already
  cancel on a message). Review with the user: whether a `Result`-aware scope is wanted for brevity.
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-scope`): **The scope hands back the Sysl handles a cancelled thread's body
  reached**, once it has waited for the thread, as a body that returns hands them back itself; they
  are then in transit, for the next code to use. *Why*: a connection given to a thread that was
  cancelled is otherwise unusable by anyone, and closing it is the scope's thread's job. *Alternatives
  weighed*: leaving them with the ended thread's owner (every later use traps); handing them back on
  the cancelled thread as it stops (it runs no code of its own then).
- Settled on the user's delegation 2026-11-10: **Actors' turns on lightweight threads** (branch `lwt-actors`, [scheduling](/spec/actors/#scheduling)).
  An actor on the shared executor, the default, runs each turn as a lightweight thread of its own,
  started with a thread's default stack (8 MiB reserved, committed as touched, reused from the
  scheduler's spares) and handing nothing over, the turn making the actor the owner of its code; a
  turn that waits parks, and one that computes is time-sliced. An actor given an executor of its own
  keeps running its turns as tasks on that executor's workers, so `Executor.fixed(n)` still bounds
  how many work at once and the 16-message fairness among its actors stays exact. To tell the two
  apart, `Executor.shared()` now gives back the same executor every time, compared with `===`.
  *Why*: a turn as its own thread is what makes a sleeping or awaiting turn free its worker; the
  shared pool counts a worker that sleeps or receives from a channel as working, so its few workers
  could all be held by waiting turns. An explicit executor is a request for a bound, and keeping it
  keeps every program that asked for one behaving as it did. *Alternatives*: one long-lived
  lightweight thread per actor, parked on an empty mailbox (a committed stack page or more for every
  idle actor, and a parked thread for each of a hundred thousand actors); every turn a thread of its
  own whatever the executor (drops the bound `Executor.fixed(n)` asks for, and the
  `actors_fairness`/`actors_waiting` behaviour on one worker); `spawn`'s executor as an
  `Option[Executor] = None` (breaks the positional `spawn(b, Stop, ex)`); a public
  `Executor.isShared()` (more surface for one library's need). Review with the user: whether an
  explicit executor should still bound an actor once every turn could be a thread.
- Settled on the user's delegation 2026-11-10 (built on branch `train-2`): **A scope's cancellation does not reach actors**
  ([cancellation](/spec/threads/#cancellation), [scheduling](/spec/actors/#scheduling)). An actor
  started by a scope's thread is a top-level actor and runs on when that thread is cancelled; an
  actor's turn, a lightweight thread started as a plain thread is, is never cancelled by a scope,
  though the scopes it runs cancel their own threads. The waits a turn makes need nothing of their
  own: a turn waits only in the library's waits (a future's `await`, so an `ask`; a sleep, a
  channel, a condition, a socket), every one already a cancellation point for a thread that can be
  cancelled. An asker cancelled in an `ask`'s `await` leaves its question in the actor's mailbox,
  handled in its place among the messages told, and the reply completes a promise nobody awaits,
  which also takes the ask's timer out. *Why*: cancellation reaches down through scopes and only
  scopes, and an actor is owned by its parent actor or by nobody, never by the thread that happened
  to start it, so tying its life to that thread's scope would make where an actor is spawned decide
  when it ends; cancelling a turn would stop an actor halfway through a message, leaving its fields
  half changed with no lock to guard them, and break one turn at a time. *Alternatives weighed*:
  stopping the actors a cancelled thread started (structured actors, which the actor tree already
  provides through parent actors); taking a cancelled asker's question back out of the mailbox
  (needs a mailbox that removes from its middle, and the actor may already be handling it); making
  a turn cancellable by the scope that told it a message (a message has no owning scope).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-server`): **the server library is in the standard library, as `TcpServer` in
  `avin.net` and `http.serve` in `avin.net.http`**, not a package and not a module of its own. *Why*:
  the TCP connection and the HTTP client it builds on are there already, the server shares the
  client's `Response` and token rules, and it is the first user of the runtime's poller and, later,
  of `scope` cancellation, which tie it to the compiler's release. *Alternatives*: a package
  `dev.avinlang.http` (versioned apart, but it would duplicate the client's types or depend on the
  standard library's privates); a module `avin.net.server` (one more import for the one type).
  Review with the user if a larger web framework is wanted later: that belongs in a package on top.
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-server`): **each connection is served in an actor's first turn, supervised
  with `Stop`**, rather than on a plain lightweight thread. *Why*: a trap in a thread stops the
  program, and an actor is the only place a trap is contained; a connection's handler is the place
  a server most needs a bug to cost one connection. The actor's first turn runs the whole
  connection and the actor stops after it; an `onStopping` given before the connection is served
  runs `failed` (which writes the `500`) and closes it, since a `stopping` block is given only after
  the body has run. *Alternatives*: a lightweight thread per connection (a handler's trap stops the
  server); an actor per request (a mailbox and a turn per request, for no gain).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-server`): **the connections' actors run on an executor of the server's own,
  `Executor.fixed(maxConnections)`**. *Why*: a connection's turn parks on its socket for as long as
  the connection lives, and on the shared pool, about one worker per processor, those parked turns
  would keep every other connection waiting for a worker; the workers are lightweight threads, so a
  worker per connection costs a parked stack each. Once actors' turns run on lightweight threads of
  their own (stage 14, item 5), the executor may go. *Alternatives*: the shared pool with each wait
  in `blocking` (a trap inside `blocking` leaves the worker counted as waiting for good).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-server`): **at the connection limit, the next connection waits unaccepted**
  in the listener's backlog until one ends, rather than being accepted and refused. *Why*: the
  client is served late instead of not at all, and the server spends nothing on it meanwhile.
  *Alternatives*: accept and close at once, or answer `503` (a burst turns into failures the client
  must retry).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-server`, reworked on `train-2`): **`stop()` cancels the acceptor's parked
  `accept`**, which waits on a thread of a scope the acceptor runs; the acceptor's own thread then
  closes the listener. No connection is made to wake it. (`lwt-server` first woke it with a
  connection of its own to the listener, the one thing that could end a parked `accept` before
  `scope` could cancel a wait.) *Why*: a listener belongs to the thread parked on it, so no other
  thread may close it, and cancellation ends a wait where it is parked without a connection the
  server would accept and drop, a descriptor per stop, or a host to reach itself on (`127.0.0.1`
  for `0.0.0.0`). The API of `stop`, `join` and `shutdown` does not change. *Alternatives*: the
  waking connection (kept until cancellation landed); an `accept` with a timeout polled for the
  stop (a delay of up to the poll, and wake-ups for nothing); a listener closable from another
  thread (against the ownership rule).
- Settled on the user's delegation 2026-11-10 (built on branch `train-2`): **a shutdown closes a connection idle between requests at once,
  unanswered, and a request read once the server is stopping is answered `503`**. The HTTP server
  waits for each request, its first included, through `TcpServer.idle`, so stopping cancels that
  wait; a request being read or answered finishes, its response saying `Connection: close`.
  (`lwt-server` let an idle connection run to its idle limit, answering `503` to a request arriving
  meanwhile, since nothing could end the wait from outside.) *Why*: an idle connection has nothing
  in flight, so closing it loses nothing, and `join` no longer waits up to `idleTimeout`, or for
  ever where it is zero. *Alternatives*: idle connections ending at their limit (a slow shutdown);
  serving requests on connections already open (a chatty client keeps the server up for ever).
- Settled on the user's delegation 2026-11-10 (built on branch `train-2`): **a shutdown reaches a connection's actor through `TcpServer.idle`,
  a scope the actor's turn runs around each idle wait**, not a scope around the turn. `idle(wait)`
  registers the wait with the server, runs `wait` on a thread of a scope opened by the calling
  thread, and parks that thread on a promise that the wait completes `false` once it answers and
  `stop()` completes `true` for every registered wait; on `true` the scope cancels the wait. Its
  result is `Some` of the wait's answer, or `None` where the server stopped first, and `None` at
  once, `wait` not run, where it was already stopping. The acceptor's `accept` goes through the
  same path, so what follows an accept (counting the connection, starting its actor) runs on a
  thread nothing cancels. *Why*: a scope's cancellation does not reach actors, and an actor's turn
  is never cancelled (settled for `train-2` above), so the server cannot cancel a turn; it can only
  cancel a scope the turn runs, and `cancel` must be called by the thread running that scope's body,
  which is why the turn parks on the promise and cancels itself. Only the wait is cancelled, so
  code answering a request, and a trap in it (which fails the connection's actor alone), stay on the
  turn. A promise per wait and a registry keyed by number wake only the waits that exist, not every
  connection on every request. *Costs*: a scope and a lightweight thread per idle wait, and a
  `Vector` copy of each read the HTTP server makes there, since a wait's result must be sendable;
  a trap in `wait` stops the program, as a trap in any thread does. **Review with the user**: it
  adds `idle` to `TcpServer`'s public API, the one way a protocol on it tells the server which
  waits a shutdown may end. *Alternatives*: the connection's whole `serve` on a scope's thread with
  the turn watching it (a trap in `serve` would stop the program instead of failing the actor, and
  a request in flight would be cancelled with it); a server-wide condition notified when any wait
  ends (wakes every idle connection per request); shutting down the read side of idle sockets from
  the stopping thread (not cancellation, and a socket belongs to the thread that uses it); a
  per-connection server handle carrying an idle flag (the same cancellation, plus a race between
  the flag and the wait's answer).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-server`): **HTTP limits and refusals.** Timeouts bound each wait, as a
  connection's do: `readTimeout` while a request is read (`408` once one runs out mid-request, and a
  quiet close where a connection sent nothing), `writeTimeout` while a response is written, and
  `idleTimeout` between requests; `maxHeaderBytes` (16 KiB) bounds the request line (`414`) and the
  head and a chunked body's trailers (`431`); `maxBodyBytes` (1 MiB) bounds a body by its length
  before it is read, or as chunks arrive (`413`). A request with both `Content-Length` and
  `Transfer-Encoding`, two lengths that differ, a coding list not ending in `chunked` or a header
  folded onto a second line is `400`, and a coding besides `chunked` `501`. Every refusal closes
  the connection after shutting its side and reading what the client still sends for half a
  second, so the answer is not lost to a reset. *Why*: RFC 9112's choices where it gives the server
  one, and per-wait timeouts keep one meaning for "timeout" across the library. *Alternatives*: a
  deadline for the whole request (stops a slow upload still making progress; review with the user
  if wanted beside the per-wait limit); decoding `gzip` request bodies (a dependency for a rare
  case).
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-server`): **a `Response` is the client's class, given makers**
  (`Response.text`, `bytes`, `empty`, `withHeader`) and `http.reasonPhrase`; a handler is a plain
  `@sendable (ServerRequest) -> Response`; routes are a vector given to `Router(routes)`, made by
  `Route(method, pattern, handler)` or `Route.get` and its siblings, and `router.handle` is the
  handler. *Why*: one response type for both sides; a vector literal spans lines, where a builder's
  chain of `.get(…)` cannot, since a line starting with `.` does not continue the one above. The
  server writes `Content-Length` and `Connection` itself and refuses (traps on) a status outside
  200 to 599. Responses are whole bodies; a streamed (chunked) response is later work.
  *Alternatives*: a builder `Router().get(…)` (a chain on one line, or a `var` reassigned); a
  response writer handed to the handler (streaming, but a handler that forgets to finish it).
  Review with the user whether a line that starts with `.` should continue the expression above.
- Settled on the user's delegation 2026-11-10 (built on branch `lwt-server`): **routing**: segments matched one for one with the request's
  decoded segments, `:name` binding a non-empty one, the first matching route winning; a `GET`
  route answers `HEAD`; `404` for a path no route matches and `405` with `Allow` for one matched for
  other methods. *Why*: the smallest router that covers a service's paths, with escaped and raw
  Unicode routing alike and an escaped `/` staying inside its segment. *Alternatives*: wildcards
  for the rest of a path and patterns in a segment (later, if programs need them).
- Settled on the user's delegation 2026-11-11 (designed on branch `incremental-design`): **Lowering is made cheaper before anything is cached**:
  the [build plan](/compiler/roadmap/#incremental-builds-build-plan) fixes tuple records (a
  tuple's ancestors listed as the product of its parts' traits, each with its own entries: 33,858 of
  marshal's 71,646 functions) and lowering's constant factors first. WHY: lowering is 75 to 80% of
  an edit's rebuild and most of it is that defect; a cache built first would key and store tens of
  thousands of instances no program needs, and its measured win would be the defect's. Alternatives:
  cache first and fix after (stores the waste and measures the wrong thing); fix only (an edit still
  lowers the whole program).
- Settled on the user's delegation 2026-11-11 (designed on branch `incremental-design`): **The unit of reuse is the instance, grouped a file per
  source module; the front end stays whole-program and uncached.** WHY: monomorphization makes the
  instance, not the module, the thing whose code exists, and closed-world passes decide per call
  site; checking marshal is about 4 s of a 65 s rebuild, and the checker numbers every symbol of the
  program by index, so caching its output per module is a rewrite, deferred until a measurement says
  the front end is the larger part. Alternatives: per-module object files with generic code emitted
  where it is used (duplicates instances across modules and loses closed-world devirtualization); a
  cache of the checked front end per module first (targets 6% of the time); whole-program LLVM text
  cached by the program's hash (only helps an unchanged program, which the executable record already
  does).
- Settled on the user's delegation 2026-11-11 (designed on branch `incremental-design`): **An instance's key is its identity spelled by qualified
  names, its typed body printed canonically, the layouts and records it reads, and the world facts
  the whole-program passes used on it**, beside the compiler's stamp, level and kind. WHY: the
  printed typed tree carries every resolution and inference the checker made, so a change elsewhere
  that alters what the body means alters its key without a dependency graph of the source; world
  facts per call site let a new implementing class rerun the passes on its callers without lowering
  them again. Alternatives: the declaration's source text plus the hashes of the modules it imports
  (misses inference changes through a callee's result, and invalidates on every unrelated change to
  an imported module); one hash of the whole program's world for the passes (any new class reruns
  every pass on every instance).
- Settled on the user's delegation 2026-11-11 (designed on branch `incremental-design`): **Emitted names are stable**: a string by its content's
  hash, a descriptor or record by its type, a closure or trap site by its function and place, the
  module-level tables in a codegen unit of their own. WHY: today's numbering by discovery order lets
  one new instance renumber the rest, which changes every unit's text and defeats both caches.
  Alternatives: keep the numbering and patch indices at emission (every reused text rewritten
  anyway); number per module (still moves when a module gains a string).
- Settled on the user's delegation 2026-11-11 (designed on branch `incremental-design`): **The instance cache lives in the program's
  `.avin/build/incremental/<kind><level>/`, content-addressed, published by rename, with no lock;
  a missing, short or mismatched entry is a miss; a build trims what its manifest does not name and
  was not used in the last hour.** WHY: the same scheme as the unit and shim caches, safe for two
  builds at once because a key's bytes are the same whoever writes them. Alternatives: a lock per
  program directory (serializes `run` and `test` and the language server); the toolchain's shared
  cache for everything (instances of a program's own types are useless to any other program; sharing
  library-only instances can come later, since the keys already allow it). Review with the user:
  whether library-only instances should later go to the toolchain's cache, shared across programs.
- Settled on the user's delegation 2026-11-11 (designed on branch `incremental-design`): **An incremental build is tested against a clean one**:
  every edit in the compiler's tests checks that the incremental build's LLVM text equals a build
  with no cache, and the counts in `AVIN_BUILD_LOG` that only the affected instances were lowered;
  `AVIN_INCREMENTAL=verify` compares every reused instance with a fresh lowering. WHY: a key missing
  a dependency is the one way this design fails, and it fails silently; the clean build is an oracle
  that needs no list of what each edit should touch. Alternatives: count-only tests (prove reuse,
  not correctness); trusting the key (no defence against a missed dependency).
- Settled on the user's delegation 2026-11-11 (built on branch `option-nested`): **`Option[Nothing]` takes the form of the options it is seen
  as inside other values.** Its one value, `None`, is null as a pointer and the constant `None`
  object as an object, so the program-wide plan keeps it a pointer when every view of it inside
  another value (a `Vector[Option[Nothing]]` as a `Vector[Option[Dog]]`, a `Result`, a function's
  result, an option, a tuple, a data field, a map value) sees it as an option kept as a pointer, and
  an object when every such view sees it as one kept as an object (`Option[int]`,
  `Option[Option[Dog]]`); a bare `Option[Nothing]` written as an option of the other form is
  converted where it is written, as before. *Why*: options of classes and strings stay pointers in
  the common program that builds a collection of `None`s, at no cost in any other. *Left as full
  objects (correct, slower)*: a program that sees it inside other values both ways, since one
  vector of it could flow to both and no single form serves; and one that sees it as an option of a
  type parameter (`Vector[Option[Nothing]]` as `Vector[Option[T]]` in generic code), since the view
  is recorded once for every instance. *Alternatives*: (a) `None` null in every option, objects
  included, removing the conflict but adding a null test to every match, method call and table
  entry on an object option; (b) copying the containing value at the view, which a view by
  definition does not do and which breaks identity of mutable containers; (c) keeping
  `Option[Nothing]` always an object, the old rule, which stores every option of the program as an
  object as soon as one nested view to a pointer option exists. With it, an `Option[Nothing]`
  written as another option is always that option's own `None` (the null, or that option's constant
  object, where an object option used to receive the `Option[Nothing]` object itself), so as an
  `Any` it has the option's type and equals that type's other `None`s, as the equality chapter's
  rule by run-time type says; one seen inside a vector as an `Option[Dog]` and then boxed is an
  `Option[Dog]` too. A view inside an object-form value still leaves the `Option[Nothing]` object
  there, as any view leaves a value as it was made.
- Settled on the user's delegation 2026-11-11 (built on branch `https`): **TLS's API** ([system library](/spec/system-library/#tls)).
  A module of its own, `avin.net.tls`: `TlsStream.connect(host, port, settings)`, and
  `TlsStream.client(tcp, host, settings)` / `TlsStream.server(tcp, settings)` over a `TcpStream`
  the caller made; `TlsClient` (the authorities it adds) and `TlsServer(certificate, key)` (PEM
  text, checked by `TlsServer.of`) as immutable sendable values. In `avin.net.http` an `https` URL
  just works (port 443 by default), `Request.withTls(client)` choosing the settings, kept across
  redirects; `http.serve(…, tls = Some(server))` serves HTTPS. A trait `ByteStream`, which
  `TcpStream`, `UnixStream` and `TlsStream` implement, lets the HTTP code run over either, and
  `ReadAhead` became public so the TLS module shares the line reading. *Why*: the URL decides
  TLS, as a reader expects; settings are values, so a request, a thread and an actor carry them
  freely; the stream over a caller's `TcpStream` keeps that connection's timeout and cancellation
  without a second set. *Alternatives*: `TlsStream` in `avin.net` itself (every TCP program would
  then build OpenSSL, since a module's bindings come with it); a `TlsClient` holding one OpenSSL
  context for many connections (a context's reference count is not atomic, so it could not be
  shared between threads; a context is made per connection instead, which reads the machine's
  store each time, a few milliseconds); a `trust(pem)` that checks the PEM at once (it would need
  a context; a PEM that is not one is `Credentials` at the connection instead).
- Settled on the user's delegation 2026-11-11 (built on branch `https`): **the trust model**. A client trusts the machine's store,
  through OpenSSL's default paths, plus what `TlsClient.trust(pem)` adds; it always checks the
  host name (a name or an address), sends it for SNI, and speaks TLS 1.2 at the least, none of
  which can be turned off, as the Sysl package `openssl` decides. *Why*: trust that only grows
  cannot be misused to accept a bad certificate; a private, corporate or test authority is one
  case. *Alternatives*: a switch to skip verification (refused: it is how bad certificates get
  accepted); a bundled set of roots (it goes stale). **Gap**: on macOS the store is the `cert.pem`
  of Homebrew's OpenSSL, not the Keychain, so a root added only to the Keychain is not trusted;
  the `sysl` session would queue reading the Keychain in the `openssl` package on request.
  Review with the user whether Keychain roots are wanted.
- Settled on the user's delegation 2026-11-11 (built on branch `https`): **TLS errors**. `TlsError` is `Transport(IoError)`,
  `Certificate(text)`, `Handshake(text)` and `Credentials(text)`; an `HttpError` gains
  `Tls(TlsError)`, a `Transport` failure staying `Io` as for `http`. After the handshake a TLS
  failure in a read or write is `IoError.Other("TLS: …")`, so a `TlsStream` is a `ByteStream`.
  `openssl` 0.2.2 reports a refused certificate as one error, so `Certificate` covers an
  untrusted, self-signed or expired certificate and one for another host alike; the mapping is
  the one function `handshakeError` in `library/avin/net/tls/tls.avin`, and `openssl` 0.2.3's
  `Error.refusal()` (`HostMismatch`, `Expired`, `Untrusted`, `Other`) lets it split `Certificate`
  into those kinds there alone. *Alternatives*: one `Tls(text)` error (a program could not tell
  a refused certificate from a broken connection); typed errors for every read and write (every
  `ByteStream` user would need a second error type).
- Settled on the user's delegation 2026-11-11 (built on branch `https`): **a connection's end and timeouts over TLS**. The end of the
  stream is the peer's `close_notify` or the TCP connection closing, both read as the end, since
  many servers close without a `close_notify` and HTTP frames its bodies; a body read to the
  connection's end can therefore be cut short undetected, as over TCP. Every wait, the handshake's
  included, is a wait of the TCP connection beneath, under its `setTimeout` limit and cancellable
  by a scope; `http` sets the request's timeout before the handshake, and `http.serve` bounds the
  handshake by `readTimeout`. A handler that traps on a TLS connection has it closed unanswered.
  *Alternatives*: an error for an end without `close_notify` (it would refuse ordinary servers);
  a handshake timeout of its own (one more limit to set). Review with the user whether a body read
  to the connection's end should require `close_notify`.
- Settled on the user's delegation 2026-11-11 (built on branch `https`): **the standard library's Sysl packages**. The library names
  the Sysl packages its own bindings reach in `library/library.hocon`, a manifest whose
  `sysl-dependencies` block is read as a package's is. One joins a program's shims only where a
  binding or handle the program compiles names a Sysl item inside it, the package's name being a
  segment of the item's name before its last (`sh.sysl.openssl.client`); a program naming the same
  package keeps its own, unless both are releases of one repository and the library's is later.
  So only programs that reach `avin.net.tls` (`avin.net.http` among them) need OpenSSL installed,
  and the Homebrew formula depends on `openssl@3`. *Why*: a dependency written as a package's,
  without making the library a package (an `avin.hocon` there would make `library/tests` a
  package's tests). *Alternatives*: always adding the library's packages (every program with a
  binding would fetch and link OpenSSL); the runtime depending on `openssl` (every program would
  link it); a table from module to package in the compiler (a second place to keep in step).
- Settled on the user's delegation 2026-11-11 (built on branch `c-view-fields`): **Views lent by a
  field and to a callback**:
  - **A field accessor lending a view says `view`, `length:` and perhaps `writable`, as a binding
    does**, and nothing else; `length:` is a C expression in `self` and the indices, evaluated after
    the field is read, whether or not it is null. *Why*: the words a binding lending its result
    already uses, so a reader learns one form. *Alternatives*: a separate `buffer:` word for fields;
    evaluating the length only for a non-null pointer, which would let a `length:` that dereferences
    the struct run only sometimes.
  - **The C unit, not the checker, refuses a `writable` view of a `const` field**, by glue that
    assigns through the pointer, since the field's type is the header's and the checker never sees
    it. *Why*: a binding's prototype says `const` in Avin's source, a field's does not. *Alternative*:
    making the accessor spell the field's C type, which repeats the header.
  - **A view lent a callback may be an `Option`**, null with any length `None`, and otherwise null
    follows a binding's rule: an empty view for a length of zero, a stop for any other
    (`` C lent the function given to `f` null``); a negative length and one past the greatest `int`
    stop the program at the binding's call. *Why*: one rule for every view. *Alternative*: refusing an
    `Option` in a callback, which would make a C library that passes null for an empty chunk stop
    the program.
  - **A view lent a callback has no lender and belongs to the code the callback runs in**: another
    thread's use stops the program, a thread the callback starts among them, since the view is not
    handed over with a body that captures it as a handle's view is with its lender. *Why*: C frees
    the memory when the callback returns, so it must not be read anywhere the closing cannot order
    before the free. *Alternative*: handing it over like a block, which lets another thread read
    while the callback returns and C frees. Review with the user only if a library needs a thread to
    read a chunk in place.
  - **A callback view's length is converted through `long`** before the view's own checks, so an
    unsigned 64-bit length past a `long`'s range stops the program as that conversion does, and any
    smaller one past the greatest `int` as `more bytes than a view holds`. *Why*: the conversions a
    callback's integer arguments already use. *Alternative*: a dedicated 64-bit unsigned comparison,
    which only changes the message.
  - **The trap texts** name the binding: `` the `ByteView` lent to the function given to `f` was
    closed when it returned`` and `` … belongs to another thread``; a field's view is a handle's view
    and says `` the `ByteView` of `Face` was closed when `Face` was used again``.

Settled on the user's delegation 2026-11-11 (built on branch `json-derive`): **JSON derivation**.

- **One `JsonError` for parsing and decoding**, the path its last field with a default:
  `JsonError(message, line, column, offset, path: List[PathStep] = [])`, `inField`/`atIndex`
  putting a step in front, `pathText()` writing `items[2].name`, and `Display` putting that text
  and a colon before the message where the path is not empty. *Why*: a program handles one error
  type from the package, and every existing `JsonError(message, line, column, offset)` keeps
  compiling. *Alternatives*: the derivation chapter's `JsonError(path, message, line, column)`,
  which drops the offset and breaks every existing construction; a separate `DecodeError`.
- **`JsonReader` reads text in memory**, `JsonReader(text, maxDepth = 512)`, with `beginObject`,
  `nextKey`, `beginArray`, `nextElement`, `takeNull`, `readString`, `readBool`, `readInt`,
  `readLong`, `readReal`, `readJson`, `skipValue`, `end` and `error(message)`, the last naming the
  start of the last token read. *Why*: the derivations need exactly these, and the tokenizer is the
  parser's own, so its messages are `parse`'s. *Alternatives*: reading a byte stream now, which is
  the streaming reader's item; these methods are the seam it plugs into. A `JsonWriter(out:
  Writer)` writes compact text only, `writeNull()` for `null`. (The streaming reader has since
  taken this reader's place: see the Proposed entry **one `JsonReader`** below.)
- **Numbers**: an `int` or a `long` reads any whole number in range, `3.0` and `1e3` included, as
  `Json.asInt`/`asLong` do; a fraction or a number out of range is an error naming it. A NaN or an
  infinity is written `null` (as `render` writes it) and `null` does not read as a `real`.
  *Alternatives*: integer literals only; `null` reading back as NaN, which would make every `real`
  field silently nullable.
- **`Option[Option[A]]`**: `Some(None)` and `None` both write `null` and read back as `None`. *Why*:
  JSON has one null. *Alternatives*: refusing to derive for nested options.
- **Maps** are `HashMap[K, A]` and `TreeMap[K, A]` with `K: JsonMapKey`, a trait the package
  implements for `string`, `int` and `long` (the digits, read back only from what `toLongOption`
  reads); a key written twice keeps its last value. *Why*: an impl of a trait is for a data type or
  a class, not for the `Map` trait, and must take its type's parameters in order, so `Map[string, A]`
  cannot be named. *Alternatives*: string keys only. **Review with the user**: a field typed as the
  `Map` trait (what a map literal is typed as) cannot derive the JSON traits; covering it needs the
  language to let a library trait be implemented for a trait type.
- **Unknown keys are skipped** and a field written twice is an error (as the chapter says); a second
  discriminator key after the first is an unknown key. A missing field's error is at the object's
  closing brace, with the object's path. The nesting limit counts a skipped value's containers too.
- **Entry points** `json.encode(value) -> string` and `json.decode[A](text, maxDepth = 512)`, the
  second refusing anything but white space after the value. *Alternatives*: only the traits'
  methods, leaving every program to make a writer and a reader.

- Settled on the user's delegation 2026-11-11 (built on branch `time-calendar`): **`startOfYear()`, `startOfMonth()` and `startOfWeek()` on a
  `DateTime` answer midnight** of that day, while `endOfMonth()` keeps the time of day, as the
  chapter says. *Why*: a start is the first moment of its span, as `startOfDay` is; keeping the
  time would make `r.startOfMonth()` mean "the same time on the 1st", which `r.withDay(1)` already
  says. *Alternatives*: keep the time of day for all four (symmetric with `endOfMonth`, but two
  spellings of `withDay(1)`); make `endOfMonth` the last microsecond of the month too (a start/end
  pair, but the chapter decided "at the same time of day"). Review with the user whether
  `endOfMonth` on a reading should rather be its month's last microsecond.
- Settled on the user's delegation 2026-11-11 (built on branch `time-calendar`): **a calendar answer outside `Date.Min` to `Date.Max` traps
  with a message naming the date** (`the date +5881580-07-31 is outside Date.Min to Date.Max`), or,
  where the answer is a count of days with no year to write (a weekday search, `startOfWeek`,
  `DateTime ± Duration`), `the date N days from 1970-01-01 is outside Date.Min to Date.Max`; the
  maker `date(y, m, d)` traps the same way, and a year far beyond the dates (`withYear(long.MaxValue)`)
  is checked before any arithmetic, so it never overflows on the way. `daysUntil` and `DateTime -
  DateTime`, whose answers are an `int` and a `Duration`, trap as an integer overflow does. *Why*:
  the chapter says leaving a type's range traps "as an integer overflow does"; a message naming the
  date says more than `integer overflow` where there is a date to name. *Alternatives*: a bare
  integer overflow trap everywhere (shorter code, a less useful message); clamping to the bounds
  (silently wrong).
- Settled on the user's delegation 2026-11-11 (built on branch `time-calendar`): **`withYear` takes a `long`**, the type `year()` answers, so
  `d.withYear(d.year() + 1)` needs no conversion; the other `with…` take an `int`. *Alternatives*:
  `int` like the makers (a conversion at every use of `year()`), or changing `year()` to `int`
  (every year of a `Date` fits, but a change to step 1's fields).
- Settled on the user's delegation 2026-11-11 (built on branch `time-calendar`): **`with…` and `startOf…` on `Zoned` wait for step 3**, which
  builds `Resolution` and the zone's handling of a skipped or repeated reading they rely on; step 2
  gives `Zoned` only `isoWeek()` and `isoWeekYear()`, which need no resolution.
- Settled on the user's delegation 2026-11-11 (built on branch `generic-variant-inference`): **A
  shared variant name takes the type its siblings decide**: a bare or called variant name several data types share, passed for
  a generic function's type argument, is decided by what the call's other arguments decide of that
  type argument: `assertEq(p.segments(0), MoveTo(10.0, 20.0))` takes `T = Segment` from the first
  argument, so `MoveTo` is `Segment`'s. It is checked after the arguments that are not such names,
  wherever it is written, so `same(MoveTo(1.0, 2.0), segment)` resolves too, and named arguments go
  the same way. The elements of a collection literal and the two branches of an `if` agree the same
  way: `[Close, segment]`, `if c then Close else segment`. Only the data type is taken from the
  others; its type arguments are left for the variant's own fields to decide, so a later argument
  may still widen them, as any argument's may. Where every argument for the type argument is such a
  name, or the others decide a type that is none of the name's owners, the use is refused with the
  usual message. Why: the name's type is fixed by the call before it is chosen, exactly as with a
  parameter of the data type itself, and checking the shared names last makes the result
  independent of argument order, which a reader expects of `assertEq(a, b)` and `assertEq(b, a)`.
  Alternatives: left to right only, so `same(MoveTo(…), segment)` is refused (order-dependent, and
  the expected value usually comes second); taking the decided type with its type arguments fixed
  too (refuses a later argument that widens them); a second checking pass over the whole call
  (the same answers here, with each argument checked twice).

- Settled on the user's delegation 2026-11-11 (built on branch `c-binding-gaps`): **An array C fills, its length known before the call**: an
  `out T *` parameter (one star, against `out T **` for an array C allocates) named with `array:`, its
  `length:` a C expression over the parameters evaluated in the glue before the call; the glue
  `malloc`s that many elements (at least one), C fills them, and they are copied into an `Array` and
  freed with `free`. A negative length or one past the greatest `int` calls nothing (the glue answers
  a zeroed result) and the length stops the program when it is read; length checks now come before the
  null check for every array C answers. It is never in an `Option` and takes no `free:`. *Why*:
  Cairo's `cairo_get_dash` writes `cairo_get_dash_count` doubles, a length no parameter carries; the
  words `array:`/`length:` are the ones an array C answers already uses, the number of stars says who
  allocates, and the answer is the same `Array` in the result. *Alternatives*: an Avin `Array` passed
  in, checked by the glue to be at least `length:` long (the library would make and size it, and a
  precondition the binding cannot see becomes the program's); a `capacity:` word on an in-array; the
  array allocated in the collected heap (a moving or concurrent collection, and a `blocking` call,
  would need it pinned).
- Settled on the user's delegation 2026-11-11 (built on branch `c-binding-gaps`): **Several arrays in one call**: `array:`/`bytes:` take a list,
  `length:` and `free:` then lists of the same length in the same order, `""` in `free:` freeing
  nothing; the glue writes each length through its own pointer and has a free function per array
  (`avin_c_array_free_<symbol>_<j>`). *Why*: the existing words, with the list shape `keeps:` already
  has; one array keeps its single-string form. *Alternatives*: repeating `array:`/`length:`/`free:`
  groups in order (order-dependent pairing nobody can see); marking the count in the prototype
  (`length(glyphs) out int *num_glyphs`), which would drop the count from the result and reuse a
  marker that means something else for `inout`; a `free:` keyed by name (no map literal in an
  annotation).
- Settled on the user's delegation 2026-11-11 (built on branch `c-binding-gaps`): **`null` in a prototype passes null**: a word before a pointer
  parameter's type; the parameter is no Avin parameter and no out value, and a function whose caller
  chooses (Cairo's clusters, wanted or not) is bound twice, once with `out` and once with `null`.
  *Why*: what is passed is fixed per binding, so the result's type stays fixed; the prototype is where
  the other per-parameter words (`out`, `inout`, `userdata`, `length`) already live. *Alternatives*:
  an Avin `bool` or `Option` parameter choosing at run time (the result's shape would depend on a
  value); a `skip:` list on the annotation (a second place saying what one parameter is).
  **Review with the user**: whether `null` should also let an Avin `Option` parameter decide.
- Settled on the user's delegation 2026-11-11 (built on branch `c-binding-gaps`): **A field accessor reads and writes an enum mirror**: the C
  unit checks the field is of an integer type (any, not only the mirrored enum) and measures it; a read
  value no variant holds stops the program as an enum result does (`` the field `f` of `H` held a
  value no variant of `M` holds``), a write stores the enumerator's value, and an `Option` of a
  mirror is refused. *Why*: a struct mirror's field already crosses this way, and Cairo's path header
  is a union member; accepting any integer type lets an `int` field holding an enum's values be
  mirrored. *Alternatives*: requiring the field to be exactly the mirrored enum (the field's type is
  the header's, which the checker never sees, so the C unit would compare type names); reading it as
  an `int` and converting in Avin (the workaround cairo used).
- Settled on the user's delegation 2026-11-11 (built on branch `c-binding-gaps`): **`attach:` failing
  answers `Err` where the binding answers `Result[H, E]`**: a binding whose C result is a handle of a class with `attach:`, which keeps
  something and answers no out values, may answer `Result[H, E]`, `E` an integer type or an enum
  mirror; when `attach:` answers non-zero, the record is undone, the handle given back (its kept list
  cleared) and `Err` of the answer returned, converted as a status code is. Without the `Result` the
  trap stays. *Why*: Cairo's `cairo_surface_set_user_data` on a surface in error answers the surface's
  own status, so the `attach:` answer is exactly the error the program wants, and no new word is
  needed; the `Result` is the existing status shape. *Alternatives*: a class-level `status:` C
  expression evaluated on every new handle (a new word, and a check on bindings that keep nothing);
  answering `None` for a refused attach (loses the reason); keeping the trap and the library's
  pre-checks (what cairo had to write).
- Settled on the user's delegation 2026-11-11 (built on branch `array-refmaps`): **An array's descriptor carries a per-element reference map** (the
  runtime half of the union tag-and-payload layout, under Settled → unions → Representation). An
  array of elements `k` words wide names one of three tracers: every element one reference
  (`avin_rt_trace_array`, as before), no reference word (the ordinary tracer with no offsets, as
  before), or a mapped element (`avin_rt_trace_array_map`), whose descriptor lists the byte offset
  within an element of each reference word, in the same trailing offset list an object's
  descriptor uses for its fields; the tracer reads exactly those words of every element and never a
  scalar word, so a union's bits word holding something that looks like a heap address keeps
  nothing alive. The compiler builds every array descriptor through `array_descriptor(…, words)`,
  which picks the fast path whenever the map fits one; a one-word element is never mapped. Handing
  a value over to another thread reads the same words through the same scanners (`scan_object`).
  The two fast paths compile to the same machine code as before. *Why*: the offset list already
  exists, is what the emitter writes for objects, and costs nothing for the arrays every program has
  today; elements of a union are at most three words, so a short list per element is cheap.
  *Alternatives*: a bit mask word per descriptor (one word, but a second format beside the offset
  list, and a bit scan per element); a tracer per element shape generated by the compiler (fastest,
  but code per instance and a new symbol per shape); keeping union elements boxed in arrays (no
  runtime change, an allocation per element).

- Settled on the user's delegation 2026-11-11 (built on branch `json-stream`): **`avin.io.Reader`**, a trait of one method, `read(max) ->
  Result[Array[byte], IoError]`, an empty array at the end, which `fs.File`, `net.TcpStream` and
  `net.UnixStream` implement with the `read` each already had. *Why*: the streaming JSON reader
  reads "a byte stream", and the three byte sources had the same method with no common type, so a
  function could take only one of them. *Alternatives*: a read function `(int) -> Result[Array[byte],
  IoError]` passed to the JSON reader (no library change, but every caller writes the lambda and
  other byte readers gain nothing); a `ByteSource` trait private to the json package (each source
  wrapped by hand). Standard input and a command's output are not readers yet; making them so is
  later work when a program needs it.
- Settled on the user's delegation 2026-11-11 (built on branch `json-stream`): **the streaming JSON reader's interface**, `JsonReader` with the
  pull events of `JsonEvent` (`next`, `peek`, `depth`) beside the calls the derivation chapter's
  decoder makes (`beginArray`, `nextElement`, `beginObject`, `nextKey`, `readString`, `takeNull`,
  `skipValue`, `error`), completed with `readBool`, `readLong`, `readInt`, `readReal`, `readValue`,
  `readDocument` and `finish`; made by `fromReader`, `fromString` and `fromBytes`. *Why*: one type
  serves both a program walking a large document and a derived decoder, and the events are the
  smallest set that names every token; a decoder's calls are each a step or two over them.
  *Alternatives*: callbacks pushed per event (a decoder cannot pull a field's value at its type);
  an iterator of events (no `Result` per step, so a fault would have to trap); a reader of several
  values in a row (JSON lines), later if a program needs it, as `parse` takes one value too.
  Review with the user: the variant names (`StartObject` … `EndOfText`), chosen to stay apart from
  `Json`'s own variants in the same module.
- Settled on the user's delegation 2026-11-11 (built on branch `json-stream`): **a stream gives `parse`'s faults at `parse`'s places**, and a
  failure to read is a `JsonError` (`the text could not be read: <the IoError's message>`) at the
  place reading stopped, rather than a separate error type. Bytes that are not UTF-8 are found as
  they are read, so a grammar fault earlier in the text is reported first, where `parse` of bytes,
  which checks the whole text first, reports the UTF-8 fault. *Why*: one error type for the decoder
  to carry, and a stream cannot look ahead to the end. *Alternatives*: `Either[IoError, JsonError]`
  per call (every decoder's signature grows); trapping on a read failure.
- Settled on the user's delegation 2026-11-11 (built on branch `json-stream`): **`JsonError`'s `line`, `column` and `offset` are `long`s**
  (they were `int`s). *Why*: a stream larger than memory runs past two gibibytes, and a minified one
  is on a single line, so an `int` column or offset would trap on overflow. *Alternatives*: `int`s
  that saturate (a wrong place, silently); a `long` offset only (a column of a long line still
  overflows).
- Settled on the user's delegation 2026-11-11 (built on branch `json-stream`): **a parent after `:` may be named through its module**,
  `class Body: shapes.Base("b"), io.Reader`, resolved as a type written `m.T` is. *Why*: it was
  refused with "expected a superclass" though the same name is a type everywhere else; found when
  `fs.File` was made an `io.Reader`. *Alternatives*: none weighed; the bare name through an import
  stays the common form.
- Settled on the user's delegation 2026-11-11 (built on branch `json-stream`): **one `JsonReader`, the streaming one, which the derived
  decoders read through**. The in-memory reader of the derivation item is gone and so are the
  token methods it added to `parse`'s private reader; `json.decode[A](text)` reads through
  `JsonReader.fromString`. Where the two readers named a call differently the streaming names stay:
  **`readValue`** (not `readJson`) and **`finish`** (not `end`). *Why*: one reader means one grammar
  and one set of places for a fault, whether the text is in hand or arriving; `readValue` pairs with
  `skipValue` and `readDocument`, and `finish` cannot be read as the end of an object or array, as
  `end` can; the streaming names had the more users (its tests, the chapter's example, three corpus
  programs) against one test and one decoder line. A text reader is made by
  `JsonReader.fromString(text)`, since the class's constructor takes a source; the derivation
  entry's `JsonReader(text, maxDepth)` no longer compiles (its only users were the package's own
  test and `json.decode`). *Alternatives*: keeping both readers behind a trait the decoders take
  (two grammars to keep in step, and a decoder call through a trait for every token); keeping both
  names of each call (two words for one thing in the chapter's table); a second constructor
  `JsonReader(text)` (the language gives a class one constructor).
- Settled on the user's delegation 2026-11-11 (built on branch `json-stream`): **where the two readers worded an error differently, the settled
  derivation wording stays**: `expected a boolean` (not `expected true or false`), `found a boolean`
  for `true` and `false` alike (not `found false`), and `the number N is not a whole number within an
  int's range` / `a long's range` (not `does not fit an int` / `within a long`), `readInt` naming the
  number as written, a fraction included. A fault in the text itself is the stream's, worded as
  `parse` words it: `tru` read as a `bool` is `unexpected word 'tru' where a value was expected`, no
  longer `expected a boolean, found 't'`, and text ending where a string was wanted is `unexpected
  end of text where a value was expected`, since the stream tokenizes before the decoder asks for a
  kind. A fault met while an array looks for its next element (`[` and the end, `[1 2]`) carries the
  array's path, not an element's (`[` decoded as a `Vector[int]` has an empty path where it had
  `[0]`), since no element has begun. *Why*: the derivation's messages were settled and name kinds throughout (`a number`, `a
  string`), and a stream cannot answer "what character is next" without reading a token.
  *Alternatives*: the stream's wordings throughout; the derivation's character-level messages, which
  would make the stream peek bytes outside its tokenizer. Review with the user: the `[0]` path a
  fault at the next element used to carry.
- Settled on the user's delegation 2026-11-11 (built on branch `json-stream`): **`json.decode[A](source: Reader, maxDepth = 512)`**, an overload
  of `decode` reading a value of a derived type as its text arrives, from a file, a connection or any
  `Reader`, refusing anything but white space after it. *Why*: the streaming reader exists so that a
  document need not be held whole, and a program decoding its own type should not have to build the
  reader and call `finish` itself. *Alternatives*: `JsonReader.fromReader` and `A.decode(r)` by hand;
  a `decode` taking a `JsonReader` (which leaves the end of the text unchecked or checks it where a
  caller wanted to read on).
- Settled on the user's delegation 2026-11-11 (built on branch `json-stream`): **`net.ByteStream` is an `io.Reader`** (`trait ByteStream:
  Sendable, Reader`, its `read` the trait's), so `TcpStream`, `UnixStream` and `TlsStream` are all
  readers and JSON reads from a TLS connection as from a plain one. *Why*: both sides arrived at
  once, the stream item making `TcpStream` and `UnixStream` readers by name and the TLS item giving
  the three a common `ByteStream`; declaring it on the trait covers every connection, the TLS one
  included, with one line. *Alternatives*: naming `io.Reader` beside `ByteStream` on each class (TLS
  left out unless named a third time).

- Settled on the user's delegation 2026-11-11 (built on branch `time-period`): **`a.monthsUntil(b)` counts a month whole when `b`'s day of the
  month has reached `a`'s** (the month difference, less one where `b`'s day is smaller, or plus one
  going backward where it is larger), and `a.until(b)` is those months and the days from `a +
  months(n)` to `b`. *Why*: it is the chapter's "as a person counts them" (2026-01-31 to 2026-02-28
  is 0, to 2026-03-01 is 1), it makes `b.monthsUntil(a) == -a.monthsUntil(b)` always, and with
  months-first addition `a + a.until(b) == b` holds in both directions (a sweep of 21,228 pairs
  tests it). `b - a.until(b) == a` and `b.until(a) == -a.until(b)` fail for some pairs near month
  ends (2026-01-30 and 2026-03-01), and the chapter now says so. *Alternatives*: the largest `n`
  with `a + months(n) <= b` (answers 1 for 2026-01-31 to 2026-02-28, against the chapter); counting
  backward periods from `b` so the reverse is the negation (breaks `a + a.until(b) == b` for
  `a > b`).
- Settled on the user's delegation 2026-11-11 (built on branch `time-period`): **`p.toIsoString()` writes a period in ISO 8601's form**,
  `P1Y2M3D`, the months split into years as the display splits them, `P0D` for the empty period,
  and a negative part with its own sign (`P-1Y-2M`, `P1M-3D`). *Why*: the display `1y 2mo 3d` is
  for people; exchange formats want ISO's, and a period's parts may differ in sign, which a single
  leading `-` cannot write. *Alternatives*: a leading `-` when every part is negative and per-part
  signs only otherwise (two rules for one value); no ISO text until step 5's parsing (a writer
  without a reader for a while). Review with the user whether the step 5 parser should read it
  (`parsePeriod`), which the chapter does not list.
- Settled on the user's delegation 2026-11-11 (built on branch `time-period`): **`toYearsPart()` and `toMonthsPart()` carry the months' sign**
  (`months(-14)` is -1 year and -2 months), as `Duration`'s parts do, and the display and ISO text
  write each part with that sign (`-1y -2mo`). **`p.isZero()`** asks for the empty period, as
  `Duration.isZero()` does. *Alternatives*: floor division (-2 years and 10 months, which reads as
  something else); no `isZero` (`p == Period.Zero` says the same, less directly).
- Settled on the user's delegation 2026-11-11 (built on branch `time-period`): **calendar steps count in a `long`**: `Date ± Period`,
  `plusYears(n)` (`12 * n` months) and `plusWeeks(n)` (`7 * n` days) never overflow on the way, so
  an answer past the dates traps naming the date (`the date +5881580-08-11 is outside Date.Min to
  Date.Max`), or the count of days where only a day step overflows, as step 2's traps do; the
  makers `years(n)` and `Period * n`, whose answers are periods, trap as an integer overflow.
  `plusYears`, `plusMonths`, `plusWeeks` and `plusDays` take an `int`, as the makers do.
- Settled on the user's delegation 2026-11-11 (built on branch `time-period`): **`Zoned ± Period` and `Zoned`'s `plus…` wait for step 3**,
  whose `Resolution` decides a period landing in a gap or an overlap.
- Settled on the user's delegation 2026-11-11 (built on branch `time-period`): **the formatter breaks a long one-line `if` only after its last
  `then`, `elif` or `else`**, keeping its tests, its earlier branches and any operators before it
  on one line. *Why*: a line after an `if`'s test is its block, so a break there changes the
  program (the formatter refused `library/avin/time/period.avin`); a break in an earlier branch
  would put the next `elif` or `else` inside a continuation line. *Alternatives*: wrapping the
  `if` in brackets to break inside it (adds tokens, which the formatter never does); leaving any
  line holding an `if` long (also loses the safe break in the last branch).

- Settled on the user's delegation 2026-11-11 (built on branch `time-parse`): **`BadShape` names what was expected**:
  `BadShape(text, at, expected)`, its message `cannot read "2026/03/08": expected '-' at byte 4`, and
  `Trailing`'s `expected the end at byte 10`, in place of the chapter's earlier `not the expected
  shape at byte 4`. *Why*: a reader fixing the text needs to know what would have been accepted
  there, and the parser knows it at the point it stops. *Alternatives*: `BadShape(text, at)` with one
  fixed message (says where but not what); a separate `expected` method computing it again from the
  text (a second parser).
- Settled on the user's delegation 2026-11-11 (built on branch `time-parse`): **a parse reads the whole shape first, then checks the
  fields in the order they are written**, so `2026-02-30T24:00` is refused for its day and
  `2026-02-30T24:00 ` for its trailing space; a zone name is looked up last. The fields an
  `OutOfRange` names are `year`, `month`, `day`, `date` (past `Date.Min`/`Date.Max` inside their
  own years), `hour`, `minute`, `second`, `fraction`, `offset`, `instant` (past
  `Instant.Min`/`Instant.Max`), `months` and `days`. *Why*: one rule for which error a text with
  several faults gets, and a shape error never hides behind a range error further left.
  *Alternatives*: failing at the first fault left to right (a range error could then mask a
  shape error after it, and the answer would depend on how far the reader had got).
- Settled on the user's delegation 2026-11-11 (built on branch `time-parse`): **a year of four digits has no sign, and a signed year has
  four digits or more**, even inside `0` to `9999` (`+2026`, `-0000` read); an unsigned fifth digit
  is a shape error at byte 4. A seventh digit of fraction is `OutOfRange(text, "fraction")` even
  when it is `0`. `t`, `z`, a comma as the decimal sign, `−` (U+2212) and a bare `±hh` offset are
  refused. *Why*: the display never writes them, and each accepted alternative is one more form
  every other reader of the text would have to accept. *Alternatives*: ISO 8601's lowercase and
  comma variants (wider, and nothing here writes them).
- Settled on the user's delegation 2026-11-11 (built on branch `time-parse`): **`parseZoned` with a zone name checks the offset against
  the zone**: the reading minus the offset is the instant, and the zone's clocks must show that
  offset then, or the text is `OutOfRange(text, "offset")`; so both readings of an autumn overlap
  read and a reading in a spring gap is refused. Any `ZoneError` from looking the name up
  (unknown, not a name, an undecodable file) is `UnknownZone(text, name)`. A fixed zone's name in
  brackets (`+05:30[+05:30]`, `Z[UTC]`) reads as the fixed zone. Built over `zone(name)`, which is
  on dev ahead of the rest of step 3. *Why*: the offset makes the instant unambiguous, and a text
  whose offset the zone never shows names no reading of that zone. *Alternatives*: trusting the
  zone and re-reading the instant from the reading alone (needs step 3's `Resolution` for gaps and
  overlaps, and drops the offset the text gave); trusting the offset and ignoring the zone's (keeps
  a `Zoned` whose offset its zone contradicts). Review with the user whether a mismatch deserves a
  variant of its own rather than `OutOfRange`'s `offset`.
- Settled on the user's delegation 2026-11-11 (built on branch `time-parse`): **`parsePeriod(text)`**, named as the chapter's other
  readers are, reads `P`, then a number of years `Y`, of months `M` and of days `D` in that order, at
  least one, each with its own `-` where negative, so every `toIsoString()` reads back equal. Forms
  `toIsoString()` does not write but whose numbers are plain also read (`P12M` as `P1Y`, `P0Y`,
  `P007D`, `P-0D`); a total of months or days beyond an `int` is out of range. `+`, `W`, a `T` time
  part, a fraction and lower-case letters are refused. *Why*: a reader for the writer, settled as
  the step 5 follow-up to the `toIsoString()` entry above. *Alternatives*: `Period.parse` on the
  companion (unlike the chapter's other readers); accepting only the canonical form (refuses
  `P12M`, which means one exact period); ISO's weeks (`P2W`, fourteen days, which `toIsoString`
  never writes).

- Settled on the user's delegation 2026-11-11 (built on branch `tls-refusal`): **why a certificate was refused**. `TlsError.Certificate`
  gains a second field, `why: TlsRefusal`, and `TlsError.refusal() -> Option[TlsRefusal]` answers it
  (`None` for the other errors). `TlsRefusal` is `HostMismatch`, `Expired`, `NotYetValid`,
  `Untrusted` and `Other(code: long)`, and its `message()` and the error's say the reason in words.
  Amended 2026-11-11 (branch `openssl-024`): the mapping to the five kinds comes from the package
  `openssl` 0.2.4, whose `Error.refusal()` groups the X509 verdicts (62 and 64, a certificate for
  another name or another IP address, host; 10 expired; 9 not yet valid; 2, 18, 19, 20, 21
  untrusted; any other `Other(code)`). `TlsRefusal` mirrors `openssl`'s `Refusal` and the private
  `TlsFailure` binds `refusal()` as a [mirror's bound method](/spec/sysl-bindings/#methods-of-a-mirror),
  so Avin keeps no table of its own; a failure with no verdict is `Other(0)`. *Why*: a program
  can tell a wrong name from an untrusted issuer, which read identically before; the reason is a
  value, so it is matched, not parsed out of text. The field is not an `Option`, since a refused
  certificate always has a verdict and `Other` carries the unusual ones. The field is `why` because
  a variant field named `refusal` would clash with the method. *Alternatives*: an `Option` field (a
  `None` that cannot happen); one variant per kind, `HostMismatch` and the like as `TlsError`
  variants (every existing `Certificate(_)` pattern would stop matching and a program that only
  cares that a certificate was refused would need four arms); keeping `Certificate(text)` and
  adding a separate query (the reason would have to be stored somewhere anyway). **Breaking**:
  `Certificate(_)` patterns become `Certificate(_, _)`. Review with the user whether the
  `Certificate` arity change is wanted over a variant per kind.

- Settled on the user's delegation 2026-11-11 (built on branch `c-binding-gaps-2`): **A field accessor reads and writes a struct
  mirror whole**: the C unit checks the field is the struct the mirror mirrors (any member path,
  through pointers and array elements); the reader's glue copies the field into memory Avin
  allocates, read into a new value as a filled `out` mirror is, the memory then freed; the writer
  lays the value out as C's struct and the glue assigns it. A mirror holding a string, directly or
  in a mirror it holds, is refused as a writer's value, and an `Option` of a mirror is refused
  either way. *Why*: FreeType's `face->bbox` and `face->size->metrics` are structs read whole, and
  one accessor per member (the package's workaround, eight accessors and a tuple) loses the type;
  the mirror's layout is already checked and measured, so the copy is the struct passed by value's
  own conversion. Writing a string would leave C holding a copy no one frees, the rule for string
  fields; a field is never null. *Alternatives*: read only, no writer (writing is the scalar rule
  extended and costs one glue function); a writer that leaks or keeps the string copies (C would
  own Avin's allocation); answering a handle on the struct inside the owner (lifetime tied to the
  owner, and every member a call). Review with the user: a mirror holding a string is read-only.
- Settled on the user's delegation 2026-11-11 (built on branch `c-binding-gaps-2`): **One pointer to a typedef of `char` is a
  string**: where a string crosses (parameter, result, `out` value, callback parameter), a C type
  written as one pointer to a typedef (`const FT_String *`, `FT_String const *`, `T *const`) is
  checked by the C unit as a typedef of the whole pointer: `char *` or `const char *` as a result,
  `const char *` as a parameter, so a typedef chain and a `typedef const char T` both work. A
  pointer to a typedef of `unsigned char` or `signed char` is refused by a check of its own whose
  message says those are bytes rather than text, for results, parameters, fields and struct
  mirrors' fields. *Why*: only the headers know what `FT_String` is, and the existing typedef rule
  already defers the string check to the C unit; fields already resolved typedefs, since their
  type comes from the header. *Alternatives*: rewriting the prototype's spelling to `char` (loses
  the header check's typedef names); a new C type kind in the parser for a pointer to a typedef
  (every handle `cairo_t *` is one, so each position would have to tell the two apart).
- Settled on the user's delegation 2026-11-11 (built on branch `c-binding-gaps-2`): **The c-bindings chapter's FreeType library is
  released with `FT_Done_Library`**, not `FT_Done_FreeType`: the latter frees the library's memory
  manager whatever the reference count, so a face kept past the library's handle would allocate
  through freed memory. The chapter's prose and both examples say so. *Why*: a `release` must give
  back exactly one reference when the class declares `retain`. *Alternatives*: none that keeps
  `keeps:` sound; the manager's small record is not given back, as the freetype package notes.

- Settled on the user's delegation 2026-11-11 (built on branch `openssl-024`): **a mirror's methods may be bound to Sysl**
  ([methods of a mirror](/spec/sysl-bindings/#methods-of-a-mirror)). A method without a body in an
  `impl` of a `@sysl` data type that names no trait is bound to the Sysl method of its name's words
  in snake_case, or the one its own `@sysl("name")` names, with `blocking` and `named` as on a
  handle's method; it may be overloaded by parameter types; its parameters and result cross as a
  binding's do. Each call converts the receiver into the Sysl value, puts it in a counted box of its
  own and calls the method on that, so a Sysl method taking `self`, `&self` or `*self` is bound
  alike, and what a `*self` method writes is lost with the box: the mirror is a value. A method
  without a body on a plain data type, or in an `impl` of a trait, still needs one; a method with a
  body takes no `@sysl`. *Why*: `openssl`'s `Error.refusal()` is a method of a struct Avin
  mirrors, and a mirror had no way to reach it, so Avin copied its table. The `impl` form, because
  a data type's body holds its variants, and a handle's body form would need a second kind of body
  member for data. The box, because the shim cannot see which receiver the Sysl method takes and
  `&self` needs a counted reference; the cost is one allocation per call. *Alternatives*: methods
  in the data's own body after its variants (a new body grammar for data); a top-level binding
  whose first parameter is the mirror, `@sysl("sh.sysl.openssl.Error.refusal")` (a path that names
  a method rather than a function, read differently from every other binding's); passing the value on the stack and
  refusing `&self` methods (most Sysl readers take `&self`); a `*self` method's write copied back
  (a mirror is immutable in Avin). Also chosen here: `TlsRefusal` itself mirrors `openssl`'s
  `Refusal` rather than a private mirror mapped to it, since a second data type with the same
  variant names cannot share the module, and the two would be the same five cases. Review with
  the user whether a public library type mirroring a package's enum is wanted: a variant `openssl`
  adds must be added to `TlsRefusal`, which Sysl reports as not matching until it is.

- Settled on the user's delegation 2026-11-11 (built on branch `stable-names`): **Each emitted table is named `@"avin.<kind>:<name>"`, the name
  spelled by qualified names in full, with no hash suffix**: a type as `avin.Option[int]`,
  `class geo.Box[]`, `object geo.Box[]`, `(int, string)`; a descriptor by its type, a variant after
  its type's (`avin.Option[int].Some`); a record by its type; a constant closure by its function; a
  module-level value by its qualified name (`object ` before an object's). WHY: readable in the
  emitted text and in a profile, injective by construction (two types of one module never share a
  name but an object and its companion, which the prefixes tell apart), and short enough: marshal's
  longest symbol is 319 bytes, 275 of its 111,642 symbols are over 200, and every one is local to
  the program, so no linker or object format limit is near. The emitted text grew 11% (126 MB to
  140 MB for marshal). Alternatives: a hash of the spelling (shorter, unreadable, and a collision
  is silent until the assembler reports it); the spelling cut at a length with a hash of the rest
  (not needed at these lengths). Emission stops with the name where two of one kind would share
  one, rather than the assembler reporting a redefinition.
- Settled on the user's delegation 2026-11-11 (built on branch `stable-names`): **A string constant is named by the first 16 hexadecimal digits
  of the SHA-256 of its bytes**, a later string sharing them by its index after a dot. WHY: the same
  string has the same name in every program and every build; 64 bits make a shared prefix within one
  program improbable past any program's size. Alternatives: the whole digest (32 more bytes on each of
  thousands of strings); the text itself (unbounded, and escaping in a symbol).
- Settled on the user's delegation 2026-11-11 (built on branch `stable-names`): **A trap site is named by the innermost top-level function or
  method declared around it, the byte offset from that declaration's start, and 12 hexadecimal
  digits of the SHA-256 of its reason**; a site in code the compiler wrote (an empty span) by the
  instance being lowered; one in a declaration the checker copies (a derived `Display` and its
  siblings, one method name on several types sharing a declaration) or outside every function by its
  file and offset. Trap sites are never copied into the units that reach them at `-O2`. WHY: lines
  added above a function move its sites' lines, which only the tables' unit holds, and leave every
  function's text as it was; a copy of a site, which no code reads, would carry its line into the
  units that reach it. Alternatives: the instance and its sites in order (a site is shared by every
  instance of a generic function, so its name would depend on which instance was lowered first);
  file and line (moves with every line added above). Review with the user: a site in a library
  template is named by file and offset, which an edit to that library file moves.
- Settled on the user's delegation 2026-11-11 (built on branch `stable-names`): **A local function or lambda is numbered among its top-level
  function's local functions of its name, from 1 in declaration order** (`<program>.pick.lambda#2`).
  WHY: two blocks may each declare a local function of one name, and the number counts only within
  the function, which an edit elsewhere never changes. Alternatives: the checker's symbol index
  (moves with every symbol declared before it); the byte offset within the function (moves with an
  edit above the lambda inside the function, which changes the function's text anyway, and is longer).
- Settled on the user's delegation 2026-11-11 (built on branch `stable-names`): **A shared constant object is named by a hash of what it holds,
  an unshared one (a vector literal's) by its module-level value and its place among that value's
  objects; a C trampoline by its binding and place.** WHY: equal shared objects are one object
  already, so their content is their identity; an unshared object's identity is its value's.
- Settled on the user's delegation 2026-11-11 (built on branch `stable-names`): **A module-level value that holds a reference keeps its slot in the
  roots frame, and its name is a private alias of that slot**; code names the alias. WHY: the slot's
  position moves when a value is added before it, and the alias keeps that position in the tables'
  unit alone; the runtime's view of the frame is unchanged. Alternatives: a global per value and a
  frame of addresses (a runtime change for the same effect).
- Settled on the user's delegation 2026-11-11 (built on branch `stable-names`): **The module-level tables are the last codegen unit of `n`, the
  functions spread over the other `n - 1`, and a unit holds its pieces in the order of their names.**
  WHY: a new string, type or trap site then changes the tables' unit and its user's alone, and a
  unit whose own pieces are unchanged is the same text whatever order lowering found them in.
  Alternatives: tables in the first unit that names them (a new early user moves them, changing two
  units); one unit per kind of table (more units for no fewer recompilations). Left as it was: a
  function called from one function alone still lives in its caller's unit, so a second caller in
  another unit moves it; and a small function copied into its callers' units at `-O2` makes an edit
  to it recompile those units too.

- Settled on the user's delegation 2026-11-11 (built on branch `manifest-avin-version`): **a package names the least Avin it builds with**
  ([the manifest](/spec/packages/#the-manifest), [the Avin a package
  needs](/spec/packages/#the-avin-a-package-needs)). `avin = "0.0.18"` in the `package` block, a
  version, met by that Avin and every later one, compared number by number (0.0.9 before 0.0.18).
  A minimum only: no ranges, no upper bound, no pre-release suffixes. Every package of the
  resolved graph is held to it, the root and each dependency at any depth, fetched or by path, by
  every command that reads manifests (build, run, test, check, `add`, `tidy`, `vendor`); a package
  needing a later Avin is refused at its `avin` value, naming the package, its requirement and the
  running version and saying to upgrade, before anything else wrong in that manifest. A malformed
  value is an error at the value. `avin new` writes the running Avin's version. `avin lsp` reports
  it on `avin.hocon` when the editor opens one, and on the manifest of the package a file being
  edited belongs to. *Why*: `cairo` needed 0.0.18 and could only say so in its README, and a
  package that needs a feature should fail with the upgrade named rather than with an unknown key
  or a type error. The key's spelling follows `sysl.hocon`, which names the Sysl a package needs
  as `sysl = "0.9.20"` in its `package` block. A minimum, because Avin's versions only add: a
  later Avin that breaks a package is a bug or a deliberate break in a release, which a range
  written in advance cannot foresee, and an upper bound would refuse every upgrade until each
  package re-releases. Checked when each manifest is read, so every path that reads one is held to
  it without a second walk of the graph; told first, because a manifest written for a later Avin
  may hold a key this one does not know. *Alternatives*: a range (`>=0.0.18, <0.1`), refused for
  the reasons above; a top-level `avin` key or a `requires { avin = … }` block (Sysl puts its own in
  `package`); a warning rather than an error (the build would then fail later and less clearly);
  checking only the root package (a dependency's need is the common case, as with `cairo`). Review
  with the user whether `avin new` should write the running version (it ties a new package to the
  Avin that made it, which may be later than the package needs).

- Settled on the user's delegation 2026-11-11 (built on branch `instance-keys`): **An instance key is a content hash in two parts: a lowered
  part (identity, body fingerprint, interfaces read) and the world facts the whole-program steps
  decided for it, both made on every build after lowering** ([the instance
  key](/compiler/#incremental-builds)). The facts are recorded where they are decided, as each step
  runs: the slots of methods with type parameters of their own, each devirtualization decision at a
  call through a table (not the whole set of descriptors a receiver type admits), and each summary
  scalar replacement read, with the finished key of every function it inlined (never one that may
  recur, so the inlined keys form no cycle). WHY: a decision is exactly what a step's rewriting of
  a function depends on, so a new implementing class changes the keys of only the calls whose
  decision it changes, and a callee's body edit reaches a caller only through a changed summary or
  an inlining; and the cache (item 6) can reuse a lowered form on the first part and rerun only the
  passes on the second. Alternatives: the callee's key for every summarized callee, as the pipeline
  first wrote it (every leaf edit would reach every transitive caller); the admitted descriptors
  rather than the decision (a class added past the guard limit would change keys for no change).
- Settled on the user's delegation 2026-11-11 (built on branch `instance-keys`): **The body fingerprint is per function, with its family's
  captures, not per family.** A local function or lambda and its top-level function share one table
  of locals; each fingerprint holds its own tree, the locals it owns or captures, and what every
  function of the family captures (with each captured local's type and mutability), which decides
  what is kept in a box. WHY: a lambda's body edit then changes the lambda and its closure entry and
  not the function around it, unless its captures change. Alternatives: one fingerprint for the
  whole family (every edit in it changes every member; simpler, and what the first build did).
- Settled on the user's delegation 2026-11-11 (built on branch `instance-keys`): **Places in the fingerprint follow trap-site naming**: a node
  inside its declaration counts from the declaration's start; a node outside it (a constant default
  argument copied into a call keeps its place in its own file) is named as `trap_name` would name
  a trap there; a derived impl's shared template counts from the start of its file; the compiler's
  own code (an empty span) is named by the instance. A constant's place is left out: no trap is
  named by it. WHY: a trap site's name is the only place a function's text holds, so this is the
  least that keeps the key sound; lines added between declarations change no key. A comment inside
  a function changes its key whether or not a trap site follows it. Alternatives: only the places of
  nodes that can trap (needs lowering's knowledge of which do, duplicated); no places at all
  (unsound: an edit moving a trap site changes the text).
- Settled on the user's delegation 2026-11-11 (built on branch `instance-keys`): **What a key reads of types is conservative where the text
  rarely depends on it**: every type argument's supertypes and method table (they choose the method
  a call on a bound runs), and a class's or data type's whole layout for any read of a field (its
  field types in order, never the names). So a new impl for a type changes the instances at that
  type that never call through a bound, and a field appended to a class changes the readers of its
  earlier fields; both cost a rebuild, never a stale instance. Review with the user if the cache's
  hit rate after edits like these matters: narrowing needs the bound calls and the fields read
  recorded during lowering, as the other facts are. Alternatives: record during lowering exactly
  which table entries and fields each instance used (precise, more state threaded through
  lowering).
- Settled on the user's delegation 2026-11-11 (built on branch `instance-keys`): **The key names the compiler by its version and its
  executable's path and modification time, not the level.** WHY: lowering does not depend on the
  level, and the pipeline keeps one instance directory per level. Alternatives: the level in the key
  as well (redundant with the directory), the executable's hash (reads 8 MB per build).
- Settled on the user's delegation 2026-11-11 (built on branch `instance-cache`): **A lowered entry is the instance's requests and its
  code by name, replayed; not a record of lowering's state** ([the instance
  cache](/compiler/#incremental-builds)). Every request lowering an instance makes of the whole
  program goes through one function of lowering (seventeen of them: a layout, a record, an object's
  record, a box, a value box, a type tested, a string, a trap site, an instance, a generated
  function, a slot, a slot of a method with type parameters of its own, a closure's shape, a constant
  closure, a constant object, a call through a table, a call of a function value), which records it
  while the instance is lowered for the cache; the outermost request alone is recorded. The code
  names what it counts by its stable name. WHY: replaying a request runs the same code a fresh
  lowering runs, so the instances, tables and records grow exactly as they would, in the same order,
  and an incomplete record fails to read back (an unknown name) rather than silently differing. A
  binding, a C callback and a C binding's body mark the instance as one no entry holds.
  Alternatives: the difference in lowering's state before and after each instance (state is keyed by
  the checker's indices, which an edit moves); a module's whole lowered program (a change anywhere
  in the module loses all of it).
- Settled on the user's delegation 2026-11-11 (built on branch `instance-cache`): **Only the program's functions with bodies get lowered
  entries.** Closure entries, bindings and the functions lowering makes (equality, hashing, rendering,
  adapters, C callbacks) are lowered every build; every function gets a text entry. WHY: they are
  small and many of them depend on the whole set of types; on marshal they are about 17,000 of
  41,583 instances and a fraction of lowering's time. Alternatives: entries for every kind
  (each kind's requests would need recording; generated functions read tables the worklist is still
  filling).
- Settled on the user's delegation 2026-11-11 (built on branch `instance-cache`): **A text entry is keyed by the finished key and the
  facts emission reads of the whole program**: for each call, the callee's symbol and signature,
  whether it may collect and whether the call may close a cycle of the call graph, and a shim's
  extensions. WHY: these decide the frame, the polls and the calling convention of a function's text
  and are not in its finished key, which covers its code. Alternatives: emission facts in the
  finished key itself (the keys would depend on emission's call graph); a hash of the finished code
  (costs an encoding of every function each build).
- Settled on the user's delegation 2026-11-11 (built on branch `instance-cache`): **The passes still run over every instance on every
  build**; devirtualization's fixed point runs over per-function summaries (the set of objects a
  receiver or a result may be) and rewrites only functions that call through a table, and the
  dispatch sites are found once per position and class or trait. Both give byte-identical text.
  WHY: measured on marshal, the passes are not where lowering an unchanged instance costs most, and
  running them only where facts changed needs every function's post-pass code, scalar summary and
  fence fact cached under a key over its callees' (a key over a cone of the call graph, with
  dispatch targets as edges); that is a design of its own. Review with the user: it is what stands
  between the cache and a lowering phase proportional to an edit. Alternatives: build the cone keys
  now.
- Settled on the user's delegation 2026-11-11 (built on branch `instance-cache`): **The cache is opt-in in this step**: `AVIN_INCREMENTAL`
  (`on`, `verify`, `off`) for `avin build` and `avin emit-llvm`, in
  `.avin/build/incremental/<kind><level>/`. WHY: item 7 of the plan makes `build`, `run`, `test` and
  the language server use it by default once the corpus has run warm. Alternatives: on by default now.
- Settled on the user's delegation 2026-11-11 (built on branch `instance-cache`): **The store is a file per module named by a hash of
  its module and keys, with a length and CRC-32 header, a manifest written last, and no lock**; a
  file is kept only if the build read it whole, and files neither named nor used in the last hour are
  trimmed, with no size limit. WHY: a name from the keys costs nothing to compute (a content hash
  would hash about 190 MB per build on marshal), the CRC catches truncation and damage, and two
  builds write the same bytes for the same keys. Alternatives: an entry per file (41,000 files per
  build); a SHA-256 per file; a size limit as the unit cache has (the unit cache's hour rule already
  keeps it to what recent builds used).
- Settled on the user's delegation 2026-11-11 (built on branch `instance-cache`): **`verify` compares the whole text and names the first
  function that differs**, in the clean build's order, or the first line where no function does.
  WHY: the function's symbol says which key missed a dependency; its key adds nothing a reader can
  act on. Alternatives: compare instance by instance before emission (needs the clean build's
  instances matched to the cached build's).

- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-string`): **String
  character-wise and parsing methods.** Choices the spec did not settle, in [`types.md`](/spec/types/#string):
  - **`toBool` is exact.** Only `"true"` and `"false"`; `toBool` traps with the number-parse
    message (`For input string: "…"`) on anything else, `toBoolOption` answers `None`. *Why:* a
    parse that accepts `"True"`, `"1"` or `"yes"` hides data errors, and the exact form is the one
    every program can state. *Alternatives:* case-insensitive; also `"1"`/`"0"`.
  - **A `radix` outside 2 to 36 traps, in the `Option` forms too.** *Why:* a bad radix is the
    caller's bug, not bad input, so `None` would hide it. *Alternative:* `None` from the `Option`
    forms.
  - **Radix digits are ASCII `0`–`9` and `a`–`z`, either case**, with the optional `+`/`-` sign of
    the decimal forms, and no prefix (`"0x10".toInt(16)` is not a number). *Alternatives:* accept a
    `0x`/`0b` prefix where it matches the radix.
  - **`split(sep, limit)`: `limit` is the most pieces; the last holds the remaining text.** A
    `limit` of 0 or less means no limit, 1 returns the whole string as one piece. *Why:* the
    reading that needs no special negative case beyond "no limit". *Alternatives:* a limit of 0
    dropping trailing empty pieces; a negative limit trapping.
  - **`stripLineEnd` removes one terminator, `\n` or `\r\n`, never a lone `\r`**, agreeing with
    `lines()`, which does not end a line at a `\r` either. *Alternative:* also a lone `\r`.
  - **`compareIgnoreCase` compares the two after `toLowerCase`**, as `equalsIgnoreCase` equates
    them, so its sign is `compare` of the lower-cased strings. *Alternative:* comparing character by
    character after upper-casing.
  - **`charIterator()` is a lazy `Iterator[char]` reading the string by offset**, with `next()` on
    an exhausted one trapping like every iterator. `map`/`flatMap`/`filter` on a string answer a
    `string`, not an iterator, and `tail`/`init` trap on the empty string as `Seq`'s do.
  Review with the user: whether `toBool` should be this strict, and whether the `Option` forms
  should answer `None` rather than trap for a radix outside 2 to 36.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-ordering`): **`clamp(x, lo,
  hi)` is a free function, and `Vector` has no `Ord` yet.** A method
  `x.clamp(lo, hi)` on every `Ord` type needs `impl[T: Ord] T`, which the checker refuses (an impl
  adds to a type parameter only where it is bounded by `Integer` alone), so `clamp` is a free
  function beside `max` and `min`, which are free functions too; it traps where `hi < lo`. Why: it
  reads like `max`/`min` and needs no language change. Alternatives weighed: widening the blanket
  impl rule (a language change with a wide reach); a method on `Integer` only (leaves strings,
  chars and reals without). `impl[T: Ord] Ord for Vector[T]` is refused because `Vector` is an
  abstract covariant class, whose added methods are variance positions, and `compare(other:
  Vector[T])` takes `T` in; `ArraySeq` is a final class and has its `Ord`. `Seq[T]` gets no
  `compare` method. Review with the user: how `Vector` should be `Ord`. The recommendation is that
  a method a trait impl adds, and no subclass overrides, is called through the static type rather
  than the method table, which would make `compare` on a covariant abstract class sound; that is a
  language change, so it waits for the user.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-ordering`): **`Reverse`,
  `compareBy`, `reverseOrder` and `thenBy` are in `avin` as plain names.**
  `Reverse` is a `data` type with `Ord` reversed and equality as `T`'s; descending sorts are written
  with it rather than a `sortedDescending()`. Why: it composes inside tuple keys, which a
  descending method cannot. Alternatives weighed: `sortedDescending`/`sortByDescending` on `Seq`
  (do not combine with a second key).
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-seq`): **Seq search, slices
  and combinatorics**, [collections](/spec/collections/):
  - **`search` answers the first of equal elements.** `Found(i)` is the lowest index holding an
    element equal to the one searched for, found by a lower-bound halving, so the answer is
    deterministic. Why: a binary search that stops at the first hit returns whichever duplicate the
    midpoints reach, which no caller can rely on and a test cannot pin; the lower bound costs the same
    O(log n) comparisons and makes `Found(i)` and `InsertionPoint(i)` one index (`i` is where the
    element is or would go, the insertion point of an equal element being its first). Alternatives:
    any equal element, unspecified (cheaper by at most one comparison, not testable); the last one.
  - **`search(x, from, until)` holds its window to the sequence** (`until` to `0 .. n`, then `from` to
    `0 .. until`) rather than trapping, as counts are held (`take`, `drop`) and unlike `slice`, which
    traps. Why: the window is a bound on where to look, not an index range to read, and an empty or
    out-of-range one has the clear answer `InsertionPoint` of its edge. Alternative: trap as `slice`
    does. `searchBy(key, wanted)` is included (the plan marked it optional); it takes the key
    function first, as `sortBy`'s sibling `distinctBy` does, and `K: Ord`.
  - **`combinations` and `permutations` are distinct by `equals` and ordered by first appearance.**
    The classes of equal elements are found with a hash map on the elements (as `distinct` does), so
    `equals` and `hash` must agree, and the vectors come in dictionary order of the classes' first
    appearance. Why: the dictionary order is stepped with O(k) or O(n) work and no memory beyond the
    class counts, and needs no `Ord`. Alternatives: the order of a recursive generation, which differs
    for a multiset; positions rather than values (every permutation of equal elements, n! of them).
    `k < 0` or `k > n` gives an empty iterator, `k == 0` and the empty sequence's permutations one
    empty vector.
  - **`indexOfSlice` and `lastIndexOfSlice` find the empty slice where the edges allow**: at
    `max(from, 0)` while that is no further than the length, and at `min(end, n)` while `end >= 0`;
    `lastIndexOfSlice(slice)` is the form with `end` the length, written as an overload as
    `lastIndexOf` is. `patch` holds `from` to `0 .. n` and a `replaced` below 0 to 0 rather than
    trapping, and takes `other` as an `Iterable[W]` with `W >: T`, widening as `++` does.
    `reduceRightOption` accompanies `reduceRight` as `reduceOption` accompanies `reduce`.
    Alternative for all: trap on an out-of-range count, as `slice` does for indices.
  - **`inits()` and `tails()` copy the elements into a vector once and slice it per step**, so each
    step is O(log n) and the n + 1 vectors share structure; they are lazy iterators, not a vector of
    vectors. `transpose()` of rows of different lengths traps (the plan's decision), and answers an
    empty vector, not a vector of empty ones, for no rows; for rows that are all empty it answers
    empty too.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-math`): **the integer methods are an inherent impl over `Integer`, not
  members of the trait.** `abs`, `sign`, `pow`, `gcd`, `lcm`, `floorDiv`, `floorMod`, `bitCount`,
  `leadingZeros`, `trailingZeros`, `isPowerOfTwo` and `toRadix` are written once in
  `library/avin/integer.avin` as `impl[T: Integer] T`, which the core library may declare. Why: it
  gives every width the methods with no new trait and no change to the closed bound. The cost: a
  type parameter bounded by `Integer` sees only the trait's members, so generic code cannot write
  `x.abs()`. Alternatives weighed: putting the methods in `Integer` as required members (every
  width implements them, but "no type implements it" then has an exception, and the arithmetic
  traits would grow by a dozen); leaving them to free functions (`math.abs(x)`, unlike
  `x.compare(y)`). Review with the user: whether generic code should reach these methods, which
  means making them members of `Integer`.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-math`): **the bitwise and shift operators work on a type parameter
  bounded by `Integer`.** `x & y`, `|`, `^`, `<<` and `>>` on a `T: Integer` are the machine's
  operators at each instance's width, with both operands `T` (`x >> T(1)`); the checker had treated
  every operator on a type parameter as a call of a method of that name, so `Integer` code had no
  bit operations at all. Why: the bound is documented as having every integer operation, and the
  integer methods need them. Alternatives weighed: methods `bitAnd`, `shiftLeft` on `Integer` (a
  second spelling of what the operators already say); doing the bit work by division and remainder
  (slower, and unreadable). A `T` with another bound still has them only as methods.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-math`): **choices inside the integer methods.** `pow(n: int)` takes an
  `int` exponent at every width and traps on a negative one (a fraction is not an integer);
  `x.pow(0)` is `1` for every `x`, `0.pow(0)` included. `gcd` is never negative and traps only when
  its value is `MinValue`'s magnitude; `lcm` is `0` when either side is, otherwise positive. `sign`
  is an `int`, as are `bitCount`, `leadingZeros` and `trailingZeros`, which count at the width
  (`(-1).bitCount()` is `32`, zero has `width` leading and trailing zeros). `toRadix` writes lower
  case, a `-` and the magnitude, as `f"%x"` does, with no prefix, and takes `int` from 2 to 36 and
  traps outside it. Why: each is the usual meaning, the least surprising for a program moving
  between widths. Alternatives weighed: a `long` exponent (never needed; an `int` is enough for
  squaring by halves); a `gcd` that keeps a negative sign (an unsigned-looking result is what callers
  want).
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-math`): **`avin.math`
  additions.** `round(x)` returns a `long`, a tie
  going away from zero, and converts as `long(x)` does for NaN and out-of-range values; `trunc(x)`
  returns a `real`, as `floor` and `ceil` do, keeping a negative zero's sign; `ulp` at the greatest
  finite value is the gap below it (a finite answer rather than infinity); `nextUp` and `nextDown`
  leave NaN and the infinity they move towards alone. Why: `round` is the one the program usually
  wants as an integer and `rint` is already the `real` form; the others follow the platform's
  functions. Alternatives weighed: `round` returning `real` (no saturation story to tell, but the
  common use then needs `long(round(x))`); `trunc` returning `long` (loses negative zero and the
  large values). Review with the user: the `round` and `trunc` result types differ, which is
  unusual to read.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-iterator`): **The eager methods added to every `Iterable` answer
  vectors.** `scanLeft` is a `Vector[B]`, `tapEach` and `concat` a `Vector[T]` (`Vector[W]` for
  `concat[W >: T]`), `span` and `partitionMap` pairs of vectors, and `flatten()` of a collection of
  collections a `Vector[T]`; `grouped` and `sliding` answer the `Iterator[Vector[T]]` a sequence's
  do, by delegating to the iterator's forms. WHY: one predictable kind, the one `partition`, `span`
  and `scanLeft` on a sequence already answer, with no higher-kinded types to name the receiver's
  own. Alternatives: `Iterable[B]` as the `Iterable` transformations answer (hides that it is a
  vector), or each collection keeping a same-kind form (a long tail of overrides). Review with the
  user: whether `tapEach` on a list should answer a list.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-iterator`): **An iterator's new lazy forms are added by
  `impl[T] Iterator[T]`, not declared in the trait.** So `padTo(total, element: T)` and
  `zipAll(other, thisElement: T, thatElement)` take the element type in a parameter of the
  covariant `Iterator`, and the forms are found before the eager ones every `Iterable` has. An
  iterator whose static type is only `Iterable` gets the eager forms, as with `map`. Alternatives:
  trait members with lower-bounded parameters (`padTo[W >: A]`), which a program's own iterator
  could override, at the cost of a widened result everywhere.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-iterator`): **`it ++ other` takes `other` as an ordinary
  `Iterable` and asks for its `iterator()` only once `it` is used up**; `concat` on an iterator is
  the same lazy join. WHY: a strict argument reads like every other call, and the iterator taken late
  is what keeps a mutable `other` read as it is when reached. Alternative: a by-name `other`, which
  would delay even evaluating the expression.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-iterator`): **`Iterator.from` is two overloads over
  `T: Integer`, `from(start)` and `from(start, step)`,** not one with `step = 1`, since the default
  would have to be `T(1)` of the caller's type. An element is computed when it is read, so
  `from(int.MaxValue)` gives `int.MaxValue` and overflow traps only when the element past it is read.
  Alternatives: a default parameter written `T(1)`; `int` and `long` overloads only.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-iterator`): **The companion iterators compute when read.**
  `fill` and `continually` evaluate their by-name element at each `next()`, `tabulate` calls `f` at
  each `next()`, `iterate` calls `f` before each element after the first, and `unfold` calls `f`
  when `hasNext()` is asked and holds the element it gave. WHY: an iterator does no work for an
  element that is not read. Alternative: `fill` and `tabulate` building a vector and iterating it,
  as the sequence companions do (eager, and `fill`'s element evaluated before any is read).
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-iterator`): **`grouped` and `sliding` on an iterator give the
  windows a sequence's give**, reading the next window when `hasNext()` is asked, holding no element
  beyond the current window, and trapping on a size or step below 1 at the call, with the messages a
  sequence's use. `BufferedIterator[A]` is a public class with `peek()` (trapping with "peek on an
  exhausted iterator") and `peekOption()`; `buffered()` on one wraps it again. `flatten()` is also
  on an iterator of collections, lazily, beside the eager one on every collection of collections.
  Alternatives: windows read at `next()` only (no lookahead, but `hasNext()` could not answer for a
  step that skips past the end); `buffered()` answering a buffered iterator itself.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-predef`): **an impl without `for` adds methods to tuple and function
  types** built from its parameters (`impl[A, B] (A, B)`, `impl[A, B] (A) -> B`), as the impl chapter
  already allowed for any type built from parameters and for traits, which function types are. The
  checker refused both ("an impl adds to a data type, a class, an object or a built-in type"; "an
  impl for every tuple of an arity implements a trait"); that was a bug against the design and is
  fixed. The choices the chapter did not settle: **a function type's methods reach the types that
  are subtypes of it** (a data type or class naming it, sequences, sets, maps, partial functions), as
  a trait's added methods reach its implementers, and **the type's own method of the same parameter
  types is found first**, as for a trait; a tuple pattern stays exact, as other non-trait patterns
  do. **A `@sendable` function type in the pattern matches only `@sendable` functions.** Such an impl
  outside the core library is an extension, since the tuple and function types are the core
  library's. Rationale: these follow the existing rules for traits and for patterns, with no new
  rule. Alternatives weighed: matching only receivers whose static type is a function type (a
  `Vector` would then lack `andThen`, though it is passed wherever an `(int) -> T` is); treating a
  `@sendable` pattern as plain (its methods would reach functions that may not run on another
  thread, against what the impl names).
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-predef`): **an impl naming a tuple or function type has at least one type
  parameter.** `impl (int, int)` with none begins a function named `impl`, since `impl` is a
  keyword only before a type and `impl(…)` declares a function; `impl() -> int` followed by a body
  reads either way, so no rule on the tokens can tell them apart. A part may be written out
  (`impl[A] (A, int)`). Alternatives weighed: allowing `impl[]` (an empty list says nothing and the
  parser refuses it for every impl); making `impl` a full keyword (breaks a function of that name).
  Review with the user: whether an impl for one concrete tuple or function type is wanted enough to
  reserve `impl (`.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-predef`): **`require(condition, message)` takes the message by name**, so
  it is built only when the requirement fails, while `assert(condition, message)` takes a plain
  string. Rationale: `require` sits at the top of ordinary functions, called on every call, and its
  message usually interpolates the arguments; building it every time would cost the common path.
  Alternatives weighed: a plain string, as `assert` (cost on every call); changing `assert` to by-name
  as well (out of this item's scope; it would be a separate change to a settled API). `todo(message)`
  takes a plain string, since it always traps. **`ensuring` is not provided**: it needs an impl for
  every type (`impl[T] T`), which only the integer family has; a postcondition is written with
  `require`/`assert` on the result.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-inplace`): **in-place operations on `Buffer`, `Deque` and `Array`, and
  `StringWriter` length and clear.** Choices the spec did not settle:
  (1) **`remove(i, n)` is an overload of `remove(i)`**, and its count parameter is named `n`, so it
  never shadows the buffer's own `count`. A range not within the collection traps (`n` greater than
  `length() - i` included, tested without overflow); `n = 0` at `length()` is allowed.
  (2) **`subtractOne(x)` answers a `bool`** (whether one was taken out) rather than the collection,
  and is declared in an `impl[T: Eq]` block, so it exists only where elements compare; it takes the
  first match, from the front. Alternatives: returning `self` for chaining (hides a miss), and a
  `-=` operator (a bool-valued compound assignment reads badly).
  (3) **`filterInPlace` and `mapInPlace` trap when the predicate or function changed the length**,
  instead of leaving the outcome unspecified or silently working on a snapshot: the check is one
  comparison per element and turns a corrupted result into a trap at the caller's line. Alternatives:
  iterate a snapshot (hides the bug and costs a copy), or leave it undefined.
  (4) **`mapInPlace` takes `(T) -> T`**, since the element type cannot change in place.
  (5) **`patchInPlace(from, other, replaced)` reads `other` first** and traps for a `from` or
  `replaced` outside the collection, where a clamping rule would hide a wrong index.
  (6) **`fillInPlace` is a separate name from the companion's `Array.fill`**, as `fill` there makes a
  new array; it shares the one value as `fill` does.
  (7) **`Array.copyTo(dest, at, from = 0, count = length() - from)`** gives `count` a default that is
  a function of `from`, which the declaration rules allow; it is overlap-safe in both directions
  (the copy runs backwards when `dest` is the same array and `at > from`). All ranges are checked
  before the first element moves.
  (8) **`StringWriter.length()` is an `int` count of bytes**, as every offset into a string is, and
  `clear()` drops the stored text so a text already taken is unaffected. A **`write(x: Display)`
  overload on `Writer` is not added**: it would sit beside `write(text: string)` (a `string` is
  `Display`) and make every call a resolution by specificity for no gain, `writeDisplayed` already
  writing any `Display` value. Revisit if a user wants `out.write(42)`. Review with the user: the
  length-change trap in (3), and leaving out `write(x: Display)` in (8).
- Settled on the user's delegation 2026-11-11 (built on branch `neg-nan-sign`): **Unary minus on a
  `real` flips the sign bit of every value, NaN included** (the machine's negation, not `-0.0 - x`,
  which leaves a NaN's sign as it was), and **`math.signBit(x: real) -> bool`** answers whether the
  sign bit is set, so the sign of a NaN or a zero can be observed. WHY: negation is the sign flip
  for every value in IEEE 754, and without `signBit` a program could tell a NaN's sign by no means
  (`hash` makes every NaN one value and both print as `NaN`). Alternatives: reading the sign through
  `copySign(1.0, x)` only (works, but reads as a trick).
- Settled on the user's delegation 2026-11-11 (built on branch `union-mir-words`): **Values of more than one machine word in the intermediate
  code** (the first part of the union tag-and-payload layout, under Settled → unions →
  Representation).
  - **Each word is a value of its own** (scalar replacement): as parameters a multi-word value is one
    parameter per word, in a local one slot per word, in an object or a closure one field per word,
    and a merge at a join is one slot per word. Only a result needs new forms: a signature's `rest`
    lists the types of the words after the first; a call defines the first word as its value and each
    later one with `CallWord`, a return gives the first as its value and each later one with
    `ReturnWord`; LLVM returns the words as one structure, in registers. *Why*: every word is an
    ordinary typed value, so the frame plan roots exactly the words of reference type, wherever each
    is live across a collection point, and never a scalar word, with no change to how roots are
    found. *Alternatives*: an LLVM structure value carried whole (the root plan would have to look
    inside it); returning the later words through slots of the caller passed by address (a slot whose
    address is passed is no root the plan follows); returning them through the thread's context (a
    store and a load per word, and a window where a reference is in no frame).
  - **Proven on tuples taken apart from calls**: `val (a, b) = f(x)`, where `f` is one of the
    program's functions with a body (not a constructor, a trait's method or a binding) and the tuple
    has at least two parts with runtime values, calls a copy of `f`, `f#words`, that answers with the
    tuple's parts, one word each; every return reads the parts out of the tuple, which is then made
    and read in one function and kept as its parts, and a call in tail position of a function yielding
    a tuple of the same words passes its words straight on. So a recursive or large function answering
    with a tuple no longer allocates it for a caller that takes it apart; a caller that keeps the tuple
    calls `f` itself, which still makes it. *Why*: a real path the existing programs run, with
    references beside scalars in every combination, and with no change to any type's representation
    or to what a program sees. *Alternatives*: a compiler-internal pair type no program could reach
    (exercised only by tests); every function yielding a tuple answering with words and callers that
    keep it making it again (a made-again tuple's type record could differ from the one the function
    made, which a type pattern on an `Any` would see).
  - **A small function answering with words is inlined where it is called**, each later word through a
    slot of its own, so `val (q, r) = divmod(a, b)` still makes no call and no tuple. Guarded calls
    through a method table meet in one value, so devirtualization leaves a call of several words
    through the table rather than guarding it. A call in tail position whose every word the block
    returns at its own place is a tail call, its whole result returned, so functions answering with
    words call each other in constant stack as functions yielding tuples do. *Alternatives*: never
    inlining such functions (a call where there was none at `-O0`); no tail calls of several words
    (deep mutual recursion through `val (a, b) = f(x)` would overflow the stack).

- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-priority-queue`): **`PriorityQueue` gives the greatest element first** (branch `stdlib-priority-queue`),
  [collections](/spec/collections/#priority-queues). `PriorityQueue[T]()` orders by `T`'s `Ord`, and
  `PriorityQueue.withOrdering(compare)` by a comparison; the element `compare` finds greatest comes
  out first. Why: it matches `max`, which every `Ord` type already answers, and `dequeue` then takes
  the same element `max()` would; a least-first queue is one `Reverse` or `reverseOrder` away either
  way. Alternative: least first (the order of a sorted sequence read from the front, the usual choice
  for shortest-path searches); it makes `PriorityQueue[int]()` the odd one out against `max`.
  Review with the user: it is the default every caller will meet.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-priority-queue`): **A queue's `iterator()` and `toVector()` are in heap order, and `toSortedVector()` is the sorted
  form** (branch `stdlib-priority-queue`). Heap order starts with the greatest element and is
  otherwise unspecified; the iterator is a snapshot taken when it is made, so enqueuing or dequeuing
  while iterating neither traps nor changes what it gives. Why: heap order costs nothing, a sorted
  iteration costs O(n log n) and a copy, and `Iterable` conformance wants the cheap one; a snapshot
  makes mutation during iteration defined. Alternatives: iterating in dequeue order; an iterator
  over the live heap (cheaper, but what it gives after a change is arbitrary).
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-priority-queue`): **Equal elements come out in no promised order** (branch `stdlib-priority-queue`): a binary heap
  is not stable. Alternative: a sequence number breaking ties first-in-first-out, at the cost of a
  word per element and a wider comparison; a caller wanting it orders by `thenBy` on its own counter.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-ordered-map`): **Insertion-ordered maps and sets: names, update order, equality** (branch `stdlib-ordered-map`).
  The concrete types are `OrderedMap[K, +V]` and `OrderedSet[A]`, conforming to the traits `SeqMap[K, +V]`
  and `SeqSet[A]`. *Why:* the concrete names say what the collection does, as `HashMap` says how it
  finds and `TreeMap` how it sorts, and no implementation (a vector of keys) leaks into them; the
  trait names follow the sequence-like "in the order added" meaning of `Seq`. *Alternatives:*
  `VectorMap`/`VectorSet` (names the layout, not the behaviour), `InsertionOrderedMap`,
  `LinkedHashMap`-style names, and `OrderedMap` as the trait with the concrete type named for its
  layout. *Update order:* updating a key already present keeps its place and replaces its value;
  removing and re-adding moves it to the end. *Why:* an update is not a new arrival, so a map used as
  a record keeps its field order. *Alternative:* an update moves the key to the end. *Equality:* two
  maps are equal when they hold the same keys bound to equal values, and two sets when they hold the
  same elements, whatever the order, and equal ones hash alike, so an `OrderedMap` equals a
  `HashMap` of the same entries as every kind of `Map` does. *Why:* the existing rule makes map
  equality independent of the kind, and an order-sensitive `==` on one kind would break transitivity
  through the traits. *Alternative:* an order-sensitive `==` between two ordered maps (reached by
  comparing `iterator()`s). *Layout:* a hash trie from key to slot and a vector of slots, removal
  leaving a gap, compacted when gaps exceed both eight and the entries. `map`, `collect` and
  `flatMap` through the trait answer hash-based collections, as for `TreeMap` through `Map`. A
  `{k: v}` or `{a, b}` literal builds one where `SeqMap`, `OrderedMap`, `SeqSet` or `OrderedSet` is
  expected. Review with the user: the names.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-ordered-map`): **A call through a method table or a bound reaches only methods of types that conform** (branch
  `stdlib-ordered-map`; a fix, recorded because it changes what the polymorphic-recursion check
  reports). The type arguments of a method's own type parameters flowed into every method of the same
  name and arity of any type, so a generic method that called a sequence's `updated` with a type built
  from its own parameter was reported as recursing into itself. They now flow only into methods of
  types that have the called method's class or trait among their ancestors. *Alternative:* keep the
  name-only approximation and rename the method (rejected: `updated` is the name every map has).
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-linked-map`): **`avin.collections.mutable.LinkedMap[K, V]` and
  `LinkedSet[A]`, the insertion-ordered mutable map and set.** The choices:
  1. *Re-binding a present key keeps its place*; `putLast(k, v)` and `moveToEnd(k)` (and a set's
     `addLast` and `moveToEnd`) make it the newest, and a key removed and bound again is new. Why:
     `m(k) = v` is a plain update everywhere else, and an insertion-ordered map whose update reorders
     it would make `m(k) += 1` shuffle the iteration; an LRU cache that wants the move says so
     with `moveToEnd`. Alternatives: every put moves to the end (access order for caches, but
     surprising for a plain map); a constructor flag selecting access order (one type with two
     behaviours).
  2. *Equality and hash ignore order*: two linked maps (or sets) are equal when they hold the same
     entries, as two mutable maps are; a `LinkedMap` is not comparable with a `Map` of the other
     kind (they are different classes). Why: `==` on maps is about contents, and the order is
     observable through iteration anyway. Alternative: order-sensitive equality, which makes
     `{a: 1, b: 2}` differ from `{b: 2, a: 1}`, unlike every other map.
  3. *The oldest and newest ends*: `headOption()` and `lastOption()` answer the ends in constant
     time (overriding `Iterable`'s, so `head()` and `last()` trap when empty), and `removeFirst()`
     and `removeLast()` answer the removed `(key, value)` (a set's, the element) and trap when empty.
     Why: `removeFirst` is the eviction step of an LRU cache. Alternatives: `removeFirst` answering
     an `Option` (an empty map is then silently ignored by a cache that expected an entry);
     `pollFirst`-style names.
  4. *Mutation during iteration*: replacing values and removing entries are defined (a removed
     entry the iterator has not reached is skipped); binding new keys never traps but whether the
     iterator visits them is not defined. Why: the entries sit in a buffer, a removal leaves a gap
     the iterator skips, and the buffer is squeezed only by an insertion, which hands the map a new
     buffer and leaves an iterator in progress stepping through the old one. Alternative: trapping on
     a structural change (a version counter), which the other mutable maps do not do either.
  5. *Representation*: a `Map[K, int]` from each key to its position, and a `Buffer` of entries in
     insertion order with gaps for removed ones, squeezed when gaps are half of at least 32
     positions; every operation is amortized constant time. Alternative: open addressing with
     order links threaded through the table, one structure instead of two but a second copy of the
     table's removal logic; it can replace this without changing the API.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-queue`): **`Queue[+T]` (immutable, two lists), built by the stdlib-queue branch.**
  - *Choice:* `Queue` is an abstract covariant class with a private two-list implementation, so its
    constructor cannot be called with a state that breaks the invariant (the front is empty only
    when the back is). It has `Queue(xs*)`, `Queue.empty[T]()` and `Queue.newBuilder`, but no
    `tabulate` or `fill`, no `++`, and no literal form. `dequeue()` answers `(T, Queue[T])`;
    `front()` is the name of the head read; `apply(i)` is O(i) and `map`/`filter` answer queues.
  - *Why:* the abstract class keeps the invariant that gives constant-time `front()` and amortized
    constant `dequeue`; a pair return keeps the type immutable; `Seq` conformance gives equality,
    hashing and the sequence methods for free, so a queue equals a vector of its elements.
  - *Alternatives weighed:* a `data` type with public `Cons`-like variants (exposes the lists and the
    invariant); a banker's queue with lazy rebalancing (worst-case bounds, but heavier and no
    better for the usual single-threaded use); `dequeue` answering `Option` only (loses the trap
    that every other `removeHead` has); `head`/`tail` names only (they remain, from `Seq`, with `tail`
    answering a `Vector`).

- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-bitset`): **`BitSet` (immutable and mutable): representation and negatives** (branch `stdlib-bitset`).
  Proposed: the immutable `BitSet` is a class over a `Vector[long]` of words, kept without empty words
  at the end, so an update copies one path and `toBitMask()` is the words; the mutable one is over a
  `Buffer[long]`. A negative element is not in any bit set: `contains` and `excl`/`remove` treat it as
  absent (`false`, set unchanged), and everything that would add one traps (`incl`, `++`, `apply`,
  the builder, `add`, `+=`, `++=`, `addAll`); the ordered queries and slices take any `int`. Why: a
  query about a number that cannot be present has the honest answer "no", which keeps generic
  code over `Set[int]` from trapping on `s(x)`, while storing a number the set cannot hold must not
  silently succeed. Alternatives weighed: trapping in `contains`/`remove` too (strict, but breaks
  `Set[int]` code that probes with arbitrary ints); ignoring negatives in `incl` (silently drops an
  element, so `s.incl(x).contains(x)` can be false); offsetting by a minimum (a different set type).
  Review with the user if bit sets of negatives (a signed range) are wanted.

- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-bitset`): **`BitSet` method names beyond the `Set` ones** (branch `stdlib-bitset`). Proposed: the immutable
  set adds `symmetricDiff` and `toBitMask`, with `BitSet.fromBitMask`; the mutable one adds
  `unionInPlace`, `intersectInPlace`, `diffInPlace`, `symmetricDiffInPlace` (taking a mutable bit
  set), the non-mutating `union`, `intersect`, `diff` and `symmetricDiff` over mutable bit sets,
  `copy()` and `toSet()` (the immutable one), beside the mutable `Set`'s `add`, `remove`, `addAll`,
  `+=`, `-=`, `++=`, `--=` and `filterInPlace`. Why: the `…InPlace` suffix names the in-place form of a
  method the set already has (as `filterInPlace` and `sortInPlace` do) and spells out what the symbol
  forms `|=` and `&=` would, which are free method names but carry no word for a reader. Alternatives:
  `|=`, `&=`, `-=` as the in-place algebra (`-=` is taken by removing an element); `unionWith` and so on
  (suggests a combining function); `addAll(other)` for the union (already means adding elements of
  any `Iterable`). `symmetricDiff` is `xor` in some libraries; the longer name reads without
  knowing bit-twiddling. The mutable bit set's `union`/`intersect`/`diff` take a mutable bit set only
  rather than any `Set[int]`, the mutable `Set` not being a `Set` trait member either.

- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-bitset`): **A mutable `BitSet` iterates over a snapshot, and its ordered queries copy it** (branch
  `stdlib-bitset`). Proposed: `iterator()`, `headOption()`, `lastOption()`, `minAfter` and
  `maxBefore` of a mutable bit set read an immutable copy of its words, so changing the set during a
  `for` is safe and visits the elements it had when the loop began. Why: it makes the iterator the
  same code as the immutable one, and a mutable hash set's iterator has no defined behaviour under
  change at all, so a stable one is the safer promise. Cost: one pass over the words per call, so a
  loop calling `minAfter` repeatedly on a large sparse set is quadratic in the words. Alternative: a
  live iterator that reads the words as it goes (no copy, but sees its own changes); revisit if
  `minAfter` is used as a scan.
- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-mutable-tree`): **A mutable tree map or set holds an immutable one and
  replaces it; iterating it while updating it is legal and sees the collection as it was.**
  `avin.collections.mutable.TreeMap` and `TreeSet` keep a private immutable `TreeMap` or `TreeSet`
  and assign a new one at each update. Why: the persistent tree is already balanced and tested, its
  `SortedTree` is private to module `avin` (so a second module could not share it without widening
  it), every ordered query comes for free, and `toTreeMap()`/`toTreeSet()` become O(1) snapshots.
  Consequence: an iterator made before an update visits the entries as they were, so a loop that
  removes or adds entries cannot skip, repeat or fault, unlike the hash-based mutable collections,
  whose iteration while updating is unspecified. Alternatives weighed: a mutable balanced tree with
  in-place rotations (faster constants, no allocation per update, but a second tree implementation,
  and iterators would need invalidation rules); a skip list (same objection). Updates allocate
  O(log n) nodes, which a mutable tree would not; revisit if a profile shows it. Review with the user:
  the snapshot-iteration guarantee is a promise a later in-place tree would have to keep.

- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-mutable-tree`): **Two types are named `TreeMap` (and `TreeSet`), one in
  `avin` and one in `avin.collections.mutable`.** As `Map` and `Set` already are. Inside the mutable
  module the bare name is the mutable one and the immutable one is imported under an alias; a program
  importing the mutable one unaliased while also using the prelude's gets the mutable one for the bare
  name, and reaches the immutable one through an aliased import (both forms are tested). Alternatives:
  distinct names (`MutableTreeMap`) which would break the parallel with `Map`/`Set`.

- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-mutable-tree`): **`removeFirst()`/`removeLast()` trap on an empty
  collection; `removeFirstOption()`/`removeLastOption()` answer `None`.** The immutable sorted
  collections pair `head()` (trapping) with `headOption()`; the removal operations follow the same
  split, the plain name being the trapping form. Alternatives: answering an `Option` from the plain
  name (as `remove(key)` does), or a `pollFirst` name.

- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-mutable-tree`): **The `range*` forms of a mutable tree collection answer a
  new mutable collection, not a view.** As `filter` and `copy()` do on the hash-based mutable
  collections; a write to the slice does not reach the original. Sharing the tree makes the slice O(log
  n) to make. Alternative: live views, which would need the mutable tree to be in place.

- Settled on the user's delegation 2026-11-11 (built on branch `stdlib-mutable-tree`): **A mutable tree set's `union`, `intersect`, `diff` and
  `subsetOf` take a mutable tree set**, as the mutable hash set's take a mutable hash set; the
  `keySet()` of a mutable tree map is an immutable `TreeSet` snapshot, as a mutable `Map`'s is an
  immutable `Set`.

**Deferred**

- Higher-kinded, path-dependent and intersection types; inferred unions; implicit parameters;
  macros.

## Proposed

- Proposed (branch `tuple-result-override`): **an override's or an implementation's result may be
  any type seen as the declared result with no conversion**
  ([declarations](/spec/declarations/)), the covariant position [generics](/spec/generics/#variance)
  already names: `-> (Dog, Dog)` overrides `-> (Animal, Animal)`, `-> (HashMap[K, V], HashMap[K, V])`
  implements `-> (Map[K, V], Map[K, V])`, `-> Option[Dog]` overrides `-> Option[Animal]`,
  `-> () -> Dog` overrides `-> () -> Animal`, and `-> string` overrides `-> Any`. A result that
  would need a box (`-> int` for `-> Any`, `-> (int, int)` for `-> (Any, int)`) or that differs at
  an invariant argument (`Array[Dog]` for `Array[Animal]`) is still refused. Rationale: the chapter
  said "for a class, a subclass", which refused exactly the shapes the variance rules call
  subtypes with one representation, so a value returned through the base needs no conversion;
  the checker now asks the same no-conversion question an element of a covariant collection does.
  Alternatives weighed: keeping results to classes only (tuples and covariant instances then need
  a differently named method, as own-kind `partition` met); also boxing a scalar result, which
  would make an override's call site convert behind a virtual call.
- Proposed (built on branch `stdlib-lazy-list`): **`LazyList[+T]` is a class whose cell is a
  private `lazy val`**, and the expression that computes the cell is dropped once it has run. The
  cell is `Option[(T, LazyList[T])]`; the class holds a function that reads the real expression out
  of a captured variable and empties it, so an evaluated cell keeps nothing the expression captured.
  WHY: a covariant class may hold no `var` of its element type, and a `lazy val` is the one immutable
  field that is computed once and kept, with its trap on a read during its own evaluation. Keeping
  the expression would hold everything it captured for as long as the cell lives (a `filter` cell
  would hold every source cell it skipped). ALTERNATIVES: an invariant node class with `var`s (the
  list could then not be covariant); a `var` of type `Any` cast back on each read (no type checking
  of the cell); keeping the expression (simpler, a space leak).
- Proposed (built on branch `stdlib-lazy-list`): **`LazyList.cons(head, tail)` takes the head
  strictly and the tail by name, and the tail is evaluated when the list's second cell is first
  read**, not when the first is: `xs.head()` never evaluates the tail expression. WHY: the first
  cell is then exactly the head, and a function building a list recursively runs one step per cell
  read. ALTERNATIVES: a by-name head too (a cell would then be one more function for a value usually
  at hand); evaluating the tail with the first cell (one list fewer per cell, but `head()` runs the
  next step).
- Proposed (built on branch `stdlib-lazy-list`): **There is no lazy cons operator; `x +: xs`
  prepends to a list already made.** A symbol method whose name ends in `:` takes its receiver on
  the right, and a receiver is evaluated where it is written, so the tail of `x op: tail` cannot be
  delayed; a left-grouping name would need a method on every element type, which only the core
  library may declare by an `impl` over all types, and would group `1 op 2 op xs` the wrong way.
  `LazyList.cons(x, tail)` is the delayed form. **Review with the user**: a language form (a
  by-name receiver for `:`-ending methods, or `::` building a lazy list where one is expected) would
  give `head #:: tail`; and `head :: tail` patterns on a lazy list need the pattern rules to reach
  beyond `List`: both are language changes, left to the user.
- Proposed (built on branch `stdlib-lazy-list`): **`LazyList` is an `Iterable`, not a `Seq`.** WHY:
  a `Seq` answers `length()`, `apply`, `lift`, `isEmpty`, `==` and its hash through `length()`, which
  never returns on an endless list, and every eager method a `Seq` adds would build a `Vector` from
  it; sequence patterns over it would call `length()` too. As an `Iterable` it keeps the forcing
  methods every collection has, its own lazy forms found first. ALTERNATIVES: a `Seq` overriding the
  methods that would not return (most of them), or a `Seq` whose `length()` simply does not return
  on an endless list.
- Proposed (built on branch `stdlib-lazy-list`): **A lazy list's `Display` and `Debug` show the
  evaluated prefix and `…`, evaluating nothing**: `[0, 1, 4]`, `[0, 1, …]`, `[…]`, with a field
  width padding the whole. WHY: displaying an endless list must return, and a display that
  evaluated cells would run a program's functions while printing. ALTERNATIVES: forcing every cell
  (does not return on an endless list); a fixed prefix forced (side effects while printing);
  `<not computed>` text (longer, and not a form the other collections use). No `Eq`, `Hash` or
  `Ord`, which could not answer for two equal endless lists.
- Proposed (built on branch `stdlib-lazy-list`): **A lazy list is not sendable**, so each belongs to
  the thread that made it; the cells' `lazy val` evaluation would be safe between threads, but the
  expressions not yet evaluated are functions that may capture anything. ALTERNATIVES: sendable when
  every expression is a `@sendable` function (every lazy method would need a sendable form).
- Proposed (built on branch `stdlib-lazy-list`): **`zip` takes another `LazyList`** and stays lazy;
  **`flatMap` takes `(T) -> Iterable[U]`**, reading each collection through its iterator as cells are
  read; **`++` takes its right operand by name**, evaluated only when the cell after the left's last
  is read (the iterator's `++` takes a strict `Iterable`, since an iterator is read once anyway);
  `toLazyList()` on every `Iterable` converts, since `from` is the integer counter's name as on
  `Iterator`. Also added beyond the item: `filterNot`, `zipWithIndex`, `fill`, `tabulate`, `lift`,
  `force()` and `isEvaluated()`.
- **Companion `from`, `Vector.iterate`, `unfold` and the sorted conversions** (branch
  `stdlib-from`). Proposed: `from(xs: Iterable[T])` on every immutable collection companion and
  on the mutable ones (`Buffer`, `Map`, `Set`, `TreeMap`, `TreeSet`, `LinkedMap`, `LinkedSet`,
  `BitSet`, `PriorityQueue`), a map's taking `Iterable[(K, V)]`; for a duplicate key the later
  pair's value wins (an ordered map keeping the key's first place), the rule `apply` and the
  builders already follow. `Vector.iterate(start, length, f)` takes the length as its second
  parameter and calls `f` `length - 1` times; `Vector.unfold` and `List.unfold` are the eager forms
  of `Iterator.unfold`. `toSortedSet()` and `toSortedMap()` are `impl` blocks on `Iterable` bounded
  by `Ord`, like `toSet()` and `toMap()`. Why: one spelling that works for every source and target,
  where the collection-specific conversions need a method per pair. Alternatives weighed: a
  `to(factory)` conversion (needs higher-kinded types); a universal `into(builder)` (not added:
  `builder.addAll(xs)` already reads as well, and `from` covers the target-first reading);
  `List.iterate`/`ArraySeq.iterate` (not added; `Vector.iterate(...).toList()` is one call);
  `Array.from` (left out as arrays are not in the item). `Iterator.from(start)` is the unrelated
  counting iterator and keeps its name.

## Open

- Member forwarding inside a class (`export field.method`, presenting a member's methods as the
  class's own), a later possibility beside module exports; and exporting a whole module as a member
  of another (`export a.b`), which needs modules as members.

- Tooling for exports: completion after `m.` and in an import group listing a module's exports
  beside its declarations, and the IntelliJ plugin's resolution following an export to the
  declaration (go-to-definition, find usages, renaming through an export).

- Value definitions between a comprehension's generators (`y = f(x)`), and what a comprehension
  over a `Map` yields when its values are not pairs (it needs `Iterable`'s own transformations).

- Packages, where Sysl's choice does not transfer ([packages](/spec/packages/)):
  - Whether a feature needs anything finer than a module: a compile-time `feature("x")` constant a
    function could test (folded away when false), or nothing, the module being the only gate.
  - Whether a module may be listed by more than one feature (on while any is on), or by one only, as
    proposed.
  - Whether a build should re-hash `vendor/` on every run, catching a hand edit to a committed
    vendored package, or trust the hash recorded beside each package as it trusts the cache's.
  - Whether a manifest names the oldest compiler it builds with (`package.avin = "0.x.y"`), since
    the standard library's modules arrive with the compiler and a package using a new one fails on an
    older compiler with "no module" rather than a version message.
  - Whether manifests gain machine conditions (Sysl's `if`, `choices`, `targets`) once Avin builds
    for more than one target.

- Lightweight threads, the open parts of the settled design above (cancellation is under
  [Proposed](#proposed)): **keeping user-interface work on the main thread** (a scheduler that runs a
  lightweight thread on the main operating system thread only, which also gives the `ui` package
  its change from another thread); **stack size and growth** (a fixed reservation first; parked
  frames moved to the heap later). The allocation chunk belongs to the carrier (proposed above), so
  thousands of parked lightweight threads hold no chunk each. The collector needs nothing new: its roots are what the
  runtime's callback marks, so a parked lightweight thread's context keeps its exact shadow stack
  and is walked as an attached thread's is, and switching stacks happens only at the runtime's own
  safepoints, so no collection sees one half done.
- Resource cleanup: a `defer` construct or a library `use`.
- Whether mailboxes may be bounded, and what a `tell` to a full one does, since `tell` never waits.

- Settled on the user's delegation 2026-11-06 (branch `check-stops-early`): **`avin check` stops after type checking**. Checking a
  program no longer lowers it, builds the shims of its Sysl bindings or builds its C unit: the
  previous `check` did all three and threw the result away, 4.9 s against 0.4 s of checking on a
  terminal-interface example. Two diagnostics came only from those later steps. **A C declaration
  that names no header** needs no C compiler to find, so `check` reports it. **A Sysl binding Sysl
  rejects, and what clang rejects of the C unit** (a constant that does not fit, a prototype the
  header contradicts) need the real tools, and stay behind `avin check --bindings`, which runs the
  whole pipeline up to the emitted code as `check` used to. Why: a type error is the common failure
  and should cost what checking costs; a binding changes rarely and `build`, `run` and `test` still
  report it. Alternatives weighed: moving the shim's type-check into the checker (a second
  implementation of Sysl's inference, certain to drift); keeping `check` as it was (the slowness
  this fixes); dropping binding validation from `check` with no flag (leaves no command that
  reports a bad binding without linking). The language server already stops at checking, so it
  matches the new `check`. Review with the user: whether `--bindings` should instead be the
  default for packages that have bindings.
