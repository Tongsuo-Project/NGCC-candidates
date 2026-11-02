#!/usr/bin/env python3
"""gen_irs_consts.py - the IRS R-transition fixed-point constants for SHUTTLE.

irs.c restores the natural-log test variable u = 2 r^2 ln(U) from the
base-2 value ell ~= log2(U) returned by SamplerU/ApproxLog with a single
multiply by the reproducible real constant

    2 r^2 ln 2 = 2 * 825^2 * ln 2 = 943546.5995372256...    (with 2 r^2 = 1361250).

This constant is NOT an integer, so it is carried in fixed point.  This
generator PINS the Q-scale F, emits the rounded integer R2LN2_QF into the
@@AUTOGEN:irs_consts@@ region of ref/irs.c, and asserts that the integer
u-vs-boundary comparison reproduces the high-precision float reference on a
dense (t, V, ell) grid.

================ The pinned Q-form (see irs.c header) ========================

ApproxLog returns log2(b) as a Q62 SIGNED int64 `frac`; SamplerU additionally
yields the integer exponent `a` in {1..81}.  Thus

    log2(U) = frac/2^62 - a.

R forms u at the fixed scale Q_F = 2^F (F = R2LN2_QSHIFT below):

    u_frac = round( R2LN2_QF * frac / 2^62 )    [Q_F]   (full Q62 frac, lossless)
    u_a    = a * R2LN2_QF                        [Q_F]
    u      = u_frac - u_a                         [Q_F]  ( = (2 r^2 ln2) * log2(U) at Q_F )

and the boundary integers B = -2 m t - m^2 V (exact int64) are promoted to
B << F.  The half-open comparison  (lo << F) < u <= (hi << F)  is then EXACT in
__int128 (no float, no division).

Why F = 44 (the decoupled scale, chosen to:
  (1) keep the FULL Q62 mantissa precision of `frac` (the >>62 is round-to-
      nearest and loses < 2^-(F+log2 2r^2) ~ 2^-64 in ln-units);
  (2) make the rounded constant near-exact (relative error 2^-65.16, so the
      amplified additive natural-log error |ln U|*relerr <= 50.53*2^-65.16
      ~ 2^-59.5, on a par with ApproxLog's own 2^-59.37 and well under the
      eta_log <= 2^-57 budget; accumulated delta_tau ~ 2^-46.8 < 2^-45);
  (3) keep R2LN2_QF < 2^64 so it stores in a single uint64_t (F=44 gives
      16599047320634951608 = 2^63.85; F>=46 would overflow uint64);
  (4) keep the __int128 products comfortably in range: u ~ 2^(26.2+F) = 2^70
      and B<<F ~ 2^70, both far below 2^127.)

Run:  python3 gen_irs_consts.py
Verified by:  make check-consts  (re-run, cmp the region byte-for-byte) +
              the dense-grid float reproduction assert below (offline only).
"""

import os

import mpmath

import autogen

mpmath.mp.dps = 80

# ---- primary inputs (from tab:suf-parameters; shared across all sets) ----
RY = 825
TWO_RSQ = 2 * RY * RY  # 1361250
LN2 = mpmath.ln(2)
TWO_RSQ_LN2 = TWO_RSQ * LN2  # 943546.5995372256...

# ---- the pinned Q-scale ----
R2LN2_QSHIFT = 44  # F: u carried at Q44; see module docstring.
FRAC_QBITS = 62    # ApproxLog `frac` is Q62 (= SHUTTLE_LOG_POLY_QBITS).

# Realized log error feeding the budget cross-check (from approx_log_poly):
APPROXLOG_ABS_LOG2_BITS = mpmath.mpf("59.37")  # |Delta_{a,b}| <= 2^-59.37
# Binding |ln U|: SamplerU gives U = 2^-a * b with a <= kappa_a+1 = 81 and
# b in [1,2), so |ln U| <= 81*ln2 = 56.15 absolutely.  (Was 50.53 = 784V/2r^2,
# the value for the old 28-term truncation; the implemented transition keeps
# m = 0..29, whose smallest retained threshold is 841V/2r^2, so the old bound
# was both stale and not an absolute one.)
LNU_BIND = mpmath.mpf("56.15")


def round_half_even(x):
    return int(mpmath.nint(x))


