# hasher

A parallel file hasher. It walks a directory tree, sends each path down a bounded channel to a pool
of worker threads, and each worker reads its file 64 KiB at a time into a SHA-256 hasher. The digests
come back on a second channel and are printed sorted by path, `digest  path` as checksum tools write
them, followed by the groups of files with the same contents. A file that cannot be read is reported
on standard error and the rest go on. With no directory given it writes a small tree to a temporary
directory, hashes that, and removes it.

```
avin build . -o hasher
./hasher ~/Documents 8
```
