---
title: Linux
summary: How Avin comes to build and run on x86_64 and aarch64 Linux - what in the toolchain assumes macOS, what it needs from Sysl, how Linux is built, tested and shipped, and the items that do it, in order.
weight: 43
---

Avin's targets are `aarch64-macos`, `x86_64-linux` and `aarch64-linux` (glibc), as
[decided](/spec/decisions/). This page is the plan for the two Linux targets: the places the
toolchain assumes macOS, what it needs from Sysl, how it is built, tested and shipped, and the items
that do the work, each one agent's. What a package says about machines is in
[packages](/spec/packages/#machines). Which items are done is in the [roadmap](/compiler/roadmap/)
and nowhere else.

## What assumes macOS

Most of the toolchain is already written for both systems: the runtime's Sysl gates its error
numbers, socket constants and `errno` location on `#if bsd`; its C keeps a Linux branch beside each
macOS one; the collector is precise and touches no system interface; the standard library reaches
the system only through Sysl bindings, and Sysl's library implements each for Linux (epoll for the
poller, `/proc` for process memory and children, glibc's numbers for signals). The table lists each
place that assumes macOS, and each Linux branch the items below have to prove on Linux.

| where | the assumption | on Linux |
|---|---|---|
| **Compiler: code and link** | | |
| `emit.sysl` module header; `codegen_units.sysl:623`; `driver.sysl:819` | The IR carries no `target triple` or `datalayout`; clang compiles it for its own default target | Right for a native build. A cross build passes `--target=<triple>` to every clang line |
| `driver.sysl:780-834` (`link`) | The link line is objects, the runtime or shims archive, the C unit, the libraries, `-lm`; ld64 resolves archives whatever their order | GNU ld reads an archive once, left to right: link with `-fuse-ld=lld`, which resolves as ld64 does, or group the archives (`-Wl,--start-group … --end-group`) |
| `driver.sysl:826` | `-lm` is the only system library named | glibc 2.34 and later hold `pthread`, `dl` and `rt` in libc; older ones need `-pthread`. Name `-pthread` on Linux |
| `driver.sysl:836-844,864` | `-g` runs `dsymutil` to gather DWARF into `<exe>.dSYM` | Already gated on `MacOS`. The ELF linker copies DWARF into the executable, so nothing replaces it; gdb and lldb read it in place |
| `driver.sysl:62` | The tools are `clang`, `sysl` and `pkg-config` by those names | Distributions install `clang-18` and the like beside or instead of `clang`: an `AVIN_CC` variable (and Sysl's `--cc`) names it |
| `driver.sysl:128-129`, `:841` | A missing tool is answered with `brew install sysl-lang/tap/sysl` and `xcode-select --install` | Name the install script and the distribution's packages (`apt install clang lld llvm pkg-config`) |
| `driver.sysl:757`; `shim.sysl:1422,1443` | `sysl build-c` is run with no `--target` and no `--cc` | Right natively; a cross build passes both |
| `driver.sysl:764` (`runtime_key`); `keys.sysl` | Cache keys name the compiler and the sources, not the target | One machine per cache is right natively; a cross build, or a prefix shipping archives for two targets, keys by target |
| `cunit.sysl:655-685` | Static linking keeps `-framework` pairs and treats libSystem, libz and libm as the system's | `-framework` never appears on Linux. The system's libraries are glibc's (`c`, `m`, `pthread`, `dl`, `rt`), never taken from an archive |
| `cunit.sysl:712` | A shared library is `.dylib`, `.tbd` or `.so` | Also versioned names, `libssl.so.3` |
| `shim.sysl:1570` | `-lc` and `-lm` from a Sysl package's needs are dropped as always linked | The same on Linux; `-lpthread`, `-ldl` are harmless and kept |
| **Runtime** | | |
| `runtime/avin/rt/lookup.sysl:56,110-111` | The lookup's pipe sets descriptor flag `2` for close-on-exec | `FD_CLOEXEC` is `1` on both systems, so this is a bug on both; macOS hides it by starting children with every other descriptor closed, and Linux does not |
| `__posix__/runner_watch.c:14,28,63` | The runner's exit is awaited on a kqueue; elsewhere it is asked about every tenth of a second | Linux's branch polls. `prctl(PR_SET_PDEATHSIG)` or a `pidfd` answers at once |
| `__posix__/stack_guard.c:78-88` | The stack's low end from `pthread_get_stackaddr_np` | The `pthread_getattr_np` branch |
| `__posix__/stack_guard.c:131-133` | An overflow arrives as `SIGSEGV` or `SIGBUS` | Linux raises `SIGSEGV`; both are caught |
| `cpu.sysl:12-24` | `struct timeval` read as `i64` then a low `i32` | Linux's `suseconds_t` is 64 bits; the low half is the count on a little-endian machine |
| `recovery.sysl:13,17,24` | `_setjmp`/`_longjmp` into 64 words | glibc has both; its `jmp_buf` is 25 words on x86_64 and 39 on aarch64, within 64 |
| `sockets.sysl:45-58`; `lookup.sysl:51-54,65-70`; `cpu.sysl:29-35` | Error numbers and `SOL_SOCKET`/`SO_ERROR` by `#if bsd` | Linux's numbers, in the `#else` branch |
| **Standard library** | | |
| `library/avin/process/process.avin:243-254` | `os()` is Sysl's `os()` | Answers `Linux`; the tests already branch on it |
| `library/avin/time/zone.avin:487-520` | The tz database at `TZDIR` or `/usr/share/zoneinfo`, the host's zone at `/etc/localtime` | The same paths; a minimal container has no tz database, so the Linux image installs `tzdata` |
| `library/avin/net/tls` | OpenSSL found by `pkg-config` | `libssl-dev`; the certificate store is the distribution's |
| `library/avin/signal/signal.avin:16-37` | Eight signals by number | The eight are the same numbers on Linux |
| **Tests** | | |
| `static_link_tests.sysl` (24 lines) | `.dylib`, `-dynamiclib`, `-install_name`, `otool -L`; the end-to-end cases return early off macOS | The same cases on Linux with `.so`, `-shared`, `-Wl,-soname` and `readelf -d` |
| `debug_info_tests.sysl:326,378` | A `.dSYM` beside the executable | Already gated; Linux asserts the DWARF sections are in the executable |
| `toolchain_tests.sysl:68-69,103,127` | `~/Library/Caches`, the Homebrew and Xcode words | The XDG cache case, the Linux words |
| `tests/programs/run/c_void_pointer_libc.out:11` | `uname` answers `Darwin` | A per-system expected output |
| `tests/programs/run/c_rusage` | macOS's `proc_pid_rusage` | Read on macOS only: a `.macos.avin` file, or a case the corpus runs only on macOS |
| **Release** | | |
| `scripts/release.sh:33,87` | The asset is `avin-<v>-darwin-arm64.tar.gz` | One asset per target |
| `scripts/release.sh:211` | `otool -L` lists what the compiler links | `ldd` / `readelf -d` |
| `scripts/release.sh:262,269` | `shasum -a 256`, `stat -f %z` | `sha256sum`, `stat -c %s` |
| `scripts/release.sh:274,311` | `mktemp -t`; "Apple's clang" on `/usr/bin` | `mktemp -d`; the distribution's clang |
| `scripts/release.sh:409-411`; `packaging/homebrew/avin.rb:8-25` | The formula is `depends_on :macos` and `arch: :arm64`, over Sysl's macOS-only formula and `openssl@3` | `on_linux` with the two Linux tarballs, once Sysl's formula has them |

## What Avin needs from Sysl

An installed Avin runs Sysl at users' builds (`sysl build-c` for the runtime when no archive
shipped, and for every program's binding shims), so **Linux Avin needs Linux Sysl on every user's
machine**, not only at release. Sysl's docs list `linux-x86_64` and `linux-arm64` among its release
platforms, implement its library for Linux, and have `x86_64-linux` and `aarch64-linux` in its target
registry; what it does not yet have:

1. **A Sysl for Linux to install.** Sysl 0.11 ships `darwin-arm64` only, and building it from source
   needs a Sysl already installed (the seed), which Linux has no way to get. Avin needs release
   tarballs `sysl-<v>-linux-x86_64.tar.gz` and `sysl-<v>-linux-arm64.tar.gz` in the macOS
   tarball's layout (or a documented way to make the first Linux seed).
2. **Sysl tested on Linux.** Sysl's CI builds only its documentation on Ubuntu. Avin needs Sysl's
   own gate green on both Linux targets each release, `sysl.fiber`, `sysl.io.poll` (epoll),
   `sysl.process` (`/proc`), `sysl.signal` and `sysl.threads` (futex) above all, since the runtime
   stands on them.
3. **Cross builds for Linux from macOS.** `sysl build --target x86_64-linux hello.sysl` on the Mac
   (Sysl 0.11.12, Apple clang 21) stops building the standard module: `stdio.h` not found. There is
   no way to name a Linux sysroot (the docs reach a sysroot only through the Android NDK's and
   wasi-sdk's clang). For `avin build --target`, a later item, Avin needs a sysroot named per target
   (a flag, a `targets.<t>` key or an environment variable) honoured by `build`, `build-c` and
   `build-lib` and by the link, which takes `-fuse-ld=lld`.
4. **Sysl's Homebrew formula on Linux** (`on_linux` with the two tarballs), for Avin's formula to
   depend on, once item 1 exists.

Sysl's items 1 and 2 hold Avin's items 4 to 8, 10 and 11 below; its item 3 holds only item 12, cross
builds.

## How Linux is built, tested and shipped

- **Built natively.** Avin is built on Linux by the Sysl on Linux, exactly as on the Mac; nothing is
  cross-compiled. An installed Avin needs Sysl and clang on the machine anyway, so a native build is
  the one users run.
- **Tested locally in a colima VM**, aarch64 Linux at native speed on Apple silicon, at most 4 CPUs
  and 6 GB: an image (`scripts/linux/Dockerfile`) with clang, lld, llvm, pkg-config, libssl-dev,
  tzdata, git, zsh and the Linux Sysl; `scripts/linux/run.zsh <command>` runs a command in it over
  the worktree. Every such run goes through marshal as `marshal submit --external --mem 6G` with a
  deadline, since the work runs in the VM and not in marshal's own processes.
- **Tested in CI** on GitHub's `ubuntu-24.04` (x86_64) and `ubuntu-24.04-arm` runners, on every push
  to `dev` and on every tag: install the Linux Sysl, build Avin, run the compiler's tests and the
  corpus. x86_64 is reached here and nowhere locally (an emulated x86_64 VM is too slow for a gate).
- **Shipped** as a tarball per target beside the macOS one, plus `install.sh`, which puts Avin and
  the Sysl it needs into a prefix (`~/.local` by default) and names the distribution's packages for
  what is missing. The Linux tarballs are built by the release workflow on the two native runners
  from the tag the release script made; the formula gains `on_linux` when Sysl's has it. No `.deb`.

## The items

Each is one agent's, in its own worktree, in this order; an item lists what it waits for. Items 1, 2,
3 and 9 need no Linux machine.

1. **`lookup-cloexec`**: the lookup's pipe set close-on-exec with `FD_CLOEXEC` (1). Test: a runtime
   test that reads `fcntl(F_GETFD)` of both ends while a lookup is held; and a run test that starts a
   child during a held lookup and finds only its three streams open (holds on Linux; on macOS it
   passes either way). Waits for nothing.
2. **`machine-conditions`**: the manifest's machines as [packages](/spec/packages/#machines) gives
   them: `package.targets`, the condition parser and its errors, `if` on dependencies, Sysl
   dependencies, C libraries and features, `install` by condition, `link-flags`, `--target` on
   `check`, and `avin tidy`/`avin vendor` over every target. Test: `package_tests` for every key,
   every error and every composition (`if` with `optional`, a dependency's conditional dependency, a
   feature that does not exist on the machine); `avin check --target x86_64-linux` on the Mac
   resolving the Linux set. Waits for nothing.
3. **`machine-files`**: `name.<symbol>.avin`, a module existing where any file is read, the import
   rule, `avin check` once per differing target, and the `avin.target` module (`os`, `arch`, `Arch`)
   as compile-time constants folded at `-O0` and `-O2`. Test: `tests/programs/run` and `reject` for
   each rule and diagnostic, checked for `x86_64-linux` and `aarch64-linux` from the Mac (checking
   builds nothing); an IR test that an untaken branch is absent at `-O0`. Waits for 2.
4. **`linux-image`**: `scripts/linux/Dockerfile` and `scripts/linux/run.zsh`, and the colima
   recipe in [releasing](/compiler/releasing/). Test: `sysl --version` and `sysl test` of a
   one-file Sysl program inside it, through marshal. Waits for Sysl's item 1.
5. **`linux-build`**: the compiler built on Linux, and its link made right there: `-fuse-ld=lld` (or
   grouped archives), `-pthread`, `AVIN_CC`, the Linux words for missing tools. Test: the compiler's
   unit tests (`sysl test compiler` without the corpus) green in the image; `toolchain_tests` for the
   new words and `AVIN_CC`. Waits for 4.
6. **`linux-runtime`**: the runtime's tests green on Linux: the stack guard, `getrusage`, the error
   numbers, the poller under the scheduler, signals, thread dumps; the runner watch made immediate
   with `PR_SET_PDEATHSIG`. Test: the runtime suite in the image; a run test that kills the runner
   and finds the program gone at once. Waits for 5.
7. **`linux-corpus`**: the execution corpus on Linux at `-O0` and `-O2` under GC stress: per-system
   expectations in the corpus driver (`NAME.linux.out` beside `NAME.out`), the macOS-only programs
   given machine files, every Linux difference found fixed. Test: the whole gate in the image, through
   marshal. Waits for 3 and 6.
8. **`linux-native-tests`**: the macOS-only cases given Linux twins: static linking (`.so`, `-shared`,
   `readelf -d`), `-g` (DWARF in the executable, a gdb backtrace naming the Avin source), the XDG
   cache. Test: they are the tests. Waits for 5.
9. **`target-keys`**: the target in every cache key (runtime archive, shims, C unit, codegen units,
   the run cache) and a shipped archive under `share/avin/lib/<target>/`. Test: `toolchain_tests`
   that two targets never share an entry, and that an installed prefix finds its own target's
   archive. Waits for nothing.
10. **`linux-ci`**: `.github/workflows/linux.yml` on both runners. Test: the workflow green on a
    branch push. Waits for 7 and 8.
11. **`linux-release`**: `scripts/release.sh` per target with the portable commands above, the
    release workflow building the Linux tarballs from the tag and running step 6 of the release
    (verify from the tarball) on each runner, `install.sh`, and the formula's `on_linux`. Test: a
    dry release on a branch producing and verifying all three tarballs; `install.sh` into a fresh
    image. Waits for 9, 10 and Sysl's items 1 and 4.
12. **`cross-build`**: `avin build --target <t>` from the Mac, with a sysroot. Test: a hello and a
    program with bindings built for `x86_64-linux` on the Mac and run in the image. Waits for 9 and
    Sysl's item 3.
