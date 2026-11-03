---
title: Compiler architecture
summary: The target pipeline of the Sysl-written Avin compiler, its runtime, and how Sysl bindings are built.
weight: 40
---

The compiler is written in Sysl, as the package in `compiler/` with modules under
`dev.avinlang.compiler`. It compiles a whole program — the application, its Avin dependencies and
the standard library — in one run.

## Pipeline

| Stage | Input → output | Responsibility |
| --- | --- | --- |
| Lex and parse | source → AST | Indentation layout and grammar, on `sh.sysl.parsing`. Keeps spans; does no name lookup. A name in operator position, after a complete operand, is an infix method call: the parser sees it as an operator token, so keywords are the only words it has to tell apart. |
| Load | `avin.hocon` → module graph | Packages, modules and files; the reserved `avin` root; the prelude, which is an Avin file. |
| Resolve | AST → resolved AST | Scopes and symbol tables. Every name is bound to a declaration symbol, and imports, visibility and shadowing are decided here, once. |
| Check | resolved AST → typed HIR | A `Type` algebra over declaration symbols; local bidirectional inference with subtype constraints; bounds, variance and `impl` coherence; initialization; pattern exhaustiveness. |
| Bindings | `@sysl` declarations → shim | Generates the Sysl shim source and its boundary signatures. |
| Monomorphize | typed HIR → mono HIR | Instantiates functions, types and generic trait methods for every use, and computes the dispatch for trait-typed calls over the closed world. |
| Lower | mono HIR → MIR | Explicit control flow, object layouts and headers, type descriptors, vtables, closures and environments, allocation, GC root slots, traps. |
| Emit | MIR → LLVM IR | Spelling only: no typing, layout or semantic decisions. |
| Link | `.ll` + runtime + shim → executable | clang; the runtime library; the cached shim library. |

Each stage has its own data structures and its own tests. No stage reaches back into an earlier stage's
input. **Types are an enum over symbols, never strings**: built-in types, nominal types, functions,
tuples, type parameters, inference variables, `Any`, `Nothing` and an error type.

**Loading a program.** `avin build <dir>` takes a directory as the program's root: every `.avin` file
under it is read in path order, passing over directories whose names begin with `.`, and each is
given the module its directory's path names, the root directory's files last as the root module.
A file whose header disagrees with its directory, or that has none outside the root, is reported
before anything is checked. `avin build <file>` is a program of that one file, in the module its
header names. The standard library is loaded the same way, from `library/` under the toolchain's
root, where every module is `avin` or under it; `library/avin/prelude.avin` is one ordinary file of
module `avin`. A package manifest will name the roots the loader walks; the files and their modules
are what the later stages see.

