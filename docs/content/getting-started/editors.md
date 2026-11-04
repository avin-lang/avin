---
title: Editors
summary: The language server, `avin lsp`, and what it answers.
weight: 10
---

`avin lsp` is a language server: it speaks the Language Server Protocol over standard input and
output, so any editor with a protocol client can show Avin's diagnostics, types and declarations as
you type. The answers come from the compiler itself, from the same parse and the same checking that
`avin check` does, so the server and the build never disagree about a program.

## Running it

An editor starts the server as a command and talks to it over its standard input and output:

```sh
avin lsp
```

`avin lsp --stdio` is the same command; the flag is what many protocol clients pass to say which
transport they use, and standard input and output is the one there is. The server finds the
library as the other commands do: through `AVIN_ROOT`, or beside the compiler. It runs until the
editor sends `shutdown` and then `exit`, and ends with status 0; an editor that closes the input
without them, or a message whose framing cannot be read, ends it with status 1.

Unsaved text is analyzed as it stands in the editor: the server reads an open document's text in
place of the file on disk, for every file of the program, and writes nothing anywhere.

## Which program a file belongs to

Every change is answered by checking the whole program the file belongs to, together with the
library, as `avin check` would check it:

- A file under the library's `library/` directory is checked as the library, module `avin` and
  the modules under it.
- A file whose `module a.b` header matches where it sits, `<root>/a/b/file.avin`, is checked with
  the program rooted at `<root>`.
- A file with no header is a root file of the program in its directory when that directory has
  modules in directories below it.
- Any other file is a program by itself.

## What it answers

The server announces exactly these capabilities in its answer to `initialize`:

| Request | Answer |
| --- | --- |
| `textDocument/didOpen`, `didChange`, `didSave`, `didClose` | Full-text synchronization: each change carries the document's whole text |
| `textDocument/publishDiagnostics` | Syntax errors, the first on each line, and the checker's errors, for every file of the program, the library's included, with an empty list for a file that has become clean. A syntax error stops the analysis at the parse, as it stops a build |
| `textDocument/hover` | For the name under the cursor, what the checker knew about it, as Avin code: a local's, a parameter's, a field's or a module value's type (`val p: Point`), a function's or a method's signature (`Point.norm() -> int`), a type's declaration, a variant with its fields. The `//` or `/** … */` comment directly above the declaration follows it |
| `textDocument/definition` | Where the name under the cursor was declared: a local, a parameter, a function, a method, a field, a type, a variant, a module-level value, an imported name or module, in the same file, another file of the program or the library |
| `textDocument/documentSymbol` | The document's outline: classes, traits and objects with their fields and methods, data types with their variants, impls with their methods, functions and module values. It is read from the parse, so it is there while the program has mistakes in it |

A call through a method table goes to the method the receiver's static type declares, and a call of
an overloaded function to the overload the checker chose.

Positions are the protocol's: a line counted from zero and a character counted in UTF-16 code
units, so a column after a non-ASCII character is the one the editor shows.

A program with [Sysl bindings](/spec/sysl-bindings/) has their shims checked by Sysl when it is
built; the server reports what the Avin parser and checker find.
