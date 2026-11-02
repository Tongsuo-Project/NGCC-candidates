# QIMEN-PIKE Optimized Implementation

This directory contains the optimized QIMEN-PIKE source tree. It keeps the same public variant names, CMake target names, and KEM interfaces as `Implementations/`, while using x86-64 BMI2/ADX assembly hot paths for NGCC-1, NGCC-2, and NGCC-3.

## Requirements

Use an x86-64 CPU with BMI2 and ADX support:

```bash
lscpu | grep -E 'bmi2|adx'
```

## Build

From the package root:

```bash
cmake -S Optimized_Implementation -B build_optimized -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON
cmake --build build_optimized -j
```

The NGCC XOF/hash backend defaults to `sm3`; `shake` is also supported:

```bash
cmake -S Optimized_Implementation -B build_optimized -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON -DPIKE_NGCC_XOF_BACKEND=sm3
cmake -S Optimized_Implementation -B build_optimized -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON -DPIKE_NGCC_XOF_BACKEND=shake
```

## Test

Run optimized finite-field tests:

```bash
ctest --test-dir build_optimized -R 'pike_test_gf_ngcc_[123]_(fp|fp2)$' --output-on-failure
```

Run protocol and KEM smoke tests:

```bash
for v in 1 2 3; do
  ./build_optimized/src/pike/ref/NGCC_${v}/test/pike-test_ngcc_${v} 100
  ./build_optimized/src/pike/ref/NGCC_${v}/test/pike-kem-test_ngcc_${v} 100
  ./build_optimized/src/pike/ref/pikex_compressed/NGCC_${v}/test/pike-compressed-test_ngcc_${v} 100
  ./build_optimized/src/pike/ref/pikex_compressed/NGCC_${v}/test/pike-compressed-kem-test_ngcc_${v} 100
done
```

## KAT Generation

Generate optimized KAT files:

```bash
cmake --build build_optimized --target kat-ngcc-1 kat-ngcc-2 kat-ngcc-3
```

By default, outputs are written to the package-level `Test_Vector/` directory with an `-Optimized` suffix:

```text
Test_Vector/KAT_KEM_NGCC-{1,2,3}-Optimized.txt
```

To use another output directory:

```bash
cmake -S Optimized_Implementation -B build_optimized -DNGCC_PIKE_KAT_OUTPUT_DIR=/tmp/qimen-kat
```

For benchmarking, pass loop counts to the field or KEM test binaries and compare against a reference build from `Implementations/` on the same machine.

## Benchmark Summary

Measured on the local x86-64 BMI2/ADX machine with Release builds and the default `shake` backend.

Finite-field basic operators use 10000 repetitions and median cycles/op:

| Variant | Field | op | Reference | Optimized |
| --- | --- | --- | ---: | ---: |
| NGCC-1 | GF(p) | add/sub/mul/sqr | 36 / 30 / 157 / 130 | 24 / 20 / 146 / 143 |
| NGCC-2 | GF(p) | add/sub/mul/sqr | 55 / 43 / 376 / 296 | 37 / 29 / 322 / 296 |
| NGCC-3 | GF(p) | add/sub/mul/sqr | 114 / 88 / 1523 / 1101 | 70 / 46 / 1348 / 1119 |
| NGCC-1 | GF(p^2) | add/sub/mul/sqr | 48 / 44 / 600 / 386 | 38 / 26 / 475 / 319 |
| NGCC-2 | GF(p^2) | add/sub/mul/sqr | 99 / 80 / 1339 / 881 | 70 / 44 / 1214 / 785 |
| NGCC-3 | GF(p^2) | add/sub/mul/sqr | 200 / 172 / 5041 / 3347 | 123 / 90 / 4300 / 2858 |

KEM demos use 20 runs and average million cycles:

| Variant | op | Reference (M cycles) | Optimized (M cycles) |
| --- | --- | ---: | ---: |
| NGCC-1 | keygen/encaps/decaps | 278.5 / 89.4 / 133.7 | 222.0 / 72.0 / 107.9 |
| NGCC-2 | keygen/encaps/decaps | 1087.4 / 304.5 / 465.3 | 1030.2 / 272.0 / 417.9 |
| NGCC-3 | keygen/encaps/decaps | 11824.1 / 2461.0 / 3682.5 | 10491.5 / 2082.4 / 3112.3 |
