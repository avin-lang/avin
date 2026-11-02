---
title: Getting started
summary: What exists today, and how to build the documentation.
weight: 5
---

Avin is at the start of its [roadmap](/compiler/roadmap/): the [specification](/spec/) is written,
and the compiler is being built to it. There is no compiler to install yet. This page will say how
to build one and run its tests when [stage 1](/compiler/roadmap/#stages) lands.

## Prerequisites

- Sysl 0.8.13, with checkouts of `parsing` and `gc` at `../sysl-lang/` relative to the repository
- clang on `PATH`
- Juicer, for the documentation

## Documentation

From the repository root:

```sh
juicer build -s docs -d docs/public
juicer serve -s docs -L
```
