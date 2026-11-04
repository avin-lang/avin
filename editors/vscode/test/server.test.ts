import { test } from "node:test"
import assert from "node:assert/strict"
import { chmodSync, mkdtempSync, writeFileSync } from "node:fs"
import { tmpdir } from "node:os"
import { join } from "node:path"
import { DEFAULT_SERVER_PATH, findExecutable, missingServerMessage, serverCommand } from "../src/server.ts"

test("the server command defaults to avin lsp --stdio", () => {
    assert.deepEqual(serverCommand(undefined, undefined, {}), { command: DEFAULT_SERVER_PATH, args: ["lsp", "--stdio"], env: {} })
    assert.deepEqual(serverCommand("  ", "  ", {}), { command: "avin", args: ["lsp", "--stdio"], env: {} })
})

test("a configured path is used, trimmed", () => {
    assert.equal(serverCommand(" /opt/x/avin ").command, "/opt/x/avin")
})

test("a repository runs the compiler through sysl run, with AVIN_ROOT and the checkout as cwd", () => {
    const s = serverCommand("/ignored/avin", "/src/avin", { HOME: "/h" })
    assert.equal(s.command, "sysl")
    assert.deepEqual(s.args, ["run", "/src/avin/compiler", "--", "lsp", "--stdio"])
    assert.equal(s.env.AVIN_ROOT, "/src/avin")
    assert.equal(s.env.HOME, "/h")
    assert.equal(s.cwd, "/src/avin")
})

test("the environment is copied, never changed", () => {
    const base = { PATH: "/bin" }
    serverCommand(undefined, "/src/avin", base)
    assert.deepEqual(base, { PATH: "/bin" })
})

test("an executable is found on PATH or at its path, and a missing one is not", () => {
    const dir = mkdtempSync(join(tmpdir(), "avin-vscode-"))
    const exe = join(dir, "avin")
    writeFileSync(exe, "#!/bin/sh\n")
    chmodSync(exe, 0o755)
    const plain = join(dir, "notes")
    writeFileSync(plain, "")
    chmodSync(plain, 0o644)
    assert.equal(findExecutable("avin", { PATH: `/nonexistent:${dir}` }), exe)
    assert.equal(findExecutable(exe, {}), exe)
    assert.equal(findExecutable("./avin", {}, dir), exe)
    assert.equal(findExecutable("avin", { PATH: "/nonexistent" }), undefined)
    assert.equal(findExecutable("notes", { PATH: dir }), undefined, "a file that is not executable")
    assert.equal(findExecutable("avin", {}), undefined, "no PATH at all")
    assert.equal(findExecutable(dir, {}), undefined, "a directory")
})

test("the missing-server message names what was tried and every setting", () => {
    const installed = missingServerMessage(serverCommand("nosuch", undefined, {}), "not found")
    assert.match(installed, /'nosuch'/)
    assert.match(installed, /avin\.server\.path/)
    assert.match(installed, /avin\.server\.repository/)
    assert.match(installed, /highlighting still works/)
    const checkout = missingServerMessage(serverCommand(undefined, "/src/avin", {}), "not found")
    assert.match(checkout, /\/src\/avin/)
})
