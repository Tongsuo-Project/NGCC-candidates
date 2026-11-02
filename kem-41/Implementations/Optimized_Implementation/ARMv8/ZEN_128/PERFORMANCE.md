# ZEN_128 ARM Performance

This file records the build method, test platform, and measured performance for
the GNU/Linux AArch64 optimized ZEN_128 implementation.

## Build

Default benchmark build:

```sh
make clean
make benchmark
```

Default KAT build:

```sh
make KAT_KEM
./KAT_KEM
```

Default compiler configuration from `Makefile`:

```text
CC ?= gcc
CFLAGS ?= -O3 -march=armv8.2-a+crypto+sha3+dotprod+sve -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra
```

Benchmark command used for the result below:

```sh
ZEN_BENCH_PIN_CPU=0 ZEN_BENCH_CPU_HZ=2000000000 ./benchmark
```

The benchmark runs 1000 measured iterations per operation by default, after 10
warm-up iterations. It writes `benchmark_ZEN_128_raw.csv` and
`benchmark_ZEN_128_summary.json`.

## Test Platform

| Item | Value |
| --- | --- |
| Cloud host | Huawei Cloud ARM instance |
| Architecture | aarch64 |
| CPU vendor | HiSilicon |
| BIOS model | virt-6.2 CPU @ 2.0GHz |
| OS | Ubuntu 24.04.4 LTS |
| Kernel | Linux 6.8.0-106-generic |
| Compiler | GCC 13.3.0 |
| Online CPUs | 2 |
| Memory | 5583998976 bytes |
| CPU features | fp asimd aes pmull sha1 sha2 crc32 sha3 sm3 sm4 sha512 sve asimddp i8mm bf16 and related ARM features |

The benchmark process was pinned to CPU 0 through `ZEN_BENCH_PIN_CPU=0`.

Hardware PMU cycle counters were not available on this cloud instance:
`perf stat -e cycles,instructions` reported the events as unsupported, and
`/sys/devices/system/cpu/cpu0/cpufreq/*` frequency files were unavailable.
Therefore the cycles below are estimated as elapsed time multiplied by the
reported virtual CPU frequency, 2.0 GHz.

## Correctness

| Check | Result |
| --- | --- |
| `make KAT_KEM && ./KAT_KEM` | PASS |
| Interface length getters | PASS |
| Fixed-seed KEM round trip | PASS |
| DRNG-seeded KEM round trip | PASS |
| Raw benchmark CSV rows | 4000 rows, all PASS |

## Performance

Results are averages over 1000 measured iterations. Time is wall-clock time from
`clock_gettime(CLOCK_MONOTONIC)`. Cycles are estimated from the 2.0 GHz reported
virtual CPU frequency.

| Operation | Average us | Median us | Estimated cycles | ops/s | Failures |
| --- | ---: | ---: | ---: | ---: | ---: |
| `kem_keygen` | 37.972 | 31.582 | 75944.63 | 26334.98 | 0 |
| `kem_keygen_derand` | 50.392 | 50.093 | 100784.91 | 19844.24 | 0 |
| `kem_enc` | 40.908 | 40.662 | 81815.52 | 24445.24 | 0 |
| `kem_dec` | 40.139 | 39.903 | 80278.23 | 24913.36 | 0 |

## Sizes And Memory

| Item | Bytes |
| --- | ---: |
| Public key | 615 |
| Secret key | 1303 |
| Ciphertext | 512 |
| Shared secret | 16 |
| Static text/rodata | 45768 |
| Static writable | 85504 |
| Static total | 131272 |
| Peak RSS after tests | 1871872 |
| Benchmark context | 2590 |

