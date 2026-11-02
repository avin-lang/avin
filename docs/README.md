# Avin documentation

This directory is the Juicer site for **https://avinlang.dev/**. Pages live in
`content/`, configuration in `site.toml`, and copied static files in `static/`.
Keep language documentation and compiler changes in the same repository.

From the repository root, with Juicer installed:

```sh
juicer build -s docs -d docs/public
juicer serve -s docs -L
```

The preview is available at http://localhost:8080/. Generated files in `public/`
are ignored. `static/CNAME` supplies the intended domain for GitHub Pages; hosting
and DNS have not been configured by this setup.

Pages use YAML frontmatter with `title`, `summary`, and optional navigation
`weight`. Start Markdown headings at `##`, since the theme supplies the page title.
Use `text` fences for illustrative Avin snippets until syntax highlighting exists.

`spec/` says what the language **is**, never what the compiler happens to do today. Implementation
status belongs only on `compiler/roadmap.md`. `spec/decisions.md` is the single list of decisions.

The `juicerdocs` and `juicercommon` themes are vendored from the local
`~/dev/juicer/docs/themes/` checkout, with its ISC license in `themes/LICENSE`.
They include prebuilt CSS and JavaScript, so the installed native Juicer command
can build the site without an asset toolchain. Make site-specific overrides
outside the vendored themes.
