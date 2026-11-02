#!/usr/bin/env python3
"""SigSize.py -- rANS reservation + signature-size model for SHUTTLE.

Emits, per param set, into @@AUTOGEN:rans_meta@@ of ref/rans.h:
  * RANS_B0, RANS_BS  -- the low-bit split widths (size-optimization sweep)
  * RANS_RESERVED_BYTES -- the per-stream overflow reserve at 2^-35

THE RESERVE.  The merged rANS stream has n_b = n*(1 + lenS + lenE)
symbols.  Per symbol the COST is ell(s) = -log2(f_s/M) using the QUANTIZED
table; the DISTRIBUTION averaging that cost is the TRUE emission PMF (rounded-
Gaussian peel for Q0/Qs incl. tails, full bucket-crossing for the hint).  With

    mu_b    = (sum_i n_i * E_{p_i}[ell_i]) / 8 + FLUSH      (FLUSH = 4*RANS_N = 8)
    sigma_b = sqrt(sum_i n_i * Var_{p_i}[ell_i]) / 8
    R       = ceil( mu_b + max( Phi^{-1}(1-2^-35) * sigma_b , Bernstein(2^-35) ) )

Phi^{-1}(1-2^-35) ~ 6.86 (vs 11.489 = Phi^{-1}(1-2^-100) at a 2^-100 target).  The
Bernstein delta bounds the heavy upper tail of the bounded per-symbol cost
ell(s) in [0, prob_bits].  R is the size of the fixed reserved `com` region; a
stream that exceeds R triggers an out-of-support-style Sign restart (probability
<= 2^-35 per stream).

THE SIZE.  The realized compact signature length is
    sig = challengeSeedBytes + (2 + R) + Rlow,
    Rlow = ceil(n*b0/8) + ceil(lenS*n*bs/8)          (per-block ceil; b0 != bs)
We also report the EXPECTED length (using mu_b instead of R) and the Shannon
entropy FLOOR (using H(.) instead of the quantized cost) so the gap to the
spec targets 1005/2155/4552 is fully attributed.

THE SWEEP.  We sweep (b0, bs) and pick the pair minimizing `sig`; that pinned
optimum is asserted equal to rans_model.PINNED_SPLIT (the cached value the
table generator already used -- the two MUST agree or the tables are stale).

Tables are read from the generated @@AUTOGEN:rans_tables@@ block in ref/rans.h
(single source of truth) for the PINNED widths; the sweep re-derives the model
PMFs/quantization for the candidate widths from rans_model.

Run:  python3 SigSize.py
"""
import math
import os
import re

import autogen
import rans_model

PROB_BITS = rans_model.PROB_BITS
PROB_SCALE = rans_model.PROB_SCALE
RANS_N = rans_model.RANS_N
FLUSH = rans_model.RANS_FLUSH_BYTES

# Phi^{-1}(1 - 2^-35): the CLT quantile for a 2^-35 per-stream overflow.
PHI_INV = 6.86


def parse_tables(path):
    """Read the FREQ arrays for the PINNED widths out of the generated
    @@AUTOGEN:rans_tables@@ region (single source of truth)."""
    txt = autogen.read_region(path, "rans_tables")
    out = {}
    for sec in rans_model.SETS:
        m = re.search(r"#if SHUTTLE_MODE == %s\b(.*?)#endif" % sec, txt, re.S)
        blk = m.group(1)

        def get(name):
            fr = [int(x) for x in
                  re.search(r"%s_FREQ\[\d+\] = \{([^}]*)\}" % name,
                            blk).group(1).split(",") if x.strip()]
            lo = int(re.search(r"%s_LO\s+\((-?\d+)\)" % name, blk).group(1))
            return fr, lo
        zf, zlo = get("RANS_Q0")
        sf, slo = get("RANS_QS")
        hf, hlo = get("RANS_HINT")
        out[sec] = dict(zf=zf, zlo=zlo, sf=sf, slo=slo, hf=hf, hlo=hlo)
    return out


def bernstein_delta(V, p=2.0 ** -35, ell_max=PROB_BITS):
    """Bernstein-style upper-tail bound for a sum of bounded costs.  Returns
    the byte-delta x/8 with x = 8*Delta_bits.  Mirrors Lithium's form with the
    p swapped to 2^-35."""
    tgt = -math.log(p)
    b = tgt * (2.0 / 3.0) * ell_max
    c = tgt * 2.0 * V
    x = (b + math.sqrt(b * b + 4 * c)) / 2.0
    return x / 8.0


