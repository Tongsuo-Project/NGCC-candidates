# New security levels: lvl2 (λ=160) and lvl6 (λ=512)

This document describes the two non-NIST security levels added to the codebase,
mirroring the existing `lvl1`/`lvl3`/`lvl5` exactly.

## Primes (form p = c·2^f − 1, p ≡ 3 mod 4)

| level | λ   | p                | bits | f    | c          | fp tag        |
|-------|-----|------------------|------|------|------------|---------------|
| lvl2  | 160 | `9·2^309 − 1`    | 313  | 309  | 9 = 3²     | `p9309`       |
| lvl6  | 512 | `113·2^1016 − 1` | 1023 | 1016 | 113 (prime)| `p1131016`    |

Selection rule (same as lvl1/3/5): for a target 2-adic valuation `f ≈ 2λ`, take the
smallest odd cofactor `c` making `p = c·2^f − 1` prime, with `bit_length(p)` close to
but not exceeding `2λ`. (See `scripts/precomp/search_new_primes.sage`.)

## Status

**Done and validated in-sandbox** (out-of-tree build, GMP-reference + project test suite):
- `src/gf/ref/lvl{2,6}/fp_*_{32,64}.c` — Montgomery field arithmetic, generated with
  Mike Scott's `monty.py` (radix-unsaturated core, self-tested) plus the hand adapter
  layer (`fp_*` wrappers, `R2`/`TWO_INV`/`THREE_INV`, prime-specific `partial_reduce`
  / `fp_decode_reduce`). **All `fp` and `fp2` tests pass** (`sqisign_test_gf_lvl{2,6}_{fp,fp2}`).
- `src/precomp/ref/lvl{2,6}/include/fp_constants.h` and `include/encoded_sizes.h`
  (hand-written; the encoded-size formulas reproduce the committed lvl1/3/5 files exactly).
- All scaffolding: `gf/precomp/ec/hd/id2iso/signature/verification/nistapi` ref dirs +
  `CMakeLists.txt`, `sqisign_parameters.txt` (num_orders=1), `api.{c,h}`.
- `scripts/precomp/cformat.py` (radix maps for the new primes),
  `scripts/precomp/precompute_sizes.sage` (lvl2 `SECURITY_BITS=160` override + the
  SQIsignTriangle size constants that the repo copy was missing), and
  top-level `CMakeLists.txt` (`SVARIANT_S = lvl1;lvl2;lvl3;lvl5;lvl6`).

**Pending (require SageMath, must run on a machine with the Sage toolchain):**
- The six precomputed crypto constant files per level (`ec_params.c`, `e0_basis.c`,
  `hd_splitting_transforms.c`, `torsion_constants.c`, `quaternion_data.c`,
  `endomorphism_action.c`) and their headers. These currently contain **placeholders**;
  the gf tests build without them, but the full scheme does not.
- The `broadwell` (AVX2) backend for the two new primes (hand-written assembly; not started).

## How to finish and build

1. **Generate the precomputed constants** (needs SageMath ≥ 10.5; with `num_orders = 1`
   the `deuring2d` library is *not* required — only the q=1 path runs):
   ```bash
   bash scripts/precomp/bootstrap_new_level.sh lvl2
   bash scripts/precomp/bootstrap_new_level.sh lvl6
   ```
2. **Build (ref) and test:**
   ```bash
   mkdir build && cd build
   cmake -DSQISIGN_BUILD_TYPE=ref -DCMAKE_BUILD_TYPE=Release ..
   make
   ./test/sqisign_test_scheme_lvl2     # keygen / sign / open
   ./test/sqisign_test_scheme_lvl6
   ```

### To verify just the field arithmetic now (no SageMath needed)
```bash
mkdir build && cd build
cmake -DSQISIGN_BUILD_TYPE=ref -DCMAKE_BUILD_TYPE=Release ..
make sqisign_test_gf_lvl2_fp sqisign_test_gf_lvl2_fp2 sqisign_test_gf_lvl6_fp sqisign_test_gf_lvl6_fp2
./src/gf/ref/lvl2/test/sqisign_test_gf_lvl2_fp   # etc.
```

## Validation done (in-sandbox, ref backend)
- `monty.py` self-test on all four generated cores; an independent GMP-reference harness
  (add/sub/mul/sqr/neg/half/div3/inv/sqrt/encode/decode/decode_reduce).
- The project's own gf tests `sqisign_test_gf_lvl{2,6}_{fp,fp2}` pass under **both**
  `-DGF_RADIX=64` and `-DGF_RADIX=32`. Full configure of all 5 levels succeeds; lvl1
  regression (gf + `sqisign_test_scheme_lvl1`) still passes.

## Notes / fixes made to shared scripts
- **`scripts/precomp/precompute_sizes.sage` was stale**: the committed `encoded_sizes.h`
  files contain `SQISIGNTRIANGLE_*` and `NEW_SIGNATURE_BYTES`, which the in-repo script did
  not emit (it emitted a different `SIGNATURE_BYTES`). Updated to emit the full set, verified
  to reproduce lvl1/3/5 exactly. If you have a newer canonical sizes script, prefer it.
- **`SECURITY_BITS` override for lvl2 (=160)** added in **two** scripts that use the
  `round(bits/128)*64` formula (which yields 128 for a 313-bit prime): `precompute_sizes.sage`
  and `precompute_torsion_constants.sage` (the latter feeds `TWO_TO_SECURITY_BITS`,
  `SEC_DEGREE`, `COM_DEGREE`). lvl6 → 512 is already correct.
- **`fp_mul_small` / `modmli`**: the current `monty.py` emits a fast multiply-by-int that is
  incorrect for a 32-bit radix with large multipliers; the generated files use the safe
  `modint`+`modmul` form (as the committed lvl1/3/5 do).
