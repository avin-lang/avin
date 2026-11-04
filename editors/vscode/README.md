# Avin for Visual Studio Code

Syntax highlighting and language-server support for Avin. Files ending in `.avin` get highlighting,
comment toggling, bracket and quote pairing, indentation after a line that opens a block, folding by
indentation, and, through `avin lsp`, whatever the language server offers: diagnostics as you type,
hover, go to definition, document symbols.

Literate files, ending in `.lavin`, are the language *Literate Avin*: a Markdown document whose
lines indented four columns are the program. The program blocks are highlighted as Avin, fenced
blocks as illustrations (as Avin when the fence is marked `avin`), headings, lists and the inline
Markdown of the prose as Markdown, and the language server answers in them as it does in `.avin`
files, at the document's own positions. `syntaxes/lavin.tmLanguage.json` is the grammar; its tests
are `samples/literate.lavin` in the grammar tests.

*An independent Avin extension for Visual Studio Code. It is not affiliated with or endorsed by
Microsoft or the Visual Studio Code project.*

## Install

The extension is not published to the Marketplace. Build the `.vsix` (see *Development*), then:

```
code --install-extension avin-0.1.0.vsix
```

or use *Extensions: Install from VSIX...* in the command palette.

## The language server

The extension runs `avin lsp --stdio`. `avin` is the compiler: build it from the repository root
with `sysl build compiler -o compiler/avin` and put `compiler/` on your `PATH`, or point
`avin.server.path` at it.

To run the server straight from a checkout, without building `avin` first, set
`avin.server.repository` to the checkout. The extension then runs
`sysl run <repository>/compiler -- lsp --stdio` with `AVIN_ROOT` set to the checkout, so a change to
the compiler is picked up by *Avin: Restart Language Server*. The first start compiles the compiler
and takes a while.

If the server cannot be started, the extension says so once and highlighting keeps working. Changing
either setting restarts the server, and so does *Avin: Restart Language Server*.

## Settings

| setting | default | meaning |
|---|---|---|
| `avin.server.path` | `avin` | the avin executable; run as `<path> lsp --stdio` |
| `avin.server.repository` | empty | a checkout to run the server from with `sysl run`, in place of `avin.server.path` |
| `avin.trace.server` | `off` | `messages` or `verbose` traces the protocol in the *Avin* output channel |

## Grammar

`syntaxes/avin.tmLanguage.json` covers comments, keywords and modifiers, the built-in types and
type names, function and method declarations (including methods named by symbols), numbers in
every base, character and string literals with their escapes, interpolated strings (`s"…"`, `f"…"`
with format specifiers, `raw"…"`, and any other interpolator) with `$name`, `${expression}` and
`$$`, operators, and annotations such as `@test` and `@sysl("…")`.

Its tests run the grammar on the TextMate tokenizer an editor uses, against the annotated samples
in `editors/grammar-tests/samples/`. From the repository root:

```
sysl test editors/grammar-tests
```

They are a package of their own, apart from the compiler's tests, and need Oniguruma
(`brew install oniguruma`). In a sample, a line comment of carets names the scopes of the columns it
marks on the line above:

```
val total = count + 1
//  ^^^^^ variable.other.definition.avin
//                ^ keyword.operator.avin -comment
```

A name after `-` is a scope the marked text must not carry, and `// <---` marks the first columns of
the line, which a caret cannot reach.

## Development

```
npm install
npm run compile
npm test
npm run package
```

`npm run compile` type-checks and bundles the client into `out/extension.cjs`; `npm test` checks the
command line the client builds to start the server; `npm run package` writes `avin-<version>.vsix`.
