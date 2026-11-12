#!/bin/zsh
# Cut a release of Avin: stamp the version, build the compiler, build the runtime's archive, package
# the two with the library and the editor grammars as a prefix, and prove the package works from where
# it was unpacked.
#
#   scripts/release.sh <version> [--out <dir>] [--expect <sha>] [--repo <dir>] [-O<level>]
#
# `--repo` names the checkout to release, the one holding this script by default. `--expect <sha>`
# releases that commit rather than `origin/dev`, which is how a branch is put through a dry run.
#
# **zsh, not sh or bash.** It runs on the Mac that builds the macOS tarball, and its arrays and
# `${var/old/new}` are zsh's. No variable here is called `path` or `status`.
#
# **It publishes nothing.** No tag, no push, no GitHub release, no formula: the last thing it does is
# print those steps, because each one is seen by other people and is taken once the tarball has been
# looked at. What it does do is commit the version stamp on the local branch, when the tree does not
# carry that version already.
#
# The steps, in order, each timed into `<out>/release.log`, with its output in `<out>/<step>.log`:
#
#   1. refuse unless the tree is clean and HEAD is the commit being released (`origin/dev`, or
#      `--expect <sha>`); a rerun on a HEAD that is already the version stamp over that commit is
#      accepted
#   2. stamp `<version>` into `const Version` (`compiler/dev/avinlang/compiler/cli.sysl`) and
#      `package.version` (`compiler/sysl.hocon`) -- a test holds the two in step -- and into the
#      standard library's `package.version` (`library/library.hocon`), which the compiler checks
#      against its own before it compiles anything; commit it
#   3. build the compiler with the `sysl` on PATH, at `-O2` unless `-O<level>` says otherwise
#   4. stage the prefix: `bin/avin`; `share/avin/library` and `share/avin/runtime`, the sources the
#      compiler reads; `share/avin/editors`, the TextMate grammars of `.avin` and `.lavin`; and
#      `share/avin/lib/libavinrt-<key>.a`, the runtime's archive, built here by the staged compiler
#      from the staged runtime so that the key is the one an installed compiler computes
#   5. package `avin-<version>-darwin-arm64.tar.gz` and its sha256
#   6. unpack it to a scratch prefix outside the repository, made read-only, and from a scratch
#      directory, with `AVIN_ROOT` unset and a fresh `AVIN_CACHE`: `avin --version`; build and run a
#      hello program, which must link the shipped runtime rather than build one; run a program that
#      reaches a Sysl binding, which builds its shims into the cache; `avin new` a package and
#      `avin test` it; and `avin lsp` answering `initialize`
#   7. write the formula with this version and sha256 into `<out>/avin.rb`, and print what is left
#
# **Memory.** A build of the compiler takes gigabytes. `AVIN_RELEASE_SLOT=<script>` names a wrapper
# every build is run through, as `zsh <script> <dir> <command...>`, so a release waits its turn rather
# than racing a gate.

set -u
setopt pipe_fail no_nomatch

die() { print -u2 -r -- "release: $*"; exit 1 }

# --- arguments -----------------------------------------------------------------------------------

version=""
repo=""
out=""
expect=""
opt=(-O2)

