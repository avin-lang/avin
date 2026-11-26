# kvstore

A key-value store served over TCP with a line protocol: `SET`, `GET`, `DEL`, `INCR`, `KEYS` and
`QUIT`, each answered on one line. A single `Store` actor owns the data, so commands from any number
of clients are applied one at a time without a lock; a `TcpServer` serves each connection in an actor
of its own, which parses a line, asks the store and writes the answer, and waits for the next line
through `server.idle` so a stop ends an idle connection at once. `--try` starts it on a free port and
talks to it with its own client: a scripted session, then four clients counting at once.

```
avin run . -- 6380
nc localhost 6380
```
