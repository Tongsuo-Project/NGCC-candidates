OPS Signature AVX2 Optimized Implementation

This directory contains the AVX2-optimized implementation in submission
directory `OPSsig-128-avx2` for the OPS signature algorithm.

It corresponds to the C reference implementation in
`Implementations/Reference_Implementation/OPSsig-128` and uses the 128-bit
parameter set implemented by this codebase. The directory now also exports the
self-evaluation artifacts required by the x86 guideline.

Main files

sign.c                      Core key generation, signing and verification logic
sign.h                      Public internal API for the signature implementation
SIG_AlgorithmInstance.c     ICCS SIG interface adapter
SIG_AlgorithmInstance.h     ICCS SIG interface declarations and metadata

packing.c / packing.h       Public key, secret key and signature packing helpers
poly.c / poly.h             Polynomial arithmetic, sampling and packing helpers
polyvec.c / polyvec.h       Polynomial vector and matrix helpers
ntt.c / ntt.h               NTT and inverse NTT
reduce.c / reduce.h         Modular reduction helpers
rounding.c / rounding.h     Rounding and hint helpers
params.h                    Scheme parameter definitions
sign.c                      Also contains the random byte wrapper used by the implementation
auxfunc.c / .h              ICCS auxiliary hash/XOF functions
drng.c / .h                 ICCS deterministic random number generator

Build targets

Makefile                    Build rules for the AVX2 implementation
make test                   Builds `test_OPSsig`
make speed                  Builds `test_speed_OPSsig`
make mem                    Builds `test_mem_OPSsig`
make kat                    Builds `kat_sig`
make vectors_test           Builds `test_vectors-260602`
make ntt_test               Builds `test_ntt_260602`
make selfeval               Builds and exports self-evaluation artifacts
make package                Creates a filtered submission archive

Notes

1.  The KAT generator writes
    `Test_Vectors/avx2/OPSsig-128/KAT_SIG_OPSsig-128-avx2.txt`.
2.  The default optimized build flags follow the x86 guideline profile
    `-O3 -march=x86-64 -mavx2 -mtune=native -flto -fomit-frame-pointer`
    plus the common C99 warning options.
3.  If POPCNT-specific tuning is needed, use `make ENABLE_POPCNT=1 ...` and
    record the reason in the dependency or report materials.
4.  The shared self-evaluation framework is located in
    `../../../Self_Evaluation`.
5.  The hash and XOF operations use the ICCS auxiliary functions provided in
    `auxfunc.c` and `auxfunc.h`.
6.  The algorithm instance string used by the API adapter is `OPSsig-128`,
    matching the corresponding reference implementation directory.
7.  The memory benchmark uses `getrusage()` and `/proc/self/status` to export
    runtime peak-memory measurements for at least 100 signing iterations.
8.  Final submission packaging should use the naming rule
    `数字签名-OPSsig-128-x86-性能优化版` .
