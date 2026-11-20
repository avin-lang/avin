# pipeline

A three-stage pipeline of threads joined by bounded channels: one thread reads the lines of an access
log, three parse them into events, and the main thread tallies what each user did. A bounded channel
makes a fast stage wait for a slow one, so nothing piles up in memory; closing a channel tells the next
stage the input has ended, `for line in lines` stops once it is drained, and a `select` with an
`after` arm watches for a stage that stalls. The program shows threads in a `scope`, `Channel`,
`Atomic`, `select`, pattern matching on the split words of a line, and a `for`/`yield` over options.

```
avin run . 2000
```
