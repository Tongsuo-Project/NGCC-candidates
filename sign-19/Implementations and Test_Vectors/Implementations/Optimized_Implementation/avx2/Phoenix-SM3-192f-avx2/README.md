# Phoenix-SM3-192f-avx2

This directory contains the AVX2-optimized implementation for the
`Phoenix-SM3-192f` signature algorithm instance.

## Build and generate KAT vectors

```sh
make kat
```

The generated vector file is written to:

```text
output/KAT_SIG_Phoenix-SM3-192f-avx2.txt
```

Expected SHA-256:

```text
9dbe82b720c02e9da61c8d4e3e97db151882002d2563cdf1c24bff65dad855df
```

See `MODIFICATION_NOTES.md` for the changes from the original GitHub reference
implementation and reproduction steps.

## File Structure

This directory contains the AVX2-optimized implementation for the Phoenix signature
algorithm. The file structure is as follows:

### Core Implementation Files

| File | Description |
|------|-------------|
| `api.h` | Cryptographic API interface declarations |
| `sign.c` | Core signing and verification logic |
| `gwots.c` | GWOTS+ signature scheme implementation |
| `gwotsx1.c` | GWOTS+ single-path implementation |
| `tfors.c` | TFORS signature scheme implementation |
| `merkle.c` | Merkle tree implementation |
| `octopus.c` | Octopus hash function implementation |
| `hash_sm3.c` | SM3 hash function wrapper |
| `thash_sm3_simple.c` | Simple tree hash implementation |
| `utils.c` | Utility functions |
| `utilsx1.c` | Single-path utility functions |
| `utilsx8.c` | 8-way parallel utility functions (AVX2) |
| `address.c` | Address management |
| `counter.c` | Counter implementation |

### AVX2 Optimized Files

| File | Description |
|------|-------------|
| `sm3_avx2.c` | SM3 AVX2 8-way parallel implementation |
| `sm3_x8.c` | 8-way parallel SM3 wrapper |
| `sm3_x4.c` | 4-way parallel SM3 wrapper (NEON compatible) |
| `sm3_neon.c` | SM3 NEON 4-way parallel implementation |
| `hash_sm3x8.c` | 8-way parallel prf_addr implementation |
| `thash_sm3_simplex8.c` | 8-way parallel tree hash implementation |

### Header Files

| File | Description |
|------|-------------|
| `api.h` | Public API interface |
| `address.h` | Address structure definitions |
| `counter.h` | Counter interface |
| `context.h` | Context structure definition |
| `drng.h` | DRNG interface |
| `hash.h` | Generic hash function interface |
| `hashx8.h` | 8-way parallel hash interface |
| `merkle.h` | Merkle tree interface |
| `octopus.h` | Octopus interface |
| `params.h` | Parameter definitions |
| `randombytes.h` | Random number generation interface |
| `sm3.h` | SM3 hash function interface |
| `sm3_internal.h` | Internal SM3 interface (AVX2/NEON) |
| `tfors.h` | TFORS interface |
| `thash.h` | Tree hash interface |
| `thashx8.h` | 8-way parallel tree hash interface |
| `utils.h` | Utility functions interface |
| `utilsx1.h` | Single-path utility functions interface |
| `utilsx8.h` | 8-way parallel utility functions interface |
| `gwots.h` | GWOTS+ interface |
| `gwotsx1.h` | GWOTS+ single-path interface |

### External Dependencies

| File | Description |
|------|-------------|
| `sm3.c` | SM3 hash function implementation |

### KAT Testing

| File | Description |
|------|-------------|
| `KAT_SIG.c` | KAT test main program |
| `SIG_AlgorithmInstance.c` | Algorithm instance interface implementation |
| `SIG_AlgorithmInstance.h` | Algorithm instance interface declaration |
| `drng.c` | Deterministic random number generator |

### Build Configuration

| File | Description |
|------|-------------|
| `Makefile` | Build script with AVX2 optimizations |
| `params/` | Parameter files directory |

### Documentation

| File | Description |
|------|-------------|
| `README.md` | This file, project description |
| `MODIFICATION_NOTES.md` | Modification notes document |
