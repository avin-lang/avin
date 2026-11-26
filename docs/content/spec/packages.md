---
title: Packages
summary: avin.hocon, dependencies by git, path and workspace, version selection, avin.sum, workspaces, features, machines and files for one machine, members of a repository, vendoring, a program of one file's dependencies header, the standard library's modules, and the commands that make and keep them.
weight: 24
---

A **package** is a directory holding an `avin.hocon` manifest. The directory is the package's program
root, exactly as for a [program without one](/spec/declarations/#visibility-and-modules): each
directory under it is a module named by its path, and the root directory's own files are the root
module, which holds `main`. A package with `main` is a program; one without is a library. A
**project** is either one package or a [workspace](#workspaces) of several.

```hocon
package {
  name = "shapes"
  version = "0.3.1"
  description = "Plane shapes and their areas"
  avin = "0.0.18"
}

dependencies {
  json     { git = "github.com/owner/json", version = "1.4.0" }
  geometry { path = "../geometry" }
  testkit  { workspace = true }
}
```

```text
shapes/avin.hocon
shapes/avin.sum            what each fetched dependency hashed to
shapes/main.avin           the root module, with main
shapes/shapes/area.avin    module shapes
shapes/shapes/solid/*.avin module shapes.solid
shapes/tests/*.avin        module tests, the package's own tests
shapes/.avin/              builds, only on a machine with no cache directory; not source
```

The manifest is optional: a directory or a single file with none above it is a program as it always
was, and needs no ceremony.

## The manifest

`package` names the package and gives its version; both are required, and `description`, `avin`
and [`targets`](#which-machines-a-package-builds-for) are optional. Beside it a manifest holds
`dependencies`, [`features`](#features),
[`workspace`](#workspaces), and for its C bindings [`c-libraries`](#c-libraries),
[`c-linking`](/spec/c-bindings/#how-a-program-links-its-c-libraries) and its shorthand
[`static-c-libraries`](/spec/c-bindings/#linking-statically).

- **A name** begins with a lowercase letter and holds lowercase letters, digits, `_` and `-`
  (`geom-tools`). It names the package, not a module: modules are named by their directories.
- **A version** is three numbers, `major.minor.patch`, with no leading zeros (`0.3.1`, not `0.3` or
  `0.03.1`).
- **`avin`** is the least Avin the package builds with, a version: `avin = "0.0.18"` is met by Avin
  0.0.18 and by every later one. Versions are compared number by number, so 0.0.9 comes before
  0.0.18. It is a minimum and nothing else: no range, no upper bound. With no `avin` the package
  asks for no particular Avin. `avin new` writes the version of the Avin that made the package.

### The Avin a package needs

Every package a program is made of is held to its `avin`: the root package, and every dependency,
directly or through another, fetched or by path. Building, running, testing or checking the program,
`avin add`, `avin tidy` and `avin vendor` all refuse a package that needs a later Avin than the one
running, at its `avin` value, naming the package, what it needs and what is running:

```text
error: `cairo` 0.2.0 needs Avin 0.0.19 or later, and this is Avin 0.0.18: upgrade Avin to 0.0.19 or later to build it
 --> cairo/avin.hocon:4:10
  |
4 |   avin = "0.0.19"
  |          ^^^^^^^^
```

That error comes before anything else wrong in the manifest: one written for a later Avin may hold a
key this one does not know, and the upgrade is what it needs. An `avin` that is not a version is an
error at its value, like any other. The version compared is the running Avin's own, three numbers: a
tree between releases is the last release, so a package naming one not yet cut is refused by it. A
program that imports a standard-library module this Avin lacks is told, with the modules it has, that
it may have been written for a later Avin, and that its package says which it needs with `avin`.
`avin lsp` reports both on the manifest: on `avin.hocon` itself
when the editor opens it, and on the manifest of the package a file being edited belongs to.
- **Every key is known.** A key the manifest does not hold (`author`, `build`, `tag`) is an error at
  the key, as are a value of the wrong kind and a block missing what it needs. Each error points at
  its line:

```text
error: `1.0` is not a version: a version is three numbers, `major.minor.patch`, such as `0.1.0`
 --> shapes/avin.hocon:3:13
  |
3 |   version = "1.0"
  |             ^^^^^
```

## Dependencies

A dependency is named in `dependencies` and says once where it comes from:

- **`git` and `version`**: a git repository by its **coordinate**, `host/owner/name`, read at the tag
  `v` and the version. A coordinate is identity, not a URL: `https://` and `.git` are refused rather
  than stripped, since two spellings of one package would be two packages. The repository is cloned
  from `https://` and the coordinate.
- **`git` and `rev`**: the same repository read at one commit, `{ git = "github.com/owner/json", rev =
  "3f9a2c1" }`, for a package that has no release or a fix not yet released. A `rev` is a commit's id
  in lowercase hexadecimal, seven to forty digits, so an abbreviation and the full id name one
  package: it is cached and recorded in `avin.sum` under the full id. A commit that no branch or tag
  reaches is found by its full id alone. A dependency has a `version` or a `rev`, never both.
- **`path`**: a directory holding a package, relative to the manifest that names it, for a package
  developed beside the one using it. A path dependency may name paths of its own, each relative to its
  own manifest. A fetched package's path names another package of
  [its own repository](#members-of-a-repository), read at the same version; a path leaving the
  repository is refused, since it exists only on the machine that published it.
- **`workspace = true`**: the member of the package's [workspace](#workspaces) whose package has the
  dependency's name.

A `git` entry may also name a [member of the repository](#members-of-a-repository) with `member`, and
any entry may say which of the package's [features](#features) it wants (`features`,
`default_features`) and whether it is taken only when a feature of this package turns it on
(`optional`).

A dependency that cannot be found is reported at the manifest line naming it: a path that is not a
directory, a directory holding no manifest, a workspace dependency outside a workspace or naming no
member, a tag the repository does not have.

### Sysl dependencies

A package whose [bindings](/spec/sysl-bindings/) reach a Sysl package other than Sysl's standard
library names it in `sysl-dependencies`, written exactly as an Avin dependency is, by `git` with a
`version` or a `rev`, or by `path`:

```hocon
sysl-dependencies {
  regex  { git = "github.com/sysl-lang/regex", version = "0.2.0" }
  digits { path = "../digits" }
}
```

These are Sysl packages, not Avin ones: Sysl fetches, selects and checks them when it builds the
bindings' shims, so they get no line in `avin.sum` and `avin add` and `avin tidy` leave them alone.
A `path` names a directory holding a `sysl.hocon`, relative to the manifest that names it, and a
fetched package may not name one. A workspace holds no Sysl package, so `workspace = true` is refused
here.

**A program builds with the Sysl dependencies of every package in it**, so a library binding a Sysl
package works in every program that uses the library, which need not name the Sysl package itself.
Two packages naming one Sysl dependency at two releases of one repository take the higher; one name
given any two other origins is refused at the manifest line, naming both:

```text
error: the Sysl dependency `digits` is github.com/o/digits v0.1.0 here and the directory /work/digits in `app`: one name is one Sysl package, so make them agree
```

**The C libraries a Sysl package needs are linked into the program.** A Sysl package that binds a
library the machine supplies says so in its own files, and Avin links what it says, as Sysl links
its own programs: each library a module names with `@link` is passed as `-l<name>`, and each module
its manifest requires under `requires { pkg_config { … } }` is linked with the flags `pkg-config
--libs` answers on the machine at that link. A program binding the `sqlite3` package therefore needs
nothing more in its own manifest than the package:

```hocon
sysl-dependencies {
  sqlite3 { git = "github.com/sysl-lang/sqlite3", version = "0.7.1" }
}
```

The libraries are kept beside the shims' cached archive, so a cached build links them too; the
`pkg-config` question is asked again at every link, so a library that moved is still found. A
`pkg_config` module the machine does not know stops the link with the command that failed. The C a
package carries in its own tree is compiled into the shims' archive and needs nothing at the link.

### C libraries

A package whose [C bindings](/spec/c-bindings/) call a C library names it in `c-libraries`: the
`pkg-config` module (or the `link` names) that finds and links it, the `headers` its bindings are
checked against, and an `install` sentence for a build that cannot find it.

```hocon
c-libraries {
  mlt { pkg-config = "mlt-framework-7", headers = ["framework/mlt.h"], install = "brew install mlt" }
}
```

A program links the C libraries of every package in it, asking `pkg-config` at every build. They are
the machine's, not fetched, unless a package vendors one by its path: they get no line in
`avin.sum`. A program whose manifest says `static-c-libraries = true` links them from their static
archives, so the machine it runs on needs none of them, and its `c-linking` block chooses library by
library; only the root package's keys are read ([C
bindings](/spec/c-bindings/#how-a-program-links-its-c-libraries)).

### What a dependency brings in

**A dependency's modules come in under their own names.** A package `geometry` whose directory holds
`geometry/` and `geometry/solid/` gives its users the modules `geometry` and `geometry.solid`, imported
exactly as the package's own documentation shows them. Nothing in an import names a package.

**Its root directory's files and its `tests` directory stay its own.** Its root module and its `main`
are what it runs as a program, and its tests are what `avin test` runs in it; neither is compiled into
a package that uses it, so a program can be depended on for its modules. `avin test` runs the tests
of the package it is given and of no other.

**A module belongs to one package.** Two packages holding one module, the package's own included, is
refused naming both:

```text
avin: the module `geometry` is in two packages, `geometry` and `shapes`: a module belongs to one package
```

**A directory holding its own `avin.hocon` is another package** wherever it sits under a root, and is
not one of the root's modules. This is what lets a repository hold several packages side by side.

### Which version is chosen

The version of a git dependency is **the highest that any manifest in the graph asks for**, never a
newer one nobody named:

```text
shapes         depends on json 1.2.0 and on table 0.4.0
table 0.4.0    depends on json 1.4.0
                               ------
               json resolves to 1.4.0
```

**A commit is never chosen among.** Every manifest in the graph that pins a package to a `rev` must
name one commit, and a package pinned to a commit is not also asked for at a release; either
disagreement is refused at the manifest line, naming both:

```text
avin: `b` pins example.com/avin/pinned to the commit 4c1e…, and another dependency pins it to 9a07…: a package is read at one commit, so make them agree
```

Selection is a function of the manifests, so builds are reproducible without a lockfile, adding a
dependency cannot silently upgrade an unrelated one, and upgrading is an edit to a number.

## Fetched packages, and `avin.sum`

A fetched package is cloned at its tag into the machine's cache,
`<cache>/avin/pkg/<coordinate>/@v<version>/`, shared by every project on the machine, with its history
removed. A build fetches what the cache has not got; the language server never fetches.

`avin.sum` sits beside the manifest and **should be committed**. It has a line for every fetched
package and version the project has read:

```text
github.com/owner/json v1.4.0 sha256:6f1b…
github.com/owner/table r3f9a2c1d5e8b4a6f7c0d1e2f3a4b5c6d7e8f9a0b sha256:41c0…
```

A package pinned to a commit is kept in the cache at `<cache>/avin/pkg/<coordinate>/@r<commit>/`, and
its line gives `r` and the commit's full id where a release gives `v` and its version.

The hash is SHA-256 over a listing of the package's files, one line each: the file's own SHA-256 in
lowercase hexadecimal, two spaces, and its path from the package's root, sorted by path. A `.git`
anywhere and the root's `.avin/` are left out. The hash is recorded beside the cache entry when it is
fetched, so a package another project fetched first is checked too.

**A package whose hash differs from its line is refused.** What that catches is what a version number
cannot describe: a tag moved, a repository rewritten, a mirror serving something else. The first
time a package is seen it is trusted and recorded; reviewing the line that appears is a person's
part. A path or workspace dependency gets no line, since a directory beside you is expected to change.

**A build only adds lines.** `avin tidy` rewrites the file to exactly the packages the project's
resolution reads, keeping each line that stays where it was and saying what it removed, and
`avin tidy --check` writes nothing and fails while the file is not that. A version read on the way to a
higher one keeps its line, since every build reads it again. A project fetching nothing is tidy with
no `avin.sum`.

## Workspaces

A **workspace** is several packages resolved as one. Its root manifest lists the members'
directories, each relative to it; `*` stands for part of one directory's name:

```hocon
workspace = ["app", "libs/*"]
```

Each member is an ordinary package with its own manifest. The workspace adds three things:

- **one version selection** over every member's dependencies, so no two members are built against two
  versions of one package;
- **one `avin.sum`**, at the workspace's root;
- **members depending on each other by name**, `geometry { workspace = true }`, with no fetch and no
  line in `avin.sum`.

A root with a `package` block is a member too, the first. A root without one compiles nothing of its
own and so takes no dependencies; a command given it is refused, listing its members. An entry with
no `*` must name a directory holding a manifest.

## Features

A package often has a part not every user wants: a server, a desktop window, a second codec. The
cost of that part is its modules, which every build compiles, and its dependencies, which every build
fetches. **A feature is a name for one such part**: what it turns on is a set of the package's own
modules, its optional dependencies, and other features.

```hocon
features {
  server {
    doc     = "an HTTP server"
    default = true
    modules = [shapes.server]
    enables = [http, "tls/client"]
  }
  desktop = [webview]
}

dependencies {
  http    { git = "github.com/owner/http",    version = "0.2.0", optional = true }
  tls     { git = "github.com/owner/tls",     version = "0.3.0", optional = true }
  webview { git = "github.com/owner/webview", version = "0.1.0", optional = true }
}
```

### Declaring one

A feature is a block, named as a package is. `desktop = [webview]` is the short form of
`desktop { enables = [webview] }`. The block's keys:

| key | meaning |
|---|---|
| `doc` | what the feature is, shown in errors and by the language server |
| `default` | `true` makes the feature part of what a user gets when it asks for nothing else |
| `modules` | the package's modules that exist only while the feature is on, each with the modules under it |
| `enables` | the optional dependencies and the other features it turns on |
| `requires` | features that must be on whenever this one is; a build with one of them off is refused |

**A member of `enables` names a feature of this package where one of that name is declared, and
otherwise a dependency.** The one exception is the feature's own name: `json = [json]` is the ordinary
way to say "the `json` feature takes the `json` package", since a feature cannot enable itself.
`"dep:json"` names the dependency anywhere else; `"tls/client"` turns on the feature `client` of the
dependency labelled `tls`, and the dependency itself where it is optional. Both are quoted, since
HOCON reads a bare `:` or `/` otherwise.

No feature is named `default`. A dependency marked `optional = true` is taken only while some feature
that is on names it; one that no feature names is refused, as is a feature naming a dependency that is
not optional, a member that is neither a feature nor a dependency, a `dep:` or `/` member naming a
dependency the manifest does not declare, and features that turn each other on in a cycle. **All of
these are reported when the manifest is read**, at the line, before anything is fetched.

### How a feature gates modules

**The module is the unit a feature gates**; there is no conditional compilation inside a file. A
module listed in a feature's `modules`, and every module under it, is part of the package only while
the feature is on: its files are not read, checked or compiled otherwise. A module is listed by one
feature at most, and the root module and `tests` cannot be listed.

**What a module may import follows from the gates, and is checked whichever features are on.** A
module may import a gated module only where its own gate implies the other's: it is gated by the same
feature, or by one whose `enables` reach that feature. The same holds for an optional dependency: only
a module whose gate turns that dependency on may import its modules. An ungated module importing
`shapes.server` is an error at the import, naming the feature it would need:

```text
error: `shapes.server` exists only while the feature `server` is on, and `shapes` is always compiled: list `shapes` under a feature that enables `server`, or import it from a module that is
 --> shapes/shapes/area.avin:3:8
```

Because the check is made against the manifest and not against one build's selection, a package that
checks with every feature on also builds with any set of them off.

**The package's tests may import any of its modules, whatever its gate, and any of its optional
dependencies.** `avin test` turns every feature on, so a test of `shapes.server` can sit in `tests`
beside the rest. A test module is read only while every module it imports is: one importing a module
that is off, or importing a test module that is not read, is passed over by that build as a gated
module is, so the package still builds, checks and tests with any set of features off, running the
tests that remain. The module is still the unit: a file of `tests` importing `shapes.server` takes
the whole of `tests` with it when `server` is off, so tests that should run without a feature go in a
module of their own, `tests/plain/`, which nothing gated reaches.

```avin
module tests

import shapes.server.{serve, port}

@test
servesOnItsPort()
  assertEq(serve() + port(), 284)
```

An import refused for the module it names is refused once for the whole import, however many names
it brings in: `import shapes.server.{serve, port}` in an ungated module is one error.

**A feature only adds.** It adds modules and dependencies; it cannot remove or change a declaration,
because nothing in a file depends on which features are on. A gated module's conformances are about
its own types and traits, and its extensions are seen only by the modules that import it, which are
gated too. So a package shared by two users, each asking for different features, can be given the
union of both requests without either seeing anything change.

### What a user asks for

A dependency entry says which of the package's features it wants:

```hocon
dependencies {
  shapes { git = "github.com/owner/shapes", version = "0.3.1", features = [desktop] }
  json   { git = "github.com/owner/json",   version = "1.4.0", default_features = false }
  table  { git = "github.com/owner/table",  version = "0.4.0", features = [-colour] }
}
```

`features` adds to the package's defaults; `default_features = false` leaves them off; a `-` member
takes one default away for this entry. A feature the package does not declare, a `-` member that is
not a default, and a `-` member beside `default_features = false` are refused, naming who asked.

**Every request for one package is added together.** A package reached from several manifests gets
every feature any of them asked for, its defaults included unless every request left them off. A
feature that turns on an optional dependency brings that dependency's own requests in, so the
selection is repeated until it adds nothing; it can only grow, so it ends.

### On the command line

`avin build`, `run`, `check`, `test` and `vendor` take three flags, which choose the **root
package's** features; a dependency's are chosen by the manifests that depend on it:

| | |
|---|---|
| `--features a,b` | turn these on beside the defaults; may be given more than once |
| `--no-default-features` | leave the defaults off |
| `--all-features` | turn on every feature the package declares |

A name the package does not declare is refused, listing those it does. **`avin test`, `avin check`
and the language server turn every feature on unless a flag says otherwise**, since a module no build
compiles is a module nothing checks. `avin tidy` resolves with every feature on, so `avin.sum` does not
depend on which flags the last build was given. A build's reuse of an executable is keyed by the
features it was built with.

### What a feature does not do

**A feature has no value a function can test.** There is no `feature("server")` constant and no
line-level gate: the module stays the only unit a feature turns on. The parts a feature turns on are
modules and optional dependencies, which are *absent* from a build with the feature off, so a branch
testing a constant would have to type-check against modules that are not there; folding the branch
away after checking cannot help, since the check is what fails. A function that behaves differently
with a feature on is two functions: the plain one in an ungated module, and the other in the
feature's module, which the program reaches through the feature's own entry point.

**A module is listed by one feature only.** A part two features share is a feature of its own, which
both `enables`:

```hocon
features {
  codec  { modules = [media.codec] }
  record { modules = [media.record], enables = [codec] }
  play   { modules = [media.play],   enables = [codec] }
}
```

That keeps every gate a single feature, so whether a module may import another is one question --
does this module's feature turn that one on -- answered the same way however many features a package
has.

**A feature may exist only on some machines.** `if` on a feature, in the condition language of
[machines](#machines), says where the feature exists; it never turns the feature on. Asking for it on
a machine where it does not exist is refused, naming the machine; a `default` feature that does not
exist on the machine is simply not part of the default there, and `--all-features`, `avin test` and
`avin check` turn on every feature that exists on the machine being built for.

```hocon
features {
  journal { doc = "logging to the systemd journal", modules = [probe.journal], if = "linux" }
}
```

## Machines

A program is built **for a machine**: an operating system and a processor. Avin builds for three:

| target | the machine |
|---|---|
| `aarch64-macos` | macOS on Apple silicon |
| `x86_64-linux` | Linux on 64-bit Intel and AMD, with glibc |
| `aarch64-linux` | Linux on 64-bit Arm, with glibc |

**The machine is the one being built for**: `--target <name>` where a command takes it, and
otherwise the machine the compiler runs on. Most packages never say anything about it: the standard
library answers the same way everywhere, and a package written against it builds for every target.
What follows is for the package that cannot, because it reaches something only one system has.

### Which machines a package builds for

`package.targets` lists the targets a package supports. Left out, it is every target Avin builds for.

```hocon
package {
  name    = "probe"
  version = "0.2.0"
  targets = ["x86_64-linux", "aarch64-linux", "aarch64-macos"]
}
```

Building a package for a target its manifest leaves out is refused, naming the package and the
targets it lists; so is depending on it from a package whose own `targets` reach further. A target
name Avin does not know is an error at its value, listing the ones it does.

### The condition language

A condition is a string naming machines: **symbols** joined by `!`, `&&`, `||` and parentheses. The
symbols are a closed set:

| symbol | holds on |
|---|---|
| `macos` | macOS |
| `linux` | Linux |
| `posix` | every system Avin builds for, today `macos` or `linux` |
| `aarch64` | 64-bit Arm |
| `x86_64` | 64-bit Intel and AMD |

```hocon
if = "linux"
if = "linux && x86_64"
if = "!macos"
```

**A condition is checked when the manifest is read**, every symbol of it, whichever machine the
build is for, so a misspelling cannot read as false and quietly leave something out of every
build:

```text
error: `lunix` is not something a condition can ask about the machine: the symbols are macos, linux, posix, aarch64 and x86_64
 --> probe/avin.hocon:9:68
```

A target's name is refused with the condition it means (`"x86_64-linux"` is written
`"x86_64 && linux"`), and so is an empty condition, which is an entry every machine takes, written
without `if`.

### Conditions in the manifest

`if` may sit on an entry of `dependencies`, `sysl-dependencies` and `c-libraries`, and on a
feature. The entry is part of a build only on a machine where its condition holds:

```hocon
package { name = "probe", version = "0.2.0", targets = ["x86_64-linux", "aarch64-linux", "aarch64-macos"] }

dependencies {
  inotify { git = "github.com/owner/inotify", version = "0.1.0", if = "linux" }
  fsevent { git = "github.com/owner/fsevent", version = "0.1.0", if = "macos" }
}

sysl-dependencies {
  seccomp { git = "github.com/owner/seccomp", version = "0.3.0", if = "linux" }
}

c-libraries {
  ssh {
    pkg-config = "libssh2"
    headers    = ["libssh2.h"]
    install { macos = "brew install libssh2", linux = "apt install libssh2-1-dev" }
  }
  security {
    link-flags = ["-framework", "Security"]
    headers    = ["<Security/Security.h>"]
    if         = "macos"
  }
}
```

- **A dependency left out by its condition is not in the build at all**: not fetched, not resolved,
  not linked. A dependency's own conditional dependencies are handed on with it, so what a program
  is made of on a machine is what every manifest in it takes there.
- **`avin tidy` and `avin vendor` answer for every machine** in the root's `targets`, as they answer
  for every feature, so `avin.sum` and `vendor/` do not depend on the machine the last command ran
  on.
- **`if` and `optional = true` compose**: such a dependency is taken where a feature turns it on
  *and* the condition holds. A condition cannot name a feature, and a feature cannot name a machine
  except through its own `if`.
- **`install` is a sentence or a block of them by condition**, so a library is installed by the words
  each system uses. The first condition that holds is the one shown; a machine none of them holds
  for is shown none.
- **`link-flags`** passes flags to the link exactly as written, for what is not a library `-l` names:
  a macOS framework (`["-framework", "Security"]`), a linker option. It sits beside `pkg-config` or
  `link`, or alone.

### Files for one machine

Inside a module, **a file whose name carries a condition symbol between its name and `.avin` is read
only on the machines the symbol holds on**: `watch.linux.avin` on Linux, `watch.macos.avin` on macOS,
`stack.aarch64.avin` on 64-bit Arm. A file with no such part is read everywhere, and so is a file
named as a symbol alone (`linux.avin`). A literate file is selected the same way
(`watch.linux.lavin`). The part is one symbol from the table above; a name whose part is not one
(`watch.lunix.avin`, `watch.Linux.avin`), or that carries two (`watch.linux.x86_64.avin`), is an error
on every machine, naming the symbols:

```text
avin: probe/probe/watch/watch.lunix.avin: `watch.lunix.avin`: `lunix` is not something a file's name can ask about the machine: a file read only on some machines is named `<name>.<symbol>.avin`, and the symbols are macos, linux, posix, aarch64 and x86_64
```

Code for one system and one processor at once is that system's file asking `avin.target.arch`
(below). A program of one file is the file it names, whatever its name: `avin run tool.linux.avin`
runs it on any machine.

```text
probe/probe/watch/watch.avin          module probe.watch, on every machine
probe/probe/watch/watch.linux.avin    module probe.watch, on Linux
probe/probe/watch/watch.macos.avin    module probe.watch, on macOS
```

```avin
// probe/probe/watch/watch.avin
module probe.watch

// Reports each file that changes under `dir`, as the system tells of it.
changes(dir: string, report: (string) -> unit) -> unit = start(dir, report)
```

```avin
// probe/probe/watch/watch.linux.avin
module probe.watch

import inotify.{Inotify, Mask}

private start(dir: string, report: (string) -> unit) -> unit =
  Inotify.open().add(dir, Mask.Modify | Mask.Create, report)
```

The module is the same module on every machine; its files on one machine are the ones read there.
So a private helper in one machine's file is seen by the module's other files on that machine, and
the shared file above calls `start`, which each machine's file supplies.

**A module exists on a machine where at least one of its files is read.** A module whose every file
carries `linux` exists only on Linux, and that is how a whole module belongs to one system.

**Every file is checked on every machine it is read on.** `avin check` type-checks the package once
for each of its `targets` on which anything differs: a file with a condition, an entry with an `if`.
A shared file calling `start` on a machine whose file does not supply it is an error there, naming
the machine:

```text
error: no function `start` is declared in `probe.watch` on `aarch64-macos`: `watch.linux.avin` supplies it on Linux, and no file read on this machine does -- add it to a file read there, or leave the machine out of `targets`
 --> probe/probe/watch/watch.avin:4:67
```

Machines that read the package alike, the same files from the same packages, are checked once
together, so a package whose only difference is a Linux file is checked twice, for macOS and for
Linux. A diagnostic every check reports is told once, as it is; one that only some report is told
once, with the machines it is for:

```text
error: string has no field `length`; a string counts its characters with `charCount` and its bytes with `byteLength` (on `x86_64-linux` and `aarch64-linux`)
 --> probe/probe/watch/watch.linux.avin:3:41
```

A machine whose package cannot be read at all (a Linux-only dependency that cannot be fetched) is
reported with its machines, `on `x86_64-linux` and `aarch64-linux`:`, and the others are still
checked. `avin check --target <name>` checks that machine alone, and names none. A package in which
nothing differs is checked once, for the machine being built for, as before.

That check needs no other machine's tools: it reads and type-checks, and builds nothing; the C unit of
the package's C bindings is built only for the machine the compiler runs on. `avin build`, `avin run`
and `avin test` read and check only the files of the machine they build for, and the language server
checks the machine it runs on.

**An import of a module that does not exist everywhere is allowed only from a file that exists no
more widely.** `import inotify` in `watch.linux.avin` is allowed, since `inotify` is taken on Linux
and the file is read only there; the same import in `watch.avin` is an error at the import, naming
the files and the machines where the module is missing:

```text
error: `inotify`, a module of the package `inotify`, exists only on x86_64-linux and aarch64-linux, where `inotify.avin` is read, and `watch.avin` is read on aarch64-macos too: import it from a file read no more widely, or make `inotify` exist on aarch64-macos
 --> probe/probe/watch/watch.avin:3:8
```

The check is made for every machine in `targets`, whichever one is being built: where anything in the
package differs by machine, the build reads the package, with its dependencies, for each of its
targets too. "No more widely" is by machine, not by symbol: a file for `x86_64` may import a module
of Linux's, since every machine reading it is Linux, and a file for `aarch64` may not, since
`aarch64-macos` reads it. A module that exists on every machine through a different file on each is
imported from anywhere.

### The machine in an expression

Where the difference between machines is a value rather than a call only one system has, it is
written as an ordinary `if` over **`avin.target`**, whose two values are compile-time constants:

```avin
import avin.target
import avin.process.MacOS

configDir(home: string) -> string = target.os match
  MacOS -> s"$home/Library/Application Support/probe"
  _     -> s"$home/.config/probe"
```

`target.os` is an `avin.process.Os` and `target.arch` an `avin.target.Arch` (`Aarch64`, `X86_64`).
Both are known when the program is compiled, so the branches not taken are folded away, at every
optimization level; and both branches are type-checked on every machine, so one cannot rot unseen
on the machine that does not take it. A branch may therefore call only what exists everywhere; a
call only one system has belongs in that system's file. `avin.process.os()` answers the same value
at run time.

What is decided as the program is compiled is a `match` on either value whose arms are variants,
alternatives of them, `_` or a name (the first arm that takes the machine's variant is the only one
compiled, a guard before it keeping the whole `match`), an `is` test of either against a variant, and
`==` or `!=` between either and a variant or the other; an `if` over such a test, alone or as the left
of `&&` or `||` that decides the whole, compiles only the branch taken. The values themselves are
declared, each in `avin.target`'s file for the machines it is true of (`target.linux.avin`,
`target.aarch64.avin`), as a package would declare them.

### What a manifest does not say about machines

There is no per-machine linker, linker script, entry point or layout: an Avin program is linked by
the platform's own linker, started at `main` by its C runtime, on a hosted system. There is no
`choices` block (exactly one of a set of features): the cases it serves are a machine's, and a
machine is already exactly one. And there is no line-level gate inside a file; the file is the
smallest thing a machine selects, as the module is the smallest thing a feature selects.

## Members of a repository

A coordinate names a repository and the package at its root. **A repository that is a workspace holds
other packages too, and `member` names one of them by its directory**:

```hocon
dependencies {
  geometry-solid { git = "github.com/owner/geometry", member = "solid", version = "0.5.0" }
}
```

`member` is a directory relative to the repository's root, as the workspace's own list writes it, and
may be nested (`"tools/probe"`). It is read at the tag the repository is fetched at, `v0.5.0` here,
or at a `rev`, and must hold an `avin.hocon` with a `package` block there. `member` is only a `git`
entry's: beside `path` or `workspace = true`, or naming a directory outside the repository, it is
refused at the line.

**A member is a package of its own, named by the coordinate, `//`, and its directory**:
`github.com/owner/geometry//solid`. That is what `avin.sum` records it under and what version
selection goes by, so the root package and each member are selected separately. Its modules come in
under their own names, as every dependency's do; the name only identifies the package.

**Members of one repository at one version are one fetch.** The repository is cloned once into
`<cache>/avin/pkg/<coordinate>/@v<version>/` and each member is read from its directory there. Each
member's `avin.sum` line holds the hash of the whole clone, which is what was fetched, so the root's
line and a member's at one version carry the same hash and each is checked:

```text
github.com/owner/geometry v0.5.0 sha256:6f1b…
github.com/owner/geometry//solid v0.5.0 sha256:6f1b…
```

**A fetched member's siblings come from the same repository.** Inside its workspace a member names
another with `{ workspace = true }` or a `path` such as `"../core"`; fetched, each of those is the
sibling's own coordinate at the version the member was fetched at, the root package's being the bare
coordinate. It is then an ordinary dependency: a user asking for a higher version of the sibling gets
it, by the usual rule. A `path` leaving the repository is refused, as any fetched package's path is.

`avin add github.com/owner/geometry//solid@0.5.0` writes the entry with `git` and `member`, under the
name the member's manifest gives it. A member that is not in the repository at that version, or a
directory there without a manifest, is refused when it is fetched, naming the coordinate and version.

## Vendoring

`avin vendor` copies every package the project resolves to into a **`vendor/` directory beside the
project's manifest**, the workspace root's in a workspace:

```text
avin vendor
fetching github.com/owner/json v1.4.0
vendored 3 packages into vendor/
```

**`vendor/` is the machine's cache placed inside the project**, with its layout:
`vendor/<coordinate>/@v<version>/` and `@r<commit>/`, each with the hash recorded beside it. A
project that has a `vendor/` resolves from it instead of the cache, and **never fetches**: a package
the build needs and `vendor/` lacks is refused, naming `avin vendor` as the way to add it. Nothing has
to be configured: the directory being there is the whole of what turns it on, so a clone of a
project with a committed `vendor/` builds with the network off.

**What is copied is what the project's tests resolve**: every feature the package declares is on
unless the feature flags say otherwise, so `avin test` on a vendored project fetches nothing either.
Each package is fetched into the machine's cache first if it is not there, gets an `avin.sum` line
if it has none, and **its copy is hashed and checked against that line** before it is put in place,
so a cache entry edited since it was fetched is refused rather than frozen into the project. A clone's
`.git` is not copied. A package already vendored as recorded is left alone, and one the resolution no
longer reads is removed, the command saying which. Path and workspace dependencies are not vendored:
they are directories being edited beside the project, as `avin.sum` already treats them.

**`vendor/` is not source.** At the project's root it is never read as a module, as `.avin/` is not,
and nothing in it is compiled as one of the project's own modules; its packages are dependencies like
any fetched one. `avin.sum` is unchanged by vendoring: it is still the record each vendored package is
checked against, and `avin tidy` still rewrites it to the resolution, independently of what `vendor/`
holds. `avin tidy` and `avin add` read the machine's cache, fetching into it, whether or not there is
a `vendor/`; after `avin add`, the build refuses the new package until `avin vendor` copies it in.

## Programs of one file

A program of one file has no manifest, and **names its dependencies in a header at its top**: the
line `// avin.hocon`, then comment lines holding exactly what a manifest's `dependencies` block
holds:

```text
#!/usr/bin/env avin
// avin.hocon
// dependencies {
//   json  { git = "github.com/avin-lang/json", version = "0.1.0" }
//   table { path = "../table" }
// }

import dev.avinlang.json

main(args: Vector[string])
    ...
```

- **The header is comments**, so it is Avin to every tool that reads the file, and the file stays a
  file: no directory, no manifest beside it. `avin run`, `build`, `check` and `test` and the
  language server read it; nothing else needs to.
- **It begins on the first line**, or on the second after a `#!` line, with `// avin.hocon` and
  nothing else (blanks aside). It runs to the first line that is not a `//` comment, so a blank line
  ends it, and a comment about the program goes after that. Its text is each line after its `//`,
  read as HOCON. A `// avin.hocon` line anywhere else in the file is an error at that line, since a
  header that is not read would be a dependency silently missing.
- **It holds `dependencies` and nothing else.** A dependency is written as in a manifest, by `git`
  with `version` or `rev`, with `member` for a [member of a repository](#members-of-a-repository),
  or by `path`, relative to the file's directory; `features` and `default_features` ask for a
  dependency's features as they do in a manifest. `workspace = true` and `optional` are refused: a
  program of one file is in no workspace and has no features of its own to turn a dependency on.
  Another key (`package`, `features`, `sysl-dependencies`, `c-libraries`) is refused at the key: a
  program that needs one is a package.
- **The dependencies resolve exactly as a manifest's do**: the same version selection, the same
  cache, the same errors at the header's line in the file:

```text
error: `gone` is a path dependency, and `../gone` is not a directory
 --> tool.avin:4:6
  |
4 | //   gone { path = "../gone" }
  |      ^^^^
```

- **Its sum file is beside it, named after it**: `tool.avin.sum` for `tool.avin`, written as
  `avin.sum` is, a line for every fetched package and version it reads, and committed with the file.
  A program of one file with only path dependencies, or none, has none. `avin tidy tool.avin`
  rewrites it, and removes it from a file whose header is gone.
- **It is not vendored.** `vendor/` belongs to one project, and a directory of programs of one file
  would share one; `avin vendor tool.avin` is refused, naming a package as the way to build with the
  network off. `avin add` writes into a manifest, so it is refused too: the header is edited by hand.
- **A file of a package, or of a program of many files, may not have one.** A package's
  dependencies are its manifest's, and a header in one of its files is refused at the header rather
  than merged with it or passed over; so is one in a file of a directory program, which has no
  manifest to put them in until it becomes a package.

**A first line beginning `#!` is not Avin**, in any file: it names the program that runs the file, so
a program of one file can be run as a command (`chmod +x tool.avin`, then `./tool.avin a b`). The
line is written `#!/usr/bin/env avin`, so `./tool.avin a b` runs `avin ./tool.avin a b`, and
**`avin <file> ...` for a file whose first line begins `#!` gives every argument after the file to
the program**: `./tool.avin -O0 x` passes `-O0` and `x`, as an interpreter passes its script's
arguments, since a command's caller does not know it is written in Avin. Every other form keeps the
rule of the command line: `avin run tool.avin -O0 -- x`, and `avin -O2 tool.avin -- x`, take `-O0`
and `-O2` as avin's own and pass only what follows `--`.

## The standard library's modules

**The standard library is the module `avin` and the modules under it**, written in Avin and shipped
with the compiler: `share/avin/library` beside `bin/avin`, or `library/` under `AVIN_ROOT` for a
source checkout. Its version is the compiler's. It is never a dependency: no manifest names it, no
`avin.sum` line records it, and no package may declare a module named `avin` or under it.

- **`avin` itself is the prelude.** Its public declarations are seen by every file without an import,
  after the file's own module and its imports.
- **Every other module of it is imported by name**, as any module is: `import avin.fs`,
  `import avin.collections.mutable.{Map, Set}`, `import avin.threads as t`. Nothing beyond the prelude
  is seen without an import. A module of the library is found by its path under the library's
  directory, exactly as a program's module is found under its root; one that is not there is an
  error at the import, listing the modules under the deepest prefix that exists:

```text
error: the standard library has no module `avin.regx`; the modules under `avin` are `avin.actors`, `avin.c`, `avin.cache`, `avin.cli`, `avin.collections`, `avin.compress`, `avin.crypto`, `avin.diff`, `avin.encoding`, `avin.fs`, `avin.html`, `avin.io`, `avin.log`, `avin.math`, `avin.metrics`, `avin.net`, `avin.process`, `avin.random`, `avin.regex`, `avin.resilience`, `avin.signal`, `avin.sqlite`, `avin.target`, `avin.threads`, `avin.time`, `avin.toml`, `avin.uuid`
 --> app/main.avin:1:8
```

- **A compilation reads the prelude and only the library modules something imports**: the prelude's
  own imports, then every library module the program, its dependencies and those modules import, to
  the end. A library module nothing imports is not read, so the standard library can grow without
  every program parsing and checking all of it. Within what is read, a function is compiled only as
  a program reaches it.
- **What belongs in it** is what every program's types are built from (the core traits, collections,
  strings, formatting) and what talks to the operating system (`avin.io`, `avin.fs`, `avin.process`,
  `avin.time`, `avin.threads`), so a script of one file with no manifest has them. A library for one
  format or domain is a package of its own, fetched like any other: JSON is the package
  `dev.avinlang.json` (see [JSON](/spec/json/)).
- **An import that leaves out `avin.`** is an error that writes out the import meant, where the
  library has a module of that name and the program has no module or declaration of its own by it:

```text
error: there is no module `fs`; the standard library's is `avin.fs`, imported as `import avin.fs`
 --> script.avin:1:8
```

- **An import of `avin.json`**, or of `json` in a program with no module or declaration of that name
  and no dependency on the package, is an error naming the package:

```text
error: JSON is the package `dev.avinlang.json`; add it to your dependencies
 --> script.avin:1:8
```

- **It has no features.** A standard module is either imported or not, which already costs a program
  nothing it does not use.

The language server completes `import avin.` from the library's directories, and its go-to-definition
opens the library's own source, which is read-only to a rename.

## Commands

**Every command finds the package from any path in it**, walking up from the file or directory it is
given to the nearest `avin.hocon` with a `package` block: `avin run shapes/main.avin` and
`avin check shapes/tests` both mean `shapes`. A path with no such manifest above it is a program
without one, as before: a file being a [program of one file](#programs-of-one-file), whose header
names its dependencies.

- `avin build`, `run`, `test`, `check`, `emit-llvm` and `parse` compile the whole package with its
  dependencies. `avin build` names the executable after the package. A package without `main` is a
  library: `avin check` checks it, its tests included, and `avin run` and `avin build`, having no
  `main` to start from, refuse it.
- `avin new <dir>` writes a program package named by the directory's last segment: its manifest,
  whose `avin` is the version of the Avin that wrote it, a module of that name (with `-` written `_`) holding one function, `main.avin` using it, a test of it in
  `tests/`, and a `.gitignore` naming the executable `avin build` writes at the root (`/shapes`) and
  `.avin/`. `avin new <dir> --lib` writes a library, the same without `main.avin`, its `.gitignore`
  naming `.avin/` alone. It refuses a directory that exists and a name that is not a package's.
- `avin clean [<path>]` removes the builds `avin run` and `avin test` keep for the package, which are
  in the user's cache directory, a directory per package and keyed by where the package is, so two
  checkouts of it never share one ([running and testing](/compiler/#running-and-testing)).
  `avin clean --all` removes every program's builds this version of Avin keeps; fetched packages stay.
- Fetched packages are not counted against the build cache's limit and a sweep of the cache never
  evicts one ([running and testing](/compiler/#running-and-testing)); `avin cache clean --packages`
  removes them, with `--older-than` only those no build has read for that long, and the next build
  that needs one fetches it again.
- `avin add <coordinate>[@<version>|@<rev>] [<package>]` adds a git dependency to the package's
  manifest: the version given, the commit given (written to the manifest as its full id), or the
  highest release the repository has tagged. It fetches the package, lists
  it under the name its own manifest gives it, and adds its hash to `avin.sum`. Adding a dependency
  already listed changes its version, to a lower one as well as a higher. A coordinate followed by
  `//` and a directory adds that [member](#members-of-a-repository). The rest of the manifest,
  comments and layout included, is left as written, and a dependency that cannot be fetched, is
  not an Avin package, or needs a later Avin than the one running, itself or through a package it
  depends on, changes nothing.
- `avin tidy [--check] [<package>]` rewrites `avin.sum`, given a package, a path in one, or a
  workspace's root, which it tidies whole.
- `avin vendor [<package>]` copies what the project resolves to into its
  [`vendor/`](#vendoring). It and `build`, `run`, `check` and `test` take the
  [feature flags](#on-the-command-line).
- `avin doc [<package>]` writes the package's [API documentation](/getting-started/#api-documentation),
  its dependencies' and the standard library's modules it reads written beside its own so every
  link stays inside the site, with every feature on.
- `avin lsp` analyzes a file in a package as the package, with its dependencies as the cache holds
  them, and an `avin.hocon` as a manifest, telling it what is wrong with it at its place.
