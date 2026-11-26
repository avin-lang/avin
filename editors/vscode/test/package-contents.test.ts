import { test } from "node:test"
import assert from "node:assert/strict"
import { execFileSync } from "node:child_process"
import { existsSync, readFileSync } from "node:fs"
import { mayStartServer, SERVER_SETTINGS } from "../src/server.ts"

const root = new URL("../", import.meta.url)
const manifest = JSON.parse(readFileSync(new URL("package.json", root), "utf8"))

const relative = (file: string): string => file.replace(/^\.\//, "")

const languages: any[] = manifest.contributes.languages
const grammars: any[] = manifest.contributes.grammars

const packaged: Set<string> = new Set(
    execFileSync("npx", ["--no-install", "vsce", "ls", "--no-dependencies"], {
        cwd: root,
        encoding: "utf8",
    })
        .split("\n")
        .map((line) => line.trim().replaceAll("\\", "/"))
        .filter((line) => line.length > 0),
)

const built = existsSync(new URL(relative(manifest.main), root))

function named(file: string, what: string, generated = false): void {
    const path = relative(file)
    test(`${what} ${path} is present and packaged`, () => {
        if (generated && !built) {
            // out/ only exists after `npm run build`; without it the ignore
            // file must at least let the bundle through.
            const ignore = readFileSync(new URL(".vscodeignore", root), "utf8").split("\n")
            assert.ok(ignore.includes("!" + path), `.vscodeignore must re-include ${path}`)
            return
        }
        assert.ok(existsSync(new URL(path, root)), `${path} is missing on disk`)
        assert.ok(packaged.has(path), `${path} is excluded by .vscodeignore`)
    })
}

named(manifest.main, "main", true)
named(manifest.icon, "icon")
for (const language of languages) {
    named(language.configuration, `configuration of ${language.id}`)
    for (const variant of Object.values<string>(language.icon ?? {})) named(variant, `icon of ${language.id}`)
}
for (const grammar of grammars) named(grammar.path, `grammar of ${grammar.language}`)

test("the extension stays on in an untrusted workspace and restricts the server settings", () => {
    const capability = manifest.capabilities?.untrustedWorkspaces
    assert.equal(capability?.supported, "limited")
    assert.ok(capability.description.length > 0)
    assert.deepEqual([...capability.restrictedConfigurations].sort(), ["avin.server.path", "avin.server.repository"])
})

test("the restricted settings are exactly the ones the extension reacts to", () => {
    assert.deepEqual([...manifest.capabilities.untrustedWorkspaces.restrictedConfigurations].sort(), [...SERVER_SETTINGS].sort())
    for (const setting of SERVER_SETTINGS) assert.ok(setting in manifest.contributes.configuration.properties, setting)
})

test("the server starts only in a trusted workspace", () => {
    assert.equal(mayStartServer(false), false)
    assert.equal(mayStartServer(true), true)
})

test("every language has a configuration, so none is left without comment toggling", () => {
    for (const language of languages) assert.ok(language.configuration, language.id)
})
