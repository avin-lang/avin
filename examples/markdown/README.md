# markdown

Turns Markdown into HTML: headings, paragraphs, bulleted and numbered lists, quotations, fenced code
with its language, and rules, and inside a line code spans, strong text, emphasis and links. Each
line is first classified as a `data Line`, and the block reader gathers runs of lines of one kind into
`data Block` values, a quotation's lines being read again as blocks of their own; the HTML of each
block is a function written as clauses, one per kind. Everything is escaped for HTML, code exactly as
written. It writes a whole page, or with `--fragment` only the body: `avin run main.avin --fragment <
../markdown.in`.
