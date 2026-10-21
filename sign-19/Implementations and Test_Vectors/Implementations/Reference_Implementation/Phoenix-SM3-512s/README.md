# Phoenix-SM3-512s

This directory contains the standalone reference implementation for the
`Phoenix-SM3-512s` signature algorithm instance.

Build and generate KAT vectors:

```sh
make kat
```

The generated vector file is written to:

```text
output/KAT_SIG_Phoenix-SM3-512s.txt
```

Expected SHA-256:

```text
9dbe82b720c02e9da61c8d4e3e97db151882002d2563cdf1c24bff65dad855df
```

See `MODIFICATION_NOTES.md` for the changes from the original GitHub reference
implementation and reproduction steps.
