# RRLWR KEM Toom-Cook-4

This implementation uses Toom-Cook-4 polynomial multiplication for the ring
arithmetic path. Polynomial coefficients are represented with 16-bit storage,
and the current multiplication path accumulates Toom-Cook-4 products directly
into the row accumulator for `ring_mul_Awin`.

## Test CPU

CPU information was collected with `lscpu` on each benchmark machine.

### Reference machine: Intel Xeon E5-2686 v4 (Broadwell-EP)

| Field | Value |
| --- | --- |
| Architecture | x86_64 |
| CPU model | Intel(R) Xeon(R) CPU E5-2686 v4 @ 2.30GHz |
| Microarchitecture | **Broadwell-EP** (Xeon E5-2600 v4, codename Broadwell) |
| CPUs | 2 |
| Cores per socket | 2 |
| Threads per core | 1 |
| Socket(s) | 1 |
| L1d cache | 64 KiB (2 instances) |
| L1i cache | 64 KiB (2 instances) |
| L2 cache | 512 KiB (2 instances) |
| L3 cache | 45 MiB (1 instance) |
| Hypervisor | Xen |
| AVX2 | supported |

### Additional machine: Intel Xeon E5-2666 v3 (Haswell-EP)

| Field | Value |
| --- | --- |
| Architecture | x86_64 |
| CPU model | Intel(R) Xeon(R) CPU E5-2666 v3 @ 2.90GHz |
| Microarchitecture | **Haswell-EP** (Xeon E5-2600 v3, codename Haswell; family 6, model 63) |
| CPUs | 2 |
| Cores per socket | 1 |
| Threads per core | 2 |
| Socket(s) | 1 |
| L1d cache | 32 KiB (1 instance) |
| L1i cache | 32 KiB (1 instance) |
| L2 cache | 256 KiB (1 instance) |
| L3 cache | 25 MiB (1 instance) |
| Hypervisor | Xen |
| AVX2 | supported |

### Additional machine: Intel Xeon Platinum 8475B (Sapphire Rapids)

| Field | Value |
| --- | --- |
| Architecture | x86_64 |
| CPU model | Intel(R) Xeon(R) Platinum 8475B |
| Microarchitecture | **Sapphire Rapids** (Xeon Platinum 8400, family 6, model 143) |
| CPUs | 192 (2 sockets × 48 cores × 2 threads) |
| Cores per socket | 48 |
| Threads per core | 2 |
| Socket(s) | 2 |
| L1d cache | 4.5 MiB (96 instances) |
| L1i cache | 3 MiB (96 instances) |
| L2 cache | 192 MiB (96 instances) |
| L3 cache | 195 MiB (2 instances) |
| NUMA nodes | 2 |
| Host | SYS-421GE-TNRT (bare metal) |
| AVX-512 | supported |

The Platinum 8475B results use the same default `Makefile` flags, including
`-mtune=sapphirerapids`, which matches this microarchitecture.

The E5-2666 v3 results were measured on **Haswell-EP**, one generation older than
the reference **Broadwell-EP** E5-2686 v4, with smaller caches and SMT enabled
under Xen. Absolute cycle counts are not directly comparable across machines;
relative rankings (which implementation is faster) are still useful.

## Benchmark Notes

All cycle counts in the tables below are **medians**, not averages.

Build speed binaries from this directory with the project `Makefile` (default
`CFLAGS` below). Rebuild after changing flags:

```sh
make clean && make speed
```

### Compile flags (default `Makefile`)

| Flag | Role |
| --- | --- |
| `-O3` | Maximum optimization |
| `-march=x86-64-v3` | Baseline ISA with AVX2, BMI1, BMI2, FMA, etc. (no AVX-512 requirement) |
| `-mtune=sapphirerapids` | Instruction scheduling tuned for Sapphire Rapids |
| `-mprefer-vector-width=256` | Prefer 256-bit SIMD over wider vectors |
| `-fomit-frame-pointer` | Omit frame pointers (smaller/faster code) |
| `-fno-stack-protector` | Disable stack canaries for benchmark builds |
| `-DNDEBUG` | Disable `assert` and other debug checks |

Equivalent excerpt from `Makefile`:

```makefile
CFLAGS += -O3
CFLAGS += -march=x86-64-v3
CFLAGS += -mtune=sapphirerapids
CFLAGS += -mprefer-vector-width=256
CFLAGS += -fomit-frame-pointer -fno-stack-protector -DNDEBUG
```

Run each speed binary **one at a time** on an otherwise idle machine. Running
multiple benchmarks in parallel (or back-to-back under load) inflates cycle
counts and is not representative.

Kyber timings use `indcpa_keypair` / `indcpa_enc` / `indcpa_dec` for PKE and
`kyber_keypair` / `kyber_encaps` / `kyber_decaps` for KEM. Build Kyber ref
speed tests with the same optimization level when comparing cycle counts.

Correctness on the E5-2666 v3 machine was verified with:

```sh
./test/unit_tests_KEM256
./test/unit_tests_KEM512
cd ../kyber/ref && ./test/test_vectors1024
```

## RRLWR-128 vs Kyber512

Commands:

```sh
./test/test_speed_KEM128
cd ../kyber/ref && ./test/test_speed512
```

### E5-2686 v4 / Broadwell-EP (reference)

