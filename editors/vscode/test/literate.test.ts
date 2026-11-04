import { test } from "node:test"
import assert from "node:assert/strict"
import { readFileSync } from "node:fs"

const read = (path: string) => JSON.parse(readFileSync(new URL(path, import.meta.url), "utf8"))
const manifest = read("../package.json")
const grammar = read("../syntaxes/lavin.tmLanguage.json")

test("a .lavin file is Literate Avin, served by the same language server", () => {
    const language = manifest.contributes.languages.find((l: { id: string }) => l.id === "avin-literate")
    assert.deepEqual(language.extensions, [".lavin"])
    assert.ok(manifest.activationEvents.includes("onLanguage:avin-literate"))
    const entry = manifest.contributes.grammars.find((g: { language: string }) => g.language === "avin-literate")
    assert.equal(entry.scopeName, grammar.scopeName)
    assert.equal(entry.embeddedLanguages["meta.embedded.block.avin"], "avin")
    assert.ok(readFileSync(new URL("../src/extension.ts", import.meta.url), "utf8").includes("language: \"avin-literate\""))
    read("../" + language.configuration.slice(2))
})

test("a line four columns in opens the program, and a line of prose ends it", () => {
    const begin = new RegExp(grammar.repository.program.begin)
    const end = new RegExp(grammar.repository.program.end)
    assert.ok(begin.test("    main() = 0"))
    assert.ok(begin.test("        nested"))
    assert.ok(!begin.test("   three columns"))
    assert.ok(!begin.test("prose"))
    assert.ok(!end.test("    still the program"))
    assert.ok(!end.test(""))
    assert.ok(!end.test("   "))
    assert.ok(end.test("Prose again."))
    assert.ok(end.test("```"))
    assert.deepEqual(grammar.repository.program.patterns, [{ include: "source.avin" }])
})

test("a fence opens at fewer than four columns and closes with its own character, at least as long", () => {
    const open = new RegExp(grammar.repository.fence.begin)
    assert.ok(open.test("```"))
    assert.ok(open.test("   ~~~~ text"))
    assert.ok(!open.test("    ```"))
    const avin = new RegExp(grammar.repository["fence-avin"].begin)
    assert.ok(avin.test("```avin"))
    assert.ok(!avin.test("```avinx"))
    const close = (opener: string) => new RegExp(grammar.repository.fence.end.replace("\\1", opener))
    assert.ok(close("```").test("```"))
    assert.ok(close("```").test("`````  "))
    assert.ok(!close("````").test("```"))
    assert.ok(!close("```").test("``` x"))
})

test("a list item runs on through blank and indented lines, and ends at the margin", () => {
    const begin = new RegExp(grammar.repository.list.begin)
    const end = new RegExp(grammar.repository.list.end)
    assert.ok(begin.test("- item"))
    assert.ok(begin.test("12) item"))
    assert.ok(!begin.test("-item"))
    assert.ok(!end.test(""))
    assert.ok(!end.test("      an example under the item"))
    assert.ok(end.test("Back at the margin."))
})
