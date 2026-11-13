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
| Lex and parse | source → AST | Indentation layout and grammar, on `sh.sysl.parsing`. Keeps spans; does no name lookup. A name in operator position, after a complete operand, is an infix method call: the parser sees it as an operator token, so keywords are the only words it has to tell apart. An operator is the longest run of operator characters; a run that is not one of the language's operators is a method's name called infix, and `++` or `--` is one only when an operand follows it on the line. |
| Load | `avin.hocon` → module graph | Packages, modules and files; the reserved `avin` root; the prelude, which is an Avin file. |
| Resolve | AST → resolved AST | Scopes and symbol tables. Every name is bound to a declaration symbol, and imports, visibility and shadowing are decided here, once. |
| Check | resolved AST → typed HIR | A `Type` algebra over declaration symbols; local bidirectional inference with subtype constraints; bounds, variance and `impl` coherence; initialization; what `@sendable`, `@isolating` and `@isolated` arguments capture and read, and the module-level state the first two reach through the whole program, once every closure's captures are known; pattern exhaustiveness; a finite set of instances, rejecting generic cycles whose type arguments grow. |
| Bindings | `@sysl` declarations → shim archive | Generates the Sysl shim source and its boundary signatures, and has Sysl check and build it. |
| C unit | `@c` declarations → C unit object + measurements | Generates one C file of header checks, measurements and glue, compiles it with clang under the C libraries' flags, and reads the measurements out of the object ([stage 13](/compiler/roadmap/)). |
| Monomorphize | typed HIR → mono HIR | Instantiates functions, types and generic trait methods for every use, and computes the dispatch for trait-typed calls over the closed world. |
| Lower | mono HIR → MIR | Explicit control flow (a deferred statement lowered again at every edge that leaves its block), object layouts and headers, type descriptors, vtables, closures and environments, allocation, GC root slots, traps. |
| Emit | MIR → LLVM IR | Spelling only: no typing, layout or semantic decisions. |
| Link | `.ll` + runtime + shim → executable | The module cut into codegen units, each compiled by its own clang at once and its object cached by its text; then clang links the objects, the runtime library, the cached shim library, the C unit's object and the C libraries' link flags. |

Each stage has its own data structures and its own tests. No stage reaches back into an earlier stage's
input. **Types are an enum over symbols, never strings**: built-in types, nominal types, functions,
tuples, type parameters, inference variables, `Any`, `Nothing`, unions and an error type. A union is
kept in normal form (`union_of`) and checked by its members; lowering lays it out as an `Any`
(`erase_unions`), and a pattern on one is a `PUnion`, a part for each member it applies to.

**Loading a program.** `avin build <dir>` takes a directory as the program's root: every `.avin` and
`.lavin` file under it is read in path order, passing over directories whose names begin with `.`, and each is
given the module its directory's path names, the root directory's files last as the root module.
A file whose header disagrees with its directory, or that has none outside the root, is reported
before anything is checked. `avin build <file>` is a program of that one file, in the module its
header names. The standard library is loaded the same way, from `library/` under the toolchain's
root, where every module is `avin` or under it; `library/avin/prelude.avin` is one ordinary file of
module `avin`. A path in a package, a directory with an `avin.hocon` found by walking up, is the
package: `package.sysl` reads the manifest with Sysl's HOCON reader, resolves the dependency graph
(the highest version asked for of each git coordinate, fetched into the machine's cache and checked
against `avin.sum`), and the loader walks each dependency's tree without its root files and `tests/`,
then the package's own; a directory holding its own manifest is passed over. The files and their
modules are what the later stages see; a dependency's `@test` functions are dropped from the program
after checking, so `avin test` runs only the package's own.