| Stage | RRLWR-128 (median cycles) | Kyber512 (median cycles) | Faster |
| --- | ---: | ---: | --- |
| PKE keygen | **78,872** | 83,969 | RRLWR-128 |
| PKE encrypt | **91,887** | 111,796 | RRLWR-128 |
| PKE decrypt | **13,555** | 38,003 | RRLWR-128 |
| KEM keygen | 98,732 | **98,689** | Kyber512 |
| KEM encaps | **105,328** | 123,047 | RRLWR-128 |
| KEM decaps | **116,503** | 161,068 | RRLWR-128 |

### E5-2666 v3 / Haswell-EP

| Stage | RRLWR-128 (median cycles) | Kyber512 (median cycles) | Faster |
| --- | ---: | ---: | --- |
| PKE keygen | **67,677** | 84,168 | RRLWR-128 |
| PKE encrypt | **78,884** | 112,223 | RRLWR-128 |
| PKE decrypt | **11,604** | 36,823 | RRLWR-128 |
| KEM keygen | **84,899** | 99,036 | RRLWR-128 |
| KEM encaps | **90,492** | 122,168 | RRLWR-128 |
| KEM decaps | **99,863** | 159,191 | RRLWR-128 |

### Xeon Platinum 8475B / Sapphire Rapids

| Stage | RRLWR-128 (median cycles) | Kyber512 (median cycles) | Faster |
| --- | ---: | ---: | --- |
| PKE keygen | 86,102 | **82,400** | Kyber512 |
| PKE encrypt | 102,536 | **100,510** | Kyber512 |
| PKE decrypt | **16,566** | 33,422 | RRLWR-128 |
| KEM keygen | 102,628 | **99,632** | Kyber512 |
| KEM encaps | 113,434 | **110,344** | Kyber512 |
| KEM decaps | **127,620** | 143,712 | RRLWR-128 |

## RRLWR-256 vs Kyber1024

Commands:

```sh
./test/test_KEM256
./test/test_speed_KEM256
cd ../kyber/ref && ./test/test_speed1024
```

### E5-2686 v4 / Broadwell-EP (reference)

| Stage | RRLWR-256 (median cycles) | Kyber1024 (median cycles) | Faster |
| --- | ---: | ---: | --- |
| PKE keygen | **215,891** | 219,185 | RRLWR-256 |
| PKE encrypt | 260,097 | **258,425** | Kyber1024 |
| PKE decrypt | **45,286** | 63,337 | RRLWR-256 |
| KEM keygen | **243,355** | 255,353 | RRLWR-256 |
| KEM encaps | 281,974 | **275,709** | Kyber1024 |
| KEM decaps | **325,312** | 352,585 | RRLWR-256 |

### E5-2666 v3 / Haswell-EP

| Stage | RRLWR-256 (median cycles) | Kyber1024 (median cycles) | Faster |
| --- | ---: | ---: | --- |
| PKE keygen | **185,648** | 222,948 | RRLWR-256 |
| PKE encrypt | **223,647** | 280,814 | RRLWR-256 |
| PKE decrypt | **38,814** | 60,425 | RRLWR-256 |
| KEM keygen | **208,785** | 260,612 | RRLWR-256 |
| KEM encaps | **241,148** | 293,800 | RRLWR-256 |
| KEM decaps | **278,105** | 361,090 | RRLWR-256 |

### Xeon Platinum 8475B / Sapphire Rapids

| Stage | RRLWR-256 (median cycles) | Kyber1024 (median cycles) | Faster |
| --- | ---: | ---: | --- |
| PKE keygen | 246,322 | **209,232** | Kyber1024 |
| PKE encrypt | 299,974 | **234,806** | Kyber1024 |
| PKE decrypt | 54,362 | **52,606** | Kyber1024 |
| KEM keygen | 268,368 | **258,106** | Kyber1024 |
| KEM encaps | 316,456 | **253,662** | Kyber1024 |
| KEM decaps | **263,000** | 314,104 | RRLWR-256 |

## RRLWR-512 Standalone

Command:

```sh
./test/test_speed_KEM512
```

### E5-2686 v4 / Broadwell-EP (reference)

| Stage | Median cycles |
| --- | ---: |
| poly_mul_toom4 | 2,297 |
| ring_mul full | 669,822 |
| ring_mul_Awin full | 668,172 |
| ring_mul 1 coefficient | 39,711 |
| ring_mul_Awin 1 coefficient | 39,226 |
| PKE keygen | 724,028 |
| PKE encrypt | 886,531 |
| PKE decrypt | 164,313 |
| KEM keygen | 765,312 |
| KEM encaps | 926,005 |
| KEM decaps | 1,099,750 |

### E5-2666 v3 / Haswell-EP

| Stage | Median cycles |
| --- | ---: |
| poly_mul_toom4 | 1,995 |
| ring_mul full | 571,846 |
| ring_mul_Awin full | 580,771 |
| ring_mul 1 coefficient | 57,682 |
| ring_mul_Awin 1 coefficient | 53,278 |
| PKE keygen | 615,543 |
| PKE encrypt | 773,828 |
| PKE decrypt | 140,541 |
| KEM keygen | 650,622 |
| KEM encaps | 801,115 |
| KEM decaps | 941,753 |

### Xeon Platinum 8475B / Sapphire Rapids

| Stage | Median cycles |
| --- | ---: |
| poly_mul_toom4 | 3,162 |
| ring_mul full | 791,090 |
| ring_mul_Awin full | 567,026 |
| ring_mul 1 coefficient | 36,484 |
| ring_mul_Awin 1 coefficient | 32,970 |
| PKE keygen | 588,826 |
| PKE encrypt | 727,604 |
| PKE decrypt | 138,780 |
| KEM keygen | 612,010 |
| KEM encaps | 747,470 |
| KEM decaps | 887,086 |
