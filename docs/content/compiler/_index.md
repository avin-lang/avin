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
| Bindings | `@sysl` declarations → shim archive | Generates the Sysl shim source and its boundary signatures, and has Sysl check and build it. |
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

## Running and testing

`avin run` and `avin test` build through the same pipeline as `avin build`, into a cache in the
program's directory, `.avin/build/` (beside the file, for a program of one file). Beside each
executable, `<name>-O2` or `<name>-tests-O2`, a record holds the key it was built for: a hash over the
compiler's version and its executable's modification time, the kind of build and its optimization,
the runtime's sources, the standard library's and the program's. A run whose key matches the record
starts the cached executable without compiling anything. `avin run` runs it with the program's
arguments and the terminal's own output, and exits with its status, or 128 and the signal's number
for a program a trap stopped.

**A test is checked at its annotation.** The checker collects every top-level function marked
`@test`, in declaration order, and holds it to taking no parameters, having no type parameters and
yielding unit; `@test("a sentence")` names it by the sentence. A program built for its tests need not
have `main`, and when it has one, its `main` is not run.

**A test executable runs one test per process.** Its `main` reads the test's number, counting from 0
in the checker's order, from its first argument, initializes the module-level values as a program's
`main` does, and calls that test; a test that returns exits 0. The runner starts the executable once
for each selected test, captures what it writes, and reads the outcome from how the process ended: a
trap's report on standard error gives the reason and place. A test that stops, by a trap or a signal
or after the runner's time limit of a minute, stops only its own process. The record of a test
executable also lists its tests, each with its file and line, so a cached run reports them without
compiling the program.

**An assertion's message is made at run time.** The core library's `trap` intrinsic takes its
reason either as a literal, which is written into the trap site, or as a string the program built,
which is passed to the runtime with the site (`avin_rt_caller_trap_message`, or
`avin_rt_trap_message` in the program's own code). `assertEq` builds its message from the `Debug`
form of both values; as for every check in the core library, the place reported is where the program
called it.

## Sysl bindings and the shim cache

A [binding](/spec/sysl-bindings/) is checked as a top-level function without a body: its
`@sysl("module.name")` must name a Sysl function by module path and name, and its parameters and
result must be boundary types — integers of 8, 16, 32 or 64 bits, `byte`, `real`, `char`, `bool`,
`unit` and `string`. Lowering gives it a body that passes its parameters, after the context, to a
shim symbol, `avin_shim_` followed by the binding's own symbol with every character other than a
letter or digit written as `_` and two hex digits, and returns what the shim returns. Since it is an
ordinary function, a binding can be passed as a value. The MIR program lists every binding of the
functions it compiles.

For a program with bindings, the driver generates one Sysl module, `avin.shims`, in a package whose
manifest depends on the runtime's package. Each binding gets an import of its Sysl function under a
private name, and an exported shim that converts each argument (a `bool` arrives as a byte, a `char`
as its code point, a `string` is copied into a Sysl `string`), calls the function with the result
declared at the binding's type, and converts the result back (a `string` is copied into a new Avin
string through the runtime's `avin_rt_new_text`). `sysl build-c` checks and builds the package into
one archive holding the runtime, the shims and the part of Sysl's library they reach, which the
program links instead of the runtime's own archive. One archive means one copy of Sysl's library, so
Sysl and Avin share standard output and keep their writes in order.

A Sysl diagnostic is placed by its line in the shim: the driver records the line of each binding's
import and the lines of its function, and reports the message at the binding's name as `the Sysl
binding \`name\` does not match \`module.name\`: …`. A diagnostic that falls elsewhere is reported
with Sysl's whole output.

The archive is cached under `.avin/shims/` in the toolchain's root, named by a SHA-256 of the
manifest, the shim source, the `sysl --version` output and the runtime's source files. A build with
the same bindings finds it there and runs no Sysl build; it is built under a temporary name and
published by rename, so concurrent builds never see a partial archive. The `check` command builds
the shims too, since only Sysl can say whether a binding is right.

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
final class, a `data` type, a tuple of such, each with only invariant parameters), one comparison
with the target's record; for an open class, a trait, an instance of a type with a covariant or
contravariant parameter, or a tuple with a part of one, a call of the runtime's `avin_rt_conforms`, which
scans the record's ancestors, so the test costs the number of ancestors. A generic instance is its
own type with its own record, so `Box[int]` and `Box[string]` differ by address. Strings are made by
the runtime as well as written as literals, so a string's descriptor names no record, and a null
record is the mark of a string. Closures and the boxes of captured variables share one record no
type pattern names. A type pattern on a value whose static type already conforms to the target tests
nothing, and on a value of another value type (possible only in a copy of generic code) never
matches.

