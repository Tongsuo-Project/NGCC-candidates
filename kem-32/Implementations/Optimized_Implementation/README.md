# QCTM optimized implementation

This directory contains the optimized implementation for QCTM128, QCTM256, and
QCTM512.

Each parameter-set directory includes:

- `KEM_AlgorithmInstance.c`
- `KEM_AlgorithmInstance.h`
- `CryptHash_AlgorithmInstance.c`
- `CryptHash_AlgorithmInstance.h`

`KEM_AlgorithmInstance.*` is the adapter for the official `API_PKC.zip` KEM KAT
interface.

`CryptHash_AlgorithmInstance.*` is the SHAKE256/XOF interface used by the
optimized implementation for key-generation stream expansion and KEM
shared-secret derivation. QCTM128 and QCTM256 use 256-bit internal seeds and
32-byte shared secrets; QCTM512 uses a 512-bit internal seed and a 64-byte
shared secret.
