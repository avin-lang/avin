# logsum

Summarizes a web server's request log kept as CSV: requests and bytes sent, how the requests ended,
the busiest routes with their average, 95th-percentile and slowest times, and the server errors by
hour. The columns are found by name from the header line, quoted fields may hold commas and doubled
quotes, and a row that is not a request is reported on standard error and skipped, through
`partitionMap` over `Result`s. Clauses with comparison patterns name a status's outcome
(`outcome(200 <= s < 300) = "success"`) and print a size in B, KiB or MiB; `--by method` or
`--by status` groups by another column. Try it with `avin run main.avin --top 3 < ../logsum.in`.
