# wordfreq

Counts the words of a text and lists the commonest, each with a bar scaled to the first. Words are
runs of letters (an apostrophe inside one is kept), counted without regard to case unless
`--keep-case` is given; `--min-length` and `--skip-common` leave out short and common words, and
`--format csv` writes the counts for a spreadsheet. The words come out of a `for` comprehension over
the files and their lines, `groupMapReduce` counts them, and one `sortBy` on `(-count, word)` ranks
them so the list is the same on every run. Try it with `avin run main.avin --top 8 --skip-common <
../wordfreq.in`.
