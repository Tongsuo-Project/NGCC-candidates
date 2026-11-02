#!/usr/bin/env python3
"""rans_model.py -- shared probability model for the SHUTTLE rANS generators.

This is the single source of truth for the THREE static rANS source laws of
SHUTTLE's signature compression, ported from the reference
``tools/rans_model.py`` but re-targeted to SHUTTLE's mechanism, which differs
on four counts:

  * TWO distinct split widths ``b0 != bs`` (block-adaptive), not the reference's
    single uniform ``T``;
  * THREE logical tables ``(Q0, Qs, h)``, not the reference's two
    ``(ZH, HINT)`` -- Q0 and Qs share the masking Gaussian but have very
    different effective std-devs (r/alpha_1 vs r/alpha_s), so they need
    separate supports/freqs;
  * the hint law buckets with size ``alpha_h`` (not the reference's ``tau``)
    and is reduced ``mod H_h`` into the contiguous range ``[0, H_h)`` -- so
    unlike the reference's signed ``[-M, M]`` hint alphabet, the SHUTTLE hint
    alphabet is the FULL ``[0, H_h)`` (the mod wrap makes negative crossings
    land near ``H_h``; coding the whole range keeps every honest ``h``
    in-support and guarantees no CDF-hole -- Description.tex:2387-2408);
  * the overflow reserve targets ``2^-35`` per stream, not the reference's
    ``2^-100`` (SigSize.py).

The three source laws (Description.tex:2365-2406), in the logical symbol order
``(Q0[.], Qs[.], h[.])`` (polynomial-major then coefficient-major):

  * Q0  -- high part of the constant block z0.  z0 = round(y/alpha_1),
           y ~ D_{Z,r}; effective sigma_z0 = r/alpha_1.  Peel b0 low bits:
           Q0 = floor(z0 / 2^b0).  Count = n * 1.
  * Qs  -- high part of the secret block z_s.  z_s = round(y/alpha_s);
           effective sigma_zs = r/alpha_s.  Peel bs low bits:
           Qs = floor(z_s / 2^bs).  Count = n * lenS.
  * h   -- the whole hint, NO split.  Per-coeff
           h = (floor(w/alpha_h) - floor((w-2z2)/alpha_h)) mod H_h, with the
           phase w mod alpha_h modelled uniform and z2 = round(y/alpha_e)
           a rounded Gaussian of divisor alpha_e.  A small integer dist on
           [0, H_h) concentrated near 0 (and, via the mod wrap, near H_h).
           Count = n * lenE.

The PMFs are HARD-TRUNCATED at the per-block tail-cut supports B_z0, B_zs, B_z2
(~11 sigma, chosen in tail_cut()); the discarded one-dimensional Gaussian tail
mass is far below the 2^-35 rANS overflow budget, and the signer's norm gate
(B_v) guarantees no accepted signature carries a coordinate outside the support.

ALL math here is deterministic and reproducible; the quantizer tie-break
(largest-`raw`, lowest index) pins the spec's under-specified "adjust the
largest-residual symbol until sum == M" rule.

This module is a LIBRARY (no output of its own); gen_rans_tables.py and
SigSize.py import it.  Run `python3 rans_model.py` for a self-check that prints
the per-set entropies / supports.

EMPIRICAL re-validation hook: every PMF builder accepts an optional
`hist` dict {symbol: count}; if given, the empirical histogram REPLACES the
theoretical PMF (normalized), so the generators can be re-run against real
Sign output without code changes -- see q0_pmf/qs_pmf/hint_pmf.
"""
import math

# ---- engine-wide constants (pinned; see rans.h) --------------------------
PROB_BITS = 10                 # M = 2^PROB_BITS = 1024 (denominator)
PROB_SCALE = 1 << PROB_BITS
RANS_N = 2                     # interleaved streams (latency hiding)
RANS_FLUSH_BYTES = 4 * RANS_N  # 4 bytes per state flushed at encode end = 8

# Tail-cut sigma multiple.  ~11 sigma: the per-coordinate truncation failure
# probability is erfc(11/sqrt2) ~ 2^-87.5, far below the 2^-35 overflow
# budget; the signer's B_v gate makes it unreachable for accepted sigs.
TAILCUT_SIGMA = 11.0

