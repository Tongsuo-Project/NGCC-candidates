# Mithril-128 optimized implementation

This self-contained PKE/KEM implementation uses x86-64 AVX2 arithmetic and
the same submission SM3/pseudoXOF hash path as the reference implementation.

## Parameters

`N=128`, `K=5`, `ELL=1`, `LOGQ=13`, `LOGP=11`,
`LOGT=3`, `LOG_ETA=1` (eta = 2). The secret coefficients are uniform
in `{-2,-1,0,1}`. Public and secret seeds are 64 bytes each.

| Object | Bytes |
| --- | ---: |
| PKE secret key | 160 |
| KEM public key | 944 |
| KEM secret key | 1136 |
| Ciphertext | 928 |
| Message / shared secret | 16 |

The Makefile fixes this directory to security-level identifier `128`.
Use the matching parameter directory when building another parameter set.

## Arithmetic

The ring uses `y^128 = -1` and `x^K = y+2`. The 32-bit NTT uses
auxiliary prime 53501953 (`0x03306001`). Public polynomials are prepared
in the Awin layout and multiplied with secret vectors using the AVX2
row-dot kernels in `arith/pointwise.S`. The helper prime is an arithmetic
implementation choice, distinct from the scheme modulus q.

## Build and test

GCC and GNU Make are required. Use an x86-64 CPU supporting AVX2, BMI2,
and POPCNT. The default flags tune the build for the local CPU.
Run from this directory:

```sh
make test run=1
make KAT run=1
```

The test target runs the functional and unit suites.

KAT output is written to `output/KAT_KEM_KEM_RRLWR128.txt`. It should
match the corresponding reference/optimized output and the package vector
`Test_Vectors/KAT_KEM_Mithril-128.txt`.

Use `make clean` to remove generated executables and local `output/` files.

## Benchmarks

```sh
make speed run=1
make breakdown run=1
```

The speed harness reports x86-64 RDTSC ticks.

[Historical benchmark data](../../BENCHMARK_HISTORY.md#avx2-baseline)
retains the previous measurements and host details for the next retest.
The recorded values have not been refreshed during this cleanup.
See the [package README](../../README.md) for the aggregate benchmark
procedure and the supported implementation layout.
