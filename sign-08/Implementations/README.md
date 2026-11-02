# DARTS Implementations

This directory contains the updated implementations of the DARTS lattice-based digital signature scheme.

## Directory Structure

```text
Implementations/
├── Reference_Implementation/
│   ├── DARTS128/
│   ├── DARTS256/
│   └── DARTS512/
├── Optimized_Implementation/
│   ├── DARTS_AVX2/
│   │   ├── DARTS128/
│   │   ├── DARTS256/
│   │   └── DARTS512/
│   └── DARTS_neon/
│       ├── DARTS128/
│       ├── DARTS256/
│       └── DARTS512/
├── Additional_Implementation/
│   └── DARTS_FIPS202/
│       ├── Implementations/
│       │   ├── Reference_Implementation/
│       │   │   ├── DARTS128/
│       │   │   ├── DARTS256/
│       │   │   └── DARTS512/
│       │   └── Optimized_Implementation/
│       │       ├── DARTS128/
│       │       ├── DARTS256/
│       │       └── DARTS512/
│       ├── KAT/
│       └── helper_scripts/
└── Third Party License/
```

- `Reference_Implementation`: portable reference code.
- `DARTS_AVX2`: optimized implementation for x86-64 processors with AVX2.
- `DARTS_neon`: optimized implementation for ARM processors with NEON.
- `DARTS_FIPS202`: reference and optimized variants using FIPS 202 primitives.
- `DARTS128`, `DARTS256`, and `DARTS512`: the three DARTS parameter sets.
- `KAT`: known-answer-test generation and results.
- `helper_scripts`: parameter, security, and size estimation scripts.

## Main Files in Each Parameter Set

| File or directory | Purpose |
|---|---|
| `sign.c`, `sign.h` | Key generation, signing, and verification |
| `params.h`, `config.h` | Parameter-set constants and configuration |
| `poly.c`, `polyvec.c`, `polymat.c` | Polynomial, vector, and matrix operations |
| `ntt.c`, `fft.c` | Transform implementations |
| `sampler.c`, `polyfix.c`, `fixpoint.c` | Sampling and fixed-point arithmetic |
| `packing.c`, `encoding.c`, `rans_byte.h` | Serialization and rANS compression |
| `symmetric.c`, `fips202.c` | Hash and extendable-output functions |
| `KAT_SIG.c`, `PQCgenKAT_sign.c` | Known-answer-test programs |
| `precomputations_rANS/` | rANS statistics and precomputed tables |
| `test/` | Correctness tests and benchmarks |
