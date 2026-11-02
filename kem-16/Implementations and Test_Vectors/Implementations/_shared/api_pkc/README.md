# API_PKC Helper Source

Package-local ICCS API_PKC helper subset used for deterministic KAT generation,
KAT replay, and initial self-evaluation (`auxfunc`, `drng`, and `KAT_KEM`).

The helper source is kept API-compatible and C99 warning-clean in this package.
It is an evaluation dependency, not a claim of production-hardened hash/XOF or
random-generation primitives.