**Operators and equality.** The checker keeps an operator as its `Arith`, `Negate` or `Compare` node
whatever its operands' type, after holding a data type, a class or a type parameter to the core
trait the operator needs (`Add`, `Ord`, `Eq`, found by name in module `avin`). Lowering decides per
instance from the concrete type: a built-in type gets the machine operation (so `a + b` in
`sum[T: Add]` at `int` is a checked add with its trap at the operator, and no call); a final class
or a data type gets a direct call of what its method table holds at the trait method's position,
its own method or the trait's default body; an open class, a trait and a trait's `Self` get a call
through the object's method table. A call written through a bound to a core operator method, or of
`equals` and `hash` on any value, is lowered the same way. `==` on a type with no `equals` of its own
is the type's built-in equality: machine comparison, string bytes, identity for objects and closures,
and for a tuple or data type a function lowering makes per instance, comparing variant tags and then
fields; the hash likewise, combining tag and field hashes. A type record also holds two **equality
entries** after the ancestor count (offsets 8 and 16): its type's `equals` on two objects and `hash` on
one, compared as exactly that type. `==` on `Any`, a trait type or an open class calls one generated
function that loads both records, answers false when they differ, compares strings (null record) by
bytes, and otherwise calls the record's entry; the hash reads the entry the same way. The entries are
made for every record an object can have, once a program makes such a comparison at all; making them
can lay out more types, so lowering repeats until no record lacks them.

**Conditional conformance.** An impl with bounds of its own is recorded as a
`ConditionalConformance`: the type's owner, the trait, and the bounds per type parameter, in the
type's own parameters. The trait joins the type's `traits` as any other, so trait positions and
method tables are laid out over every instance alike, and the impl's methods see the bounds as their
parameters' own. Whether an instance has the conformance is decided by `holding_ancestor`, which
walks the type's conformances as `ancestor_type` does but passes a recorded one only where the type
arguments meet its bounds (`meets_bound`: a type parameter through its bounds in scope, a built-in
through its impls, a class or data type recursively). It reads a `Conformances` value: the types,
the built-in and conditional conformances, the core traits, and the bounds of the type parameters in
scope, which lowering, asking only of concrete types, leaves empty. Every place a conformance is
decided goes through it: a bound held to a type argument, a conversion (`view_as`, so a covariant
type argument too), the type record's ancestors, the static decision that a type pattern always
matches, and a call of a method the type has only through the conformance, a method of the impl or
the trait's, which the checker reports on an instance missing the bounds. Such an instance's table
leaves the trait's positions empty, so the impl's methods are never compiled at its arguments; the
`equals` and `hash` entries of `Eq` and `Hash`, which every type meets, are unaffected.

An impl with bounds of its own and no trait is recorded as a `ConditionalImpl`, the same owner,
bounds and parameters without a trait; its methods are declared as any impl's, so they overload the
type's others and clash with one of the same parameter types. `refuse_unmet_conformance`, at every
call and method value, finds the impl a method came from by its first type parameter's span and
reports the first parameter and bound the receiver's type arguments miss (`unmet_condition`). A
class's method is decided at the arguments the receiver gives the declaring class, through
`ancestor_type`, so a subclass instance is held to its superclass's impls. The methods are ordinary
generic methods, compiled for an instance only where a call reaches them.

**Derived traits.** `derives` is written out on the syntax tree (`derive.sysl`) before the checker
declares anything, so a derived method is declared, checked and compiled as one written by hand, and
every node made for it carries the span of its `derives` entry. A class gains each derived trait
after its parents and an `override` of the trait's method over its constructor parameters, read
through `self` and `other`; a data type gains an impl per trait (none for `Eq` and `Hash`, which it
has by value), its method matching `(self, other)` or `self` against each variant, and for `Ord`
comparing variant positions when the variants differ. `Eq` joins `equals` calls with `&&`, `Hash`
folds the fields' `hash` with wrapping `*% 31 +%`, `Ord` returns the first nonzero `compare`, and
`Display` and `Debug` write the name and each field through the trait's own method, rendering into a
`StringWriter` first when the spec has a width. A generic type's `Ord`, `Display` and `Debug` are
conditional conformances: an impl bounding every parameter by the trait, for a data type as always
and for a generic class in place of the parent and the method. The rules a list breaks on its own (an unknown trait, a repeat, a trait the type also
implements, `Eq` without `Hash` on a class) are reported while writing it out. The rest needs types:
once every conformance is known, the checker holds each field to the trait derived from it, with the
bounds the parameters have in the derived impl, names a field that falls short, and leaves that
derived method unchecked.

