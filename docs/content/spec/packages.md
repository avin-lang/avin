---
title: Packages
summary: avin.hocon, dependencies by git, path and workspace, version selection, avin.sum, workspaces, features, members of a repository, vendoring, the standard library's modules, and the commands that make and keep them.
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
shapes/.avin/              what building leaves; not source
```

The manifest is optional: a directory or a single file with none above it is a program as it always
was, and needs no ceremony.

## The manifest

`package` names the package and gives its version; both are required, and `description` is optional.
Beside it a manifest holds `dependencies`, [`features`](#features) and [`workspace`](#workspaces).

- **A name** begins with a lowercase letter and holds lowercase letters, digits, `_` and `-`
  (`geom-tools`). It names the package, not a module: modules are named by their directories.
- **A version** is three numbers, `major.minor.patch`, with no leading zeros (`0.3.1`, not `0.3` or
  `0.03.1`).
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
  own manifest. A fetched package may not name a path, which exists only on the machine that
  published it.
- **`workspace = true`**: the member of the package's [workspace](#workspaces) whose package has the
  dependency's name.

A `git` entry may also name a [member of the repository](#members-of-a-repository) with `member`, and
any entry may say which of the package's [features](#features) it wants (`features`,
`default_features`) and whether it is taken only when a feature of this package turns it on
(`optional`).

A dependency that cannot be found is reported at the manifest line naming it: a path that is not a
directory, a directory holding no manifest, a workspace dependency outside a workspace or naming no
member, a tag the repository does not have.

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
holds.

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
error: the standard library has no module `avin.jsn`; the modules under `avin` are `avin.collections`, `avin.fs`, `avin.io`, `avin.process`, `avin.threads`, `avin.time`
 --> app/main.avin:1:8
```

- **A compilation reads the prelude and only the library modules something imports**: the prelude's
  own imports, then every library module the program, its dependencies and those modules import, to
  the end. A library module nothing imports is not read, so the standard library can grow without
  every program parsing and checking all of it. Within what is read, a function is compiled only as
  a program reaches it.
- **What belongs in it** is what every program's types are built from (the core traits, collections,
  strings, formatting) and what talks to the operating system (`avin.io`, `avin.fs`, `avin.process`,
  `avin.time`, `avin.threads`). A library for one domain is a package of its own, fetched like any
  other.
- **It has no features.** A standard module is either imported or not, which already costs a program
  nothing it does not use.

The language server completes `import avin.` from the library's directories, and its go-to-definition
opens the library's own source, which is read-only to a rename.

## Commands

**Every command finds the package from any path in it**, walking up from the file or directory it is
given to the nearest `avin.hocon` with a `package` block: `avin run shapes/main.avin` and
`avin check shapes/tests` both mean `shapes`. A path with no such manifest above it is a program
without one, as before.

- `avin build`, `run`, `test`, `check`, `emit-llvm` and `parse` compile the whole package with its
  dependencies. `avin build` names the executable after the package. A package without `main` is a
  library: `avin check` checks it, its tests included, and `avin run` and `avin build`, having no
  `main` to start from, refuse it.
- `avin new <dir>` writes a program package named by the directory's last segment: its manifest, a
  module of that name (with `-` written `_`) holding one function, `main.avin` using it, a test of it in
  `tests/`, and a `.gitignore` naming `.avin/`. `avin new <dir> --lib` writes a library, the same
  without `main.avin`. It refuses a directory that exists and a name that is not a package's.
- `avin add <coordinate>[@<version>|@<rev>] [<package>]` adds a git dependency to the package's
  manifest: the version given, the commit given (written to the manifest as its full id), or the
  highest release the repository has tagged. It fetches the package, lists
  it under the name its own manifest gives it, and adds its hash to `avin.sum`. Adding a dependency
  already listed changes its version, to a lower one as well as a higher. The rest of the manifest,
  comments and layout included, is left as written, and a dependency that cannot be fetched or is
  not an Avin package changes nothing.
- `avin tidy [--check] [<package>]` rewrites `avin.sum`, given a package, a path in one, or a
  workspace's root, which it tidies whole.
- `avin vendor [<package>]` copies what the project resolves to into its
  [`vendor/`](#vendoring). It and `build`, `run`, `check` and `test` take the
  [feature flags](#on-the-command-line).
- `avin lsp` analyzes a file in a package as the package, with its dependencies as the cache holds
  them.
