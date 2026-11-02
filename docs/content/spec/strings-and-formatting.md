---
title: Strings and formatting
summary: Sysl's full interpolation and formatting surface, rendered through Display and Debug.
weight: 21
---

Avin has Sysl's string literal, interpolation and formatting surface. The normative reference for
the details is Sysl's `docs/content/reference/strings.md`; Avin differs only where noted here.

| Form | Meaning |
| --- | --- |
| `"…"` | Ordinary string with escapes |
| `raw"…"` | Raw string, with no escape processing |
| Text blocks | Multi-line literals with Sysl's indentation and line-joining rules |
| `s"…"` | Interpolation: `$name`, `${expression}`, `$$` |
| `f"…"` | Formatted interpolation: a specifier after a hole (`${x}%08.3f`) |

Sysl's specifier grammar applies: conversion codes, flags, width, precision, sign and justification.
Width and precision may be expressions. Specifiers are type-checked at compile time against the
hole's type.

## Rendering

A hole is rendered by its type's `Display` implementation, or by `Debug` for `%?`. A value whose type
implements neither is a compile error. There is no fallback that prints an address. The specifier
applies to the whole rendered value.

## Lowering

The compiler keeps literal segments, typed holes, parsed specifiers and source spans in its
intermediate representation, then lowers an interpolation to direct `Writer` calls into a string
builder. Each hole is evaluated once, left to right, and no value is boxed to format it. User-defined
interpolators are not part of Avin.