while (( $# > 0 )); do
    case $1 in
        --out) (( $# > 1 )) || die "--out needs a directory"; out=$2; shift 2 ;;
        --expect) (( $# > 1 )) || die "--expect needs a commit"; expect=$2; shift 2 ;;
        --repo) (( $# > 1 )) || die "--repo needs a checkout"; repo=$2; shift 2 ;;
        -O?*) opt=($1); shift ;;
        -*) die "unknown flag '$1'" ;;
        *) [[ -z $version ]] || die "one version, please -- '$version' and '$1'"; version=$1; shift ;;
    esac
done

[[ -n $version ]] || die "usage: scripts/release.sh <version> [--out <dir>] [--expect <sha>] [--repo <dir>] [-O<level>]"
[[ $version =~ '^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$' ]] ||
    die "'$version' is not a version -- three numbers, and an optional '-<pre-release>'"

repo=$(cd "${repo:-${0:A:h}/..}" && git rev-parse --show-toplevel) || die "'${repo:-${0:A:h}/..}' is not inside a git checkout"
proj=$repo/compiler

out=${out:-$repo/release-out/$version}
mkdir -p $out || die "cannot create $out"
out=${out:A}
log=$out/release.log
: > $log

command -v sysl > /dev/null || die "no 'sysl' on PATH: the compiler is built with it"

slot=${AVIN_RELEASE_SLOT:-}
[[ -z $slot || -f $slot ]] || die "AVIN_RELEASE_SLOT names '$slot', which is not a file"

asset=avin-$version-darwin-arm64.tar.gz
tarball=$out/$asset

say() { print -r -- "$*" | tee -a $log }

mmss() { printf '%02d:%02d' $(( $1 / 60 )) $(( $1 % 60 )) }

# One timed step. Its output goes to `<out>/<name>.log`, and a failure names that file.
took=()
step() {
    local name=$1; shift
    local began=$SECONDS
    say "== $name"
    if ! "$@" > $out/$name.log 2>&1; then
        say "FAILED $name after $(mmss $(( SECONDS - began ))) -- see $out/$name.log"
        tail -20 $out/$name.log | tee -a $log
        exit 1
    fi
    local spent=$(mmss $(( SECONDS - began )))
    took+=("$name $spent")
    say "ok $name ($spent)"
}

# A build, run in a directory, through the slot wrapper when there is one.
heavy() {
    local dir=$1; shift
    if [[ -n $slot ]]; then
        zsh $slot $dir "$@"
    else
        (cd $dir && "$@")
    fi
}

# --- 1. the tree ---------------------------------------------------------------------------------

check_tree() {
    local dirty
    dirty=$(git -C $repo status --porcelain) || return 1
    if [[ -n $dirty ]]; then
        print -r -- "the tree is not clean:"
        print -r -- $dirty
        return 1
    fi

    local base=${expect:-origin/dev}
    local want head
    want=$(git -C $repo rev-parse --verify "$base^{commit}") || { print "cannot read '$base'"; return 1 }
    head=$(git -C $repo rev-parse HEAD)

    if [[ $head == $want ]]; then
        print -r -- "HEAD $head is $base"
        return 0
    fi

    # A rerun: HEAD is this version's stamp, one commit over the commit being released.
    if [[ $(git -C $repo rev-parse HEAD^) == $want ]] &&
       [[ $(git -C $repo log -1 --format=%s) == "avin $version" ]]; then
        print -r -- "HEAD $head is the 'avin $version' stamp over $base ($want)"
        return 0
    fi

    print -r -- "HEAD $head is not $base ($want) -- release the commit you mean, or pass --expect"
    return 1
}

step 01-tree check_tree
base_commit=$(git -C $repo rev-parse ${expect:-origin/dev})

# --- 2. the version ------------------------------------------------------------------------------

version_file=$proj/dev/avinlang/compiler/cli.sysl
manifest=$proj/sysl.hocon
library_manifest=$repo/library/library.hocon

stamp() {
    local text old new

    text=$(< $version_file)
    old=$(grep -m1 '^const Version: string = "' <<< $text) || { print "no 'const Version' in $version_file"; return 1 }
    new="const Version: string = \"$version\""
    [[ $old == $new ]] || print -r -- "${text/$old/$new}" > $version_file

    text=$(< $manifest)
    old=$(grep -m1 '^  version = "' <<< $text) || { print "no package.version in $manifest"; return 1 }
    new="  version = \"$version\""
    [[ $old == $new ]] || print -r -- "${text/$old/$new}" > $manifest

    text=$(< $library_manifest)
    old=$(grep -m1 '^package { name = "avin-library", version = "' <<< $text) || { print "no package.version in $library_manifest"; return 1 }
    new="package { name = \"avin-library\", version = \"$version\" }"
    [[ $old == $new ]] || print -r -- "${text/$old/$new}" > $library_manifest

    grep -qxF "package { name = \"avin-library\", version = \"$version\" }" $library_manifest || { print "the stamp did not take in $library_manifest"; return 1 }
    grep -qxF "const Version: string = \"$version\"" $version_file || { print "the stamp did not take in $version_file"; return 1 }
    grep -qxF "  version = \"$version\"" $manifest || { print "the stamp did not take in $manifest"; return 1 }

    if [[ -z $(git -C $repo status --porcelain) ]]; then
        print -r -- "already stamped $version"
        return 0
    fi

    git -C $repo diff --stat
    local changed
    changed=$(git -C $repo diff --name-only | sort | tr '\n' ' ')
    [[ $changed == "compiler/dev/avinlang/compiler/cli.sysl compiler/sysl.hocon library/library.hocon " ]] || { print "the stamp touched $changed"; return 1 }

    git -C $repo commit -q -am "avin $version" || return 1
    git -C $repo log -1 --oneline
}

step 02-version stamp
release_commit=$(git -C $repo rev-parse HEAD)
say "release commit: $release_commit"

# --- 3. the compiler -----------------------------------------------------------------------------

sysl_version=$(sysl --version 2>&1)
sysl_version=${sysl_version%%$'\n'*}
say "built with: $(command -v sysl) -- $sysl_version"

build_compiler() {
    rm -f $out/avin
    heavy $repo sysl build compiler $opt -o $out/avin || return 1
    [[ -x $out/avin ]] || { print "the build wrote no $out/avin"; return 1 }
    otool -L $out/avin
}

step 03-compiler build_compiler

# --- 4. the prefix -------------------------------------------------------------------------------

stage=$out/stage
cache=$out/cache

stage_prefix() {
    rm -rf $stage $cache
    mkdir -p $stage/bin $stage/share/avin/editors $stage/share/avin/lib || return 1
    cp $out/avin $stage/bin/avin || return 1
    # Only what git tracks: no build directories, caches or editor droppings.
    local tree
    for tree in library runtime; do
        mkdir -p $stage/share/avin/$tree || return 1
        git -C $repo archive HEAD $tree | tar -x -C $stage/share/avin || return 1
    done
    cp $repo/editors/vscode/syntaxes/avin.tmLanguage.json $repo/editors/vscode/syntaxes/lavin.tmLanguage.json \
        $stage/share/avin/editors/ || return 1

    # The runtime's archive, built by the staged compiler from the staged runtime into a cache of its
    # own: the one archive that build makes is the one an installed compiler looks for.
    mkdir -p $out/prime || return 1
    print -r -- 'main() = println("primed")' > $out/prime/prime.avin
    (cd $out/prime && env -u AVIN_ROOT AVIN_CACHE=$cache $stage/bin/avin build prime.avin -o prime) || return 1
    [[ $($out/prime/prime) == primed ]] || { print "the priming program did not run"; return 1 }
    local built=($cache/runtime/libavinrt-*.a(N))
    (( ${#built} == 1 )) || { print "expected one runtime archive in $cache/runtime, found ${#built}"; return 1 }
    cp $built[1] $stage/share/avin/lib/ || return 1
    ls -la $stage/share/avin/lib
}

step 04-stage stage_prefix

# --- 5. the tarball ------------------------------------------------------------------------------

package() {
    rm -f $tarball
    # No AppleDouble files: `-C stage .` makes the tarball a prefix, and brew installs all of it.
    COPYFILE_DISABLE=1 tar -czf $tarball -C $stage . || return 1
    (cd $out && shasum -a 256 $asset > $asset.sha256) || return 1
    tar -tzf $tarball | grep -v '^\./share/avin/\(library\|runtime\)/.'
    cat $out/$asset.sha256
}

step 05-package package
sha=$(cut -d' ' -f1 $out/$asset.sha256)
size=$(stat -f %z $tarball)

# --- 6. from the tarball, not the tree -----------------------------------------------------------

# Outside the repository, so nothing the checkout holds can be found by accident.
scratch=$(mktemp -d -t avin-release) || die "cannot make a scratch directory"
prefix=$scratch/prefix
work=$scratch/work
vcache=$scratch/cache

# The installed compiler, as a user runs it: no AVIN_ROOT, and a cache of its own.
installed() { env -u AVIN_ROOT AVIN_CACHE=$vcache $prefix/bin/avin "$@" }

unpack() {
    mkdir -p $prefix $work || return 1
    tar -xzf $tarball -C $prefix || return 1
    [[ -x $prefix/bin/avin && -f $prefix/share/avin/library/avin/prelude.avin && -d $prefix/share/avin/runtime ]] ||
        { print "the tarball is not a prefix"; return 1 }
    # Read-only, as a Homebrew keg is to its user: a compiler that writes into its prefix fails here.
    chmod -R a-w $prefix || return 1
    ls -la $prefix $prefix/bin $prefix/share/avin $prefix/share/avin/lib
}

version_check() {
    local said
    said=$(cd $work && installed --version) || return 1
    print -r -- $said
    [[ $said == "avin $version" ]]
}

hello() {
    mkdir -p $work/hello || return 1
    print -r -- 'main()
    println("Hello, Avin!")
    println(6 * 7)' > $work/hello/hello.avin

    (cd $work/hello && installed build hello.avin -o hello) || return 1
    local said
    said=$($work/hello/hello) || return 1
    print -r -- $said
    [[ $said == $'Hello, Avin!\n42' ]] || { print "the built program printed the wrong thing"; return 1 }

    # With only the system's tools on PATH: Apple's clang, and no sysl.
    said=$(cd $work/hello && env -u AVIN_ROOT AVIN_CACHE=$vcache PATH=/usr/bin:/bin $prefix/bin/avin run hello.avin) || return 1
    print -r -- $said
    [[ $said == $'Hello, Avin!\n42' ]] || { print "'avin run' printed the wrong thing"; return 1 }

    # The shipped archive was linked: nothing was built into the cache, so Sysl was never run.
    [[ ! -d $vcache/runtime ]] || { print "the installed compiler built a runtime rather than linking the shipped one"; ls -la $vcache/runtime; return 1 }
    [[ ! -d $vcache/shims ]] || { print "hello reached a Sysl binding"; ls -la $vcache/shims; return 1 }
}

bindings() {
    mkdir -p $work/bindings || return 1
    print -r -- 'main() = println("avin".toUpperCase())' > $work/bindings/upper.avin
    local said
    said=$(cd $work/bindings && installed run upper.avin) || return 1
    print -r -- $said
    [[ $said == AVIN ]] || { print "the program with a binding printed the wrong thing"; return 1 }
    ls -la $vcache/shims
}

new_package() {
    (cd $work && installed new greeter) || return 1
    (cd $work/greeter && installed test .) > $out/package-test.out 2>&1
    local rc=$?
    cat $out/package-test.out
    (( rc == 0 )) || return 1
    grep -qE '^[1-9][0-9]* passed, 0 failed' $out/package-test.out || { print "no passing summary"; return 1 }
    local said
    said=$(cd $work/greeter && installed run .) || return 1
    print -r -- $said
    [[ $said == "Hello, world!" ]]
}

lsp() {
    local init='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}}'
    local down='{"jsonrpc":"2.0","id":2,"method":"shutdown"}'
    local quit='{"jsonrpc":"2.0","method":"exit"}'
    local said
    said=$(cd $work && printf 'Content-Length: %d\r\n\r\n%s' ${#init} $init ${#down} $down ${#quit} $quit | installed lsp --stdio) ||
        { print -r -- $said; return 1 }
    print -r -- $said
    [[ $said == *'"id":1'* && $said == *'"capabilities"'* ]] || { print "no answer to initialize"; return 1 }
}

step 06a-unpack unpack
step 06b-version version_check
step 06c-hello hello
step 06d-bindings bindings
step 06e-package new_package
step 06f-lsp lsp

package_summary=$(grep -E '^[0-9]+ passed' $out/package-test.out | tail -1)

# The scratch prefix was made read-only, which `rm -rf` cannot remove.
chmod -R u+w $scratch
rm -rf $scratch

# --- 7. the formula, and what is left ------------------------------------------------------------

formula=$out/avin.rb
template=$repo/packaging/homebrew/avin.rb
[[ -f $template ]] || die "no formula template at $template"
text=$(< $template)
text=${text/'version "0.0.0"'/"version \"$version\""}
text=${text/'sha256 "SHA256"'/"sha256 \"$sha\""}
print -r -- $text > $formula
grep -qF "version \"$version\"" $formula && grep -qF "sha256 \"$sha\"" $formula ||
    die "the formula template at $template has no 'version \"0.0.0\"' or no 'sha256 \"SHA256\"' to fill in"

say ""
say "== done: $tarball"
say "   size $size bytes, sha256 $sha"
say "   built with $sysl_version"
say "   avin test on a new package: $package_summary"
for t in $took; say "   $t"
say "   formula: $formula"

cat <<STEPS | tee -a $log

== What is left, and NOT done by this script ==

1. Push the stamp and tag it:
     git -C $repo push origin HEAD:dev
     git -C $repo tag -a v$version $release_commit -m "avin $version"
     git -C $repo push origin v$version

2. The GitHub release:
     gh release create v$version --repo avin-lang/avin --latest --title "avin $version" --notes "<notes>" $tarball

3. Hash the asset the release URL serves, not the local file:
     curl -sL -o $out/dl.tar.gz https://github.com/avin-lang/avin/releases/download/v$version/$asset
     shasum -a 256 $out/dl.tar.gz
   It must print $sha.

4. The formula: copy $formula to Formula/avin.rb in the avin-lang/homebrew-tap repository,
   then commit and push the tap.

5. Install and test it as work:
     sudo -n -u work -H /opt/homebrew/bin/brew update
     sudo -n -u work -H /opt/homebrew/bin/brew install avin-lang/tap/avin
     sudo -n -u work -H /opt/homebrew/bin/brew test avin
STEPS
