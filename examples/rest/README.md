# rest

A REST service of to-do items, read and written as JSON through the `dev.avinlang.json` package's
derived codecs, and kept in memory in a map behind a `Mutex`. Routes take their parameters typed
(`paramAs[int]`, `queryOption[bool]`) and their bodies decoded (`jsonBody[Draft]`), each with `?`,
so a malformed id, query or body is answered `400` or `415` with no code of the handler's own. `--try`
starts it on a free port and takes it through every route with the HTTP client, sixteen clients
adding items at once among them.

The package names `json` by a path in this repository's tree; a program of one's own depends on it
by its release instead: `json { git = "github.com/avin-lang/json", version = "0.0.1" }`.

```
avin run . -- 8080
curl -d '{"title": "write docs"}' -H 'Content-Type: application/json' localhost:8080/todos
```