def model_for(P, b0, bs):
    """Build (q0, qs, h) PMFs + quantized freqs for candidate widths."""
    q0 = rans_model.q0_pmf(P["r"], P["a1"], b0)
    qs = rans_model.qs_pmf(P["r"], P["asec"], bs)
    h = rans_model.hint_pmf(P["r"], P["ae"], P["ah"], P["Hh"])
    l0, h0 = rans_model.support(q0)
    ls, hs = rans_model.support(qs)
    if (h0 - l0 + 1) > 256 or (hs - ls + 1) > 256:
        return None
    try:
        q0syms, q0freq = rans_model.quantize(q0, l0, h0)
        qssyms, qsfreq = rans_model.quantize(qs, ls, hs)
        hsyms, hfreq = rans_model.quantize(h, 0, P["Hh"] - 1)
    except ValueError:
        return None
    return dict(q0=q0, qs=qs, h=h, q0syms=q0syms, q0freq=q0freq,
                qssyms=qssyms, qsfreq=qsfreq, hsyms=hsyms, hfreq=hfreq)


def size_model(P, b0, bs, m):
    """Compute (R, mu_b, sigma_b, sig, exp_sig, ent_floor, Rlow) for widths."""
    n, lenS, lenE, lam = P["n"], P["lenS"], P["lenE"], P["lam"]
    mu0, v0 = rans_model.cost_moments(
        m["q0freq"], [m["q0"].get(s, 0.0) for s in m["q0syms"]])
    mus, vs = rans_model.cost_moments(
        m["qsfreq"], [m["qs"].get(s, 0.0) for s in m["qssyms"]])
    muh, vh = rans_model.cost_moments(
        m["hfreq"], [m["h"].get(s, 0.0) for s in m["hsyms"]])
    n0, ns, nh = n * 1, n * lenS, n * lenE
    mean_bits = n0 * mu0 + ns * mus + nh * muh
    var_bits = n0 * v0 + ns * vs + nh * vh
    mu_b = mean_bits / 8.0 + FLUSH
    sigma_b = math.sqrt(var_bits) / 8.0
    d_clt = PHI_INV * sigma_b
    d_bern = bernstein_delta(var_bits)
    R = math.ceil(mu_b + max(d_clt, d_bern))
    Rlow = (n * b0 + 7) // 8 + (lenS * n * bs + 7) // 8
    cseed = lam // 4
    sig = cseed + (2 + R) + Rlow
    exp_sig = cseed + 2 + mu_b + Rlow
    # entropy floor (ideal, unquantized, no reserve): seed + H(Q) com bits +
    # the raw low-bit body Rlow (the low bits ARE part of the signature, so
    # the fair floor includes them).
    ent_bits = (n0 * rans_model.entropy(m["q0"])
                + ns * rans_model.entropy(m["qs"])
                + nh * rans_model.entropy(m["h"]))
    ent_floor = cseed + ent_bits / 8.0 + Rlow
    return dict(R=R, mu_b=mu_b, sigma_b=sigma_b, d_clt=d_clt, d_bern=d_bern,
                sig=sig, exp_sig=exp_sig, ent_floor=ent_floor, Rlow=Rlow,
                var_bits=var_bits)


def sweep(P):
    """Return (best_b0, best_b1, best_sig) minimizing sig over (b0, bs)."""
    best = None
    for b0 in range(0, 5):
        for bs in range(0, 10):
            m = model_for(P, b0, bs)
            if m is None:
                continue
            sm = size_model(P, b0, bs, m)
            if best is None or sm["sig"] < best[2]:
                best = (b0, bs, sm["sig"])
    return best