A [literate](/spec/declarations/#literate-source) `.lavin` file is read by the parser as its program
laid over the document (`literate.sysl`): the parsing package's `tangle_literate` decides which lines
are program text and refuses a tab in an indentation or an unclosed fence, and every other byte
becomes a space, so the program has the document's bytes at the document's offsets. The lexer
measures indentation past the four-column margin. Every later stage reads that program as its
unit's source, and every span, line and column is the document's with no mapping.

Names are resolved by module. Every top-level declaration is held under its qualified name, `a.b.X`
(a root module's under `X`), and a bare name in a file resolves to the first of its own module's
declaration, its file's import, and the `avin` module's public declaration that exists. Imports are
checked once every declaration of the program is known, and a wrong one is dropped. Object-file
symbols carry the module path, so two modules may each declare `helper`.

## Running and testing

`avin run` and `avin test` build through the same pipeline as `avin build`, into a cache of the
program's own in the user's cache directory: `build/<name>-<key>/` in the toolchain's cache,
`~/Library/Caches/avin/<version>/` on macOS, `$XDG_CACHE_HOME/avin/<version>/` or
`~/.cache/avin/<version>/` elsewhere, or the directory `AVIN_CACHE` names. `<name>` is the program's
directory's name, or its file's without the extension, and `<key>` twelve hex digits of a hash of its
absolute path, so two copies of one program, such as two worktrees of a repository, never share a
cache, and a compiler of another version never reads this one's. Nothing is written beside the
program, and a program of one file run from anywhere leaves its directory as it was. Only on a
machine with no cache directory (no `HOME`, no `XDG_CACHE_HOME`, no `AVIN_CACHE`) is the cache
`.avin/build/` in the program's directory instead, or beside the file for a program of one file.
`avin clean [<program>]` removes the program's cache, and the `.avin/build/` beside it if there is
one; `avin clean --all` removes every program's cache this version keeps, and leaves the fetched
packages, the runtime's archives and the shims. `avin build` is not a cache: it writes the
executable where `-o` says, or in the working directory. Beside each
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

## Incremental builds

A rebuild after an edit does again only the work the edit can change. The design follows where a
build's time goes ([measurements](/compiler/roadmap/#incremental-builds-build-plan)): on a program of
some size, lowering (monomorphization, devirtualization and scalar replacement) is most of an edit's
rebuild, emission and one codegen unit's clang come next, and checking the whole program is a few
seconds. So the unit of reuse is **the instance**, a function at one list of type arguments as
lowering makes it, and the front end stays a whole-program pass that runs on every build.

**What is kept.** Three caches, from the bottom up:

| Cache | Holds | Key | Where |
| --- | --- | --- | --- |
| Unit objects | each codegen unit's object | the unit's text, clang's version, the level | the toolchain's cache (exists) |
| Instances | each instance's finished LLVM text, the instances, records, strings and trap sites it requests, and the summary the whole-program passes read of it | the instance key, below | `incremental/<kind><level>/` in the program's build cache |
| Executable | the linked program | the build key of [running and testing](#running-and-testing) | the program's build cache (exists) |

Instances are stored a file per source module, holding the instances of that module's functions
(the functions lowering makes are the module `<generated>`'s), so a module whose functions'
instances are all reused is a module file read and not written again.

An instance's entries are two. Its **lowered entry**, under its own key (the key's first part,
below, which is known when the worklist reaches the instance), holds the requests lowering it made
of the whole program, in order, then its code. A request is everything lowering an instance asks of
or tells the program: a layout, a record, a box, a string, a trap site, another instance, a method
table slot, a closure, a constant object, a call through a table, a type tested, a call of a function
value. Each is made through one function of lowering, which records it while the instance is lowered
for the cache, every type in it spelled by qualified names; only the outermost request is recorded,
since replaying it makes the ones inside it again. In the code, every number that counts the
program's functions, descriptors, records, strings, constants, closures, trap sites, module-level
values or table slots is replaced by the name of what it counts. A later build that reaches an
instance whose own key has an entry replays the requests, so the instances and tables grow exactly as
lowering the instance would grow them, and reads the code back by those names; an entry naming a type
or function the program no longer has is read without being made and lowered fresh instead. A trap
site inside the instance's declaration is kept counted from the declaration's start, so a function
that moved keeps its entry and its sites report their new lines. Only the program's own functions
with bodies get lowered entries: closure entries, bindings and the functions lowering makes are small
and lowered every build. Its **text entry**, under its finished key and the facts emission reads of
the whole program for it (for each call, the callee's symbol and signature, whether it may collect
and whether the call may close a cycle of the call graph; a shim's extensions), holds its LLVM text
and the declarations the text needs.

**The instance key.** An instance's text is reused only while everything it was made from is the
same. The key is a hash over:

- the compiler (its version and its executable's path and time) and whether the build is for tests;
  lowering is the same at both levels, so the level is the instance directory's, not the key's;
- the decisions every instance shares: whether the program keeps its pointer-shaped options as
  pointers, which lowering decides for the whole program;
- the instance's identity: its symbol, the function's qualified name, overload and type arguments,
  every type spelled by qualified names (never by the checker's indices, which an edit elsewhere
  moves); a function lowering makes is known by its kind and type;
- the function's **body fingerprint**: its typed tree printed in a canonical form, which carries
  every callee, field and conversion the checker resolved and every type it inferred, so an edit to
  another function's signature that changes what this body means changes its fingerprint. Each node's
  place is counted from the start of the declaration its trap sites are named by, so lines added
  above it change nothing and a comment inside it changes it; a constant's place, which names no
  trap, is left out. A local function or lambda shares its top-level function's table of locals, so
  each fingerprint also holds what every function of that family captures (which decides the locals
  kept in a box), and nothing else of a sibling's body;
- the **interfaces it reads**: each callee's signature and annotations; the layout of every type it
  makes, reads a field of or takes apart by variant, its fields' types in order (never their names,
  since a field is reached by its place); the supertypes and method table of every type it tests
  against and of every type argument, which choose what a call on a bound runs; and each module-level
  value it reads, with, for one made at compile time, the fingerprint of its initializer;
- the **world facts** the whole-program steps used on it: the slots lowering gave the methods with
  type parameters of their own it calls through a table; what devirtualization decided at each of
  its calls through a table (left, made direct to a method, or guarded by descriptors and their
  methods); and what scalar replacement read of the others (each callee's summary, and where the
  function has fences, whether each callee may return with a store not yet fenced), with the key of
  every function it inlined.

A function lowering makes for a type reads that type's supertypes and table, and, for equality and
hashing field by field, its layout and its fields' types' supertypes and tables. The keys are made
on every build in two parts: an instance's **own key** (the compiler, the shared decisions, its
identity, its body fingerprint and the interfaces it reads) as the worklist reaches it, and its
**finished key** (its own key and its world facts) after the whole-program passes; the build log's
`keys` line counts them. The compiler's tests
build a program of three modules, edit it in each way the design names and compare every key, the
library's included: the keys that change are exactly those listed, and every function whose emitted
text changed has a changed key.

**Closed-world monomorphization stays correct** because the set of instances is computed on every
build, by the same worklist as today: a reused instance contributes the requests it recorded
instead of being lowered, so the set grows and shrinks exactly as a clean build's would. An
instance no longer requested is not emitted; a new one is lowered fresh. **Devirtualization and
scalar replacement** stay whole-program passes, run on every build over every instance's lowered
form. Devirtualization's whole-program part runs over summaries: each function's summary is, for
what it returns and for the object of each of its calls through a table, the set of what it may be
(an object of one descriptor, what one function returns, or unknown), found once from the function
alone, and the fixed point of what each function returns is found over the summaries; only a function
that calls through a table is rewritten. The passes run over every instance rather than only where
an instance's world facts changed, which needs a key over the cone of the call graph a function's
passes read (see [decisions](/spec/decisions/#proposed)).

**Stable names.** The emitted text names everything by what it is, never by the order the checker
or lowering found it in, so one new function, string or type renames nothing else. Each table's
symbol is `@"avin.<kind>:<name>"`:

| Kind | Named by |
| --- | --- |
| a function | its module, name, overload and type arguments; a local function or lambda by its top-level function and its number among that function's local functions of its name (`<program>.pick.lambda#2`) |
| a type in a name | its qualified spelling: `avin.Option[int]`, `class geo.Box[]`, `object geo.Box[]`, `(int, string)` |
| `str` | the first 16 hexadecimal digits of the SHA-256 of its bytes |
| `desc`, `methods`, `obj` | the type it lays out, a variant after its type's (`avin.Option[int].Some`); `box of <type>`, `cell of <machine type>`, `closure of <function>`, `string` |
| `type` | the type the record describes |
| `fn` | the function of the constant closure |
| `data` | a shared constant object by a hash of what it holds; any other by its module-level value and its place among that value's objects |
| `trap` | the innermost top-level function or method declared around it, the byte offset from that declaration's start, and 12 hexadecimal digits of a hash of its reason |
| `global` | the module-level value's qualified name; one holding a reference is an alias of its slot of the roots frame |
| `c.trampoline` | the binding passing it and its place among that binding's |

Emission stops with the name where two things of one kind would share one, a defect of the naming
the assembler would otherwise report as a redefinition. The module-level tables, every global and
alias, are the last codegen unit, and the functions are spread over the others; a unit holds its
pieces in the order of their names, and a trap site, which holds its line, is never copied into the
units that reach it. So a function or string added to a program recompiles its unit and the tables'
unit alone. The tables are made from the instance set on every build.

**The `-O0`/`-O2` split.** Each level has its own instance directory; nothing is shared between
them, since the passes and the copies made for inlining differ. A build at one level never trims the
other's.

**Two builds at once.** Every entry is content-addressed, so two builds writing one key write the
same bytes. A module file is named by a hash of its module and the keys it holds, starts with its
format, the length and the CRC-32 of what follows, and is written under a temporary name and
published by rename; a reader that finds one missing, short or failing its checksum passes over it,
its entries misses, and the build writes it again. A file the build read whole is touched rather
than written. The manifest naming the module files of the build is written last, the same way. No
lock is taken: whichever manifest is published last names files the other build left in place. The
executables `run` and `test` keep beside it, and their records, are published by rename too. A
build trims the module files that its manifest does not name and that no build used in the last
hour, and any temporary file left that long, as the unit cache spares what is fresh; there is no
size limit.

**On by default.** Every command that builds a program reads and writes the instance cache: `avin
build`, `avin run`, `avin test` and `avin emit-llvm`. It is kept in
`incremental/<kind><level>/` in the program's build cache, beside the executables `run` and `test`
build ([running and testing](#running-and-testing)): `program-O2` or `tests-O0`, since a build for
tests is another program, with keys of its own, and the two kinds never share a file. Every program,
a program of one file too, has a build cache of its own, so two programs never trim each other's
entries. On a machine with no cache directory, where a program of one file shares its directory's
`.avin/build` with every other file there, its directories are named after it (`hello-program-O2`
for `hello.avin`) for the same reason. A run
that finds its executable already built reads no cache at all. `avin check` keeps none, and the
language server only checks, so it builds nothing to keep.

`AVIN_INCREMENTAL` chooses otherwise: `off` uses no cache and reads none that is there; `verify` uses
it and then builds the program again without it, and stops with the first function whose text
differs from the clean build's; `on`, unset or empty is the default. Any other value is an error for
the commands that build.

A cache needs no care. Another compiler (a newer release, or the same one built again, since the
compiler's executable and its time are in every key) finds no entry it can use, builds as from
nothing and writes its own; the files it does not name are trimmed an hour after their last use. A
module file that is short, damaged or missing, and a manifest naming anything but a module file of
its own directory, are passed over and written again. Deleting a build cache, with `avin clean` or
by hand, is always safe. Its size
is about what the program's text is: on marshal (7,400 lines, with its packages and the library) a
level's directory is 22 module files and 185 MB, so building and testing it at `-O2` keeps two of
those.

**The build log.** A build reports what it did when `AVIN_BUILD_LOG` names a file, or `-` for the
error channel. The file is emptied when the build starts, so it holds that build alone; a file that
cannot be written is an error before any work is done. Unset or empty, nothing is written and no
clock is read. Each phase writes one line, `phase=<name> ms=<n>` followed by its counts as
`key=<n>` words, in this order:

| Phase | Counts |
|---|---|
| `load` | `modules`: the modules read, the library's included |
| `check` | `modules`: the modules checked |
| `cunit` | none: the C unit of the program's C bindings |
| `cache` | with the instance cache only: `files`, `damaged`, `lowered`, `texts`: the module files the manifest names, those passed over, and the lowered and text entries read |
| `lower` | `functions`, `strings`, `descriptors`, `tests`, `reused`: the last, the instances whose code came from the cache |
| `keys` | `instances`, `bodies`, `distinct`: the instance keys made, one per function lowered; the bodies fingerprinted, a generic function's once for all its instances; and how many keys differ |
| `shims` | `bindings`, `handles`: the Sysl shims built |
| `emit` | `bytes`, `reused`: the size of the LLVM text, and the functions whose text came from the cache |
| `store` | with the instance cache only: `written`, `kept`, `trimmed`: module files written, those left as they were, and those trimmed |
| `split` | `units`: the codegen units the module was cut into |
| `units` | `units`, `compiled`, `reused`: units clang compiled, and units taken from its cache |
| `link` | none |

A run that finds its executable already built writes the one line `phase=cached ms=0 built=0`.
The keys and phase names are stable; later phases add lines and keys, never rename them.

**Testing it.** A build reports what it did through that log: each phase's time,
and the counts of instances reused, lowered and passed again, modules reused and units compiled.
The compiler's tests build a program of several modules in a temporary directory and edit it, each
edit with the counts it must give: a body edited in one module lowers only that module's instances
and recompiles only the units holding them; a signature edited lowers its callers too; a class
added that implements a trait passes again the instances that call through it and lowers nothing
else; a field added lowers every user of the type; an instance no longer requested is gone from
the output. **The oracle is a clean build**: for every edit, the incremental build's LLVM text is
the same as a build with no cache, so a key that misses a dependency fails a test rather than
shipping a stale instance. `AVIN_INCREMENTAL=verify` builds with the cache and again without it and
compares the two texts, naming the first function where they differ. The compiler's tests hold each
edit of the matrix to lowering exactly the instances whose own keys no earlier build made an entry
for, and to a clean build's text; a cache whose files are truncated, damaged or missing, or were made
by another compiler, gives a clean build's text and is written again; two builds of one program at
once both succeed and leave a cache a third build verifies; and a sample of the corpus builds warm to
a clean build's text with every instance taken from the cache. Through the commands themselves, they
build, run and test a program, edit it, and hold each command's rebuild to a clean build's program and
text, with `verify` passing; hold `off` to reading and writing nothing; survive damaged files, a
manifest of nonsense, another compiler's cache and trimming; and run two `avin test`, and an `avin
run` beside an `avin test`, of one program at once.

**The corpus, warm.** With `AVIN_INCREMENTAL=verify` in the environment of `sysl test compiler`, the
execution corpus is built warm as well as cold: each program is built once with an empty cache,
edited (a function nothing calls is added at the end of its first file, which changes the file and
no line of what the program does or reports), and built again from that cache in `verify` mode, so
every program's warm build is compared with its clean one; the warm build is the one run. `avin
test` of the library's and the packages' tests runs in `verify` mode the same way. Unset, the gate
builds the corpus once, with no cache.

## Sysl bindings and the shim cache

A [binding](/spec/sysl-bindings/) is checked as a top-level function without a body: its
`@sysl("module.name")` must name a Sysl function by module path and name, and its parameters and
result must be boundary types — integers of 8, 16, 32 or 64 bits, `byte`, `real`, `char`, `bool`,
`unit`, `string`, data types declared with `@sysl("module.Name")`, which mirror a Sysl enum or
struct, and `Array`, `Vector`, `Option` and `Result` of boundary types. A mirror's fields are held to the
boundary types once every variant is declared, and a mirror that holds itself is rejected. Lowering
gives a binding a body that passes its parameters, after the context, to a shim symbol, `avin_shim_`
followed by the binding's own symbol with every character other than a letter or digit written as
`_` and two hex digits, and returns what the shim returns. A result the shim makes objects for
follows the parameters with its **layouts**: the descriptor of each variant of each data type in it,
or the variant's one constant object, an array's descriptor, and for a vector the descriptors of
its array and of the vector, in an order the binding's `Crossing` records. Since it is an ordinary function, a binding can be passed as a value. The MIR
program lists every binding of the functions it compiles, with how each value crosses.

For a program with bindings, the driver generates one Sysl module, `avin.shims`, in a package whose
manifest depends on the runtime's package and on every Sysl package that the `sysl-dependencies` of
the program's packages name: loading a package gathers them over its whole dependency graph, a
directory made absolute and two releases of one name reduced to the higher, and the program carries
them to the build. Each binding gets an import of its Sysl function under a
private name, and an exported shim that converts each argument (a `bool` arrives as a byte, a `char`
as its code point, a `string` is copied into a Sysl `string`), calls the function with the result
declared at the binding's type, and converts the result back (a `string` is copied into a new Avin
string through the runtime's `avin_rt_new_text`). An `Option`, a `Result` or a mirror has one
conversion function per type and direction, shared by every binding: into Sysl it reads the variant
from the object's descriptor and each field at its offset; out of Sysl it matches the Sysl value,
allocates the variant by its layout and stores each converted field, keeping the object in a frame
of the thread's stack while its fields' objects are made. A mirror's integer field converts to and
from whatever integer type Sysl declares. An `Array` or a `Vector` of any element type but `byte`
is copied element by element: into Sysl each element is converted onto a `Buf` whose elements are
the slice the function is passed; out of Sysl a new array is allocated by its layout and kept in a
frame while each element is converted and stored, then a vector's object is allocated around it.
Since the Sysl function may answer a slice or a `Buf`, the conversion out of Sysl of a type that holds
a collection is one overload per spelling of its Sysl type, the `Buf` one taking the slice of its
elements, and the result is declared without a type so the function's own chooses the overload; a
call no overload takes is reported at the binding as the Sysl types the Avin type is made from. `sysl build-c` checks and builds the package into
one archive holding the runtime, the shims and the part of Sysl's library they reach, which the
program links instead of the runtime's own archive. One archive means one copy of Sysl's library, so
Sysl and Avin share standard output and keep their writes in order.

The C libraries the archive's Sysl packages need are not in it. `sysl build-c` reports them after a
build: what an `@link` named, and the `pkg_config` modules the packages require. The driver keeps
them in a file beside the archive, `<archive>.link`, one to a line (`lib sqlite3`, `pkg sqlite3`),
written before the archive is published; an archive found without it is built again. The link
passes each `lib` as `-l<name>`, leaving out `c` and `m`, which every program is linked with, and
then the words `pkg-config --libs` prints for the `pkg` modules, asked at every link.

**Codegen units.** clang compiles one module on one thread, so the link cuts the program's LLVM
text into codegen units and compiles them at once, one clang each (`codegen_units.sysl`):
one unit per processor, at most eight, none smaller than 256 KiB of text, and one module as before
for text under 512 KiB; `AVIN_CODEGEN_UNITS=<n>` sets the number, 1 restoring the single module.
`emit-llvm` still writes the one module. A function only one other function names is placed in
that function's unit, following a chain of them to its head, so it stays local and is still inlined
into its one caller; every other function is placed by a hash of its name. A global
lives in the first unit that names it, an alias with the global it points into, and a global nothing
names in the first unit. A local named from another unit is promoted: renamed `avin.cgu:<name>`, which
no Avin, Sysl or C symbol can spell, and given hidden linkage, visible to the link's other objects
and to nothing after it. At `-O2` a function whose body costs at most 32 instructions, and a
constant whose line is at most 2 KiB, is also copied as `available_externally` into each unit that
names it, so a call or a read across units can still be inlined or folded while the program keeps
one definition of each. A module holding `module asm`, a comdat or an `ifunc` (emission writes
none) stays whole. Each unit's object is kept under `units/` in the toolchain's cache, keyed by the
compiler's and clang's versions, the level and the unit's text, so an edit recompiles only the units
whose text it changed; a build that compiled a unit trims the directory to 1 GiB, oldest first,
sparing whatever was used in the last hour. The test corpus, which builds each program once,
keeps none.

A Sysl diagnostic is placed by its line in the shim: the driver records the line of each binding's
import and the lines of its function, and reports the message at the binding's name as `the Sysl
binding \`name\` does not match \`module.name\`: …`. A mirror's import and its conversions are its
own lines, reported at its declaration as `the Sysl mirror \`Name\` does not match
\`module.Name\`: …`. A diagnostic that falls elsewhere is reported with Sysl's whole output.

The archive is cached under `shims/` in the user's cache for this compiler version
(`~/Library/Caches/avin/<version>` on macOS, `AVIN_CACHE` when it is set), never in the toolchain's
root, which an installed compiler only reads. It is named by a SHA-256 of the manifest, which names each Sysl package
at its release or commit, the shim source, the `sysl --version` output, the runtime's source files,
each named by its path in the runtime, and the source files of each Sysl package named by directory;
a package's source files are its Sysl, manifest, sum and C files, outside its `.sysl` working
directory. The executables `avin run` and `avin test` cache are keyed by the same Sysl packages. A build with
the same bindings finds it there and runs no Sysl build; it is built under a temporary name and
published by rename, so concurrent builds never see a partial archive. The `check` command stops after type checking and builds no shims, so it is fast; `check --bindings`
builds them too, since only Sysl can say whether a binding is right.

## C bindings and the C unit

Built by [stage 13](/compiler/roadmap/); the language is in [C bindings](/spec/c-bindings/). A C
binding is **not** a shim: it is a direct call from the program's own code.

- **Check** parses each `@c` prototype into a C signature (scalars, pointers, `const`, typedef
  names, function pointers, and the `out`, `inout`, `userdata` and `length(p)` markers) and holds it
  against the Avin signature: which Avin type each C type may cross as, out values joined into the
  result, a callback's parameters without its `userdata`. It records each binding's **crossing**,
  as a Sysl binding's is recorded.
- **The C unit.** The driver writes one C file for the program, `#include`s the headers every
  package's `c-libraries` names (a package's after those of every package whose handle classes its
  bindings name, and an entry that is a C identifier as `#include NAME`), and emits per binding a `_Static_assert` that
  `__builtin_types_compatible_p(__typeof__(&symbol), <the prototype's function pointer type>)`, and
  one that the function its `free:` names takes a pointer and answers nothing; per typedef crossing
  as a string, that it is `char *` or `const char *` (only the latter as a parameter); per
  typedef in an integer position, its size and sign; per mirror, size, alignment and offsets; per
  enumerator and constant, its value. Measurements are stored in one `const` table of bytes after a
  marker, which the driver finds in the object file's data, so nothing is run, no object format is
  parsed, and a build for another target measures that target. `pkg-config` is asked through the
  toolchain's `pkg_config`, which a build for another target points at that target's. The same file defines the glue: a function per field accessor, expression
  `retain`/`release`/`forget`/`as`, buffer `length`, and per binding or callback that passes a struct
  by value (taking and answering it through pointers, so clang does the platform's aggregate
  classification). A clang diagnostic is placed back at the binding by the line the driver recorded
  for it. The object is cached under `cunit/` in the user's cache by a SHA-256 of the C source, the
  `pkg-config --cflags` and `--libs` answers and `clang --version`.
- **Lower** gives each binding a body in MIR: convert each argument (a string into a NUL-terminated
  copy in memory off the heap, `avin_rt_c_string_in`'s, freed by `avin_rt_c_string_free` once the
  result is converted, a `bool` into `0` or `1` for an integer used as a truth value, a checked
  integer narrowing to the measured width, `None` to null, a
  handle to its C pointer after the closed and owner checks, through its `as` glue where needed),
  call the C symbol or its glue, then convert the result (a string copied in and checked as UTF-8
  by `avin_rt_c_string_out`, an `Array[byte]` by `avin_rt_c_bytes_out`, then the `free:` function
  called on it; an integer used as a truth value compared with zero; null to `None` or a trap, a
  status to `Result`, a pointer
  to a new handle object, through `retain` when `borrowed`). A `blocking` call is bracketed by
  `avin_rt_enter_safe` and `avin_rt_leave_safe` after every argument is converted (an array argument
  copied off the heap) and before the result is. A handle object is four words: its C pointer (null
  once given back or taken by a `consumes:` call), its owner word, the list of C references it
  holds to the handles it keeps (off the heap, the glue's), and the list of Avin objects it keeps,
  pairs of `(Any, Any)` the collector traces. The C unit's glue defines per handle class `i`
  `avin_c_give_back_i`, which `close()` calls, `avin_c_finalize_i`, the descriptor's finalizer, which
  calls it, and with `retain` `avin_c_retain_i`; a class with `retain` and no `kept:` also gets
  `avin_c_keep_i`, which takes a C reference for a holder and one to each C reference the kept
  handle holds, given back in order after the holder's own `release`, so the collector's sweep order
  never frees a kept object first. A `kept:` name is the pointer of the first held reference of its
  class. Each `as:` entry `j` is `avin_c_as_i_j`, answering the other pointer (null for null) after a
  `_Static_assert` that its expression has the entry's type. A field accessor, by its symbol `n`,
  names its field's type `avin_c_field_type_n` (`__typeof__` of the field with its indices at zero,
  checked to be the kind its Avin type needs, and measured as a result's type is), reads or writes
  it through `avin_c_field_get_n` or `avin_c_field_set_n`, taking each index as a `long long`, and
  answers each index's bound through `avin_c_field_bound<j>_n`, which lowering compares with the
  index before the read, trapping unless `0 <= index < bound`. With `forget:`, `avin_c_give_back_i`
  runs it for each callback the handle keeps before `release`.
- **Callbacks.** Check reads a parameter of function-pointer type into the function pointer's own
  prototype, paired with the binding's `userdata` of the name its own `userdata` has. Lowering makes
  the binding's body call `avin_rt_c_callback_new` for each function it passes, a record off the heap
  holding the function value rooted as a Sysl callback's is (`Callback`), whose address is the
  `userdata`; the record is freed by `avin_rt_c_callback_free` once the call returns, or, for a
  function `keeps:` names, handed to the holder's kept list by the glue's `avin_c_keep_callback` and
  freed by `avin_rt_c_callback_drop` when the holder is given back, after `forget:`. For each such
  parameter, lowering also makes a MIR function taking the record and C's other arguments, which
  converts them as results are, calls the function value and converts its answer as an argument
  is; emit wraps it in the trampoline, `@avin.c.trampoline.<n>`, with C's convention, whose address
  is passed for the pointer: it calls `avin_rt_c_callback_enter`, which runs `callback_enter`
  (attaching a thread the runtime has never seen where the parameter is `@threaded`, stopping the
  program otherwise, and checking the owner), then the MIR function with the context it answered,
  then `avin_rt_c_callback_leave`. In a program with a trampoline, every call of a C library
  function is planned as one during which the collector may run, its live values rooted, since C
  may call back.
- **Buffers.** `ByteView`, `MutByteView` and `ByteBlock` (`library/avin/c`) are laid out by lowering
  as the runtime's `CMemory` (`runtime/avin/rt/cmemory.sysl`): address, owner, length, closed flag,
  the closed and foreign reasons and the lender, the last three traced. Their methods are `c_view_*`
  and `c_block_new` intrinsics, each a call of `avin_rt_<name>` with a trap site, which checks the
  lender's owner (a block's own), the closed flag and the bounds. A binding marked `view` (or naming
  `view:`) is glued like `array:`, the glue writing its `length:`; lowering allocates the view, fills it
  and stores it in the lender's fifth word, which every bound method, field accessor and `close()` of
  the lender clears after setting the view's closed flag. A block is passed as its address and length
  after `avin_rt_c_memory_check`, never copied. With `attach:`, the C unit defines
  `avin_c_attach_i(pointer, record)` over a static `key:` object and `avin_rt_c_attach_destroy`;
  lowering makes the record at a handle's first keep (`avin_rt_c_attach_new`, kept in the sixth word),
  gives it to C, and after each keep hands it the head of the handle's kept list
  (`avin_rt_c_attach_set`), which the collector marks while C holds the record; the handle's
  `avin_c_give_back_i` lets go of it with `avin_rt_c_attach_drop`.
- **Emit** declares each C symbol and glue function with the target's parameter attributes
  (`signext`/`zeroext` below 32 bits) and calls it; nothing in the emitter knows about C beyond that.
- **Link** adds the C unit's object and, for each package's C libraries, `-l<name>` or the words
  `pkg-config --libs` prints, asked at every link.

## Runtime

`runtime/` is a Sysl package, built into a library archive when its sources change rather than once
per program, and linked into every executable. The archive is named by a key over the compiler's
version and the runtime's sources: a release ships it prebuilt as `share/avin/lib/libavinrt-<key>.a`,
and otherwise it is built into `runtime/` in the user's cache, so a program that reaches no Sysl
binding is built without Sysl. It contains:

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
with no fields, closures that capture nothing, and the values made at compile time below) live
outside the heap as constant data with the same 16 bytes ahead of them, their descriptor and a word
left zero; the collector ignores any reference outside its heap, so it leaves them alone. Code names
a constant object by the address of its payload written out, `getelementptr inbounds (i8, ptr
@avin.data.3.const, i64 16)`, rather than by an alias: LLVM sees through the expression to the
constant, so a load from it folds, and so does a load from a `select` or `phi` of two of them.

**Values made at compile time.** A data value or a tuple built only of literals and other such
values is not allocated: lowering lays it out as its descriptor lays out an object, fields at their
offsets, and the emitter writes it as read-only data. Equal ones are one object, since a data value
has no identity, so `FormatSpec()` passed to every field of a derived `display` is one constant. A
module-level `val` whose initializer is built of such values, other such `val`s and vector literals
of at most 32 of them is made the same way and has no storage: nothing initializes it before `main`,
a read of it is the constant, and a read of a field of it the field's value. A vector literal is a
`Vector1` over a constant `FrozenArray` leaf (a `Vector0` when empty), laid out as the library lays
out the vectors it builds; since a vector has an identity, only a module-level value's literal,
which runs once, is made this way, and two equal ones are two objects. The checker keeps a
collection literal's elements beside the builder calls that make it (`Collection` in the typed
tree), so lowering can choose. A derivation's shapes are such values, so what a template reads of
them, a variant's index, a field's name, whether it has an annotation, is a constant in the
compiled code. `AVIN_GC_STRESS=1` makes every
allocation collect first, and the execution tests run that way, so a reference the compiler failed
to root is found at the first allocation that could lose it. `AVIN_GC_STRESS=2` also collects at
every safepoint poll, which finds a reference let go while a loop that allocates nothing still uses
an address computed from it, such as the elements of an array laid out for C.

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
types. The table is constant data placed before the class's descriptor, in one global, with one
word between them, the descriptor's call entry; so slot `s` is the word `8(s + 2)` bytes before the
object's descriptor; the descriptor itself keeps its layout and the runtime never reads the table.
Every descriptor has the call entry, the word directly before it: what a call of the object as a
function value runs, taking the object and then the call's arguments. A closure's is its code, so a
closure holds only what it captures; a class or data type that is a function (a sequence, a set, a
map, or any type naming a function type as a parent) has its `apply` there, filled once the program
calls a function value at all; any other object's is null. A call of a function value loads the
descriptor and the call entry and calls it indirectly, the same for a lambda and a collection. Each instance of a generic class has its own descriptor and table, filled with the method
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

**Devirtualization.** Once lowering has compiled every function and filled every method table, a pass
over the finished MIR (`devirt.sysl`) rewrites calls through tables wherever the whole program shows
which method runs. Each rewrite runs exactly the method the table would have given, so overrides,
open classes, impl-added methods and trait defaults keep their meaning:

- **Known receiver.** A call whose receiver's descriptor is known is a direct call of what that
  descriptor's table holds. A value's descriptor is known when it was allocated in the function, read
  back from a local slot whose every store is an object of that one descriptor (a slot whose address
  is passed on is never followed), or returned by a function whose every `return` yields an object of
  one descriptor. Function results are solved together, from "undetermined" down through "one
  descriptor" to "unknown", until nothing changes. So `val s: Shape = Rect(2, 3); s.area()` is direct,
  and so are `hasNext` and `next` on the iterator a sequence's `iterator()` makes: `for` keeps its
  settled meaning, a traversal through `iterator()`, and its calls become direct and inlinable.
- **One implementation.** A call is direct when every descriptor in the program of a type conforming
  to the receiver's class or trait, at any type arguments it can be seen through, holds one and the
  same method at the call's position. An instance whose type argument is a number, `char`, `bool` or
  `unit` where the receiver's is a different type, or the reverse, is left out: such a type has no
  subtype but `Nothing` and is never boxed inside a container, so the `Vector[Any]` a call of
  `println` makes never reaches a call on a `Vector[int]`. A trait with one implementing class, or an open class no subclass overrides, needs no
  table load. A boxed value's descriptor belongs to no class or data type, so it always counts as a
  method of its own.
- **Guarded.** Otherwise, when the descriptors of the types the receiver's exact static type admits
  hold at most 8 different methods (16 descriptors), each of at most 40 MIR instructions, the call
  reads the receiver's descriptor and compares it with each of theirs in turn; a match calls that
  method directly, and an object of any other descriptor takes the method from its table as before.
  The result meets in a slot of its own. Measured at `-O2`, a guard pays only when the methods are
  small enough for LLVM to inline: before a call that stays a call, the compares cost what the table
  load they replace saved. So a `Vector`'s `apply`, whose depth classes each walk their part of the
  tree, keeps its table call.

Calls through more methods, through larger ones, and on receivers of type `Any` stay as they were,
as does a call whose method returns nothing where the method it overrides returns a value (an
override yielding `Nothing`): its machine signature is not the call's.
Every function of the program is emitted with internal linkage, since nothing outside the module calls
one by name; that lets LLVM inline a method into each caller and drop the copy, which is what makes the
direct calls pay.

**Escape analysis and scalar replacement.** After devirtualization a pass over the MIR
(`scalar.sysl`) keeps objects that never leave the function that makes them as their fields. It has
three steps:

- **Inlining.** Lowering makes a data value, a tuple or a class's object with `Alloc`, so an object
  is made in its constructor, an iterator in a sequence's `iterator()`, and it is used in the
  methods it is passed to; nothing can be learned of it until those calls are in the function. A
  direct call is inlined where it makes such an object or is handed one, when the callee has at
  most 80 MIR instructions and cannot recur through direct calls. Two summaries of each function,
  solved for the whole program from "no" upward until nothing changes, decide which calls those
  are: whether everything it returns is an object it makes (or one an inlined call of its own
  makes), and, for each parameter, whether the function lets the object go anywhere but into its
  fields, its descriptor, a null test, its own return and the parameters of functions with the same
  property. A third summary marks the functions that return only parameters they are handed, a
  visitor's methods returning the visitor: a call of one is not a source the object could not be
  kept through, since once inlined its result is its argument. So the chains of a `match`'s arms,
  `visitor.variant(V).field(…).finish(V)`, are inlined call by call to the end, though they meet in
  one result and a short arm finishes first. Inlining runs in rounds, each exposing the calls the
  next inlines, up to 8 rounds and 2000 added instructions per function. An inlined body's parameters are the call's arguments, its
  values, slots and blocks are numbered after the caller's, and each return stores the result to a
  slot of its own and jumps to a block that loads it and goes on with the rest of the caller's block.
- **Replacement.** The references of a function are grouped: a value, the local slots it is stored
  to (slots whose address is only loaded and stored), the values loaded back from them, and both
  operands of a `select`. A group is kept as its fields when every reference in it comes from an
  `Alloc` in the function and every use reaches a field (an `Offset` used only as the address of a
  load or store, or the object's own address for its first field), reads the descriptor, or
  compares with null, and its `Alloc` is not of a handle, Sysl or C: a handle's finalizer gives
  back the reference it holds, and runs only when the collector frees the object, so a handle made
  and used up in one expression (`consume(newShared(1))`, `newObject(2)` discarded) stays an
  object. Anything else lets the object escape: a store into an object, an array or a
  module-level value, a call (inlining has already taken the calls that could be followed), a
  return, a capture (a store into a closure), boxing as `Any`, a call through a table or a function
  value, a comparison by identity, and so any hand-over to another thread. A group's objects must
  also never be in use two at a time: a forward pass over the blocks takes every reference of the
  group as current from where it is made until the next `Alloc` of the group, and current again
  where it is stored to or loaded from a current one; a use that may not be current on some path
  (an older object kept beside a newer one made by the same `Alloc` in a loop) keeps the whole group
  as objects. A kept group gets one local slot per field offset, of the field's machine type (a
  field holding references is a slot of reference type), and one for the descriptor where its
  objects are made with more than one descriptor and the descriptor is read. Each `Alloc` becomes
  stores of zero to the group's fields (and of its descriptor), each field access a plain load or
  store of the slot, a read of the descriptor the constant descriptor, and a null test a constant.
  The fields that hold references are local slots of reference type, so the frame plan roots them
  wherever they are live across a collection point, as it does any local variable, and a sequence
  only an iterator holds stays alive; the object itself has no header, no root and no atomic
  access, since no other thread can reach it.
- **Fences.** A `fence release` is needed only where a store another thread could see has been
  made since the last fence. A forward pass over each function's blocks drops every fence before
  which, since the function started or the last fence, there has been no store other than to a
  local slot or the context's caller site, no allocation, no atomic operation, no call through a
  table or a function value, no runtime entry point other than the quiet ones that store nothing,
  and no call of a function that may return with such a store unfenced (found for the whole program
  at once). So the fence that ended the making of an object kept as its fields goes, and a fence
  after a heap store that may be part of another object's making stays.

So `for x in xs` over a `Vector`, a `List`, an `ArraySeq` or a `Range` value keeps its iterator in
slots, `val (q, r) = divmod(a, b)` makes no tuple once `divmod` is inlined, and a derived method's
visitor costs nothing. A tuple returned by a function too large to inline, or a recursive one, is
made only for a caller that keeps it: a caller taking it apart gets its parts as words (below). A
`for` over an `Iterable` whose `iterator()` call stays guarded keeps its iterator on the heap.

**Values of several machine words.** A value of more than one machine word is one MIR value per
word, each of its own machine type: that many parameters, slots of a local, fields of an object or
a closure, and slots where branches meet. A result is the one place needing more: a signature's
`rest` lists the types of the words after the first, a call defines the first word as its value and
each later one with `CallWord`, and a return gives the first as its value and each later one with a
`ReturnWord` in its block, after anything that may collect. LLVM returns the words as one structure,
in registers. Since every word is an ordinary value, the frame plan roots exactly the words of
reference type that are live across a collection point and never a scalar word. A call in tail
position whose every word the block returns at its own place is made as a tail call, its whole
result returned; inlining stores each later word of a return to a slot of its own, which the
`CallWord`s load; devirtualization makes such a call direct but never guards it.

Lowering uses them for tuples taken apart from calls: `val (a, b) = f(x)`, where `f` is one of the
program's functions with a body (no constructor, trait method or binding) and the tuple has at least
two parts with runtime values, calls `f#words`, a copy of `f` answering with the tuple's parts. Each
of its returns reads the parts out of the tuple, which scalar replacement then keeps as its parts, and
a call in its tail position of a function yielding a tuple of the same words calls that function's
copy and hands its words on. A caller that keeps the tuple calls `f`, which still makes it.

**Unions with a scalar member.** `concrete` erases a union of references to `Any`, as which it is
laid out, and keeps a union with a scalar member, whose words are a 32-bit tag, 64 bits for a scalar
member and, where a member is a reference, the reference (`union_scalars` numbers the scalar members
from 1 by their type keys; tag 0 is a reference). Lowering keeps such a value as a reference to its
cell, an object of those words made once and only read, so every operation is written once, on the
cell: injection (`inject_cell`, from a member, a smaller union, or a value boxed into a reference
member), a type test by the tag and then the reference's record (`cell_test`), taking a member out
(`cell_member`), widening to a wider union (`recell`) and narrowing to a smaller one
(`narrow_cell`), conversion to and from an `Any` (`cell_as_any`, `any_as_cell`), `==` and the hash
by the tag, and `display`, `debug` and `toString` by the member's own implementation. A signature
names such a parameter or result `MCell`; `union_words` (`union_words.sysl`) then gives each
function one parameter per word, a cell made from them at its entry, returns of the words read out
of the returned cell, and each call its arguments' words and a cell made of the words it gets back,
except a call whose value the block only returns, which passes the words straight on. Every cell of
a function is then made in it and only read, so scalar replacement keeps it as its fields, and a
union in a local, a parameter or a result allocates nothing. Storage holds the words themselves:
`storage_type` gives such a union the machine type `MCell` wherever a value outlives a call (a field
of a data value, a tuple or a class, an array element, a closure's capture, a captured `var`'s box,
a module-level value), 24 bytes with a reference member and 16 without, and the descriptor's
reference offsets, or an array's element map, list only the reference word. `store_at` reads the
words out of the cell and stores each, and `load_at` loads them and makes a cell of them, which
scalar replacement keeps as its fields again, so storing a union and reading it back allocates
nothing. A load believes a tag of 0 (a reference) only beside a non-null reference word and reads
any other zero tag as the first scalar member's, so a racing read or an array element not yet
filled never yields a null reference. A module-level union keeps its tag and bits in 16 bytes of
storage and its reference word in a module-level reference of its own, among the roots
(`global_reference`, named `#reference`, after the `lazy val` state words). An `Atomic`'s field
alone stays a reference to a cell (descriptors `union cell` and `union cell of scalars`), one word
its operations access indivisibly. A tuple with a union part is never answered as words
(`tuple_words`): the call makes the tuple. An integer member wider than 64 bits is no scalar of
the bits (`is_boxed_member`): it is boxed and kept in the reference word under tag 0, its type
tested by its box's record, so such a union has a reference word. The checker converts a union with a scalar member to
an `Any` or a trait with `Box`, as it does a scalar; a member call through such a trait boxes the
receiver too, keeping it a variable so `dispatch` sees the union and renders it by its tag.

A tuple type pattern is tested by the tuple's record: each record of a type kept as a reference to
an object lists `Any`'s record first among the ancestors a tuple's part is seen through, so a
`(string, int)` is an `(Any, int)`, and a tuple type with an `Any` part is never tested by its record
alone. A part naming a union of references, erased to `Any` in the record, is then tested on the
object the tuple holds there (`union_part_tests`).

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

**Options as pointers** (`representation.sysl`). An `Option[T]` whose `T` is always a non-null
reference to an object with a descriptor is kept as one pointer: `Some(x)` is `x` itself and `None`
is null, so making either allocates nothing and a match compares with null. `T` is then a class, a
trait, an object, an `Array` or another class instance, `Any`, a string, a tuple, a function, or a
`data` type other than `Option`. Every other option stays an object: one of a built-in value type
(`int`, `real`, `bool`, `char`, `byte`, `unit`) and one of an option, since `Option[Option[C]]`
cannot spend null on both of its `None`s. `Option[Nothing]` has only `None`, which either form
holds, so it takes the form of the options it is seen as inside other values (below): a pointer,
null, unless one of them is an object. The choice is lowering's (construction,
variant patterns, structural `==` and hash, which compute exactly what the object form computes),
and the program cannot tell:

- Where such an option must be an object, lowering makes the object it stands for, laid out as every
  option of its type is: `None`'s constant object, or a new `Some` holding the pointer. That is a box
  to `Any`, a type pattern that tests it as an object or finds it as a trait, a call through a method
  table, and the receiver or a `Self` argument of a trait's default body, which is compiled once for
  objects (a `Self` result is taken back). Where an object becomes such an option (a type pattern's
  binding, a record's equality entries), lowering takes the pointer out of it again.
- Only that object has a method table. Its descriptor's table holds, for each method of the type, an
  adapter (`avin.adapter.…`) that takes the object where the trait's method takes `Self`, calls the
  method with the pointer, and gives back an object for a `Self` result; a default body sits there
  directly.
- A binding's shim converts it with `OptionForm(pointer)`: null is `None`, any other pointer the
  `Some`'s value, converted as its type is.
- A null is no root and no field the collector follows: it ignores every reference outside its heap,
  null included. Making an option allocates nothing, so it publishes nothing, and the memory model is
  unchanged.

A **view** takes a value as another type with no conversion, and the pointer must never be read where
an object is expected. The checker records every view `upcast` and `join` make, and every type
pattern (`TProgram.views`, `TProgram.type_tests`); before lowering anything, `option_plan` keeps
every option of the program an object where one of them could:

- an option viewed as anything but another option, except as an `Any` where it is written (a box),
  including inside a type argument, a tuple or a function type, as `List[Option[Dog]]` seen as
  `List[Any]`; or an option of a pointer-shaped argument viewed as one of another, as
  `Option[Option[Dog]]` seen as `Option[Any]`;
- a type parameter viewed as anything but an `Any` it is boxed to, unless the view is through a bound
  of a class or trait `Option` does not conform to, or a function type, widening no argument to `Any`
  or to a trait `Option` conforms to (a bound holds exactly, so an instance's argument has the
  bound's own arguments);
- a type pattern whose target's arguments the tested value's static type does not fix, as one on an
  `Any`, where the target names `Any`, a type parameter or a trait `Option` conforms to inside it,
  such as `List[Any]`: the object it finds may hold options of any form; a pattern on a value whose
  static type fixes them is checked as the view the other way;
- a trait `Option` conforms to declaring a method that takes or returns `Self` inside another type.

Two options whose arguments are both pointer-shaped share the one form, so their views need nothing.
An `Option[Nothing]` written as another option converts to that option's `None` where it is written
(`NoneAs`: null, or the constant `None` object), so that is no view. Inside another value it cannot
be converted: a `Vector[Option[Nothing]]` seen as a `Vector[Option[Dog]]`, a
`Result[Option[Nothing], E]` as a `Result[Option[Dog], E]`, a function yielding one as one yielding
an `Option[Dog]`, and the same through options, tuples, data fields and map values. `option_plan`
notes what each such view sees it as; seen only as options kept as pointers, `Option[Nothing]` is a
pointer, seen only as objects (`Option[int]`, `Option[Option[Dog]]`), an object, and seen as both, or
as an option of a type parameter, it can be neither, and every option of the program is an object.

`Any` is a reference to an object. The checker inserts a **box** node wherever a value converts to
`Any`; lowering makes it nothing for a value already kept as a reference to an object with a
descriptor (a string, a tuple, a `data` value, an object, a closure), and for a number, `char` or
`bool` a new one-field object whose descriptor is that type's box descriptor; `unit` has one
constant box. A built-in value or a tuple converts to a trait it implements the same way (`to_trait`
in the checker, which overload resolution also accepts through `boxes_to`): a box descriptor holds
its type's method table, each entry an adapter that reads the value out of the box (and boxes a
`Self` result) before calling the built-in's method, and a tuple's descriptor holds its table
directly. Tables of boxes, tuples and strings are filled as calls through new positions appear, as
data types' are. A **type record** per type is constant data that every descriptor of the type names,
at offset 56: the number of the type's ancestors and their records' addresses. A class's ancestors
are its superclasses and every trait it and they conform to, with their type arguments as it sees
them. A tuple's record lists no other tuple types: it holds its parts' records instead, so the
tuple types a tuple may be seen as, the product of its parts' ancestors, cost nothing until a type
pattern names one. A type
pattern loads the object's descriptor and its record: for a type nothing else can be (a built-in, a
final class, a `data` type, a tuple of such, each with only invariant parameters), one comparison
with the target's record; for an open class, a trait, an instance of a type with a covariant or
contravariant parameter, or a tuple with a part of one, a call of the runtime's `avin_rt_conforms`, which
scans the record's ancestors, so the test costs the number of ancestors, and for a tuple target
compares the parts' records one by one: a part is one of the target's when its type is the same,
a tuple seen as it in turn, or a class or data type with it among the ancestors its declarations
give it (the record's first `part_view`), which keep the part's representation. Only a record some
object has as its own type, through a layout, a box or a closure, gets equality and rendering
entries; a record that a type pattern names, or that is only another's ancestor or part, has
none, as no object can reach them through it. A generic instance is its
own type with its own record, so `Box[int]` and `Box[string]` differ by address. A closure's
descriptor names the record of its function's own type in the copy it was made from, and a function
type, whose parameters and result vary and which a class may extend, is always tested through
`avin_rt_conforms`. Strings are made by
the runtime as well as written as literals, so the program emits one descriptor for every string
(`string_descriptor`), naming the record of `string` and holding its method table; its literals
name it, and `main` hands it to the runtime (`avin_rt_string_descriptor`) before anything else, so
the strings the runtime makes have it too. Closures and the boxes of captured variables share one record no
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
function that loads both records, answers false when they differ, compares strings (the record of
`string`) by bytes, and otherwise calls the record's entry; the hash reads the entry the same way. The entries are
made for every record an object can have, once a program makes such a comparison at all; making them
can lay out more types, so lowering repeats until no record lacks them.

Two **rendering entries** follow (offsets 24 and 32): `display` and `debug` on an object of the type,
a writer and a spec, through the type's own implementation where it conforms, or writing the type's
name in the spec's field where it does not. A call of `display`, `debug` or `toString()` through a
table on a receiver of `Any`, or of a trait that does not require the method's trait, does not use
the table, which may have no such position for the object's type: `display` and `debug` call a
generated function that loads the object's record and calls its entry, and `toString()` calls the
core library's `displayString` at `Any`, which does the same through `display`. A receiver of
`Display`, `Debug` or a trait requiring them keeps the table, since every object it holds conforms.
The `display` entries are made like the equality entries, once a program shows such a value at all,
and the `debug` entries once it debugs one.

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
call and method value, finds the impl a method came from by its first type parameter's site and
reports the first parameter and bound the receiver's type arguments miss (`unmet_condition`). A
class's method is decided at the arguments the receiver gives the declaring class, through
`ancestor_type`, so a subclass instance is held to its superclass's impls. The methods are ordinary
generic methods, compiled for an instance only where a call reaches them.

**Derived traits.** Every derivable trait has a template, `derive Trait for T`, in its own module;
the core library's `Eq`, `Hash` (`equality.avin`), `Ord` (`ordering.avin`), `Display` and `Debug`
(`format.avin`) are templates like any library's. `derives` is written out on the syntax tree
(`derive.sysl`) before the checker declares anything: each entry makes an impl of the template's
body for the type (`derive_through_template`), declared in the template's file so that the body's
names resolve as the template's do, and carrying a `DeriveSite` that names the type, its unit and
the entry. The impl's type parameters are the type's, by position, each bounded by the trait and
renamed to a name no source can spell, so the template's `T`, `Writer` or any other name it uses
means what its author meant whatever the type calls its parameters. The type gains its shape, as
private module values of its module. A template is checked once over its `T`; in each impl made
from it, every call of a member of `Shaped` on `T` is replaced by code written for the type's
variants and fields (`shaped_call`), checked in the type's file, and what goes wrong in the impl is
reported at the `derives` entry (`report_derived`).

What is the compiler's own is when the core library's five apply. A class derives `Eq` and `Hash`
together and `Ord` only beside an `Eq`; a data type takes no `Eq` or `Hash` impl, having both by
value; a name among the five always means the core library's trait (`core_template`), and an impl or
parent the module writes counts as a core trait's only where its bare name reaches the core library
from that file (`names_core`). Every type parameter carries a `site`, its file and offset, or for a
derived impl's the type's place and the trait; the bounds a parameter has are recorded by owner,
position and site (`bound_key`), so two impls of one type, wherever each is declared, keep their
bounds apart. The rules a list breaks on its own (an unknown trait, a repeat, a trait the type also
implements, `Eq` without `Hash` on a class) are reported while writing it out. The rest needs types:
once every conformance is known, and before any body is checked, the checker holds each field to
the `Ord`, `Display` or `Debug` derived from it (`check_derivation`), with the bounds the parameters
have in the derived impl, names a field that falls short, and leaves that impl's methods unchecked.

Every data type derives `Display` and `Debug` this way without `derives`, unless an impl of its own
in the module implements the core trait or declares its method; both are declared at the type. These
**automatic derivations** are held to the fields after the written ones, repeatedly until nothing
changes, since a type left without `Display` leaves the types holding it without `Display` in turn.
A field that falls short is not reported: the type is withdrawn from the trait (its conformance,
its table positions and the impl's methods go, recorded as withdrawn so lowering never compiles
them), and a bound or interpolation that then fails names the field in its diagnostic.

Lowering compiles a data type's trait implementations only when called, and fills a method table
position only once some call goes through it: each new position is filled in every table made so
far, which may make instances to compile, until no position is new. So a program that never shows a
value carries no `Display` or `Debug` code, and one calling `toString()` through `Display` carries it
only for the types it makes. A call records its receiver's static type at the position
(`slot_receivers`; a generic method's slot carries it too), and a table is filled there only where
its type always is one of those types; a call lowering makes with no static receiver, an equality
decided at run time, fills the position in every table (`used_slots`). Without that, a trait method
making an instance at a larger type argument, as `Iterator.zipWithIndex` makes an
`Iterator[(T, int)]`, would fill that instance's table, make the next larger one, and never end.

The supertypes an instance has through **variance** are open-ended (`List[Dog]` is a `List[Animal]`
and a `List[Any]`), so a record does not list them all. Lowering notes each record a type pattern
tests through `avin_rt_conforms`; once every function is lowered, each record gains those tested
records whose types its own conforms to by variance, by the checker's rule (`view_as`). A test
still costs one scan of one record.

A covariant view needs no conversion, and stays sound under monomorphization, because variance
applies only between reference type arguments (an option kept as a pointer is held to that by
`option_plan`; see **Options as pointers**). Every field and every part of a field whose type
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

A static member is declared as a method is, its first parameter standing for the receiver, so
implementations line up with the trait's member, take its table position and are compared with it
exactly as methods are; the symbol is marked `static_member`. A call on a type (`int.zero()`,
`T.zero()`) is a method call whose receiver is a `TypeReceiver`, an expression with the type and no
value: on a known type it is a `Call` of that type's member, and on a type parameter a `BoundCall`.
Lowering passes nothing for the receiver and gives the function no parameter for it, and a
`BoundCall` on a type takes the implementation from the type's own table even for an open class,
since no object is involved.

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

A thread that runs out of stack stops the program the same way, with `avin: trap: stack overflow`
and no place. The runtime's C handler (`runtime/avin/rt/__posix__/stack_guard.c`) is installed by
each thread before it runs Avin code, on the main thread when the runtime starts and on a started
thread before its closure runs, and removed when that closure returns. Installing it records the
lowest address of the thread's own stack, read from the operating system, and gives the thread an
alternate signal stack, which is what lets the handler run when the ordinary stack is the thing that
is full. A fault is reported only if its address lies in the guard region at the bottom of the
faulting thread's stack; any other fault is a real crash, so the handler restores the default action
and returns, and the program ends with the segmentation fault it would have had. The place is left
out because the context's caller site holds where the program last entered the library, which is not
where its recursion was. The handler writes the message with `write(2)` first, then flushes standard
output, which is not safe inside a signal handler: an alarm ends the process if the flush never
returns, and output buffered at the moment of the fault may be lost when the fault came from inside
the C library's own output routines. The process ends as any trap does.

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

**A trap in an actor fails the actor** (`runtime/avin/rt/recovery.sysl`). The actors library runs
an actor's start, its messages and its `stopping` blocks through `avin_rt_actor_guarded`, which
records in the context a return point (`_setjmp`), the shadow stack's top, the caller site, the
owner and the count of regions in doubt, and calls the closure through the closure runner. Every
trap entry point first offers the trap there: with a return point recorded and no region in doubt
entered since, the runtime flushes standard output, reports `avin: trap in Counter#3: <reason> at
<place>`, puts the recorded words back and returns to the turn (`_longjmp`), the reason, an Avin
string, stored in the array the library passed before anything is allocated. The regions in doubt
are counted in the context: a lock held (`avin_rt_lock_acquire` to `avin_rt_lock_release`, so a
`Mutex` and a `Once` body), a `lazy val` initializing, and an Avin function Sysl called; a trap
inside one stops the program as before, its report naming the actor, as a binding's conversion
trap does. The stack-overflow handler is unchanged, so an actor's overflow stops the program.
`avin_rt_stop_reported` stops the program from an actor whose failure escalated past the top, the
trap already reported.

Programs are multi-threaded, so the runtime keeps no per-thread state in globals. Each thread has a
**context**, and every compiled function, closure entry and runtime entry point takes the running
thread's context as a hidden first argument. A function that holds references links its frame of
roots onto its thread's shadow stack by writing the context's first word directly, on entry and on
every return, without calling the runtime. A collection walks the frames of every attached
context; the main thread's is attached before the program's entry function runs. Another thread
gets a context of its own from `avin_rt_thread_attach` before it runs compiled code and gives it
back with `avin_rt_thread_detach` when it has finished, with no frame left on it.

**What a frame holds.** The collector can run only at a *collection point*: an allocation, a poll
(below), a call of one of the program's functions that may collect, a call through a function value,
and a call of a runtime entry point that may allocate, poll or block. A function may collect when it
allocates, polls, calls through a function value or calls a runtime entry point or function that
may; this is found for the whole program at once, so a call of an accessor that only reads fields is
no collection point. Only the eight runtime entry points that do none of these (comparing, hashing
and searching strings, hashing reals and identities, testing conformance, clearing array elements)
are known to be quiet; every other entry point and every Sysl binding's shim may collect.

A reference, whether a value or a local variable, is a root only where it is **live across** a
collection point: made before it and used after it. Liveness is computed backward over the
function's blocks, and an address computed from a reference, a field's or an element's, keeps that
reference live while the address is in use. References passed to a function of the program are the
callee's to keep: inside the callee a parameter is a root only if the callee uses it after one of its
own collection points. A runtime entry point that may collect is the exception, since it reads its
arguments after allocating, so references passed to it are live across the call.

References that are never live at once share a frame slot: slots are given out in the order the
references' extents over the function begin, each to a reference whose extent begins after that of
the slot's last occupant ended. A value is stored to its slot where it is made, and a local variable
that needs a slot lives in it. The frame is not cleared on entry: only a slot the collector could read
before the function has stored to it on some path is set to null, and a slot whose reference has died
still holds a valid reference, since its object has been kept alive by that same slot at every
collection since. A function with no reference live across a collection point has **no frame** at
all, so a leaf that only reads fields, or one whose branches only read fields and trap, pushes
nothing.

Threads share the heap, and one lock serializes claiming memory, collection and changes to the list
of contexts; the collector itself runs single-threaded. **Each thread allocates from a chunk of its
own.** A context holds a chunk of the heap (the collector's thread-local allocation chunks, from
the gc 0.3.1 release), and a small object is carved from it with no lock and no atomic operation.
A thread takes the lock only to claim a fresh chunk when its own is spent, or for an object too
large for any chunk. The collection budget counts a chunk whole from the moment it is claimed, a
thread that detaches retires its chunk, and a collection retires the chunk of every context while
the world is stopped, so the next allocation of each thread claims a fresh one. Threads that
allocate therefore scale with their number rather than queueing on the lock. **A collection stops
the world.** Each context
is either *running*, when its thread may touch references, or *safe*, when its frames hold every
reference it still needs and it touches none until it is running again. The collecting thread,
holding the lock, raises the `Stop` bit of the flag `avin_rt_requested`, waits until every other
attached context is safe, collects, lowers the bit and wakes the others; the scheduler raises the
flag's other bit, `Yield`, to time-slice lightweight threads (see *Scheduling* below), and the same
poll answers both. A lightweight thread that is not running is always safe. A thread becomes safe in
three ways:

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
  collection is over, and a lightweight thread asked to yield yields there. Every reference live across a poll is already in a frame where the poll is
  placed.
- **Waiting for the lock.** Claiming a chunk takes the lock with one compare-and-swap when it is
  free; a thread that has to wait for it waits safe, since the holder may be collecting. An
  allocation the thread's chunk cannot serve is therefore a safepoint.
- **Around a blocking call.** `avin_rt_enter_safe` and `avin_rt_leave_safe` bracket an operation
  that may wait (a lock, a condition, a channel, a join, reading input), so a blocked thread never
  holds up a collection; leaving waits for a collection in progress to finish. The shim of a Sysl
  binding declared `blocking` brackets its Sysl call the same way, after converting every argument
  (and copying a handle's counted reference into a local) and before converting the result, so the
  call touches nothing on the Avin heap while the collector may run.

The handshake is a pair of sequentially consistent stores and loads, the flag by the collector and
a context's state by its thread, so either the thread sees the flag and wakes the collector, or the
collector sees the thread safe. With one context attached the flag is never raised, so a program
with one thread takes no poll's slow path and makes no system call for a collection.

The emitter gives the [memory model](/spec/threads/#the-memory-model) its two guarantees.
**Every load and store another thread can reach is an `unordered` atomic one**: a field of an
object, which covers class and data fields, a closure's captures and the box of a captured `var`;
an element of an array; a byte of a string; and a module-level value, in `@avin.roots` or a global
of its own. A function's own slots, and the slots of its callers whose addresses a local function is
given as parameters, belong to one thread and are accessed plainly, as are the header words and the
constant descriptors behind them. An atomic access is made at the width the value is laid out at, a
whole power-of-two number of bytes: a `bool` is a byte and an `i5` a byte, an `i24` four, an `i48`
eight, truncated after a load and zero-extended before a store. An integer wider than 64 bits is
accessed as its 64-bit words, lowest first, each an `unordered` access of its own, so no word is torn
and a racing load may combine words of different stores. On arm64 and x86-64 an `unordered` load or
store is the same instruction a plain one is.

**Making an object ends with `fence release`**, once its last field is stored and before the
reference can reach anything else: a data value or tuple after its fields, a closure after its
captures, a box after its value, a value boxed as `Any` after the value, and a class's object at the
end of its constructor, after the class body has run. A constant object needs none, nor does an
object kept as its fields (above, under escape analysis and scalar replacement). On arm64 the
fence is one `dmb ish`; on x86-64 it is no instruction at all, only an order the compiler keeps.
Arrays and strings are made by the runtime, which makes the fence for them through `published`: a
string after its bytes are written, `avin_rt_new_array` after the array's length and zero bytes, and
a slice after its elements are copied, as are the runtime's lock and condition objects. A string
writer's storage is never handed out, so filling it needs none. The elements the library stores into
a new array after `avin_rt_new_array` returns are published by the library: the intrinsic
`array_publish`, which only the core library may declare, lowers to the same `fence release` and
ends the fill in `Array.fill`, `Array.tabulate`, `Deque.toArray` and the growth of a buffer's or a
deque's array. An array filled and then wrapped in a `Vector` is covered by the vector's
constructor. A function that fills a new array and lets it escape ends the fill with `publish`.

**Atomics.** `avin.threads.Atomic[T]` holds its value as its first field and declares `load`,
`store`, `swap` and `compareAndSet` as the intrinsics `atomic_load`, `atomic_store`, `atomic_swap`
and `atomic_compare_set`, which only that class may declare. Lowering makes each one MIR instruction
on the field's address at the instance's type, and the emitter makes it one `seq_cst` LLVM atomic at
the value's storage width: `load atomic`, `store atomic`, `atomicrmw xchg` and `cmpxchg`, whose
success bit is the result. A `bool` or a narrow integer is widened as a shared field is, a `real` is
accessed as the 64 bits of its encoding, and a reference as a `ptr`, so a compare-and-set compares
identity. `compareAndSet` of a value without identity (data, tuple, string, `Any`) is library code
over two intrinsics, `atomic_by_value`, which lowering folds to a constant for the instance's `T`,
and `atomic_compare_set` on the word: it loops on `==` and a word compare-and-set. An integer wider than 64 bits has no single atomic access, so each operation on one is the
field's word-at-a-time accesses between `avin_rt_wide_atomic_begin` and `avin_rt_wide_atomic_end`,
which take and give back one spin lock every such operation shares. `add`, `sub` and `update` are
library code, a loop of `load` and `compareAndSet`, so an integer that would overflow traps before
anything is stored.

**Locks and conditions.** The synchronization library reaches eight intrinsics, each a runtime
entry: `lock_new`, `lock_acquire`, `lock_release`, `condition_new`, `condition_wait`,
`condition_wait_for`, `condition_notify` and `condition_notify_all`. A lock is an object on the
collected heap holding whether a thread holds it and a queue of the threads waiting for it, and a
condition one holding a queue alone; neither holds a reference, and the collector never moves
either, so a waiter may be linked from one. Each queue has a guard, a futex lock held for a few
instructions. A thread waits as the waiter its context holds: a lightweight thread parks, and any
other thread sleeps on one condition shared by all such threads until its own waiter is woken
(`runtime/avin/rt/scheduler.sysl`). A lock let go goes to its oldest waiter, which then holds it.
`condition_wait_for` takes a time in milliseconds and answers whether a notification came before it
ran out; a lightweight thread's timed wait is a timer the scheduler's monitor fires. A thread that
waits links a frame holding the objects it waits on onto its stack, waits safe, and unlinks the
frame once it is running again, so what it waits on stays alive whatever the program still holds.
A condition wait joins the condition's queue before it lets the lock go, and takes the lock again
once it has been woken or its time has run out.

**Starting a thread.** `avin.threads` reaches three intrinsics. `thread_start` calls
`avin_rt_thread_start` with a closure taking nothing and returning nothing, which stores the body's
result where the `Thread` reads it, and the stack size. The entry makes the new lightweight thread's
record: its context, with a frame of two slots at the bottom of its stack, the closure and the
thread's *joint*, the object it is joined by, allocated on the collected heap; and its fiber, on a
stack of the requested size (64 KiB at the least) reserved through `sysl.fiber`. That frame is linked
onto the calling thread's own stack while the joint is allocated and the heap lock is awaited, then
moved to the new context, which is attached, safe, before the thread is put on the scheduler's ready
queue. So the closure is a root at every moment from the call to the thread's end. A worker that
takes the thread stores its carrier in the context and switches to it; the thread marks its context
running, calls the closure's entry (the word before its descriptor) with its context and the
closure, and when the closure returns wakes its joiners, detaches its context and switches away for
the last time, its worker giving the stack back. `thread_join` calls `avin_rt_thread_join`, which
waits on the joint's queue until the thread has ended, and returns at once once it has; the joint
is collected once nothing reaches the `Thread`. `thread_current` answers the calling context's
address, which the scope compares with its owner's.

**Scheduling.** The scheduler (`runtime/avin/rt/scheduler.sysl`) keeps one ready queue under one
lock, and workers, operating system threads started one at a time as threads become ready, up to the
processor count, each with a carrier of its own. A worker runs no compiled code: it switches to a
lightweight thread, and once the thread switches back it does what the thread asked, letting go of
the guard a parked thread waits under, putting a yielding one at the back of the queue, or giving an
ended one's stack back. Since a waker finds a waiter only under that guard, no thread is woken before
it has left its worker's stack. The monitor, one more thread, ticks every millisecond while a worker
is busy: it fires timed waits that have run out, raises the `Yield` bit of `avin_rt_requested` and
the worker's own flag when its thread has run 10 ms while another is ready, which the next poll's
call of `avin_rt_safepoint` answers by yielding, and releases a worker that `avin_rt_enter_safe` has
marked blocked for a whole tick while threads are ready, so another worker runs in its place.

**Ending the program.** A trap, a failed allocation and `exit` begin by taking a lock that is never
given back: the first thread to take it flushes standard output, reports and stops the process, and
any other waits safe for the end, so a program stops once with one report however many threads fail.
A trap ends the process with the trap signal from whichever thread raises it. When `main` returns,
the entry's `main` returns its status to the C library, whose exit flushes standard output and ends
the process with every other thread in it.

## Standard library

`library/` holds Avin source under the module root `avin`, starting with the prelude every program
sees. The compiler knows a small, declared set of
**intrinsics**: arithmetic on built-in types, allocation, field access and the runtime entry points.
Each is declared in Avin source with `@intrinsic("…")` and has no body. Everything else, including
`println`, is ordinary library code.

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

**Tuple families.** Code cannot be generic over a tuple's arity, so the core library's `Display`,
`Debug` and `Ord` for tuples are written by the compiler for each arity. The parser records the
widest tuple each file writes, as a type, a value or a pattern (`SourceFile.widest_tuple`), and
`tuple_impls_unit` (`tuples.sysl`) adds a file of module `avin` holding the three impls for every
arity from two up to the widest: `impl[A0: Ord, A1: Ord] Ord for (A0, A1)`, comparing part by part.
No wider tuple can arise, since a tuple type is written before a value of it exists.
`resolve_tuple_impl` resolves such an impl, allowed only in the core library, onto the **tuple
family** of its arity: the tuple of the parameters `TypeVar(tuple_owner(n), i, "Ai")`, one owner
per arity, so the three impls share one type and one table as the integer family's do, while each
parameter's bounds are the impl's own (`bound_key` tells them apart by declaration site). Its
`BuiltinConformance` carries the owner and the parameters, and `builtin_covers` lets it stand for
every tuple of that many parts; `tuple_parts_hold` adds the condition that each part meets the
parameter's bounds, in `meets_bound`, `builtin_traits` and method lookup (`method_holds`). A call of
a family method takes the tuple's parts as its type arguments, in `receiver_type_args` and in
lowering's `implementation_args`, so each tuple type is an instance calling its parts' methods
directly.

**Rendering an interpolation.** `s"…"` is checked into a join of its pieces with `+`. A hole of a
built-in type becomes a `Render` node, lowered to the runtime's rendering of the value (a constant
for `bool` and `unit`); a hole of any other type, a type parameter's included, becomes a call of its
`toString()`, which needs the type to implement `Display`.

A built-in type's impl may declare a method as an intrinsic; its declared type takes the receiver
first. `impl string` in `library/avin/string.avin` does so for the string operations that read the
object itself: `byteLength` (the length word, loaded and narrowed to `int`), `byteAt` (a checked
byte load), `slice`, `repeat` and a private `find`, the last three over the runtime's
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
descriptor gives one element's size where a class's gives the object's, and carries the class's
method table and type record as any class's does. Which of an element's words are references is
the descriptor's tracing (`array_descriptor` in `mir.sysl` builds it from the element's word map):
when each element is one reference its kind's tracer is the runtime's `avin_rt_trace_array`, which
marks every element; when no word is, the ordinary tracer, which marks none, with no offsets; and an
element of several words that mixes the two (a union's tag, bits and reference) lists the byte
offset of each reference word within an element, which `avin_rt_trace_array_map` reads in every
element and no other word. Handing a value over reads the same words through the same scanners
(`scan_object` in `runtime/avin/rt/array.sysl`). The checker recognizes the class by
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
whose companion object has a `newBuilder` method, or else the core library's `Vector`, looked up by
qualified name so a program's own `Vector` never takes its place. The literal becomes a block: a
hidden local holding `Companion.newBuilder()`, one `addOne` call per element, and `result()`. The
result type is unified with the expected type, or with the instance of the expected trait the
collection is (`ancestor_type`), which settles the element type, and the elements are then checked as `addOne`'s
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
  neither), and `NAME.out` and `NAME.err` beside it hold the output it must write. A test for a
  known bug keeps its correct expectations and gains a `// ignore: REASON` line (the reason is
  required; an empty one is a failure): the program is then neither compiled nor run, and the
  summary line (`tests/run: N passed, M failed, K ignored`) counts it apart and lists it with its
  reason. The same line works in `tests/reject`, in `examples/`, and in a program of many files
  under `tests/programs/`, where it goes in the main module. Removing the line un-ignores the test.
- **Rejection tests** are Avin programs in `tests/reject/` that must fail to compile; each
  `// error: TEXT` line names text the diagnostics must contain.
- **Tooling tests** (`runner_tests.sysl`) drive `avin run` and `avin test` through the command line,
  with the program's output sent to files: forwarded output and exit status, traps, the cache's reuse,
  the runner's report and its filter.
- **Library tests** will be Avin `@test` functions in the core library; `avin test` runs a program's
  tests, and does not yet run the core library's own.
- **Semantic references**: where Avin defines arithmetic as Sysl does, a test compares against an
  independently compiled Sysl program.
