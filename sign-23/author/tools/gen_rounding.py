#!/usr/bin/env python3
"""gen_rounding.py - division-free round-to-nearest (ties UP, toward +inf) magic
reciprocals for SHUTTLE CompressY, plus the alpha_h highbits shift constants.

CompressY divides each y-block by a positive divisor alpha in {alpha_1, alpha_s,
alpha_e} = {90/135/144, 10/5/3, 5/5/3} (NOT powers of two), ROUNDING to nearest.
These divides act on SECRET-derived data (the uncompressed response), so NO
hardware idiv / `% const` is allowed.

ROUNDING RULE: round half UP (ties toward +inf), i.e. CompressY(v) =
floor((2v + alpha) / (2 alpha)) = floor((v + alpha/2)/alpha) for any sign of v.
This rule is SHIFT-INVARIANT: round((v + alpha*x)/alpha) = round(v/alpha) + x for
every integer x.  The commitment-reconstruction step the verifier runs
(reconstruct comY from the compressed response z1) relies on exactly this
translation identity CompressY(y + StretchS(x)) = CompressY(y) + x.  Round-half-
AWAY-from-zero (the previous rule) is NOT shift-invariant: at a half-integer tie
the round direction depends on the sign, so the identity breaks whenever a block
divisor alpha is EVEN (the tie v mod alpha = alpha/2 is then an integer
residue).  That broke the identity on the even-divisor blocks (alpha_1=90,
alpha_s=10 for SHUTTLE-128; alpha_1=144 for SHUTTLE-512), injecting +-1 errors
into z1 that the matrix product spreads into wrong HighBits buckets, so the
signer rejected ~81% (mode 128) / ~4% (mode 512) of attempts at the B_v gate.
Round-half-up has no such sign-dependent tie and restores the identity for ALL
divisors, even or odd.

BRANCHLESS DIVISION-FREE KERNEL.  For signed v in (-2^20, 2^20) we want
    q = round_half_up(v / alpha) = floor((2v + alpha) / (2 alpha)).
Round-half-up is shift-invariant, so with an offset OFF = K*alpha (a multiple of
alpha, K chosen so V = v + OFF >= 0 for every reachable v) we have
    round_half_up(v/alpha) = round_half_up(V/alpha) - K.
For NON-NEGATIVE V, ties-up and ties-away coincide, so round_half_up(V/alpha) =
floor((2V + alpha)/(2 alpha)) is realized by the classic magic reciprocal
    RECIP = ceil(2^SHIFT / (2 alpha)),   floor(P/(2 alpha)) == (P*RECIP) >> SHIFT.
Folding both the +alpha tie bias and the 2*OFF*RECIP offset into a single
constant ROUND_BIAS = alpha*RECIP + 2*OFF*RECIP, the C kernel is just
    q = (2*v*RECIP + ROUND_BIAS) >> SHIFT;   res = q - ROUND_K;     (ROUND_K = K)
with NO sign mask / abs / re-sign (the shifted operand 2*v*RECIP + ROUND_BIAS =
2*V*RECIP + alpha*RECIP is always >= 0 because V >= 0, so the arithmetic shift is
an exact floor).  We PROVE the kernel exact for every v in (-2^20, 2^20) against
Python's round_half_up, and SHIFT is the smallest shift that is (i) exact over
that range and (ii) keeps 2*(2^20-1)*RECIP + ROUND_BIAS < 2^63 (the int64
product bound).

alpha_h (1024/1024/2048) is a power of two -> highbits is a +alpha_h/2 bias then
a right shift by LOG2_ALPHA_H; no magic needed (emitted as a sanity static
assert only).

Emits: ../tools/rounding_consts.h  (included by ref/rounding.c via -I../tools).
Run:  python3 gen_rounding.py
"""

import os

VMAX = 1 << 20  # prove exact for |v| < 2^20 (>> any reachable response coeff)


def round_half_up(v, alpha):
    """Round v/alpha to nearest, ties toward +inf, for any signed integer v.
    floor((2v + alpha) / (2 alpha)) with Python floor-division (handles v<0)."""
    return (2 * v + alpha) // (2 * alpha)


def find_magic(alpha):
    """Return (RECIP, SHIFT, ROUND_BIAS, ROUND_K) s.t. for all v in
    (-VMAX, VMAX),  ((2*v*RECIP + ROUND_BIAS) >> SHIFT) - ROUND_K
    == round_half_up(v, alpha), with the int64 product staying < 2^63."""
    D = 2 * alpha
    K = (VMAX + alpha - 1) // alpha   # ceil(VMAX/alpha): OFF = K*alpha >= VMAX
    off = K * alpha
    for shift in range(16, 62):
        recip = (1 << shift) // D + 1  # ceil(2^shift / (2*alpha))
        bias = alpha * recip + 2 * off * recip  # +alpha/2 tie bias + offset fold
        # int64 safety: max operand at v = VMAX-1 must be < 2^63
        if 2 * (VMAX - 1) * recip + bias >= (1 << 63):
            continue
        ok = True
        # exhaustive check over the reachable signed range
        for v in range(-(VMAX - 1), VMAX):
            if (((2 * v * recip + bias) >> shift) - K) != round_half_up(v, alpha):
                ok = False
                break
        if ok:
            return recip, shift, bias, K
    raise RuntimeError(f"no magic found for alpha={alpha}")


