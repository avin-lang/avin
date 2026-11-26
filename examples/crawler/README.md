# crawler

A concurrent web crawler. A fixed number of worker threads take URLs from a shared frontier channel,
fetch each page with the HTTP client, and add the links they find on the same site, resolved against
the page and stripped of fragments. A set behind a `Mutex` keeps each page to one fetch, and a
`WaitGroup` counting the pages still to crawl tells a closer thread when the frontier will get no
more, so it closes the channel and the workers end. `--try` crawls a small site it serves itself: pages
in cycles, a broken link, a link off the site, and a chain of posts deeper than the crawl goes.

```
avin run . -- http://localhost:8080/ 8
```
