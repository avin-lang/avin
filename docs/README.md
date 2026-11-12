# Avin documentation

This directory is the Juicer site published at **https://avin-lang.github.io/avin/** (and at
**https://avinlang.dev/** once its DNS points there). Pages link to one another by site paths
(`/spec/`), so read them on the site rather than here. Pages live in
`content/`, configuration in `site.toml`, and copied static files in `static/`.
Keep language documentation and compiler changes in the same repository.

From the repository root, with Juicer installed:

```sh
juicer build -s docs -d docs/public
juicer serve -s docs -L
```

The preview is available at http://localhost:8080/. Generated files in `public/`
are ignored. `.github/workflows/docs.yml` builds the site on every push to `dev` that touches
`docs/` and deploys it to GitHub Pages, for the address in the repository variable
`DOCS_BASE_URL`, or the Pages address until it is set.

Pages use YAML frontmatter with `title`, `summary`, and optional navigation
`weight`. Start Markdown headings at `##`, since the theme supplies the page title.
Use `text` fences for illustrative Avin snippets until syntax highlighting exists.

A complete program a page shows as one is run-checked: open its fence with ```` ```text run ````
and follow it with a ```` ```text output ```` fence holding exactly what it prints (prose may sit
between the two). The compiler's corpus test (`corpus_tests.sysl`) builds every such program at
`-O0` and `-O2` and fails, naming the page and line, when it does not compile, does not exit 0, or
prints anything else. The words after `text` do not change how the page renders. A fragment that is
not a whole program stays an unmarked `text` fence.

`spec/` says what the language **is**, never what the compiler happens to do today. Implementation
status belongs only on `compiler/roadmap.md`. `spec/decisions.md` is the single list of decisions.

The `juicerdocs` and `juicercommon` themes are vendored from the local
`~/dev/juicer/docs/themes/` checkout, with its ISC license in `themes/LICENSE`.
They include prebuilt CSS and JavaScript, so the installed native Juicer command
can build the site without an asset toolchain. Make site-specific overrides
outside the vendored themes.