The supertypes an instance has through **variance** are open-ended (`List[Dog]` is a `List[Animal]`
and a `List[Any]`), so a record does not list them all. Lowering notes each record a type pattern
tests through `avin_rt_conforms`; once every function is lowered, each record gains those tested
records whose types its own conforms to by variance, by the checker's rule (`view_as`). A test
still costs one scan of one record.

A covariant view needs no conversion, and stays sound under monomorphization, because variance
applies only between reference type arguments. Every field and every part of a field whose type
mentions a type argument is then a reference (the argument itself, or a tuple, function, `data`
value or object built from it), so `Box[Dog]` and `Box[Animal]` have the same layout and the same
method-table shape, and code of either instance reads the other's objects correctly. A call through
a method table runs the code of the object's own instance: a `Box[Dog]` seen as a `Box[Animal]`
runs `Box[Dog]`'s method, whose results are `Dog`s and so `Animal`s, and whose parameters, a
covariant parameter never being one, take what the view passes. A direct call (a final class's
method, an `impl`'s method of a `data` type) runs the code of the static instance, `Box[Animal]`'s,
on the `Box[Dog]` object: the layouts agree, and that code treats every value as the wider type the
view promises, so anything reified it does (a type pattern on `T`, a new `Box[T]`) is done at the
view's types, which the caller expects. The declaration check is what keeps both paths sound: a `+T`
never reaches a parameter or a mutable field, so neither code can store a `Cat` where a `Dog`
instance's field expects a `Dog`, and never reaches an invariant argument, so neither code can hand
out an invariant `Cell[T]` that would let one be stored later.

A type parameter's bounds are resolved where it is declared and kept by owner, position and source
place, since the own parameters of two methods of one class share an owner and positions; a class's
or data type's parameters are looked up at the type's declaration wherever they are named. Type
arguments written in declarations are held to their bounds once every conformance is known (after
the method tables are placed), and those of calls as each call is checked, after the arguments,
whose types also settle the bounds' own type arguments. A trait's `Self` is a type parameter owned
by `self_owner` of the trait, bounded by the trait: a call substitutes the receiver's type for it,
and comparing an implementation with a trait's method substitutes the implementing type. A call on
a value of a type parameter is a `BoundCall` carrying the method's position in every table that
holds it (a trait's position, or an open class's). Lowering knows the receiver's type in each
instance: for a final class or a `data` type it takes the implementation at that position in the
type's table and calls it directly, at the type's arguments followed by the call's own; for a trait
type or an open class it dispatches as a call on that type does. A trait's default bodies are one
function for every conforming type, so `Self` stays in them, lowered as a reference, and their
calls on `Self` values dispatch.

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

A trap inside the core library reports the program's own call instead, through the context's
**caller site**, a word at offset 24 holding the address of a trap site. Code generated for the
program's functions sets it, before each call that may enter the library, to a site with no reason
at the call's start: a direct call of a library function, or any call through a method table or a
function value, which may reach the library. A call between two of the program's functions sets
nothing, and no function takes a parameter for it. The library's functions, and the functions
lowering makes, never set it; around each call that may run the program's code (a call of a
function of the program's, through a method table or of a function value) they read it before and
write it back after, since the program's code may enter the library again and move it. A trap in
the library's code, a `fail` or a failed check alike, ends in `avin_rt_caller_trap`, which reports
the reason of the library's own site at the place the caller site holds; before the program has
entered the library at all, the caller site is null and the library's own place is reported. The
function that initializes the module-level values counts as the library's: each value is computed
by a function of its own, which is the program's.

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

- **At a poll.** The emitter places a poll before every terminator with a back edge, found by a
  depth-first walk of the function's blocks, so every loop polls once per iteration, and before
  every call that may recur. Which calls may recur comes from the strongly connected components of
  the whole program's call graph, in which a call through a function value may reach every
  function whose address is taken (every closure entry and every method-table entry): a call
  recurs when its callee is in the caller's component. Such a call needs a poll only where no poll
  has run yet in this call of the function on every path to it, so every recursion polls once per
  level, on the paths that recurse: `fib` polls before its first recursive call and its leaves never
  poll. A function that no recursion passes through runs for a bounded time between its callers',
  its callees' and its own loops' polls, and never polls at all for its calls. A poll is a relaxed
  atomic load of the flag and a branch, weighted as almost never taken, to a call of
  `avin_rt_safepoint`, which is declared cold and laid out of line; the thread parks there until the
  collection is over. Every reference is already in a frame wherever a poll is placed.
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

An intrinsic that is not a string operation is lowered to a function whose body calls one runtime entry point, named in the
checker's `intrinsic_entry` table with its parameters and result. The formatting library
(`format.avin`) reaches the runtime this way: `write_out` and `write_int` write to standard output,
an integer's digits rendered on the stack; `render_int`, `render_real` and `render_char` make a
value's text; `quote_text` and `quote_char` its quoted, escaped literal; `text_size` and
`text_width` count a string's bytes and characters; `text_append` and `text_take` keep a
`StringWriter`'s text in a string object of its own whose length is its capacity, filled in place
and doubled when full, and copy the written part out.

**Built-in types conforming to traits.** An `impl Trait for int` records a conformance, and the
checker builds a method table for each built-in type, as for a data type, but holding only the
impl's methods: a built-in value is no object, so it can neither carry a table nor take a trait's
default bodies, which are compiled once for objects. A bounded call in an instance whose type
argument is a built-in type finds its method in that table and calls it directly.

**The integer family.** `Integer` is a core trait the compiler gives its members: every `Int(bits)`
meets it in `meets_bound`, and `refuse_closed_trait` rejects any impl of it and any `:` naming it. An
impl in module `avin` whose target is its own one parameter, bounded by `Integer` alone
(`impl[T: Integer] Display for T`), is resolved by `resolve_family_impl` onto the **integer family**:
the type parameter `TypeVar(integer_owner(), 0, "T")`, which the impl and all its methods share as a
data type's methods share its owner, and which `scope_bounds` bounds by `Integer` wherever it is
named. Its conformance is a `BuiltinConformance` whose type is the family, and `builtin_covers`
lets such a conformance, and a method whose receiver is the family, stand for every integer type:
in `builtin_traits`, `meets_bound`, method lookup on an integer receiver and the built-in tables. A
trait so implemented has no other impl for any width. The family's conformances get a table of
their own; a width with impls of its own (a program's trait for `i5`) has a table holding both, the
family's methods matched against the trait as the family sees them. A call of a family method takes
the receiver's width as its first type argument (`receiver_type_args`), and lowering does the same
when it finds the method in a table (`implementation_args`), so each width is an instance whose
operators are the machine's and whose conversions (`long(x)`, `T(n)`, checked by
`integer_param_named` and `as_integer`) resize to the instance's width. A width with no impl of its
own takes the family's table in `builtin_table`.

**Rendering an interpolation.** `s"…"` is checked into a join of its pieces with `+`. A hole of a
built-in type becomes a `Render` node, lowered to the runtime's rendering of the value (a constant
for `bool` and `unit`); a hole of any other type, a type parameter's included, becomes a call of its
`toString()`, which needs the type to implement `Display`.

