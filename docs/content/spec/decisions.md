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
  as continuation lines. No trailing commas are added. The formatter keeps every token and
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
  form, `i8.wrap(x)` (spelling proposed below), keeps the low bits. Not yet: the limits of a type parameter bounded by `Integer` (`T.MaxValue`).
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
- Settled by the user 2026-11-04: [`avin.time`](/spec/system-library/#avin-time): `Duration`, `Instant` (monotonic) and `WallTime`
  (wall clock, a `Duration` since the 1970 epoch) are data types counting whole microseconds in a
  `long`, overflow trapping; the two clocks are distinct types so their readings never mix. A
  duration implements `Add`, `Sub`, `Neg`, and scales by a `long` with its methods `*` and `/`; a
  reading combines with a duration through its methods `+` and `-`, and two readings of one clock
  through `-` (or `since`), which yields their `Duration`. The
  text forms are a duration's largest-unit form (`1.5ms`) and an ISO 8601 UTC timestamp. Sleeping
  and calendar fields beyond the timestamp text are not part of it yet.
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
  see the Sysl parameter's type, so it lets Sysl choose: a Sysl type with a destructor, always held
  by a counted reference, is passed as the handle's reference, which the parameter shares; one
  without is passed as the parameter's type asks of a generic helper, by value (a copy) or as a
  pointer to the handle's own value. A `&T` parameter of a type without a destructor receives a
  reference to a copy, since Sysl binds the helper's type parameter to `T` where a `&T` is expected;
  a `*T` parameter of a type with one is refused by Sysl at the binding.
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
  - The socket is a handle on a runtime type, `avin.rt.Connection`, holding `sysl.net`'s `Socket`
    with a destructor that closes it, since `sysl.net` closes a socket only when asked; so closing
    the handle, or the collector freeing a forgotten one, closes the socket. The runtime also turns
    `sysl.net`'s errors into the four a connection tells apart.
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
  for every `T`, which stay specified and unbuilt.
  **Review with the user:** a `&Signal[T]` parameter of a type without a destructor receives a
  reference to a copy under Sysl 0.9.0, as for every such handle, so a Sysl function setting a signal
  is not seen by the handle; the next Sysl release settles the shim's helper at the `&T`, and
  `tests/programs/run/sysl_generic_handle_sharing` waits on it.

**Deferred**

- Public C interoperability. Until then, C is reached through Sysl.
- Higher-kinded, path-dependent, union and intersection types; implicit parameters; macros.
- Async and await.

## Proposed

- Proposed on branch `sysl-callbacks` 2026-11-05: [passing Avin functions to
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
- **A member call on the name of a data type whose variant of its own name has no fields**
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

## Open

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

- Resource cleanup: a `defer` construct or a library `use`.
- Whether a function value can ever be sent between actors, with a type that says what it captures.
- Whether mailboxes may be bounded, and what a `tell` to a full one does, since `tell` never waits.
