#!/usr/bin/env python3
"""gen_bounds.py - exact integer squared-norm thresholds for SHUTTLE; patches
the @@AUTOGEN:bounds@@ region of ref/params.h, with a full audit log.

The norm gates compare an EXACT integer squared-norm `norm_sq` against an
integer threshold, never a square root (constant-time + exact).  SHUTTLE's
gates are INCLUSIVE:

  KeyGen : B_k' <= ||StretchS(1,s,e')|| <= B_k
  Sign   : ||(z_1,z_2')|| <= B_v
  Verify : ||(z_1,z_2')|| <= B_v   (SAME bound B_v as Sign)

Since norm_sq is an integer, the canonical inclusive-gate rounding rule is:

  BK_SQ     = floor(B_k^2)    accept upper iff  norm_sq <= BK_SQ
  BK_LOW_SQ = ceil(B_k'^2)    accept lower iff  norm_sq >= BK_LOW_SQ
  BV_SQ     = floor(B_v^2)    accept       iff  norm_sq <= BV_SQ

Unlike schemes with distinct sign/verify thresholds, SHUTTLE's Sign and Verify
SHARE one bound B_v, so only BV_SQ is emitted for that gate.  B_k' = 290 is an
integer in all three sets, so BK_LOW_SQ = ceil(290^2) = 84100 exactly.

Full-precision reals are taken verbatim from tab:suf-parameters.  Worst-case
norm_sq ~ B_v^2 ~ 6.3e8 fits int64; no scaling.

Run:  python3 gen_bounds.py
Verified by:  make check-consts  (re-run, cmp the region byte-for-byte)
"""

import os
from decimal import Decimal, getcontext

import autogen

getcontext().prec = 80

# Full-precision (verbatim from tab:suf-parameters): (B_k, B_k', B_v).
BOUNDS = {
    128: (Decimal("295.06"), Decimal("290"), Decimal("7356.68")),
    256: (Decimal("296.19"), Decimal("290"), Decimal("10385.53")),
    512: (Decimal("292.76"), Decimal("290"), Decimal("25194.90")),
}


def floor_dec(x):
    return int(x.to_integral_value(rounding="ROUND_FLOOR"))


def ceil_dec(x):
    return int(x.to_integral_value(rounding="ROUND_CEILING"))


BLOCK = """\
#if SHUTTLE_MODE == {mode}
/* B_k = {bk}  B_k' = {bkp}  B_v = {bv} */
#define BK_SQ INT64_C({bk_sq})     /* floor(B_k^2):  KeyGen upper, accept iff nsq <= BK_SQ */
#define BK_LOW_SQ INT64_C({bkl_sq}) /* ceil(B_k'^2):  KeyGen lower, accept iff nsq >= BK_LOW_SQ */
#define BV_SQ INT64_C({bv_sq}) /* floor(B_v^2):  Sign+Verify, accept iff nsq <= BV_SQ */
#endif
"""


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    logdir = os.path.join(here, "log")
    os.makedirs(logdir, exist_ok=True)
    params_h = os.path.normpath(os.path.join(here, "..", "ref", "params.h"))
    logpath = os.path.join(logdir, "bounds_derivation.txt")

    blocks, summary, log = [], [], []
    log.append("gen_bounds.py -- SHUTTLE exact integer squared-norm thresholds")
    log.append("")
    log.append("Inclusive-gate rounding rule:")
    log.append("  BK_SQ     = floor(B_k^2)    (KeyGen upper, '<='  on B_k)")
    log.append("  BK_LOW_SQ = ceil(B_k'^2)    (KeyGen lower, '>='  on B_k')")
    log.append("  BV_SQ     = floor(B_v^2)    (Sign+Verify, '<='  on B_v, SHARED)")
    log.append("")
    for mode in (128, 256, 512):
        bk, bkp, bv = BOUNDS[mode]
        bk2, bkp2, bv2 = bk * bk, bkp * bkp, bv * bv
        bk_sq = floor_dec(bk2)   # ||.|| <= B_k    -> norm_sq <= bk_sq
        bkl_sq = ceil_dec(bkp2)  # ||.|| >= B_k'   -> norm_sq >= bkl_sq
        bv_sq = floor_dec(bv2)   # ||.|| <= B_v    -> norm_sq <= bv_sq
        log.append(f"=== SHUTTLE-{mode} ===")
        log.append(f"  B_k  = {bk}    B_k^2  = {bk2}")
        log.append(f"    -> BK_SQ     = floor(B_k^2)  = {bk_sq}   (accept iff nsq <= BK_SQ)")
        log.append(f"  B_k' = {bkp}    B_k'^2 = {bkp2}")
        log.append(f"    -> BK_LOW_SQ = ceil(B_k'^2)  = {bkl_sq}   (accept iff nsq >= BK_LOW_SQ)")
        log.append(f"  B_v  = {bv}    B_v^2  = {bv2}")
        log.append(f"    -> BV_SQ     = floor(B_v^2)  = {bv_sq}   (Sign+Verify, accept iff nsq <= BV_SQ)")
        log.append("")
        summary.append(f" *   SHUTTLE-{mode}: BK_LOW_SQ={bkl_sq} <= ||.||^2 <= BK_SQ={bk_sq}, "
                       f"BV_SQ={bv_sq}")
        blocks.append(BLOCK.format(mode=mode, bk=bk, bkp=bkp, bv=bv,
                                   bk_sq=bk_sq, bkl_sq=bkl_sq, bv_sq=bv_sq))

    body = ("/* EXACT integer squared-norm thresholds (see tools/gen_bounds.py +\n"
            " * tools/log/bounds_derivation.txt).  Inclusive-gate rule:\n"
            " *   KeyGen accept iff BK_LOW_SQ <= norm_sq <= BK_SQ  (B_k' <= ||.|| <= B_k)\n"
            " *   Sign/Verify accept iff norm_sq <= BV_SQ          (||.|| <= B_v, SHARED)\n"
            + "\n".join(summary) + "\n */\n"
            + "\n".join(blocks))
    autogen.patch_region(params_h, "bounds", body)

    with open(logpath, "w") as f:
        f.write("\n".join(log) + "\n")
    print("\n".join(log))
    print(f"Patched @@AUTOGEN:bounds@@ in {params_h}")
    print(f"Wrote audit log {logpath}")


if __name__ == "__main__":
    main()
