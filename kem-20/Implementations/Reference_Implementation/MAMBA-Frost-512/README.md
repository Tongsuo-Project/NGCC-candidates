# MAMBA-Frost-512 API_PKC reference wrapper

This directory preserves the official `kem_*` API and KAT generator. The
adapter calls the existing Frost-512 core directly. `randombytes_adapter.c`
routes core randomness to the official deterministic `drng_algorithm` context,
which makes API_PKC KAT output reproducible without changing the core RNG API.

The local `common/sha3/fips202.c` file is an API_PKC auxiliary adapter: Frost `shake128`/`shake256` call sites are routed to `pseudoXOF()` from the official `auxfunc.c`. The official `auxfunc.c/.h`, `drng.c/.h`, and `KAT_KEM.c` files are included verbatim.

Build with `make`, run 1000 agreement tests with `make check`, and verify KAT
reproducibility with `make kat-repro`.
