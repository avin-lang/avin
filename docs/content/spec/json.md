---
title: JSON
summary: "JSON is the package dev.avinlang.json, not a module of the standard library: depending on it, and what an import of avin.json says."
weight: 29
---

JSON is **the package `dev.avinlang.json`**, written in Avin in its own repository,
`github.com/avin-lang/json`, and not a module of the standard library. A program adds it to its
dependencies, and its module's import binds `json`:

```text
dependencies {
  json { git = "github.com/avin-lang/json", version = "0.0.1" }
}
```

```avin
import dev.avinlang.json
import dev.avinlang.json.{Json, JsonEncoder, JsonDecoder}
```

The package's README is its reference: `Json` values, reading text with the line and column of any
error, writing compact, indented or ASCII text, the accessors that take a value apart, the streaming
`JsonReader` over any `avin.io.Reader`, and a program's own types through `derives JsonEncoder,
JsonDecoder`. Those derivations are ordinary `derive` templates in the package's own module, found
as any library's are ([derivation](/spec/derivation/)); the compiler knows nothing of JSON.

## `import avin.json`

An import of `avin.json`, whole, of a declaration in it, of a group or under another name, is an
error naming the package:

```text
error: JSON is the package `dev.avinlang.json`; add it to your dependencies
 --> script.avin:1:8
```

So is `import json` in a program that has no module or declaration of its own named `json` and does
not depend on the package:

```text
error: there is no module `json`; JSON is the package `dev.avinlang.json`; add it to your dependencies
```

A program that does depend on it and writes `import json` is told the module is missing, as for any
other name; one with a module of its own named `json` imports that module as `json` and may reach the
package beside it under another name, `import dev.avinlang.json as lib`.
