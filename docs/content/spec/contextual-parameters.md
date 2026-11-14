---
title: Contextual parameters
summary: given values and using parameters carry a context — a transaction, a logger, a request — through the calls that need it, resolved lexically at compile time.
weight: 21
---

Some values belong to the work in progress rather than to any one call: the database transaction a
request runs in, the logger it writes to, the request's own identity, the clock a test controls.
Every function on the way down needs to pass them on, and most of those functions do nothing else
with them. A **contextual parameter** is such a value. A function declares it after `using`, a
caller declares a **`given`** of its type, and each call finds it without its being written:

```text
import app.db.{Db, Row, Transaction}

insert(row: Row, using tx: Transaction) = tx.execute("insert into rows values (?, ?)", row.values)

saveAll(rows: Vector[Row], using tx: Transaction)
  for row in rows do insert(row)

main()
  val db = Db.open("app.sqlite")
  given tx = db.begin()
  saveAll(loadRows())
  tx.commit()
```

`saveAll(loadRows())` passes `tx` to `saveAll`, and inside `saveAll` the parameter `tx` is itself a
given, so `insert(row)` passes it on. Each filled argument is decided by the checker and compiled as
if it had been written; nothing is looked up at run time.

## What givens are for

**A given is a value of the program's context, never a conformance.** Which methods a type has, how
it compares, hashes, displays or encodes, is answered by its [impl](/spec/declarations/#impl) and
asked for by a [bound](/spec/generics/#bounds), coherently: one answer per trait and type,
everywhere. Givens do not take part in that:

- **A bound is never met by a given, and a given never changes what a bound finds.** `[T: Ord]`
  calls `T`'s own `compare`, whatever givens are in scope.
- **A contextual parameter's type names no type parameter**, of its function or of an enclosing
  class, trait or impl: `using codec: Codec[T]` is an error. Its type is fixed where the function is
  declared, so every call resolves it the same way at every instance. What a type parameter needs
  of its argument is a bound.
- **A given has no type parameters.** A given is a value, and no value in Avin is generic.
- **A given's type is a type of the program's own kind**: a class, an object's type, a trait used as
  a type, or a `data` type, with closed type arguments (`Pool[Connection]`). A built-in type, a
  tuple, a function type and a union are refused: a given `int` would be given wherever any `int`
  is asked for. A trait that mentions `Self` or has static members is a bound and not a type, so it
  is no given's type either.

An ordering other than a type's natural one is an argument of the one call it belongs to
(`sortWith`, `sortBy` with `Reverse`), not a given: it is a decision about that sort, not part of the
surrounding work.

## Declaring a given

```text
given tx = db.begin()
given log: Logger = FileLogger("app.log")
```

`given name = value` declares a `val` named `name` that is also a given; `given name: Type = value`
gives its type, which is then the given's type. A given is a `val`: it is never reassigned, and
mutable context is an object whose own state changes.

- **Every given has a name.** The name is how a call passes the value explicitly, how a diagnostic
  names it, and how another file imports it.
- **A given is declared in a block, at a module's top level, or in an object's body.** In a block it
  is in scope from its declaration to the end of the block, as a `val` is; at the top level, in
  every file of its module; in an object's body, in that body. A class, trait or `data` body declares
  none: a class takes its context as `using` parameters of its constructor (below).
- **A given is not in scope in its own value**, so `given reqLog = log.child(id)` reads the `log`
  given of an outer scope.
- **One scope declares one given of a type.** A second given of the same type in one block, at one
  module's top level or in one object's body is an error at the second.
- A module or object given is made when that module's values or that object are, in the same order.
  `private` keeps a module's given to its module.
- **`given` is a keyword only where it begins a given**: at the start of a statement in a block, and
  where a declaration starts at a module's top level or in an object's body. Elsewhere it is an
  ordinary name, so `args.given("out")` and a field named `given` are unaffected.

## `using` parameters

```text
log(message: string, level: Level = Level.Info, using logger: Logger) = logger.write(level, message)

class OrderService(using db: Db, logger: Logger)
  place(o: Order)
    log(s"placing ${o.id}")
    db.transaction(using tx -> insert(o.row))
```

`using` marks every parameter after it as contextual. The section comes last in the list, after any
repeated parameter, and each of its parameters has a name and a type (in a lambda, the type may be
inferred). `using` is a keyword only where it is followed by a parameter's name in a parameter list;
a parameter named `using` (`using: Mode`) is still one.

- **Functions, methods, static members, extensions' methods, class constructors and actors' parameter
  lists take a `using` section.** A `data` variant, `main`, a `@test` function or hook, a `@sysl` or
  `@c` binding, and a function with no parameter list take none.
- **Inside the body a contextual parameter is a given**, at the level of the function's parameters,
  so calls in the body pass it on. A class constructor's contextual parameters are private fields,
  and givens in its field initializers and methods; an actor's are givens in its body and its
  `receive`.
- **A trait's method with a `using` section is implemented with the same section**: the same names
  and types, in order, as an override takes its method's parameter types. A call through the
  trait's type or a bound resolves the section where it is written.
- A function written as [clauses](/spec/declarations/#clauses) writes its `using` section on its
  signature line; the clauses match the other parameters, and each clause's body has the givens.
- A contextual parameter may have a **default**, used where a call finds no given:
  `now(using clock: Clock = SystemClock) -> Instant`. The default is evaluated where the function is
  declared, as every default is.

## Calls

A call fills each contextual parameter, in order, from the first of these that answers:

1. **An argument passed by name**, `insert(row, tx = other)`. A contextual parameter is passed
   explicitly only by name; a positional argument never reaches it, and one written for it is an
   error saying to name it.
2. **The given in the nearest scope** whose type conforms to the parameter's (below).
3. **The parameter's default.**

Where none answers, the call is an error. The section takes no part in choosing an overload: the
written arguments choose it, and its contextual parameters are filled afterwards, so a missing given
is an error at the chosen overload, never a reason to choose another. A method with one required
parameter is still [called infix](/spec/operators-and-control-flow/#infix-method-calls), its
section filled the same way.

Filling an argument from a given evaluates nothing: a given is a value already made. The written
arguments are evaluated in the order written; then the omitted parameters' defaults in parameter
order, as before, and every default, of an ordinary parameter too, sees the contextual parameters,
which are filled first: `level: Level = logger.defaultLevel` before `using logger: Logger` is
legal.

## Resolution and scope

A call looks for a given outward from where it is written, level by level, and stops at the first
level holding one that fits:

1. the enclosing blocks, innermost first, each with the givens declared before the call;
2. the contextual parameters of the enclosing function, and of each function or lambda enclosing
   that, innermost first;
3. the contextual parameters of the enclosing class's constructor or actor, for its methods, field
   initializers and `receive`;
4. the givens of the enclosing object's body, for its methods and fields;
5. the module's own top-level givens together with the givens the file imports.

- **A given fits where its type conforms to the parameter's type**, as an argument's must: a given
  `FileLogger` fills `using logger: Logger`, and a given `Logger` does not fill
  `using file: FileLogger`.
- **The nearer level wins outright.** A given in a block hides the module's, whatever their types;
  there is no ranking by how specific a type is.
- **Two givens that fit at one level make the call an error naming both**, and the call passes one
  by name. Within a block or an object this happens only for two different types conforming to the
  parameter's; at the module level, also for a module's given and an imported one.

## Importing givens

**A file sees another module's given only by importing it by name**: `import app.clock.{systemClock}`
brings `systemClock` in as a value and as a given; `import app.clock.systemClock` and an `as` rename
do the same, and so does importing it through a module that [exports](/spec/declarations/#visibility-and-modules)
it. Importing the module whole, `import app.clock`, makes `clock.systemClock` readable as a value
but not a given. There is no wildcard import, of givens either.

A given decides, without anything written at the call, which value a function receives, so every
file that relies on one says which in its imports. An extension may be seen through any import of
its module because it can only add a method a call names; a given changes what a call does.

## Lambdas and function values

- **A lambda's body resolves contextual parameters where the lambda is written**, as any name in it
  is resolved, and a given it fills a call with is captured as any value it reads is.
- **A lambda's parameters after `using` are givens in its body**: `db.transaction(using tx ->
  insert(row))`. They are positional parameters of the function value all the same: the lambda's
  type is `(Transaction) -> unit`, and whoever calls it passes the transaction.
- **A function type has no `using` section**, as it has no names and no defaults: a function value is
  called with positional arguments only, all of them.
- **A function or method taken as a value has its section filled where it is taken**:
  `rows.foreach(insert)` with a given `tx` in scope is `rows.foreach(row -> insert(row))`, the
  transaction captured there. Where none is in scope it is the error a call would be.

## Generic code

A call with a `using` section inside a generic function resolves it where the generic function is
written, once, since the section's type names no type parameter; each instance passes the same
given. The bounds of the generic function's type parameters are untouched by givens, and a bounded
call such as `T.zero()` finds its member through the bound, as always. Errors are reported at the
generic definition, not at an instantiation.

## Threads and actors

A given is a lexical value, not a thread's or a task's: nothing carries it across `spawn`, a future
or a message by itself. **A closure that fills a call from a given captures it**, so a `@sendable`
body, a thread's `spawn` and an executor's task are checked for it exactly as for any captured value,
and a given that is not [sendable](/spec/actors/#what-may-be-sent) is an error there naming it. An
actor that needs a context takes it as a `using` parameter of its declaration, filled where the
actor is made and held to the rules of any actor parameter.

## Diagnostics

Each error is at the place a reader would change, and says what to write:

- *no given*: "`insert` needs a `Transaction` for `tx`, and no given of that type is in scope:
  declare one (`given tx = …`), take one as `using`, import one, or pass `tx = …`". Where a module
  the file imports whole declares one, it adds: "`app.db` declares the given `current`, which this
  file does not import: `import app.db.{current}`".
- *two givens*: "two givens fit `tx: Transaction` of `insert` here: `primary` (a `PgTransaction`,
  line 4) and `replica` (a `LiteTransaction`, line 5); pass `tx = …`".
- *a second given of one type*: "`replica` is a second given of type `Transaction` in this block,
  beside `primary` (line 4)".
- *positional*: "`tx` is a contextual parameter of `insert`, passed by name: `tx = …`".
- *a type parameter*: "`codec: Codec[T]` names the type parameter `T`; a contextual parameter's
  type is fixed where its function is declared, and what `T` needs is a bound: `[T: Codec]`".
- *a built-in type*: "a given is of a class, object, trait or data type: `int` would be given
  wherever an `int` is asked for; give it a type of its own (`data Retries(count: int)`)".

## Why it is sound

Each rule above keeps Avin's nominal typing and its coherent conformances intact; this is the
reason for each.

- **Coherence: a given never competes with an impl.** A trait's implementation for a type is one
  per program, found from the type alone (`impl`, [declarations](/spec/declarations/#impl)), and a
  given cannot replace, shadow or add to it, since no bound ever consults a given. So the classic
  incoherence cannot arise: a `TreeSet[Person]` built in one file orders by `Person`'s `Ord`, and
  code in a file with any givens at all that inserts into it, merges it with another set or searches
  it orders by that same `Ord`; two sets of one type never disagree on their order. Where an order
  other than the natural one is wanted, it is a value the structure is built with and keeps (a
  comparison function passed to `sortWith`, or a key wrapped in `Reverse`, whose type is then a
  different type with its own `Ord`), so the order travels with the value, never with the scope
  that happens to use it.
- **Determinism: one answer per call, decided at compile time.** Resolution is lexical: the levels
  are fixed by where the call is written, the nearest level that fits wins, and two fits at one
  level are an error rather than a choice. No ranking, no import order and no instantiation affects
  it, and a contextual type names no type parameter, so every instance of a generic function
  receives what the one checked definition resolved. The same source always fills the same
  argument.
- **Nominal matching only.** A given fits by conformance, and conformance in Avin is declared: a
  class's superclass and `:` list, a type's impls. A given is never matched by the members it
  happens to have, and a given's type is a nominal type of the program (a class, an object, a trait
  type, a `data` type); the structural types (tuples, function types, unions) and the built-in
  types, which carry no intent of their own, are refused, so no value fills a parameter merely
  because its shape fits.
- **Checked like a written argument.** A filled argument is type-checked exactly as the same
  argument written at the call would be, against the parameter's declared type; a default used in
  its place is checked where it is declared. There is no unchecked path: nothing is cast, nothing
  is looked up by name at run time, and a call that cannot be filled does not compile.
- **No conversions.** A given only ever fills a contextual parameter with a value already of a
  conforming type. It never changes a value's type, never makes a method appear on a type and never
  makes an expression typecheck that would not with the argument written out.
- **No escape from the sharing rules.** A given is a `val` read where the call is written; a closure
  that reads it captures it, and the thread, task and actor checks see that capture as any other. A
  non-sendable given cannot reach another thread through a contextual parameter, and a `var` is
  never a given, so no box is shared by accident.
- **Visible where it acts.** A file sees another module's given only by naming it in its imports, so
  adding a given to a library module cannot change what any existing call in another file
  receives; at worst a newly named import makes a call ambiguous, which is an error, not a silent
  change.

## Cost

**At run time, none.** Every contextual argument is decided by the checker and compiled as a
written argument; a given is a `val`. **At compile time, little and bounded:** each contextual
parameter at a call looks through the levels above, usually a handful, each holding a few givens,
and tests each with the conformance the checker already decides for arguments. Nothing is searched
recursively, no instance is made to find a given, and no choice is retried, since a given has no
type parameters and no contextual parameters of its own to resolve.

## What this does not do

- **No implicit conversions.** A given fills a contextual parameter and nothing else; a value of one
  type never becomes another unasked.
- **No givens that are functions**, built from other givens on demand, and no chains of them: a given
  computed from others is declared with a value that uses them, `given reqLog = log.child(id)`.
- **No generic givens and no givens for bounds** (above).
- **No contextual function types**: a function value's type is its positional parameters.
- **No dynamically scoped context**: what a call receives is decided where it is written.