A built-in type's impl may declare a method as an intrinsic; its declared type takes the receiver
first. `impl string` in `library/avin/string.avin` does so for the string operations that read the
object itself: `byteLength` (the length word, loaded and narrowed to `int`), a private `byteAt` (a
checked byte load), `slice`, `repeat` and a private `find`, the last three over the runtime's
`avin_rt_string_slice`, `avin_rt_string_repeat` and `avin_rt_string_find`, and a private
`parseReal` over `avin_rt_string_parse_real`, which reads the number with Sysl's parser and answers
not-a-number for a refusal. A direct call of one is
lowered where it is made, so its checks (an offset out of range, an offset inside a character, a
negative count) trap at the caller's line; the intrinsic's function body, used when the method is
passed as a value, reports them at its declaration. The runtime's `avin_rt_new_text` refuses a
string longer than the largest `int`, so every byte offset is an `int`. The other string methods are
Avin over these, and casing, replacing and the white-space test are Sysl bindings
(`sysl.text.to_upper`, `to_lower`, `replace_all`, `sysl.unicode.is_space`).

**Arrays.** `Array[T]` is a final class in `library/avin/array.avin`, conforming to `Iterable[T]`,
whose objects are not laid out from fields: an array is its length, a 64-bit word, then its
elements, each at its element type's store width (a power of two up to a word for integers, a byte
for `bool`, a word for a real or a reference, none for `unit`), with no gaps. Each instance's
descriptor gives one element's size where a class's gives the object's, lists no reference
offsets, and carries the class's method table and type record as any class's does; when the
elements are references its kind's tracer is the runtime's `avin_rt_trace_array`, which marks every
element, and otherwise the ordinary tracer, which marks none. The checker recognizes the class by
its module and name (`is_array_class`); it is the one class whose methods may be intrinsics, and its
constructor cannot be called. `length`, `apply`, `update` and `slice` are intrinsics lowered at each
call: the length word, then for an element an unsigned compare of the index against it (so a
negative index is out of range too, trapping with `index out of range` at the caller's line), the
element's address as the header plus the sign-extended index times the size, and a load or store.
A private generic intrinsic, `newArray[T](n)`, traps on a negative length and calls
`avin_rt_new_array` with the instance's descriptor, which allocates the zeroed object and stores the
length; only `Array.fill`, `tabulate` and `empty` call it, and they set every element before the
array is seen. `avin_rt_array_slice` copies a range of elements into a new array of the same
descriptor. `xs(i)` on a value whose type has an `apply` method calls it; `xs(i) = v` is checked as
`xs.update(i, v)`, and the compound and yielding forms hold `xs` and the indices in hidden locals.
A private intrinsic `fail(reason)`, of type `Nothing`, traps with `reason`: a literal, as `Range`
and the library's iterators give, or a string made at run time, as the assertions give. Being the library's code, it reports the program's call that entered
the library (the caller site above).

`FrozenArray[+T]`, private to the core library, is an array seen through a type that only reads it
(`is_array_storage`): its `length`, `apply` and `slice` are the array intrinsics, and
`freeze(a)` (`array_freeze`) is the array itself, so a `Vector` keeps its elements unboxed with no
copy and stays covariant. `clearElements` (`array_clear`, `avin_rt_array_clear`) zeroes a range, so
a buffer's vacated capacity keeps nothing alive.

**Sequence literals** (`sequence_literal` in `check.sysl`) are checked against the expected type. A
`List` is built as `::` chains (`list_literal`). Otherwise the collection is the expected class
whose companion object has a `builder` method, or else the core library's `Vector`, looked up by
qualified name so a program's own `Vector` never takes its place. The literal becomes a block: a
hidden local holding `Companion.builder()`, one `add` call per element, and `result()`. The result
type is unified with the expected type, or with the instance of the expected trait the collection
is (`ancestor_type`), which settles the element type, and the elements are then checked as `add`'s
arguments. With no element type expected, each element is checked alone and their types are joined
as `if` branches are, the narrower references passed as the joined type. The builder's open type
argument is recorded with a message about empty literals, so a later use may settle it.

**`for` loops** are checked as the `while` loops they mean, in the manner of `do … while`: the
source is evaluated once into a hidden local, and the element is bound by the pattern at the top of
each iteration, after which the loop steps, so `continue` goes straight to the test. A range written
in place keeps its bounds in locals of their integer type and counts with a wrapping add that is
only taken below the upper bound, so it makes no `Range`, no call and no allocation per element;
a `Range` value is counted the same way from its fields, its step's sign choosing the comparison and
a wrapped step ending the loop; an array is indexed from 0 below the length read once; any other
source with an `iterator` method is traversed by `hasNext()` and `next()` on the iterator it hands
out, dispatched through the trait. `loop_tests.sysl` holds the counts of calls and allocations each
form makes.

**`for` with `yield`** is parsed as `ForYield`, its generators each holding their pattern, source and
guards, and checked as the calls it means (`for_yield_expr`). The first source is checked; each of
its guards becomes a `withFilter` call on what came before, and the whole is called with `map` (the
last generator) or `flatMap`, each given a lambda of one hidden parameter whose body binds it by the
generator's pattern, as a loop's element is bound, and goes on with the guard, the yielded value or
a `ForYield` of the remaining generators. Each call goes through the method resolution a written call
uses, overloads included, with the comprehension's expected type passed to the outermost one; a
receiver without the method is reported at the generator's source. The lambdas are named apart so a
diagnostic says "this guard" rather than "this lambda", and `return`, `break`, `continue` and `?`
in them are reported as unable to leave the comprehension.

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
- **Tooling tests** (`runner_tests.sysl`) drive `avin run` and `avin test` through the command line,
  with the program's output sent to files: forwarded output and exit status, traps, the cache's reuse,
  the runner's report and its filter.
- **Library tests** will be Avin `@test` functions in the core library; `avin test` runs a program's
  tests, and does not yet run the core library's own.
- **Semantic references**: where Avin defines arithmetic as Sysl does, a test compares against an
  independently compiled Sysl program.
