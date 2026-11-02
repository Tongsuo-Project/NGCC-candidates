# HARE-128 / KR / x86 optimized implementation

Active HARE v1.5 KR `[51,41]_2` covering-code backend.

## Role

x86 AVX2/PCLMUL performance implementation using the shared optimized kernels and the same API/KAT format as Reference.

API algorithm name: `HARE-128-kr`.

## Public API sizes

| Field | Bytes |
|---|---:|
| Public key | 2629 |
| Expanded secret key | 2677 |
| Ciphertext | 4688 |
| Shared secret | 16 |

The expanded secret key layout is `ekKEM || dkPKE || sigma || seedKEM`.  The
seed-only reconstruction size is `16` bytes, but this implementation
uses the expanded API layout above.

## Main parameters

| Parameter | Value |
|---|---:|
| `PARAM_N` | 20899 |
| `PARAM_K` (bytes) | 16 |
| `PARAM_OMEGA = PARAM_OMEGA_R` | 79 |
| `PARAM_OMEGA_E` | 145 |
| `PARAM_N1` | 32 |
| `PARAM_N2` | 640 |
| `PARAM_L1 = PARAM_N1 * PARAM_N2` | 20480 |
| `PARAM_ALPHA` | 11 |
| `PARAM_COMP_N` | 51 |
| `PARAM_COMP_K` | 41 |
| covering radius | 2 |

## Files

```text
api.h                         public KEM byte sizes and instance name
parameters.h                  HARE instance parameters and derived sizes
reed_solomon.h                parameter-derived Reed-Solomon declarations
KEM_AlgorithmInstance.c/.h    API_PKC-facing wrapper functions
```