BODY_HEAD = (
    "/* Per-set rANS low-bit split widths + overflow reserve "
    "(tools/SigSize.py).\n"
    " * RANS_RESERVED_BYTES sized for a 2^-35 per-stream overflow over the "
    "TRUE\n"
    " * emission PMFs (Phi^{-1}(1-2^-35)~6.86); RANS_B0/RANS_BS are the "
    "size-sweep\n"
    " * optimum.  Symbol costs come from the quantized rANS tables.\n"
    "%s"
    " */\n")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.normpath(os.path.join(here, "..", "ref", "rans.h"))
    tabs = parse_tables(path)
    print("SigSize.py -- SHUTTLE rANS reservation (2^-35 overflow, true "
          "PMFs)\n")
    log = []
    log.append("SigSize.py -- SHUTTLE rANS reserve + sig-size model "
               "(reproducible)")
    log.append("")
    log.append("Reserve R sized for 2^-35 per-stream overflow "
               "(Phi^{-1}(1-2^-35)~6.86,")
    log.append("Bernstein p=2^-35).  Sizes are APPROXIMATE (theoretical PMFs); "
               "re-validated empirically")
    log.append("against empirical Sign output and re-pinned.")
    log.append("")
    blocks = []
    summary = []
    for sec, P in rans_model.SETS.items():
        b0 = rans_model.PINNED_SPLIT[sec]["b0"]
        bs = rans_model.PINNED_SPLIT[sec]["bs"]
        # Sweep cross-check: the size-optimal pair MUST equal the pinned one
        # the table generator already used (else the committed tables are
        # stale for the sweep optimum).
        sb0, sbs, ssig = sweep(P)
        assert (sb0, sbs) == (b0, bs), (
            "SHUTTLE-%s sweep optimum (b0=%d,bs=%d) != PINNED_SPLIT "
            "(b0=%d,bs=%d); regenerate tables for the new optimum" %
            (sec, sb0, sbs, b0, bs))
        # Build the model at the pinned widths and CROSS-CHECK the FREQ arrays
        # against the committed tables (single source of truth).
        m = model_for(P, b0, bs)
        T = tabs[sec]
        assert m["q0syms"][0] == T["zlo"] and m["q0freq"] == T["zf"], sec
        assert m["qssyms"][0] == T["slo"] and m["qsfreq"] == T["sf"], sec
        assert m["hsyms"][0] == T["hlo"] and m["hfreq"] == T["hf"], sec
        sm = size_model(P, b0, bs, m)
        tgt = P["target"]
        # Bernstein overflow bound at R (sanity report).
        tail_bits = max(sm["d_clt"], sm["d_bern"])
        bnd = math.exp(-((tail_bits * 8) ** 2) /
                       (2 * sm["var_bits"]
                        + (2.0 / 3.0) * PROB_BITS * tail_bits * 8))
        print("--- SHUTTLE-%s (b0=%d bs=%d) ---" % (sec, b0, bs))
        print("  n_b=%d  mu_b=%.1fB sigma_b=%.2fB  Delta_CLT=%.1f "
              "Delta_Bern=%.1f -> R=%d" %
              (P["n"] * (1 + P["lenS"] + P["lenE"]), sm["mu_b"], sm["sigma_b"],
               sm["d_clt"], sm["d_bern"], sm["R"]))
        print("  overflow at R: Pr[len > R] <= 2^%.1f" % math.log2(bnd))
        print("  Rlow=%dB  est sig=%dB (target %d, delta %+d)" %
              (sm["Rlow"], sm["sig"], tgt, sm["sig"] - tgt))
        print("  [expected sig ~%.0fB (mu_b); entropy floor ~%.0fB; both "
              "exceed the target -> targets are aspirational]" %
              (sm["exp_sig"], sm["ent_floor"]))
        log.append("=== SHUTTLE-%s (b0=%d bs=%d) ===" % (sec, b0, bs))
        log.append("  sweep optimum == PINNED_SPLIT (b0=%d,bs=%d): OK" %
                   (b0, bs))
        log.append("  mu_b=%.2fB sigma_b=%.3fB Delta_CLT=%.2f Delta_Bern=%.2f "
                   "-> R=%d" % (sm["mu_b"], sm["sigma_b"], sm["d_clt"],
                                sm["d_bern"], sm["R"]))
        log.append("  Pr[com_len > R] <= 2^%.1f  (target 2^-35)" %
                   math.log2(bnd))
        log.append("  Rlow=%dB  realized sig=%dB (target %d, delta %+d)" %
                   (sm["Rlow"], sm["sig"], tgt, sm["sig"] - tgt))
        log.append("  expected sig ~%.0fB (mu_b), entropy floor ~%.0fB; "
                   "floor alone exceeds target by %.0fB" %
                   (sm["exp_sig"], sm["ent_floor"], sm["ent_floor"] - tgt))
        log.append("")
        summary.append(" *   SHUTTLE-%s: b0=%d bs=%d R=%d (mu_b=%.0f, "
                       "sigma_b=%.1f); est sig=%dB (target %d, %+d)" %
                       (sec, b0, bs, sm["R"], sm["mu_b"], sm["sigma_b"],
                        sm["sig"], tgt, sm["sig"] - tgt))
        blocks.append("#if SHUTTLE_MODE == %s\n"
                      "#define RANS_B0 %d\n"
                      "#define RANS_BS %d\n"
                      "#define RANS_RESERVED_BYTES %d\n"
                      "#endif" % (sec, b0, bs, sm["R"]))
    body = (BODY_HEAD % ("\n".join(summary) + "\n") + "\n".join(blocks))
    autogen.patch_region(path, "rans_meta", body)
    logdir = os.path.join(here, "log")
    os.makedirs(logdir, exist_ok=True)
    with open(os.path.join(logdir, "sigsize.txt"), "w") as f:
        f.write("\n".join(log) + "\n")
    print("\nPatched @@AUTOGEN:rans_meta@@ in %s" % path)
    print("  NOTE: sizes are approximate (theoretical PMFs); the entropy "
          "floor exceeds")
    print("  the spec targets -> targets flagged aspirational (re-validated "
          "empirically).")
    print("  audit log: tools/log/sigsize.txt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
