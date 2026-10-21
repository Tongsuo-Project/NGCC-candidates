# HARE-512 / KR / Reference implementation

Active HARE v1.5 KR `[51,41]_2` covering-code backend.

## Role

Portable ISO C99 implementation using the shared Reference core and API_PKC helper interface.

API algorithm name: `HARE-512-kr`.

## Public API sizes

| Field | Bytes |
|---|---:|
| Public key | 21812 |
| Expanded secret key | 22004 |
| Ciphertext | 39294 |
| Shared secret | 64 |

The expanded secret key layout is `ekKEM || dkPKE || sigma || seedKEM`.  The
seed-only reconstruction size is `64` bytes, but this implementation
uses the expanded API layout above.

## Main parameters

| Parameter | Value |
|---|---:|
| `PARAM_N` | 173981 |
| `PARAM_K` (bytes) | 64 |
| `PARAM_OMEGA = PARAM_OMEGA_R` | 259 |
| `PARAM_OMEGA_E` | 449 |
| `PARAM_N1` | 151 |
| `PARAM_N2` | 1152 |
| `PARAM_L1 = PARAM_N1 * PARAM_N2` | 173952 |
| `PARAM_ALPHA` | 13 |
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
