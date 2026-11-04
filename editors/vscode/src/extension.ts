import * as vscode from "vscode"
import { LanguageClient, type LanguageClientOptions, type ServerOptions, TransportKind } from "vscode-languageclient/node"
import { findExecutable, missingServerMessage, serverCommand, SERVER_SETTINGS } from "./server"

let client: LanguageClient | undefined

// Whether the missing-server message has been shown since the extension started, or since the user
// last asked for a restart: an unavailable server is said once, not on every file opened.
let warned = false

function warnOnce(message: string): void {
    if (warned) return
    warned = true
    void vscode.window.showWarningMessage(message)
}

async function start(): Promise<void> {
    const config = vscode.workspace.getConfiguration("avin")
    const server = serverCommand(config.get<string>("server.path"), config.get<string>("server.repository"))

    if (findExecutable(server.command, server.env, server.cwd) === undefined) {
        warnOnce(missingServerMessage(server, "not found"))
        return
    }

    const serverOptions: ServerOptions = {
        command: server.command,
        args: server.args,
        transport: TransportKind.stdio,
        options: { env: server.env, cwd: server.cwd },
    }
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: "file", language: "avin" }],
        outputChannelName: "Avin",
    }

    const starting = new LanguageClient("avin", "Avin", serverOptions, clientOptions)
    try {
        await starting.start()
        client = starting
    } catch (err) {
        warnOnce(missingServerMessage(server, String(err)))
        try {
            await starting.dispose()
        } catch {
            // The client never ran, so there is nothing left to stop.
        }
    }
}

async function stop(): Promise<void> {
    const running = client
    client = undefined
    await running?.stop()
}

async function restart(): Promise<void> {
    await stop()
    await start()
}

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    context.subscriptions.push(
        vscode.commands.registerCommand("avin.restartServer", async () => {
            warned = false
            await restart()
        }),
        vscode.workspace.onDidChangeConfiguration(async e => {
            if (SERVER_SETTINGS.some(s => e.affectsConfiguration(s))) {
                warned = false
                await restart()
            }
        }),
        { dispose: () => void stop() },
    )
    await start()
}

export async function deactivate(): Promise<void> {
    await stop()
}
