# ublockith-d3-256s — Optimized Implementation

Optimized implementation of **ublockith-d3-256s**.

Key policy for this target:
- PRG is fixed to **Ballet** (`USE_BALLET=1`).
- `PRG_ACCE` default is **1** (x86 uses AVX2 parallel-enc for Ballet-256/256).
- Bit-exact is preserved by executing the same Ballet encryption round equations as the portable coreE path.
- No SM4/AES path in this target (`UBLOCKITH_ENABLE_SM4_ACCE=0`).

## Directory Structure

```
.
├── KAT_SIG.c                                  # KAT vector generator entry
├── Makefile                                   # Build rules (x86/non-x86 defaults)
├── README.md                                  # This document
├── auxfunc.c / auxfunc.h                      # Portable hash helper interface
├── bavc.c / bavc.h                            # BAVC commit/reconstruct logic
├── compat.c / compat.h                        # Compatibility wrappers
├── drng.c / drng.h                            # Deterministic RNG for KAT/bench
├── endian_compat.h                            # Endian helpers/macros
├── fallbacks.h                                # Platform fallback defines
├── fields.c / fields.h                        # GF arithmetic (optional PCLMUL on x86)
├── macros.h                                   # Shared macros
├── owf.c / owf.h                              # One-way function helpers
├── params.c / params.h                        # Parameter-set definitions
├── prg.c / prg.h                              # PRG interface/implementation (Ballet)
├── random_oracle.c / random_oracle.h          # Random-oracle / transcript hashing
├── run_kat_sig.sh                             # KAT runner script
├── sig_impl.c / sig_impl.h / sig_impl_internal.h # Core sign/verify implementation
├── ublockith_em_d3_256f.c / .h                   # Public API wrapper
├── ublockith_em_d3_256f_AlgorithmInstance.c / .h # Algorithm instance entry points
├── ublockith_em_d3_256f_bench.c                  # Benchmark entry (ms + Mcyc)
├── universal_hashing.c / .h                   # VOLE universal hashing
├── utils.c / utils.h                          # Common utility helpers
├── vole.c / vole.h                            # VOLE protocol logic
├── x86_caps.h                                 # x86 feature detection helpers
├── utils_ballet/
│   ├── BalletDef.h                            # Ballet shared definitions
│   └── Ballet256256_coreE.c / .h              # Ballet-256/256 coreE backend
└── utils_ublock/
    ├── ublock.h                               # uBlock public declarations
    ├── ublock_internal.h                      # uBlock internal declarations
    ├── ublock_core.c                          # uBlock round core
    ├── ublock_affine.c                        # uBlock affine helpers
    ├── ublock_witness.c                       # uBlock witness generation
    ├── ublock_constraints.c / .h              # uBlock constraint system
    ├── ublock_ecb.c                           # uBlock ECB wrapper
    ├── ublockith_ublock_256.c / .h            # ublockith-specific uBlock wrapper
    └── sig_impl_ublock.c                      # ublockith sign/verify backend
```

## Build

Default build is release:

```bash
make ublockith_em_d3_256f_bench
```

Build options:

| Variable | Default | Description |
|----------|---------|-------------|
| `NDEBUG` | `1` | `1`: release (`-O3 -march=native -flto`), `0`: base flags |
| `PRG_ACCE` | `1` | x86 enables AVX2 parallel-enc PRG (bit-exact with portable coreE) |
| `FIELD_PCLMUL` | `1` on x86, else `0` | Enables x86 PCLMUL field acceleration |

Notes:
- `USE_BALLET=1`, `HAVE_BALLET_CORE=1`, `UBLOCKITH_ENABLE_SM4_ACCE=0` are fixed in this target.

## Run

Benchmark:

```bash
./ublockith_em_d3_256f_bench
./ublockith_em_d3_256f_bench 100
./ublockith_em_d3_256f_bench 100 8
```

Benchmark arguments:
- `ITERATIONS` defaults to `100`.
- `MESSAGE_STEP` is optional and gives the per-iteration message-length increment in bytes.
- If `MESSAGE_STEP` is omitted, Optimized signature benches use `0`, so every message remains fixed at 56 bytes.
- `MESSAGE_STEP=0` fixes every message at 56 bytes; `MESSAGE_STEP=8` uses 56, 64, 72, ... bytes.
- Benchmark messages are deterministic ASCII strings beginning with `Chinith sm4th ublockith vistrutith 56-byte bench msg v1.`.
- Thus `./ublockith_em_d3_256f_bench` and `./ublockith_em_d3_256f_bench 100` run 100 fixed-56-byte sign/verify iterations, while `./ublockith_em_d3_256f_bench 100 8` forces the increasing-message run.

Baseline check command:

```bash
make NDEBUG=1 PRG_ACCE=1 FIELD_PCLMUL=1 USE_BALLET=1 ublockith_em_d3_256f_bench && ./ublockith_em_d3_256f_bench 20
```

KAT:

```bash
bash run_kat_sig.sh
```

KAT output is written to `output/` in this folder.
