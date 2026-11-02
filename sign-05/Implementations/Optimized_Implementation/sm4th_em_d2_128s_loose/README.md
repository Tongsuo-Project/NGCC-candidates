# sm4th-em-d2-128s-loose - Optimized Implementation

Optimized implementation of **sm4th-em-d2-128s-loose** with x86-oriented acceleration.

Protocol parameters follow `SM4th-EM-d2-128s-loose` in `Chinith-260621.pdf`:
`lambda=128`, `lambda_f=128`, `lambda_iv=128`, `ell=1024`, `tau=11`,
`w_grind=7`, `T_open=103`, `B=16`, `d=2`. The loose variant keeps SM4 as
the PRG, uses a 128-bit IV, and its EM-d2 leaf PRG path emits 162 bytes.

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
├── hygon_cis_sm4_probe.c                  # Hygon CIS SM4 runtime probe for KAT
├── macros.h                               # Shared macros
├── owf.c / owf.h                          # One-way function helpers
├── params.c / params.h                    # Parameter set definitions
├── prg.c / prg.h                          # PRG interface/implementation
├── random_oracle.c / random_oracle.h      # RO / transcript hashing helpers
├── run_kat_sig.sh                         # KAT runner script
├── sig_impl.c / sig_impl.h                # Core sign/verify implementation
├── sig_impl_internal.h                    # Internal sig implementation types
├── sig_timing.c / sig_timing.h            # Optional sign/verify component timing
├── sm4_witness.c                          # Witness expansion for SM4
├── sm4th_em_d2_128s_loose.c / sm4th_em_d2_128s_loose.h   # Public API wrapper
├── sm4th_em_d2_128s_loose_AlgorithmInstance.c / .h # Algorithm instance entry points
├── sm4th_em_d2_128s_loose_bench.c                  # Benchmark entry
├── sm4th_sm4_128.c / sm4th_sm4_128.h      # SM4TH-specific SM4 wrapper
├── universal_hashing.c / .h               # VOLE universal hashing
├── utils.c / utils.h                      # Common utility helpers
├── utils_sm4/                             # SM4 components
│   ├── sm4.h                              # SM4 shared declarations
│   ├── sm4_affine.c                       # Affine transforms
│   ├── sm4_asm.S                          # x86 assembly SM4 backend
│   ├── sm4_core.c                         # SM4 core rounds
│   ├── sm4_ecb.c                          # ECB backend wrapper
│   ├── hygon_cis_sm4.c / .h               # Hygon CIS SM4 wrapper
│   ├── hygon_cis_sm4_asm.S                # Hygon CIS SM4 assembly backend
│   ├── sm4_sbox.c                         # S-box tables/logic
│   └── sm4ni/                             # Imported SM4NI reference sources
│       ├── README.md
│       ├── sm4_ref.c / sm4_ref.h
│       └── sm4ni.c
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
make sm4th_em_d2_128s_loose_bench
```

Build options:

| Variable | Default | Description |
|----------|---------|-------------|
| `NDEBUG` | `1` | `1`: release (`-O3 -march=native -flto`), `0`: keep base flags |
| `FIELD_PCLMUL` | `1` on x86-64/i386, `0` otherwise | Enable GF(2^128) carryless-multiply acceleration |
| `PRG_ACCE` | `1` on x86-64/i386, `0` otherwise | Enable the ordinary x86 SM4NI/AES-NI simulation PRG path |
| `HYGON_SM4_ACCE` | `0` | Enable the Hygon CIS SM4 instruction PRG backend; only on Hygon x86-64 CPUs |
| `USE_SBOX_TABLE` | `1` | Enable the portable SM4 S-box lookup table |
| `COMPONENT_TIMING` | `0` | `1` prints sign/verify component timing |
| `COMPONENT_TIMING_VERBOSE` | `0` | `1` prints every measured timing span |

Hygon SM4-only ISA build:

```bash
make clean
make PRG_ACCE=0 HYGON_SM4_ACCE=1 KAT_SIG sm4th_em_d2_128s_loose_bench
```

## Run

Benchmark:

```bash
./sm4th_em_d2_128s_loose_bench
./sm4th_em_d2_128s_loose_bench 100
./sm4th_em_d2_128s_loose_bench 100 8
```

`./sm4th_em_d2_128s_loose_bench` runs 100 fixed-56-byte sign/verify iterations by default.
The signed message is the deterministic 56-byte ASCII string `Chinith sm4th ublockith vistrutith 56-byte bench msg v1.`.
The optional second argument is `MESSAGE_STEP`; it defaults to `0`, and `8` runs increasing message lengths `56, 64, 72, ...`.

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
- On Hygon x86-64: build/run `hygon_cis_sm4_probe` first; if it passes, build/run
  `KAT_SIG` with `PRG_ACCE=0 HYGON_SM4_ACCE=1`.
- On other x86: build/run `KAT_SIG` with default optimized flags.
- On non-x86: prints one log line and forces portable fallback flags to match reference output.
- Set `HYGON_SM4_KAT=1` to force the Hygon SM4 probe path, or `HYGON_SM4_KAT=0`
  to force the ordinary x86 path.

KAT output is written to `output/`.

From the repository root, compare Optimized/Reference/Test_Vector hashes with:

```bash
bash Chinith/check_kat_hash.sh --variant sm4th_em_d2_128s_loose
```
