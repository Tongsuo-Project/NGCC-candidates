# YuanYang DSA Optimized Implementation

This directory contains a self-contained DSA implementation for YuanYang-512,
YuanYang-1024, and YuanYang-2048. Select a parameter set with `PARAM=512`,
`PARAM=1024`, or `PARAM=2048`. The tree does not compile files from the
reference implementation.

## Targets

- `make PARAM=<set> all`: build the KAT tools, benchmark programs, and
  functional test for one parameter set.
- `make PARAM=<set> KAT_SIG`: build the NGCC-style signature KAT generator.
- `make PARAM=<set> check_kat`: build the deterministic KAT replay checker.
- `make PARAM=<set> verify_kat`: build the verifier-only KAT checker.
- `make PARAM=<set> benchmark`: run the full keygen/sign/verify benchmark.
- `make PARAM=<set> benchmark_batch`: run the full signing versus offline
  presampling benchmark.
- `make PARAM=<set> benchmark-generic`: run the benchmark with AVX2 disabled
  at compile time and the generic runtime path selected.
- `make PARAM=<set> kat_ngcc`: generate, replay, and verify deterministic
  signature KATs with `-DNGCC_KATS`.
- `make PARAM=<set> functional_test`: build a keygen/sign/verify and
  malformed-input test.
- `make benchmark-all`: run `benchmark` for 512, 1024, and 2048.
- `make benchmark-batch-all`: run `benchmark_batch` for all parameter sets.
- `make kat-all`: run `kat_ngcc` for all parameter sets.
- `make clean`: remove local build products and generated KAT output.

## Layout

- `Makefile`: parameter-selecting build and validation targets.
- `functional_test.c`: API and malformed-input regression test.
- `cpu_features.[ch]`: x86 feature detection and runtime dispatch.
- `sysrng.[ch]`: operating-system RNG backend used outside deterministic KAT
  builds.
- `yuanyang-512/`, `yuanyang-1024/`, `yuanyang-2048/`: parameter-set-local DSA
  sources, tables, KAT tools, benchmarks, and `keygen/ntrugen` solver copies.

The optimized implementation keeps integer signing and verification hot paths
with optional AVX2 dispatch. Floating-point code remains in key generation.
