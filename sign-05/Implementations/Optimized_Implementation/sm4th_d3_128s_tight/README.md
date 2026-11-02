# sm4th-d3-128s-tight - Optimized Implementation

Optimized implementation of **sm4th-d3-128s-tight** with x86-oriented
acceleration. This tight variant uses SM4 for the OWF and Ballet-256/256 as the
PRG, with 256-bit IVs and 256-bit PRG seeds. The SM4 Intel/Hygon PRG backends
are intentionally not part of this target.

## Directory Structure

```
.
├── KAT_SIG.c                              # KAT vector generator entry
├── Makefile                               # Build rules (x86/non-x86 defaults)
├── README.md                              # This document
├── auxfunc.c / auxfunc.h                  # SM3 helper interface
├── bavc.c / bavc.h                        # BAVC commit/reconstruct
├── bench_cycles.h                         # Cycle-counter and stats helpers
├── compat.c / compat.h                    # Compatibility wrappers
├── drng.c / drng.h                        # Deterministic RNG used by KAT/bench
├── endian_compat.h                        # Endian helpers/macros
├── fallbacks.h                            # Feature fallback defines
├── fields.c / fields.h                    # GF arithmetic (with optional PCLMUL)
├── macros.h                               # Shared macros
├── owf.c / owf.h                          # One-way function helpers
├── params.c / params.h                    # Parameter set definitions
├── prg.c / prg.h                          # Ballet PRG interface/implementation
├── random_oracle.c / random_oracle.h      # RO / transcript hashing helpers
├── run_kat_sig.sh                         # KAT runner script
├── sig_impl.c / sig_impl.h                # Core sign/verify implementation
├── sig_impl_internal.h                    # Internal sig implementation types
├── sig_timing.c / sig_timing.h            # Optional sign/verify component timing
├── sm4_witness.c                          # Witness expansion for SM4
├── sm4th_d3_128s_tight.c / .h             # Public API wrapper
├── sm4th_d3_128s_tight_AlgorithmInstance.c / .h # Algorithm instance entry points
├── sm4th_d3_128s_tight_bench.c            # Benchmark entry
├── sm4th_sm4_128.c / sm4th_sm4_128.h      # SM4TH-specific SM4 wrapper
├── universal_hashing.c / .h               # VOLE universal hashing
├── utils.c / utils.h                      # Common utility helpers
├── utils_ballet/                          # Ballet-256/256 PRG components
│   ├── Ballet256256_coreE.c / .h          # Ballet encryption/key schedule
│   └── BalletDef.h                        # Ballet types/macros
├── utils_sm4/                             # SM4 OWF components
│   ├── sm4.h                              # SM4 shared declarations
│   ├── sm4_affine.c                       # Affine transforms
│   ├── sm4_asm.S                          # x86 assembly SM4 OWF backend
│   ├── sm4_core.c                         # SM4 core rounds
│   ├── sm4_ecb.c                          # ECB backend wrapper
│   └── sm4_sbox.c                         # S-box tables/logic
├── vole.c / vole.h                        # VOLE protocol logic
└── x86_caps.h                             # x86 feature detection helpers
```

## Requirements

- GCC >= 7
- GNU Make
- C11 runtime (`-lm`)

## Build

Default build is release:

```bash
make sm4th_d3_128s_tight_bench
```

Build options:

| Variable | Default | Description |
|----------|---------|-------------|
| `NDEBUG` | `1` | `1`: release (`-O3 -march=native -flto`), `0`: keep base flags |
| `FIELD_PCLMUL` | `1` on x86-64/i386, `0` otherwise | Enable GF(2^128) carryless-multiply acceleration |
| `PRG_ACCE` | `1` on x86-64/i386, `0` otherwise | Enable Ballet PRG AVX2 acceleration |
| `SM4_OWF_ACCE` | `1` on x86-64/i386, `0` otherwise | Enable the x86 SM4 assembly backend for the single SM4 OWF |
| `USE_SBOX_TABLE` | `1` | Enable the portable SM4 S-box lookup table |
| `COMPONENT_TIMING` | `0` | `1` prints sign/verify component timing |
| `COMPONENT_TIMING_VERBOSE` | `0` | `1` prints every measured timing span |

Portable C-reference-style build:

```bash
make clean
make NDEBUG=1 PRG_ACCE=0 SM4_OWF_ACCE=0 FIELD_PCLMUL=0 KAT_SIG sm4th_d3_128s_tight_bench
```

## Run

Benchmark:

```bash
./sm4th_d3_128s_tight_bench
./sm4th_d3_128s_tight_bench 100
./sm4th_d3_128s_tight_bench 100 8
```

`./sm4th_d3_128s_tight_bench` runs 100 fixed-56-byte sign/verify iterations by
default. The signed message is the deterministic 56-byte ASCII string
`Chinith sm4th ublockith vistrutith 56-byte bench msg v1.`. The optional second
argument is `MESSAGE_STEP`; it defaults to `0`, and `8` runs increasing message
lengths `56, 64, 72, ...`.

The bench prints wall-clock averages, the cycle-counter source, average Mcycles,
and per-iteration min/median/p90/max stats. The median is the typical iteration
time. The p90 value is the 90th percentile: 90% of iterations are no slower than
this value. On shared Hygon hosts, prefer the `per-iteration Mcyc/op stats`
median and p90 over the wall-clock average.

KAT:

```bash
bash run_kat_sig.sh
```

Behavior of `run_kat_sig.sh`:
- On x86: build/run `KAT_SIG` with default optimized flags.
- On non-x86: prints one log line and forces portable fallback flags to match reference output.

KAT output is written to `output/`.
