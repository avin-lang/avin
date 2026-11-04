---
title: Packages
summary: avin.hocon, dependencies by git, path and workspace, version selection, avin.sum, workspaces, and the commands that make and keep them.
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
- **`path`**: a directory holding a package, relative to the manifest that names it, for a package
  developed beside the one using it. A path dependency may name paths of its own, each relative to its
  own manifest. A fetched package may not name a path, which exists only on the machine that
  published it.
- **`workspace = true`**: the member of the package's [workspace](#workspaces) whose package has the
  dependency's name.

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
```

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
- `avin tidy [--check] [<package>]` rewrites `avin.sum`, given a package, a path in one, or a
  workspace's root, which it tidies whole.
- `avin lsp` analyzes a file in a package as the package, with its dependencies as the cache holds
  them.
