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
| `textDocument/references` | Every place the program names what the name under the cursor names, in every file of the program and the library, in the order of their files; the declaration's own name with them when the request's `includeDeclaration` asks for it |
| `textDocument/documentHighlight` | The places in the document that name the same thing, the declaration marked as a write and each use as a read |
| `textDocument/semanticTokens/full` | Every name the checker resolved in the document, classified by what it names; the legend is below |

A call through a method table goes to the method the receiver's static type declares, and a call of
an overloaded function to the overload the checker chose. References follow the same choice: the
references of a trait's method are the calls made through the trait, and those of a class's
override are the calls on that class, so asking from `Shape.area` does not list a call the checker
resolved to `Circle.area`.

### Semantic tokens

Names are coloured by what the checker resolved them to, so a field and a local of the same name,
or a function and a variable, are told apart wherever they are written. Keywords, literals,
operators and comments are left to the editor's grammar.

| Token type | Names |
| --- | --- |
| `namespace` | A module, in an import and before a member read through it |
| `class` | A class or an object |
| `struct` | A data type with one variant, and that variant constructing it |
| `enum` | A data type with several variants |
| `interface` | A trait |
| `typeParameter` | A type parameter |
| `enumMember` | A variant of a data type with several |
| `function` | A top-level or local function |
| `method` | A method of a class, a trait or an impl |
| `property` | A field |
| `parameter` | A parameter |
| `variable` | A local or a module-level value |

| Modifier | Meaning |
| --- | --- |
| `declaration` | The name where it is declared |
| `readonly` | A `val`: a local, a field or a module value that cannot be assigned |
| `static` | A static member, a member of the type rather than of its values |
| `defaultLibrary` | Declared in the standard library |

A name the checker reached no declaration for has no token, so while a syntax error stops the
analysis the editor's grammar alone colours the file.

Positions are the protocol's: a line counted from zero and a character counted in UTF-16 code
units, so a column after a non-ASCII character is the one the editor shows.

A program with [Sysl bindings](/spec/sysl-bindings/) has their shims checked by Sysl when it is
built; the server reports what the Avin parser and checker find.
