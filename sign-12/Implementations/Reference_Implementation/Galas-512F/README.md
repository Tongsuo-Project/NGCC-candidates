# Galas reference implementation

This directory contains the portable C reference implementation for the NGCC
submission of the Galas signature scheme.

The implementation follows the API_PKC signature interface through
`ngcc/SIG_AlgorithmInstance.{c,h}`. The cryptographic code is in `galas/`, and
the NGCC auxiliary code is in `ngcc/`.

## Instances

Eight instances are supported:

```text
GALAS_160S  GALAS_160F
GALAS_256S  GALAS_256F
GALAS_384S  GALAS_384F
GALAS_512S  GALAS_512F
```

The corresponding public labels are `Galas-160S`, `Galas-160F`, ...,
`Galas-512F`.

## Building and testing

The Makefile uses the reference self-evaluation profile
`-std=c99 -Wall -Wextra -Wpedantic -O2` by default:

```bash
make check
make test_all
make kat_all
make bench_all REPS=100
```

`make kat_all` regenerates the eight KAT files under `../../kat/` and verifies
each file immediately after generation. The KAT driver preserves the
`Seed_Len` and `Seed` fields from the API_PKC template and uses them to seed
the deterministic key generation path.

To build one instance manually, pass the instance macro explicitly, for
example:

```bash
gcc -std=c99 -Wall -Wextra -Wpedantic -O2 \
  -DGALAS_INSTANCE=GALAS_256S \
  -Igalas -Ingcc ngcc/kat_gen.c galas/*.c ngcc/SIG_AlgorithmInstance.c \
  ngcc/auxfunc.c ngcc/drng.c -o kat_gen_256s
```

The Makefile is the recommended entry point because it uses the exact source
list needed by the signature implementation.

## API and randomness

The reference implementation uses:

- `pseudoXOF` through the Galas XOF/random-oracle wrapper;
- API_PKC DRNG for deterministic KAT key generation;
- the NGCC `SIG_AlgorithmInstance` functions for key generation, signing, and
  verification.

The benchmark target uses a deterministic 64-byte message and reports average
latency, cycles, throughput, key/signature sizes, and peak resident set size.

The key layout is `pk = x || y` and `sk = x || k`, where
`y = Gala_k(x)`.