Names are resolved by module. Every top-level declaration is held under its qualified name, `a.b.X`
(a root module's under `X`), and a bare name in a file resolves to the first of its own module's
declaration, its file's import, and the `avin` module's public declaration that exists. Imports are
checked once every declaration of the program is known, and a wrong one is dropped. Object-file
symbols carry the module path, so two modules may each declare `helper`.

## Runtime

`runtime/` is a Sysl package, built into a library archive when its sources change rather than once
per program, and linked into every executable. It contains:

- program entry, arguments and exit status;
- the collector interface over `sh.sysl.gc` — heap setup and growth, collect-on-pressure, a
  shadow-stack root walk, and finalizers;
- type descriptors: size, trace map, type identity and dispatch tables;
- traps with messages and source locations;
- the string, number-formatting and text primitives the core library builds on.

Its ABI is private between the compiler and the runtime, and not visible to Avin code.

The heap is one block reserved when the program starts: 8 GiB, or `AVIN_HEAP_LIMIT` bytes. The
operating system commits its pages as they are first used, so the heap grows within the reservation
and costs nothing until then. A program whose live data outgrows the limit stops with a message
naming it.

The collector owns each object's header, which sits just before the object: the word 16 bytes
before an object's address holds its **kind**, the collector's record of how to trace it, and the
word after that is the collector's own. An object's address is its first field. Every descriptor
begins with that kind — the runtime's tracer, and no finalizer or weak hooks — followed by the
variant's tag, its size, the number of its references, its type record, and the offsets of its
fields that hold references, so the kind an object
was allocated with is its descriptor, one load away. Constant objects (string literals, variants
with no fields, closures that capture nothing) live outside the heap as constant data with the same
16 bytes ahead of them, their descriptor and a word left zero; the collector ignores any reference
outside its heap, so it leaves them alone. `AVIN_GC_STRESS=1` makes every
allocation collect first, and the execution tests run that way, so a reference the compiler failed
to root is found at the first allocation that could lose it.

An object of a class is laid out as a one-variant `data` value is: one 8-byte slot per field (wider integers take more), the class parameters first and then the fields
the body declares, in order. A class with no fields still allocates, since every object has its own
identity. The constructor is an ordinary function named after the class: it allocates the object,
stores its parameters into their fields, runs the body, initializing each body field where it is
declared, and returns the object, which it holds in a local named `self` so closures made in the
body capture it as they would any local. A method is an ordinary function whose first parameter is
the receiver. The constructor and the methods share the class's type parameters, a method's own
following them, so a generic class's constructor and methods are instantiated per list of type
arguments as generic functions are, and a field's type at a particular instance is its declared
type with the receiver's type arguments substituted. A call of a method of a final class is a
direct call. A method named without a call is checked into a lambda of the method's type without
the receiver: the receiver is evaluated into a hidden local the lambda captures, and the lambda's
body calls the method on it, directly or through the method table as a call written there would.

A subclass's object starts with its superclass's fields, in the superclass's order, so the
superclass's code finds each field at the offset it expects in any subclass's object. An `open` or
`abstract` class's body is compiled as its *initializer*, `<Class>.<init>`, a function that takes
the object and the constructor's parameters; the class's constructor allocates and calls it, and a
subclass's initializer calls its superclass's first. An abstract class's constructor and its
abstract methods are never compiled.

Each class has a method table with one slot per method a subclass may override: its superclass's
slots in the same positions, each holding the class's override where it has one, then one for each
overridable method the class adds. A method is identified by its name and parameter types, so
each overload of a name has a slot of its own, and an override takes the slot of the inherited
method with its parameter types; each overload is also a symbol of its own, named by its parameter
types. The table is constant data placed directly before the class's
descriptor, in one global, so slot `s` is the word `8(s + 1)` bytes before the object's
descriptor; the descriptor itself keeps its layout and the runtime never reads the table. Each instance of a generic class has its own descriptor and table, filled with the method
instances for its type arguments. A call through a receiver whose static type is an `open` or
`abstract` class loads the descriptor from the object's header, loads the slot, and calls it indirectly with the
overridden method's machine signature, which every override shares; `super.m()` is a direct call.

A trait's methods are found through the same table, with the same two loads. The whole program is
compiled together, so every class conforming to a trait is known: each trait is given one position,
its `base`, and its methods take the positions `base`, `base + 1`, … in the table of **every** class
conforming to it, directly or through a superclass or a trait it requires. Positions are chosen
first-fit, trait by trait: the lowest `base` at which the trait's methods lie past every conforming
class's own slots and clear of the traits already placed in each. A position a class's traits do
not use is a null word. A call through a trait-typed receiver loads the descriptor and the word at
the trait method's fixed position, so it costs what a virtual call costs: no search, no hashing, no
second table. The price is table size, since positions are global, and tables are small constant
data. At each trait position the class's table holds its own override, else what its superclass's
table holds under that name, else the trait's default body. A trait's `val` takes a position like a
method of no parameters; the checker fills it, in each class or data type that supplies it with a
field, with a generated method that loads the field, so a read through the trait is a virtual call
and a read through the class is the field load. A trait-typed value is a plain
reference to the object, so an upcast from a class to a trait, and from a trait to one it requires,
is free; a call through a final class's type, even of a method it inherits from a trait, is direct.
A trait's method declared `override` replaces the default of a required trait's method: it takes no
position of its own (`trait_slot` of it is the overridden method's), and among the bodies a class
or data type inherits under one name, one that another of them overrides, through a chain of trait
overrides, is dropped before two remaining bodies are reported as a conflict.

A method with type parameters of its own, a trait's or an overridable class method, holds a
position in the checker's tables like any other, but no one function fills it: each list of its own
type arguments is a different function. Lowering leaves its position empty and gives the method a
slot past `generic_base`, the length of the longest table any type has, for each list of own type
arguments a call through a table uses, keyed by the checker position, the name and those
arguments. A slot is made when lowering first meets such a call, and filled in the descriptor of
every type whose table holds a method of that name and arity at that position, with the type's
implementation instantiated at the type's arguments followed by the call's; a descriptor made later
is filled in every slot already made. The descriptors are emitted only after the worklist is
empty, so every slot the program uses is in every table that needs it. Two unrelated methods
sharing a position and arguments share a slot, which is safe, since no type holds both. Comparing
two such methods' signatures renames each one's own type parameters by position to
`method_params_owner()`, so an override with a type parameter of another name still matches.

A trait has a constructor symbol, so instantiating one is reported as for an abstract class, and it
is never compiled.

An `impl` is resolved before any type's fields: its target and trait are named with the impl's type
parameters standing for the type's own (`data_owner` or `class_owner` of the type), so its methods'
types are the type's as any other method's are. An impl of a class or an object joins it: its trait
is added to the class's `traits` beside those its header states, and its methods to the class's
methods, marked as overriding when the impl is of a trait, so the method table, inheritance and
dispatch are the class's own machinery. An impl of a `data` or built-in type gives each method a
`receiver`, the type of its `self`; such a method is called directly, and inside it the checker
finds the type's methods, and a one-variant type's fields, by bare name. A `data` type conforming
to a trait has a method table like a class's, with no slots of its own: trait positions are placed
over classes and data types together, and the table holds at each position the type's method of
that name or the trait's default. Every variant's descriptor carries that table before it, the
constant objects of fieldless variants included, so a trait-typed call on a data value is the same
two loads. The type record lists the traits too, so a `t: Trait` pattern on an `Any` matches.

`Any` is a reference to an object. The checker inserts a **box** node wherever a value converts to
`Any`; lowering makes it nothing for a value already kept as a reference to an object with a
descriptor (a string, a tuple, a `data` value, an object, a closure), and for a number, `char` or
`bool` a new one-field object whose descriptor is that type's box descriptor; `unit` has one
constant box. A **type record** per type is constant data that every descriptor of the type names,
at offset 56: the number of the type's ancestors and their records' addresses. A class's ancestors
are its superclasses and every trait it and they conform to, with their type arguments as it sees
them; a tuple's are the tuple types its parts' ancestors make, one combination at a time. A type
pattern loads the object's descriptor and its record: for a type nothing else can be (a built-in, a
final class, a `data` type, a tuple of such), one comparison with the target's record; for an open
class, a trait or a tuple with a part of one, a call of the runtime's `avin_rt_conforms`, which
scans the record's ancestors, so the test costs the number of ancestors. A generic instance is its
own type with its own record, so `Box[int]` and `Box[string]` differ by address. Strings are made by
the runtime as well as written as literals, so a string's descriptor names no record, and a null
record is the mark of a string. Closures and the boxes of captured variables share one record no
type pattern names. A type pattern on a value whose static type already conforms to the target tests
nothing, and on a value of another value type (possible only in a copy of generic code) never
matches.

An `object` is an instance of a class of its own, kept apart from its companion class's: its
constructor and methods are separate functions, and its layout and descriptor are its own. Each
module-level `val`, `var` and object is a **global** of the program, and its initializer is
checked as a function with no parameters whose result is the value; an object's initializer is its
constructor. The checker records what each initializer reads, following every function it calls and
every lambda it makes, orders the globals so that each comes after what it reads, keeping
declaration order where nothing decides, and reports a cycle by its chain. The order becomes one
function, `<init>`, that stores each initializer's result into its global; `main` in the emitted
program calls it before the program's entry function. A global that holds a reference is a slot of
one frame, `@avin.roots`, laid out as a function's shadow-stack frame is, which `main` hands to the
runtime before anything is stored in it; every collection walks it beside the threads' frames. A
global of any other type is an LLVM global of its own. All start zero.

A trap stops the program with one line on standard error naming what failed and where:

```text
avin: trap: integer overflow at gcd.avin:12:9
```

The place is the operation's: the operator of an arithmetic operation, compound assignment or
`++`, the start of a conversion such as `byte(n)`, a negation's `-`. The line and column count from
1, and the column counts characters. The file is named without its directories, so the message
reads the same wherever the program was built. The reasons are `integer overflow` (which includes
the most negative value divided by -1), `division by zero`, `remainder by zero`, `value out of range
for byte`, `value out of range for char` and `surrogate code point is not a char`; a `match` or a
function's clauses that no value falls through still name their place, as a check on the compiler.

Lowering records each place a program can trap as a **trap site**: the reason and the file as
constant strings, then the line and column. Every check made at a site branches to one cold block of
its function that calls the runtime with the site's constant address, so the checks of one operation
share a block and two operations never do; the instances of a generic function share its sites. The
runtime flushes what the program has written to standard output first, so no output is lost, then
writes the message and stops.

Programs are multi-threaded, so the runtime keeps no per-thread state in globals. Each thread has a
**context**, and every compiled function, closure entry and runtime entry point takes the running
thread's context as a hidden first argument. A function that holds references links its frame of
roots onto its thread's shadow stack by writing the context's first word directly, on entry and on
every return, without calling the runtime. A collection walks the frames of every attached
context; the main thread's is attached before the program's entry function runs. Another thread
gets a context of its own from `avin_rt_thread_attach` before it runs compiled code and gives it
back with `avin_rt_thread_detach` when it has finished, with no frame left on it.

Threads share the heap, and one lock serializes allocation, collection and changes to the list of
contexts; the collector itself runs single-threaded. **A collection stops the world.** Each context
is either *running*, when its thread may touch references, or *safe*, when its frames hold every
reference it still needs and it touches none until it is running again. The collecting thread,
holding the lock, raises the flag `avin_rt_requested`, waits until every other attached context is
safe, collects, lowers the flag and wakes the others. A thread becomes safe in three ways:

- **At a poll.** The emitter places a poll at the entry of every function that calls another,
  after its frame is linked and its arguments stored, and before every terminator with a back edge,
  found by a depth-first walk of the function's blocks, so every loop polls once per iteration and
  every recursion once per call. A poll is a relaxed atomic load of the flag and a branch to a call
  of `avin_rt_safepoint`, which is declared cold; the thread parks there until the collection is
  over. A function with neither calls nor loops never polls.
- **Waiting for the lock.** Allocation takes the lock with one compare-and-swap when it is free; a
  thread that has to wait for it waits safe, since the holder may be collecting. Allocation is
  therefore a safepoint.
- **Around a blocking call.** `avin_rt_enter_safe` and `avin_rt_leave_safe` bracket an operation
  that may wait (a lock, a channel, a join, reading input), so a blocked thread never holds up a
  collection; leaving waits for a collection in progress to finish.

The handshake is a pair of sequentially consistent stores and loads, the flag by the collector and
a context's state by its thread, so either the thread sees the flag and wakes the collector, or the
collector sees the thread safe. With one context attached the flag is never raised, so a program
with one thread takes no poll's slow path and makes no system call for a collection.

## Standard library

`library/` holds Avin source under the module root `avin`, starting with the prelude every program
sees. The compiler knows a small, declared set of
**intrinsics**: arithmetic on built-in types, allocation, field access and the runtime entry points.
Each is declared in Avin source with `@intrinsic("…")` and has no body. Everything else, including
`print`, is ordinary library code.

## Tests

`sysl test compiler`, run from the repository root, runs every test.

- **Compiler unit tests** use Sysl's runner, one file per stage or feature. Parser tests compare a
  compact rendering of the tree: `1 + 2 * 3` shows as `(+ 1 (* 2 3))`.
- **Runtime tests** are the runtime package's own Sysl tests, which the compiler's tests run.
- **Execution tests** are Avin programs in `tests/run/`, compiled and run at `-O0` and `-O2`. A
  program's leading `// expect: exit N` or `// expect: trap` says how it ends (exit 0 when it says
  neither), and `NAME.out` and `NAME.err` beside it hold the output it must write.
- **Rejection tests** are Avin programs in `tests/reject/` that must fail to compile; each
  `// error: TEXT` line names text the diagnostics must contain.
- **Library tests** are Avin `@test` functions run by `avin test`, once that runner exists.
- **Semantic references**: where Avin defines arithmetic as Sysl does, a test compares against an
  independently compiled Sysl program.
