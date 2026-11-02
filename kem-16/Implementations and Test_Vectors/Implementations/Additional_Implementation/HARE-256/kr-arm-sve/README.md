# HARE-256 / KR / ARM/SVE Additional implementation

Active HARE v1.5 KR `[51,41]_2` covering-code backend.

## Role

AArch64 ARMv8.2-A/SVE implementation using the shared ARM/SVE kernels; the GF2X base multiply uses NEON PMULL when enabled.

API algorithm name: `HARE-256-kr`.

## Public API sizes

| Field | Bytes |
|---|---:|
| Public key | 6580 |
| Expanded secret key | 6676 |
| Ciphertext | 11790 |
| Shared secret | 32 |

The expanded secret key layout is `ekKEM || dkPKE || sigma || seedKEM`.  The
seed-only reconstruction size is `32` bytes, but this implementation
uses the expanded API layout above.

## Main parameters

| Parameter | Value |
|---|---:|
| `PARAM_N` | 52379 |
| `PARAM_K` (bytes) | 32 |
| `PARAM_OMEGA = PARAM_OMEGA_R` | 131 |
| `PARAM_OMEGA_E` | 224 |
| `PARAM_N1` | 81 |
| `PARAM_N2` | 640 |
| `PARAM_L1 = PARAM_N1 * PARAM_N2` | 51840 |
| `PARAM_ALPHA` | 9 |
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