# Per-set parameters (verbatim from tab:suf-parameters via ref/params.h; the
# generators assert these against the committed params.h-derived values).
#   n     : ring dimension
#   lenS  : #poly in z_s block         (= ell)
#   lenE  : #poly in z_2/hint block    (= m)
#   r     : masking Gaussian rate (= R_y = 825 all sets)
#   a1    : alpha_1  (z0 divisor)
#   asec  : alpha_s  (z_s divisor)
#   ae    : alpha_e  (z_2 divisor, drives the hint)
#   ah    : alpha_h  (hint bucket size)
#   q     : modulus  (only used for H_h cross-check)
#   Hh    : 2(q-1)/alpha_h  (hint range; NON power of two)
#   lam   : security level (challengeSeedBytes = lam/4)
#   target: spec sig-size target
SETS = {
    "128": dict(n=256,  lenS=3, lenE=3, r=825, a1=90,  asec=10, ae=5, ah=1024,
                q=15361, Hh=30,  lam=128, target=1005),
    "256": dict(n=512,  lenS=3, lenE=2, r=825, a1=135, asec=5,  ae=5, ah=1024,
                q=61441, Hh=120, lam=256, target=2155),
    "512": dict(n=1024, lenS=3, lenE=2, r=825, a1=144, asec=3,  ae=3, ah=2048,
                q=59393, Hh=58,  lam=512, target=4552),
}


def tail_cut(sigma):
    """Per-block truncation support B = ceil(TAILCUT_SIGMA * sigma)."""
    return int(math.ceil(TAILCUT_SIGMA * sigma))


# ---- rounded-Gaussian source PMFs ---------------------------------------

def rounded_div_pmf(r, alpha, B):
    """PMF of k = round(y / alpha) for y ~ D_{Z,r}, truncated to |k| <= B.

    Mirrors the spec law Pr[z=k] = (1/Z_r) sum_{y: round(y/alpha)=k}
    exp(-y^2/2r^2).  The y-range covers every y whose rounded quotient stays
    in [-B, B]; rounding is round-half-up (floor(y/alpha + 1/2)), matching the
    \\lfloor\\cdot\\rceil convention.  Returns a dict {k: prob}.
    """
    pmf = {}
    ymax = int(math.ceil((B + 0.5) * alpha)) + 1
    Z = 0.0
    for y in range(-ymax, ymax + 1):
        w = math.exp(-(y * y) / (2.0 * r * r))
        k = int(math.floor(y / alpha + 0.5))      # round-half-up = round()
        if -B <= k <= B:
            pmf[k] = pmf.get(k, 0.0) + w
            Z += w
    return {k: v / Z for k, v in pmf.items()}


def peel_pmf(base_pmf, b):
    """Distribution of the retained quotient Q = floor(z / 2^b) (arithmetic
    shift, so Q = z >> b for the C peel) given the base z-PMF.  b=0 is the
    identity (rANS-coded in full)."""
    out = {}
    for z, p in base_pmf.items():
        Q = z >> b if b else z          # arithmetic floor for negative z too
        out[Q] = out.get(Q, 0.0) + p
    return out


def _hist_to_pmf(hist):
    s = float(sum(hist.values()))
    return {int(k): v / s for k, v in hist.items()}


def q0_pmf(r, a1, b0, hist=None):
    """Q0 quotient PMF: peel b0 low bits off z0 = round(y/alpha_1).
    If `hist` (empirical {Q0:count}) is given it overrides the theory."""
    if hist is not None:
        return _hist_to_pmf(hist)
    sigma = r / a1
    B = tail_cut(sigma)
    return peel_pmf(rounded_div_pmf(r, a1, B), b0)


def qs_pmf(r, asec, bs, hist=None):
    """Qs quotient PMF: peel bs low bits off z_s = round(y/alpha_s).
    If `hist` (empirical {Qs:count}) is given it overrides the theory."""
    if hist is not None:
        return _hist_to_pmf(hist)
    sigma = r / asec
    B = tail_cut(sigma)
    return peel_pmf(rounded_div_pmf(r, asec, B), bs)


def hint_pmf(r, ae, ah, Hh, hist=None):
    """Hint marginal PMF on the FULL contiguous range [0, H_h).

    For each z2 (rounded Gaussian, divisor alpha_e), the displacement is
    d = 2*z2; the bucket-crossing count has magnitude a = floor(|d|/alpha_h)
    with prob 1-{|d|/alpha_h} and a+1 with prob {|d|/alpha_h}, signed by the
    sign of z2, then reduced mod H_h into [0, H_h) (Description.tex:2387-2408).
    The mod wrap sends negative crossings near H_h, so the support is bimodal
    (near 0 AND near H_h).  Coding the WHOLE [0, H_h) range (every symbol gets
    f_s >= 1) keeps any honest h in-support and gives the no-CDF-hole
    guarantee.  If `hist` (empirical {h:count}) is given it overrides
    theory."""
    if hist is not None:
        return _hist_to_pmf(hist)
    sigma2 = r / ae
    Bz2 = tail_cut(sigma2)
    z2pmf = rounded_div_pmf(r, ae, Bz2)
    out = {}
    for z2, p in z2pmf.items():
        d = 2 * z2
        if d == 0:
            out[0] = out.get(0, 0.0) + p
            continue
        sign = 1 if d > 0 else -1
        a, rem = divmod(abs(d), ah)
        k0 = (sign * a) % Hh
        out[k0] = out.get(k0, 0.0) + p * (1.0 - rem / ah)
        if rem:
            k1 = (sign * (a + 1)) % Hh
            out[k1] = out.get(k1, 0.0) + p * (rem / ah)
    s = sum(out.values())
    return {k: v / s for k, v in out.items()}