ALPHAS = {
    128: (90, 10, 5),
    256: (135, 5, 5),
    512: (144, 3, 3),
}
ALPHA_H = {128: 1024, 256: 1024, 512: 2048}


def log2_exact(x):
    assert x & (x - 1) == 0, x
    return x.bit_length() - 1


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "rounding_consts.h")
    logdir = os.path.join(here, "log")
    os.makedirs(logdir, exist_ok=True)
    logpath = os.path.join(logdir, "rounding_derivation.txt")

    lines = []
    log = []
    log.append("gen_rounding.py -- CompressY round-half-up magic reciprocals")
    log.append(f"Exhaustively verified exact for v in (-2^20, 2^20) = (-{VMAX},{VMAX}).")
    log.append("Kernel: q = (2*v*RECIP + ROUND_BIAS) >> SHIFT;  res = q - ROUND_K")
    log.append("        == round_half_up(v, alpha) = floor((2v+alpha)/(2 alpha))")
    log.append("Shift-invariant (ties toward +inf): restores the CompressY")
    log.append("translation identity CompressY(y+alpha*x)=CompressY(y)+x for all alpha.")
    log.append("")

    lines.append("/* rounding_consts.h -- AUTO-GENERATED by tools/gen_rounding.py.")
    lines.append(" * Division-free round-to-nearest (ties UP, toward +inf) magic")
    lines.append(" * reciprocals for CompressY (alpha_1/alpha_s/alpha_e) + the alpha_h")
    lines.append(" * highbits shift.  Round-half-up is SHIFT-INVARIANT, so it preserves")
    lines.append(" * the CompressY translation identity for every divisor (round-half-")
    lines.append(" * away broke it on even divisors).  Verified exact for |v| < 2^20.")
    lines.append(" * DO NOT hand-edit; regenerate via `python3 tools/gen_rounding.py`. */")
    lines.append("#ifndef SHUTTLE_ROUNDING_CONSTS_H")
    lines.append("#define SHUTTLE_ROUNDING_CONSTS_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append('#include "params.h"')
    lines.append("")

    for mode in (128, 256, 512):
        a1, asec, ae = ALPHAS[mode]
        log.append(f"=== SHUTTLE-{mode} ===")
        lines.append(f"#if SHUTTLE_MODE == {mode}")
        for name, alpha in (("1", a1), ("S", asec), ("E", ae)):
            recip, shift, bias, koff = find_magic(alpha)
            log.append(f"  alpha_{name} = {alpha}: RECIP={recip} SHIFT={shift} "
                       f"ROUND_BIAS={bias} ROUND_K={koff}  (2*alpha={2*alpha})")
            lines.append(f"#    define RCP_ALPHA_{name} INT64_C({recip}) "
                         f"/* alpha={alpha}: ceil(2^{shift}/(2*alpha)) */")
            lines.append(f"#    define SH_ROUND_{name} {shift}")
            lines.append(f"#    define ROUND_BIAS_{name} INT64_C({bias}) "
                         f"/* alpha*RECIP (+alpha/2 tie) + 2*K*alpha*RECIP (offset) */")
            lines.append(f"#    define ROUND_K_{name} INT32_C({koff}) "
                         f"/* offset quotient K = ceil(2^20/alpha) */")
        lh = log2_exact(ALPHA_H[mode])
        lines.append(f"#    define LOG2_ALPHA_H {lh} /* alpha_h={ALPHA_H[mode]} = 1<<{lh} */")
        lines.append("#endif")
        lines.append("")
        log.append(f"  alpha_h = {ALPHA_H[mode]}: LOG2_ALPHA_H = {lh}")
        log.append("")

    lines.append("/* Cross-check: alpha_h must be the power of two LOG2_ALPHA_H encodes. */")
    lines.append("_Static_assert(ALPHA_H == (1 << LOG2_ALPHA_H),")
    lines.append('               "alpha_h must equal 1 << LOG2_ALPHA_H");')
    lines.append("_Static_assert((ALPHA_B & (ALPHA_B - 1)) == 0,")
    lines.append('               "alpha_b must be a power of two");')
    lines.append("")
    lines.append("#endif /* SHUTTLE_ROUNDING_CONSTS_H */")

    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
    with open(logpath, "w") as f:
        f.write("\n".join(log) + "\n")
    print("\n".join(log))
    print(f"Wrote {out}")
    print(f"Wrote audit log {logpath}")


if __name__ == "__main__":
    main()
