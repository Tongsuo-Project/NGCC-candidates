# MAMBA-Frost-192 optimized AVX2 API_PKC KEM instance

This directory wraps the existing Frost-192 optimized x86 implementation under the official API_PKC KEM interface (KEM_AlgorithmInstance.h/.c). It does not change Frost parameters, key formats, ciphertext formats, shared-secret formats, FO/implicit-rejection logic, or RNG call semantics.

## Parameters

| field | bytes |
|---|---:|
| public key | 9712 |
| secret key | 11528 |
| ciphertext | 9760 |
| shared secret | 24 |

The sizes come from Frost/src/api_frost192.h and are checked by params.h against the API_PKC macros.

## API mapping

| API_PKC function | Frost optimized core function |
|---|---|
| kem_keygen | crypto_kem_keypair_Frost192 |
| kem_enc | crypto_kem_enc_Frost192 |
| kem_dec | crypto_kem_dec_Frost192 |

## Optimized source path

The build uses _FAST_, -mavx2, and -maes. The AVX2/AES-NI path is provided by:

- Frost/src/frost192.c
- Frost/src/kem.c
- Frost/src/noise.c
- Frost/src/frost_macrify.c
- Frost/src/util.c
- common/aes/aes_ni.c
- common/sha3/fips202.c


rost_macrify_reference.c is intentionally not included in this optimized directory.

## Commands

`sh
make
make check
make kat-generate
make kat-repro
make kat-install
make perf-run
make clean
`

make check runs 1000 keypair/encaps/decaps agreement rounds. make kat-repro generates the KAT twice and compares the files. make perf-run writes perf_MAMBA-Frost-192.csv with average cycles and ops/s for keypair, encaps, and decaps.

## Requirements

- GCC or Clang on x86/x86_64.
- CPU support for AVX2 and AES-NI. make runs check_avx2 before executing optimized binaries.
- No third-party library is required for the default AES128 matrix backend; AES-NI sources are compiled from this directory; hash/XOF calls go through the API_PKC `pseudoXOF()` adapter.
