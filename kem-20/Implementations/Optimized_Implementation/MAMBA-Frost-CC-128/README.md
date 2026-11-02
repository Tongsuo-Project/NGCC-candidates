# MAMBA-Frost-CC-128 optimized AVX2 API_PKC KEM instance

This directory wraps the existing Frost-CC-128 optimized AVX2 implementation under the official API_PKC KEM interface (KEM_AlgorithmInstance.h/.c). It does not change Frost-CC parameters, key formats, ciphertext formats, shared-secret formats, FO/implicit-rejection logic, or RNG call semantics.

## Parameters

| field | bytes |
|---|---:|
| public key | 5152 |
| secret key | 6736 |
| ciphertext | 5192 |
| shared secret | 16 |

The sizes come from Frost-CC/src/api_frostcc128.h and are checked by params.h against the API_PKC macros.

## API mapping

| API_PKC function | Frost-CC core function |
|---|---|
| kem_keygen | crypto_kem_keypair_FrostCC128 |
| kem_enc | crypto_kem_enc_FrostCC128 |
| kem_dec | crypto_kem_dec_FrostCC128 |

## Source path

- Frost-CC/src/frostcc128.c
- Frost-CC/src/kem.c
- Frost-CC/src/noise.c
- Frost-CC/src/util.c
- Frost-CC/src/frost_macrify.c
- common/aes/aes_ni.c
- common/sha3/fips202.c

## Commands

`sh
make
make check
make kat-generate
make kat-repro
make kat-install
make clean
`

Optimized builds additionally support `make perf-run` and require x86/x86_64 AVX2 plus AES-NI CPU support.

KAT mode uses the official API_PKC DRNG through `randombytes_adapter.c`. Frost-CC `shake128`/`shake256` call sites are routed by `common/sha3/fips202.c` to `pseudoXOF()` from the official `auxfunc.c`.