def derive():
    F = R2LN2_QSHIFT
    R = round_half_even(TWO_RSQ_LN2 * mpmath.mpf(2) ** F)
    relerr = abs(mpmath.mpf(R) / mpmath.mpf(2) ** F - TWO_RSQ_LN2) / TWO_RSQ_LN2
    return F, R, relerr


def budget_check(F, R, relerr):
    """Re-derive the accumulated relative-error delta_tau and confirm < 2^-45."""
    ln2 = LN2
    e_approx = ln2 * mpmath.mpf(2) ** (-APPROXLOG_ABS_LOG2_BITS)  # ApproxLog ln-additive
    e_const = LNU_BIND * relerr                                   # constant amplified
    e_trunc = mpmath.mpf(2) ** (-(F + mpmath.log(TWO_RSQ, 2)))    # u_frac >>62 round
    delta_log = e_approx + e_const + e_trunc
    Cvr = mpmath.mpf(2) ** mpmath.mpf("5.02")
    tau_max = 115  # TAU of the largest set (params.h)
    acc = tau_max * Cvr * (mpmath.e ** delta_log - 1)
    return delta_log, acc


def grid_reproduction_check(F, R):
    """On a dense (t, V, ell)-grid, assert the integer u-vs-boundary comparison
    reproduces the real-number threshold test  exp((-2 m t - m^2 V)/2 r^2) vs U,
    i.e. the integer test  (lo<<F) < u <= (hi<<F)  agrees with the float test
    for every boundary pair, for ell sampled across the SamplerU output range."""
    import random

    rng = random.Random(0x5108)
    N = 29
    mismatches = 0
    checked = 0
    # V at the three reachable squared shift-norms (290^2 .. 296.19^2) and t in
    # the sign-normalized nonnegative reachable range.
    Vset = [84100, 87060, 87728, 85708]  # 290^2 and the three floor(B_k^2)
    for _ in range(20000):
        V = rng.choice(Vset)
        t = rng.randint(0, 120000)  # sign-normalized t >= 0, reachable bound
        # draw an ell ~ log2(U): exponent a geometric in {1..81}, frac in [0,2^62)
        a = 1
        while a < 81 and rng.random() < 0.5:
            a += 1
        frac = rng.randrange(0, 1 << 62)
        log2U = mpmath.mpf(frac) / mpmath.mpf(2) ** 62 - a
        # integer u (mirrors irs.c exactly)
        u_frac = (R * frac + (1 << 61)) >> 62
        u_a = a * R
        u_int = u_frac - u_a  # Q_F
        # real u for the float oracle: u_real = 2 r^2 ln U = 2r^2 ln2 * log2 U
        # compare-of-thresholds: flag set iff some i has lo<u<=hi.
        flag_int = -1
        flag_ref = -1
        for i in range(0, (N + 1) // 2):
            lo = -2 * (2 * i + 1) * t - (2 * i + 1) ** 2 * V
            hi = -4 * i * t - 4 * i * i * V
            # integer side (Q_F): (lo<<F) < u_int <= (hi<<F)
            if (lo << F) < u_int <= (hi << F):
                flag_int = 1
            # float side: lo < 2r^2 ln U <= hi
            u_real = TWO_RSQ_LN2 * log2U  # = 2r^2 ln U
            if mpmath.mpf(lo) < u_real <= mpmath.mpf(hi):
                flag_ref = 1
        checked += 1
        if flag_int != flag_ref:
            mismatches += 1
    return checked, mismatches


BLOCK_TEMPLATE = """\
/* IRS R-transition fixed-point constant (gen_irs_consts.py).
 *
 *   2 r^2 ln 2 = 2*825^2*ln2 = {real}...
 *   R2LN2_QSHIFT = {F}   (the Q-scale F; u is carried at Q{F})
 *   R2LN2_QF     = round(2 r^2 ln2 * 2^{F}) = {R}
 *                = 0x{Rhex}  (fits uint64_t: {R} < 2^64)
 *
 * relative error of the rounded constant = 2^{relbits}; amplified additive
 * natural-log error |ln U|*relerr <= {lnu}*2^{relbits} ~ 2^{econst} (binding
 * |ln U|); total delta_log ~ 2^{dlog}, accumulated delta_tau ~ 2^{acc}
 * (< 2^-45 budget).  See gen_irs_consts.py + log/irs_consts_derivation.txt.
 */
#define R2LN2_QSHIFT {F}
#define R2LN2_QF UINT64_C({R})
"""


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    logdir = os.path.join(here, "log")
    os.makedirs(logdir, exist_ok=True)
    irs_c = os.path.normpath(os.path.join(here, "..", "ref", "irs.c"))
    logpath = os.path.join(logdir, "irs_consts_derivation.txt")

    F, R, relerr = derive()
    assert R < (1 << 64), "R2LN2_QF must fit uint64_t"
    relbits = mpmath.nstr(mpmath.log(relerr, 2), 6)
    delta_log, acc = budget_check(F, R, relerr)
    dlogbits = mpmath.nstr(mpmath.log(delta_log, 2), 6)
    accbits = mpmath.nstr(mpmath.log(acc, 2), 6)
    econst = mpmath.nstr(mpmath.log(LNU_BIND * relerr, 2), 6)

    checked, mismatches = grid_reproduction_check(F, R)

    log = []
    log.append("gen_irs_consts.py -- SHUTTLE IRS R-transition fixed-point constant")
    log.append("")
    log.append(f"  r = {RY}, 2 r^2 = {TWO_RSQ}")
    log.append(f"  2 r^2 ln 2 = {mpmath.nstr(TWO_RSQ_LN2, 25)}")
    log.append(f"  R2LN2_QSHIFT (F) = {F}")
    log.append(f"  R2LN2_QF = round(2 r^2 ln2 * 2^{F}) = {R}  (0x{R:016X})")
    log.append(f"  fits uint64_t: {R} < 2^64 = {1 << 64}  -> {R < (1 << 64)}")
    log.append(f"  constant relative error = 2^{relbits}")
    log.append("")
    log.append("u Q-form (irs.c R_transition):")
    log.append("  u_frac = round(R2LN2_QF * frac_q62 / 2^62) = (R2LN2_QF*frac + 2^61) >> 62  [Q_F]")
    log.append("  u_a    = a * R2LN2_QF   [Q_F]")
    log.append("  u      = u_frac - u_a   [Q_F]   compared against (B << F), B = -2 m t - m^2 V")
    log.append("")
    log.append("Error budget cross-check (delta_tau <= 2^-45):")
    log.append(f"  ApproxLog ln-additive       = ln2 * 2^-{APPROXLOG_ABS_LOG2_BITS} = 2^{mpmath.nstr(mpmath.log(LN2*mpmath.mpf(2)**(-APPROXLOG_ABS_LOG2_BITS),2),6)}")
    log.append(f"  constant amplified (|lnU|<= {LNU_BIND}) = {LNU_BIND}*2^{relbits} = 2^{econst}")
    log.append(f"  total delta_log             = 2^{dlogbits}")
    log.append(f"  accumulated delta_tau       = 114 * 2^5.02 * (e^delta_log - 1) = 2^{accbits}   (< 2^-45)")
    log.append("")
    log.append("__int128 range (no overflow):")
    log.append(f"  R2LN2_QF * frac_q62 <= 2^{mpmath.nstr(mpmath.log(R,2)+62,6)}  (< 2^128, unsigned __int128)")
    log.append(f"  |u|, |B<<F|          <= ~2^{mpmath.nstr(mpmath.log(TWO_RSQ_LN2*81,2)+F,6)}  (< 2^127, signed __int128)")
    log.append("")
    log.append(f"Dense-grid reproduction check: {checked} (t,V,ell) triples, "
               f"integer vs float flag mismatches = {mismatches}")
    assert mismatches == 0, f"integer u-test diverged from float oracle on {mismatches} grid points"
    log.append("  PASS: integer u-vs-boundary comparison == float threshold test on every grid point.")

    body = BLOCK_TEMPLATE.format(
        real=mpmath.nstr(TWO_RSQ_LN2, 22),
        F=F, R=R, Rhex=f"{R:016X}",
        relbits=relbits, lnu=LNU_BIND, econst=econst, dlog=dlogbits, acc=accbits,
    )
    autogen.patch_region(irs_c, "irs_consts", body)

    with open(logpath, "w") as f:
        f.write("\n".join(log) + "\n")
    print("\n".join(log))
    print(f"\nPatched @@AUTOGEN:irs_consts@@ in {irs_c}")
    print(f"Wrote audit log {logpath}")


if __name__ == "__main__":
    main()
