class Avin < Formula
  desc "Natively compiled, garbage-collected application language"
  homepage "https://github.com/avin-lang/avin"
  # `scripts/release.sh <version>` writes this file into its output directory with the version and
  # the tarball's sha256 filled in; that copy is the one that goes into the tap.
  version "0.0.0"

  # macOS arm64 only: the tarball is built on the author's machine by the release script, which
  # proves it from where it was unpacked before it is published.
  depends_on arch: :arm64
  depends_on :macos

  url "https://github.com/avin-lang/avin/releases/download/v#{version}/avin-#{version}-darwin-arm64.tar.gz"
  sha256 "SHA256"

  # Run-time dependencies. avin links every program with clang, which Xcode's command-line tools
  # provide. The runtime's archive ships prebuilt, so a program that reaches no Sysl binding needs
  # nothing more; a program that does -- the standard library's string case mapping, files,
  # processes and threads are bindings -- has its shims built by the Sysl compiler, which brings
  # LLVM's archiver with it.
  depends_on "sysl-lang/tap/sysl"

  # A collection out of Sysl is converted by two generic shim overloads, for a slice and a `Buf`,
  # that a program can reach at the same type argument, which needs Sysl 0.9.0; a handle passed to a
  # `&T` parameter is shared rather than copied, and a socket closes when its last copy goes, from
  # Sysl 0.9.1; the runtime's lightweight threads stand on `sysl.fiber` and `sysl.io.poll`, from Sysl
  # 0.9.6, with a stack's real bounds from 0.9.7; and a command's `detach`, the memory a process holds
  # and descriptors closed across a program's start come from Sysl 0.9.8; `exec` and
  # waiting for a child by its pid, and a missing process told as not found, from Sysl 0.9.9; and the
  # members of a process group and the children of a process from Sysl 0.9.10. The runtime's
  # package is built with Sysl 0.9.19 and names that release. A dependency cannot name a least
  # version, so it is checked here.
  MINIMUM_SYSL = "0.9.19".freeze

  def install
    sysl = Formula["sysl-lang/tap/sysl"]
    if sysl.any_version_installed? && sysl.version < Version.new(MINIMUM_SYSL)
      odie "avin needs Sysl #{MINIMUM_SYSL} or later; run `brew upgrade sysl-lang/tap/sysl` first"
    end

    # The tarball is already a prefix -- bin/avin, and share/avin holding the library, the runtime
    # and its prebuilt archive, and the editor grammars -- and the compiler finds share/avin beside
    # itself through the bin/ symlink. What it builds goes to the user's cache, never into the keg.
    prefix.install Dir["*"]
  end

  test do
    assert_match "avin #{version}", shell_output("#{bin}/avin --version")

    # A tarball without its library installs a compiler that starts and cannot compile anything.
    assert_predicate pkgshare/"library/avin/prelude.avin", :file?
    assert_predicate pkgshare/"runtime", :directory?
    assert_predicate pkgshare/"editors/avin.tmLanguage.json", :file?

    (testpath/"hello.avin").write <<~AVIN
      main()
          println("Hello, Avin!")
          println(6 * 7)
    AVIN

    # Drives the whole toolchain: the installed library, the shipped runtime archive, IR emission and
    # the link. 42 is computed by the compiled program, so a back end that got arithmetic wrong fails
    # here.
    ENV["AVIN_CACHE"] = (testpath/"cache").to_s
    assert_equal "Hello, Avin!\n42\n", shell_output("#{bin}/avin run #{testpath}/hello.avin")
  end
end
