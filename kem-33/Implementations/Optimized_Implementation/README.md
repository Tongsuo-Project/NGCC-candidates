# QUBE Optimized Implementation

This directory contains the x86-64/AVX2 optimized QUBE KEM implementation for
the API_PKC interface. It builds the optimized parameter sets `qube-1`,
`qube-3`, `qube-4`, and `qube-5`, corresponding to QUBE-128, QUBE-256,
QUBE-384, and QUBE-512.

## Layout

```text
Optimized_Implementation/
├── CMakeLists.txt
├── benchmark/
│   ├── benchmark.h
│   └── benchmark_kem.c
├── lib/
│   ├── api_pkc/            # API_PKC auxfunc/drng interfaces
│   ├── bitpolymul_x86/     # AVX2 polynomial multiplication backend
│   └── fips202/            # SHAKE/SHA3 backend used by symmetric.c
├── src/
│   ├── common/             # KEM wrapper, code, symmetric layer and utilities
│   └── x86/                # Common x86 code and qube-1/3/4/5 parameter sets
└── tests/
    ├── kat_kem.c           # API_PKC KAT driver
    ├── poly-test.h
    └── qube-test.c         # Functional test with built-in timing records
```

## Build

Build out of tree:

```bash
cmake -S . -B /tmp/qube-opt-build
cmake --build /tmp/qube-opt-build --parallel
```

Executables are written to `/tmp/qube-opt-build/bin/`.

On Apple Silicon, an x86_64 cross build can be generated with:

```bash
cmake -S . -B /tmp/qube-opt-x86-build -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build /tmp/qube-opt-x86-build --parallel
```

Running the generated binaries on Apple Silicon requires an x86_64 execution
environment such as Rosetta.

## Targets

For each enabled parameter set, CMake builds:

```text
qube-opt-1        kat-opt-1        benchmark-kem-1
qube-opt-3        kat-opt-3        benchmark-kem-3
qube-opt-4        kat-opt-4        benchmark-kem-4
qube-opt-5        kat-opt-5        benchmark-kem-5
```

`qube-6`, `qube-7`, and `qube-8` sources are kept in the tree but are not part
of the default optimized build.

## Test

Run the functional test for one parameter set:

```bash
/tmp/qube-opt-build/bin/qube-opt-1
```

The test program performs polynomial multiplication checks, KEM encapsulation
and decapsulation checks, and prints timing records for keygen, encapsulation,
and decapsulation. A successful KEM run reports `TEST [1000] PASSED` and prints
matching shared secrets.

Generate KAT output:

```bash
/tmp/qube-opt-build/bin/kat-opt-1
```

## Benchmark

Run the standalone KEM benchmark:

```bash
/tmp/qube-opt-build/bin/benchmark-kem-1
/tmp/qube-opt-build/bin/benchmark-kem-3
/tmp/qube-opt-build/bin/benchmark-kem-4
/tmp/qube-opt-build/bin/benchmark-kem-5
```

The benchmark warms up key generation, encapsulation, and decapsulation, checks
that the encapsulated and decapsulated shared secrets match, then reports
average cycle and wall-clock timings.

## Cleanup

Out-of-tree CMake build directories under `/tmp` are local scratch space and
should not be committed. Remove them when they are no longer needed:

```bash
rm -rf /tmp/qube-opt-build /tmp/qube-opt-x86-build
```
