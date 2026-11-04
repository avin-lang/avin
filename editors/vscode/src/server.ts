// How the extension starts the Avin language server. Kept free of the `vscode` module so the tests
// can build the very same command line the extension does.

import { accessSync, constants, statSync } from "node:fs"
import { delimiter, isAbsolute, join } from "node:path"

export interface ServerCommand {
    command: string
    args: string[]
    env: NodeJS.ProcessEnv
    cwd?: string
}

export const DEFAULT_SERVER_PATH = "avin"

// The settings that decide the server's process; changing one restarts it.
export const SERVER_SETTINGS = ["avin.server.path", "avin.server.repository"]

// The command line that runs the language server on stdio. With `repository` set, the compiler is
// run from that checkout through `sysl run`, with AVIN_ROOT naming the checkout so the server finds
// its library and runtime; otherwise the installed `avin` (or `configured`) is run as it is.
export function serverCommand(configured: string | undefined, repository?: string, base: NodeJS.ProcessEnv = process.env): ServerCommand {
    const repo = repository?.trim() ?? ""
    if (repo !== "") {
        return {
            command: "sysl",
            args: ["run", join(repo, "compiler"), "--", "lsp", "--stdio"],
            env: { ...base, AVIN_ROOT: repo },
            cwd: repo,
        }
    }
    const command = configured !== undefined && configured.trim() !== "" ? configured.trim() : DEFAULT_SERVER_PATH
    return { command, args: ["lsp", "--stdio"], env: { ...base } }
}

// Where `command` would be run from: itself when it names a path, otherwise the first match on the
// environment's PATH; undefined when there is no executable file there. Checking first keeps a
// missing server to one message of ours, rather than the client's own errors.
export function findExecutable(command: string, env: NodeJS.ProcessEnv, cwd?: string): string | undefined {
    const runnable = (file: string): boolean => {
        try {
            accessSync(file, constants.X_OK)
            return statSync(file).isFile()
        } catch {
            return false
        }
    }
    if (command.includes("/")) {
        const file = isAbsolute(command) || cwd === undefined ? command : join(cwd, command)
        return runnable(file) ? file : undefined
    }
    for (const dir of (env.PATH ?? "").split(delimiter)) {
        if (dir === "") continue
        const file = join(dir, command)
        if (runnable(file)) return file
    }
    return undefined
}

// The message shown once when the server cannot be started. Highlighting keeps working without it.
export function missingServerMessage(server: ServerCommand, reason: string): string {
    const what = server.cwd !== undefined
        ? `'sysl run' on the checkout at ${server.cwd}`
        : `'${server.command}'`
    return `Could not start the Avin language server (${what}: ${reason}). Syntax highlighting still works. ` +
        `Put avin on your PATH, or set "avin.server.path" to it, or "avin.server.repository" to a checkout.`
}
