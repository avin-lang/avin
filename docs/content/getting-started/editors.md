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
library as the other commands do: in `share/avin` beside an installed compiler, in a source
checkout beside its `compiler/avin`, or through `AVIN_ROOT`. It runs until the
editor sends `shutdown` and then `exit`, and ends with status 0; an editor that closes the input
without them, or a message whose framing cannot be read, ends it with status 1.

Unsaved text is analyzed as it stands in the editor: the server reads an open document's text in
place of the file on disk, for every file of the program, and writes nothing anywhere.

A [literate](/spec/declarations/#literate-source) `.lavin` document is answered like an `.avin`
file: its program is the lines indented four columns, and every position the server reports or
reads, a diagnostic's range, a hover's, a definition's place and the outline's, is the document's
own line and column. The VS Code extension in `editors/vscode` registers `.lavin` files as
*Literate Avin*, highlights their program blocks as Avin and their prose as Markdown, and starts the
same server for them.

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
| `textDocument/documentSymbol` | The document's outline: classes, traits and objects with their fields and methods, data types with their variants, impls with their methods, functions and module values. A function's local functions are its children, at any depth, and a `@test` function carries the test's description as its detail, or `@test` where it has none. It is read from the parse, so it is there while the program has mistakes in it |
| `workspace/symbol` | The declarations of every program an open document belongs to, every file of it whether open or not (a dependency's files are left out): types, variants, classes, traits and objects, functions, module values, actors, machines with their states, and methods. Each names its module as its container, or for a method, a variant or a state its type (`geo.Point`), and its location is its name. A name matches when it holds the query's characters in order, not necessarily together, an ASCII letter matching either case (`pnt` finds `Point`); an empty query matches everything. At most 500 are listed |
| `textDocument/foldingRange` | Whole lines: each indented block from the line that opens it to its last line of code (a declaration's body; a class, impl, actor, machine or state body; a match's arms and an arm's body; any other block), a text block, a run of two or more comment lines (kind `comment`), the run of `import` lines (kind `imports`), and in a literate file each run of program text, its prose left out (kind `region`). Regions nest; none is one line |
| `textDocument/references` | Every place the program names what the name under the cursor names, in every file of the program and the library, in the order of their files; the declaration's own name with them when the request's `includeDeclaration` asks for it |
| `textDocument/documentHighlight` | The places in the document that name the same thing, the declaration marked as a write and each use as a read |
| `textDocument/prepareRename` | The name under the cursor and where it is written, or the reason it cannot be renamed. [Rename](#rename) below says which names may be |
| `textDocument/rename` | The edit that gives the name under the cursor a new name everywhere the program names it, in every file of the program |
| `textDocument/semanticTokens/full` | Every name the checker resolved in the document, classified by what it names; the legend is below |
| `textDocument/completion` | What may be written at the cursor, as the checker sees the scope there; triggered by `.` as well as by typing a name. [Completion](#completion) below says what each place offers |
| `textDocument/formatting` | The document in the style `avin fmt` writes, as one edit over the whole text; no edit when it is in the style already, and none while it does not parse. The request's options are not read: the style has no settings |
| `textDocument/rangeFormatting` | The top-level declarations the range touches, formatted as one edit over their whole lines; nothing outside them changes, text blocks keep every byte, and in a literate file only program lines are edited. No edit when they are in the style already, when the range holds no declaration, and none while the file does not parse |

A call through a method table goes to the method the receiver's static type declares, and a call of
an overloaded function to the overload the checker chose. References follow the same choice: the
references of a trait's method are the calls made through the trait, and those of a class's
override are the calls on that class, so asking from `Shape.area` does not list a call the checker
resolved to `Circle.area`.

### Rename

Rename is built on the same references. It renames locals, parameters, fields, methods, types, a
module's functions and values, pattern bindings and type parameters, with the names a call writes
for a parameter or a record's field (`scale(width = 3)`), in `.avin` and `.lavin` files alike. A
method is renamed together with the methods it overrides or implements and those that override or
implement it, so renaming `Shape.area` renames `Circle.area` too, from either end. A name written
with its module (`geo.area`) is replaced after the last dot, and an import's alias, which is a name
of its own, is left alone.

A rename is refused, with the reason as the error's message, when it would change what the program
means:

- the name is declared in the core library or a dependency, or a method of the family implements or
  overrides one that is (an `impl Add for Point`'s `add`);
- the name is a module or an operator;
- the new name is a reserved word, or is not a name (a letter or `_`, then letters, digits and `_`);
- the new name would clash with one in scope where the old one is used. The server checks the
  program as the edit would leave it: every name must still mean what it meant, and no new
  diagnostic may appear;
- the program has a syntax error, since the places of its names are not known.

### Completion

Completion is answered by the checker, at the place the cursor is, so what it offers is exactly
what would compile there:

| Where | What it offers |
| --- | --- |
| After `.` on a value | Its static type's members: fields, methods, inherited ones and those of the traits it conforms to, the methods impls add to it or to its traits, and an extension's where the file sees it; a private member only where the code may use it |
| After a module's name, `io.` | The module's public declarations |
| After a type's name, `Map.` | Its static members, and the members of its companion object |
| A name in an expression | The locals and parameters in scope at the cursor, then the enclosing class's members, then the module's declarations, then the file's imports, then the core library's, then the keywords that begin an expression: `val`, `var`, `if`, `match`, `for`, `while`, `loop`, `return`, `break`, `continue`, `true`, `false` |
| Where a type is written: after `:`, after `->`, in `[…]` | Types only: type parameters, the module's types, imported ones, the core library's and the built-in types |
| In an import, `import a.` or `import a.b.{…}` | The modules below the one named and its public declarations; inside braces, the declarations only |

Each item carries what hover would show for it, a signature or a type, and the comment above its
declaration, and is sorted by how near its scope is. The editor filters the list as you type. Where
the editor says at `initialize` that it expands snippets, a call is completed with a placeholder for
each argument without a default: `fetch(${1:thing}, ${2:times})`.

The line being typed rarely parses, so the server completes in a copy of the document: the word at
the cursor is replaced by a stand-in name, and where that still does not parse, the rest of the line
is replaced by the brackets that close it, then the lines after the cursor's declaration are blanked.
The first copy that parses is checked with the program, and what the checker found at the stand-in
is the answer. Nothing else is kept: the document's diagnostics, hover and references stay those of
its last change. In a literate document only the program is completed; the prose, comments and
strings offer nothing. A completion in the largest file of the library takes about 130 ms.

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