# ---- deterministic quantizer (tie-break) ---------------------------------

def quantize(pmf, lo, hi):
    """Quantize a PMF on the contiguous alphabet [lo, hi] to integer
    frequencies summing to exactly PROB_SCALE (= M), every entry >= 1.

    Tie-break (DETERMINISTIC): f(s) = max(1, round(PMF(s)*M));
    add the residual M - sum(f) to the SINGLE largest-`raw` bucket; on a tie
    take the LOWEST index (Python max(range, key=...) returns the first max).
    Returns (syms, freqs) or raises if the alphabet starves (sum of forced
    minima already exceeds M) -- the caller bumps PROB_BITS if that happens.
    """
    syms = list(range(lo, hi + 1))
    n = len(syms)
    if n > PROB_SCALE:
        raise ValueError("alphabet %d > M=%d: bump PROB_BITS" % (n, PROB_SCALE))
    raw = [max(1, round(pmf.get(s, 0.0) * PROB_SCALE)) for s in syms]
    imax = max(range(n), key=lambda i: raw[i])
    raw[imax] += PROB_SCALE - sum(raw)
    if raw[imax] < 1 or sum(raw) != PROB_SCALE:
        raise ValueError("alphabet %d starves at M=%d: bump PROB_BITS" %
                         (n, PROB_SCALE))
    return syms, raw


def support(pmf):
    """(lo, hi) of the contiguous span covering every nonzero-mass symbol."""
    ks = [k for k, v in pmf.items() if v > 0]
    return min(ks), max(ks)


def entropy(pmf):
    return -sum(p * math.log2(p) for p in pmf.values() if p > 0)


def cost_moments(freqs, pmf_vals):
    """(mu, Var) of the per-symbol rANS cost ell(s) = -log2(f_s/M), where the
    cost uses the QUANTIZED table `freqs` but the averaging distribution is the
    TRUE emission PMF `pmf_vals` (aligned index-for-index with `freqs`)."""
    mu = m2 = 0.0
    for g, p in zip(freqs, pmf_vals):
        ell = -math.log2(g / PROB_SCALE)
        mu += p * ell
        m2 += p * ell * ell
    return mu, m2 - mu * mu


# ---- pinned split widths (size-optimization sweep result) ----------------
# Chosen by the (b0, bs) sweep in SigSize.py to MINIMIZE the realized sig size
# (= challengeSeedBytes + 2 + R + raw-low-bits).  The heuristic
# b ~ floor(log2 sigma) - 1 gives b0 = {2,1,1}, bs = {5,6,7}; the realized
# optimum lands at b0 = 2 (all sets), bs = {6,7,7} -- recorded here and
# re-derived/asserted by SigSize.py (the single source of truth is the sweep;
# these are the cached result for the C macro emit and a sanity anchor).
PINNED_SPLIT = {
    "128": dict(b0=2, bs=6),
    "256": dict(b0=2, bs=7),
    "512": dict(b0=2, bs=7),
}


def _selftest():
    print("rans_model.py self-check (theoretical PMFs)\n")
    for sec, P in SETS.items():
        b0 = PINNED_SPLIT[sec]["b0"]
        bs = PINNED_SPLIT[sec]["bs"]
        q0 = q0_pmf(P["r"], P["a1"], b0)
        qs = qs_pmf(P["r"], P["asec"], bs)
        h = hint_pmf(P["r"], P["ae"], P["ah"], P["Hh"])
        l0, h0 = support(q0)
        ls, hs = support(qs)
        # cross-check H_h
        assert P["Hh"] == 2 * (P["q"] - 1) // P["ah"], sec
        # hint must live in [0, H_h)
        assert min(h) >= 0 and max(h) < P["Hh"], (sec, sorted(h))
        print("--- SHUTTLE-%s (b0=%d bs=%d) ---" % (sec, b0, bs))
        print("  Q0 : support [%d,%d] (%d syms)  H=%.3f bit/coef" %
              (l0, h0, h0 - l0 + 1, entropy(q0)))
        print("  Qs : support [%d,%d] (%d syms)  H=%.3f bit/coef" %
              (ls, hs, hs - ls + 1, entropy(qs)))
        print("  h  : range [0,%d) (%d syms)  H=%.3f bit/coef "
              "(bimodal near 0 and %d)" %
              (P["Hh"], P["Hh"], entropy(h), P["Hh"] - 1))
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
