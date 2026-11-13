---
title: Releasing
summary: How a release of the compiler is cut, what its tarball holds, and what is left to publish it.
weight: 42
---

A release is cut on the Mac that builds it, by one script that publishes nothing:

```sh
scripts/release.sh 0.0.1
```

It refuses unless the tree is clean and `HEAD` is `origin/dev` (`--expect <sha>` names another
commit, which is how a branch is put through a dry run, and `--repo <dir>` another checkout). Then,
each step timed into `release-out/<version>/release.log` with its output beside it:

1. **The version.** It stamps `const Version` in `compiler/dev/avinlang/compiler/cli.sysl` and
   `package.version` in `compiler/sysl.hocon`, which a test holds in step, and in
   `library/library.hocon`, which the compiler checks against its own version and refuses a library
   that differs from, and commits the stamp as
   `avin <version>` on the local branch, unless the tree carries that version already.
2. **The compiler**, built at `-O2` by the `sysl` on `PATH`.
3. **The prefix.** `bin/avin`; `share/avin/library` and `share/avin/runtime` as git tracks them;
   `share/avin/editors`, the TextMate grammars of `.avin` and `.lavin`; and
   `share/avin/lib/libavinrt-<key>.a`, the runtime's archive. The archive is built by the staged
   compiler from the staged runtime, so its key, a hash of the compiler's version and the runtime's
   sources, is the one an installed compiler computes, and an installed compiler links it without
   running Sysl.
4. **The tarball**, `avin-<version>-darwin-arm64.tar.gz`, and its sha256.
5. **The proof, from the tarball.** It unpacks the tarball outside the repository, makes the prefix
   read-only as a Homebrew keg is, and from a scratch directory, with `AVIN_ROOT` unset and a fresh
   `AVIN_CACHE`, runs `avin --version`; builds and runs a hello program with only `/usr/bin` and
   `/bin` on `PATH`, which must link the shipped archive; runs a program that reaches a Sysl binding,
   whose shims Sysl builds into the cache; makes a package with `avin new` and runs `avin test` on
   it; and has `avin lsp` answer `initialize`.
6. **The formula**, `packaging/homebrew/avin.rb` with the version and sha256 filled in, written to
   `release-out/<version>/avin.rb`.

`AVIN_RELEASE_SLOT=<script>` runs the build through a wrapper, as `zsh <script> <dir> <command...>`,
so a release waits its turn behind a test run.

## What is left

The script ends by printing the steps it does not take, each of which other people see:

1. push the stamp to `dev`, then tag the release commit `v<version>` and push the tag;
2. create the GitHub release `v<version>` in `avin-lang/avin` with the tarball;
3. download the asset the release serves and check its sha256 is the one the script printed;
4. copy the filled formula into `Formula/avin.rb` of `avin-lang/homebrew-tap` and push the tap;
5. `brew install avin-lang/tap/avin` and `brew test avin`.

## What an installed compiler needs

`clang`, which links every program, from Xcode's command-line tools; and `sysl`, which builds the
shims of the Sysl bindings a program reaches, and which the formula depends on. A program that
reaches no binding is built with clang alone. The runtime archives and shims the compiler builds go
to the user's cache, `~/Library/Caches/avin/<version>`, never into the installed prefix.

The repository has no license file yet, so the tarball carries none.
