import { test } from "node:test"
import assert from "node:assert/strict"
import { readFileSync } from "node:fs"

const config = JSON.parse(readFileSync(new URL("../language-configuration.json", import.meta.url), "utf8"))
const increase = new RegExp(config.indentationRules.increaseIndentPattern)
const decrease = new RegExp(config.indentationRules.decreaseIndentPattern)

test("a line that opens a block indents the next one", () => {
    for (const line of [
        "main()",
        "countPrimes(limit: int) -> int",
        "area(shape: Shape) -> real = shape match",
        "    val total =",
        "    xs.map(x ->",
        "    if n < 2",
        "    if ready then",
        "    elif late",
        "    else",
        "    while i < n do",
        "    for x in xs",
        "    loop",
        "class Buffer[T](private val items: Array[T]): Seq[T]",
        "data Shape",
        "trait Show",
        "object Buffer",
        "impl[T: Display] Display for Buffer[T]",
        "    else // otherwise",
    ]) assert.ok(increase.test(line), line)
})

test("a line that is complete in itself does not", () => {
    for (const line of [
        "    println(total)",
        "    if n < 2 then return false",
        "    while i < n do i += 1",
        "    val x = 1",
        "    return a == b",
        "    a <= b",
        "    val same = a ==",
        "class Point(x: int, y: int) = derived",
    ]) assert.ok(!increase.test(line), line)
})

test("else and elif dedent to their if", () => {
    assert.ok(decrease.test("    else"))
    assert.ok(decrease.test("    elif late"))
    assert.ok(!decrease.test("    elsewhere()"))
})

test("comments are // and there are no block comments", () => {
    assert.equal(config.comments.lineComment, "//")
    assert.equal(config.comments.blockComment, undefined)
})
