#!/usr/bin/env python3
"""gen_rans_tables.py -- emit the THREE rANS frequency tables per SHUTTLE set
into the @@AUTOGEN:rans_tables@@ region of ref/rans.h.

Three tables per set (prob_bits=10, each FREQ sums to 1024):
  * RANS_Q0   : retained quotient Q0 = floor(z0 / 2^b0), z0 = round(y/alpha_1)
  * RANS_QS   : retained quotient Qs = floor(z_s / 2^bs), z_s = round(y/alpha_s)
  * RANS_HINT : the whole hint h in [0, H_h) (no split), bucket-crossing law

Quantization (deterministic): f(s) = max(1, round(PMF(s)*1024)); the
single largest-`raw` bucket (lowest index on a tie) is adjusted so each table
sums to exactly 1024.  Per slot we also emit the Granlund-Montgomery
multiply-by-reciprocal (RCP/RSH/BIAS) for the division-free encoder and a
1024-entry val->slot SLOT lookup for the O(1) decoder; verify_sym brute-checks
RCP == plain divide over the encoder's x range.

Run:  python3 gen_rans_tables.py          # regenerate + write tools/log/...
"""
import os
import random

import autogen
import rans_model

PROB_BITS = rans_model.PROB_BITS
PROB_SCALE = rans_model.PROB_SCALE
RANS_L = 1 << 23                 # encoder lower bound (matches rans.h)


# ---- Granlund-Montgomery reciprocal for the encoder's x/freq -------------
# The encoder replaces  x_new = (x // freq) << PB + x % freq + start  with
#   q = (x*RCP >> 32) >> RSH ;  x_new = x + BIAS + q*(PSCALE - freq).
# With shift = ceil(log2 freq), RCP = ceil(2^(shift+31)/freq), RSH = shift-1,
# q == floor(x/freq) EXACTLY for all x in [0, 2^31).  The encoder's x is
# always < x_max = (RANS_L>>PB<<8)*freq <= 2^31, so the byte stream (hence the
# KAT) is unchanged.  freq==1 is the special case (RCP=~0, RSH=0,
# BIAS=start+M-1).  Verified per slot by verify_sym below.
def rans_sym_init(start, freq):
    assert 1 <= freq <= PROB_SCALE
    if freq < 2:
        return 0xFFFFFFFF, 0, start + PROB_SCALE - 1
    shift = 0
    while freq > (1 << shift):
        shift += 1                                  # ceil(log2 freq)
    rcp = ((1 << (shift + 31)) + freq - 1) // freq  # ceil(2^(shift+31)/freq)
    assert 0 <= rcp <= 0xFFFFFFFF
    return rcp, shift - 1, start


def _enc_ref(x, start, freq):                       # plain-division reference
    return ((x // freq) << PROB_BITS) + (x % freq) + start


def _enc_rcp(x, start, freq, rcp, rsh, bias):       # reciprocal (mirrors C u32)
    q = (((x * rcp) >> 32) & 0xFFFFFFFF) >> rsh
    return (x + bias + q * (PROB_SCALE - freq)) & 0xFFFFFFFF


def verify_sym(start, freq, rcp, rsh, bias):
    """Double-check the reciprocal state-update equals the plain divide over a
    thorough sample of the encoder's x range [1, x_max).  GM proves
    exactness; this guards against any transcription/formula error."""
    x_max = ((RANS_L >> PROB_BITS) << 8) * freq     # == 2^21 * freq
    kmax = x_max // freq                            # == 2^21
    rng = random.Random(0x9E3779B9 ^ (freq << 8) ^ (start & 0xFFFF))
    ks = set(range(0, min(kmax, 2048) + 1)) | {kmax}
    ks |= {rng.randrange(0, kmax + 1) for _ in range(4000)}
    xs = {1, 2, x_max - 1}
    for k in ks:
        for x in (k * freq - 1, k * freq, k * freq + 1):
            if 1 <= x < x_max:
                xs.add(x)
    xs |= {rng.randrange(1, x_max) for _ in range(4000)}
    for x in xs:
        if _enc_rcp(x, start, freq, rcp, rsh, bias) != _enc_ref(x, start, freq):
            raise SystemExit("rcp verify FAIL: freq=%d start=%d x=%d" %
                             (freq, start, x))


def _fmt_rows(vals, per=32):
    lines = []
    for i in range(0, len(vals), per):
        lines.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    return "\n".join(lines)


def emit_table(name, syms, freqs):
    """Render the C declarations for one table.  syms is the contiguous
    alphabet (syms[0] = LO); freqs sum to 1024."""
    lo = syms[0]
    n = len(syms)
    cdf = [0]
    for f in freqs:
        cdf.append(cdf[-1] + f)
    assert cdf[-1] == PROB_SCALE, "%s: freqs sum %d != %d" % (name, cdf[-1],
                                                              PROB_SCALE)
    rcp, rsh, bias = [], [], []
    for start, f in zip(cdf, freqs):                # start[slot] = cdf[slot]
        rc, sh, bi = rans_sym_init(start, f)
        verify_sym(start, f, rc, sh, bi)
        rcp.append(rc)
        rsh.append(sh)
        bias.append(bi)
    # Decoder slot lookup: SLOT[v] = the slot s with CDF[s] <= v < CDF[s+1],
    # for every v in [0, 1024).  Replaces the O(n) linear scan with one load.
    # n <= 256 so uint8 suffices.  CDF[0]=0, CDF[N]=1024 -> full coverage, no
    # CDF hole (every v maps to a valid slot; the support check is done at the
    # pack_sig/unpack_sig wrapper level on the decoded symbol value).
    assert n <= 256, "%s: alphabet %d > 256, SLOT needs a wider type" % (name, n)
    slot = []
    s = 0
    for v in range(PROB_SCALE):
        while not (cdf[s] <= v < cdf[s + 1]):
            s += 1
        slot.append(s)
    assert slot[0] == 0 and slot[-1] == n - 1
    fb = ", ".join(str(f) for f in freqs)
    cb = ", ".join(str(c) for c in cdf)
    rb = ", ".join("%uu" % v for v in rcp)
    sb = ", ".join(str(v) for v in rsh)
    bb = ", ".join("%uu" % v for v in bias)
    return ("#define %s_LO   (%d)\n"
            "#define %s_N    (%d)\n"
            "static const uint16_t %s_FREQ[%d] = { %s };\n"
            "static const uint16_t %s_CDF[%d] = { %s };\n"
            "/* encoder x/freq via multiply-by-reciprocal (ryg rANS; exact, "
            "see gen_rans_tables.py) */\n"
            "static const uint32_t %s_RCP[%d] = { %s };\n"
            "static const uint8_t %s_RSH[%d] = { %s };\n"
            "static const uint32_t %s_BIAS[%d] = { %s };\n"
            "/* decoder val->slot direct lookup (%d entries; O(1), replaces "
            "linear CDF scan) */\n"
            "static const uint8_t %s_SLOT[%d] = {\n%s\n};\n" %
            (name, lo, name, n,
             name, n, fb,
             name, n + 1, cb,
             name, n, rb,
             name, n, sb,
             name, n, bb,
             PROB_SCALE,
             name, PROB_SCALE, _fmt_rows(slot)))


BODY_HEAD = (
    "/* THREE rANS frequency tables per set (prob_bits=10, FREQ sum=1024; "
    "see tools/gen_rans_tables.py):\n"
    " *   RANS_Q0_*  : retained quotient Q0 = floor(z0/2^b0), z0=round(y/a1)\n"
    " *   RANS_QS_*  : retained quotient Qs = floor(z_s/2^bs), z_s=round(y/as)\n"
    " *   RANS_HINT_*: the whole hint h in [0,H_h) (no split)\n"
    " * Q0/Qs alphabets are contiguous [LO, LO+N-1]; the hint alphabet is the\n"
    " * full [0,H_h) (mod-wrap makes it bimodal near 0 and H_h).  CDF has N+1\n"
    " * prefix sums (CDF[0]=0, CDF[N]=1024).\n"
    "%s"
    " */\n")


def main():
    print("gen_rans_tables.py -- SHUTTLE three-table rANS frequency tables\n")
    log = []
    log.append("gen_rans_tables.py -- SHUTTLE rANS frequency tables "
               "(reproducible)")
    log.append("")
    log.append("Three static models per set (prob_bits=%d, M=%d), quantized "
               "with the" % (PROB_BITS, PROB_SCALE))
    log.append("deterministic largest-`raw` (lowest-index) tie-break so each "
               "FREQ sums to M.")
    log.append("Q0/Qs are the retained quotients floor(z/2^b) of the rounded "
               "Gaussian z;")
    log.append("hint is the whole [0,H_h) bucket-crossing law.  Per slot the "
               "GM reciprocal")
    log.append("RCP/RSH/BIAS is verify_sym-checked == plain divide over the "
               "encoder x range.")
    log.append("")
    blocks = []
    summary = []
    for sec, P in rans_model.SETS.items():
        b0 = rans_model.PINNED_SPLIT[sec]["b0"]
        bs = rans_model.PINNED_SPLIT[sec]["bs"]
        q0 = rans_model.q0_pmf(P["r"], P["a1"], b0)
        qs = rans_model.qs_pmf(P["r"], P["asec"], bs)
        h = rans_model.hint_pmf(P["r"], P["ae"], P["ah"], P["Hh"])
        l0, h0 = rans_model.support(q0)
        ls, hs = rans_model.support(qs)
        q0syms, q0freq = rans_model.quantize(q0, l0, h0)
        qssyms, qsfreq = rans_model.quantize(qs, ls, hs)
        # hint coded over the FULL [0, H_h) range (every symbol f_s >= 1)
        hsyms, hfreq = rans_model.quantize(h, 0, P["Hh"] - 1)
        Hq0 = rans_model.entropy(q0)
        Hqs = rans_model.entropy(qs)
        Hh = rans_model.entropy(h)
        print("--- SHUTTLE-%s (b0=%d bs=%d) ---" % (sec, b0, bs))
        print("  Q0  : support [%d,%d] (%d syms)  H=%.3f bit/coef" %
              (l0, h0, h0 - l0 + 1, Hq0))
        print("  Qs  : support [%d,%d] (%d syms)  H=%.3f bit/coef" %
              (ls, hs, hs - ls + 1, Hqs))
        print("  hint: range [0,%d) (%d syms)  H=%.3f bit/coef" %
              (P["Hh"], P["Hh"], Hh))
        log.append("=== SHUTTLE-%s (b0=%d bs=%d) ===" % (sec, b0, bs))
        log.append("  Q0  : support [%d,%d] (%d syms)  H=%.4f bit/coef" %
                   (l0, h0, h0 - l0 + 1, Hq0))
        log.append("  Qs  : support [%d,%d] (%d syms)  H=%.4f bit/coef" %
                   (ls, hs, hs - ls + 1, Hqs))
        log.append("  hint: range [0,%d) (%d syms)  H=%.4f bit/coef" %
                   (P["Hh"], P["Hh"], Hh))
        log.append("  quantizer: all FREQ sum == %d, min freq >= 1; "
                   "GM reciprocals verify_sym-PASS" % PROB_SCALE)
        log.append("")
        summary.append(" *   SHUTTLE-%s: Q0 |%d| H=%.2f; Qs |%d| H=%.2f; "
                       "hint |%d| H=%.2f" %
                       (sec, h0 - l0 + 1, Hq0, hs - ls + 1, Hqs, P["Hh"], Hh))
        block = ("#if SHUTTLE_MODE == %s\n" % sec
                 + emit_table("RANS_Q0", q0syms, q0freq)
                 + emit_table("RANS_QS", qssyms, qsfreq)
                 + emit_table("RANS_HINT", hsyms, hfreq)
                 + "#endif\n")
        blocks.append(block)
    body = (BODY_HEAD % ("\n".join(summary) + "\n") + "\n" + "\n".join(blocks))
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.normpath(os.path.join(here, "..", "ref", "rans.h"))
    autogen.patch_region(path, "rans_tables", body)
    logdir = os.path.join(here, "log")
    os.makedirs(logdir, exist_ok=True)
    with open(os.path.join(logdir, "rans_tables.txt"), "w") as f:
        f.write("\n".join(log) + "\n")
    print("\nPatched @@AUTOGEN:rans_tables@@ in %s" % path)
    print("  reciprocal tables (RCP/RSH/BIAS) self-check: PASS (x/freq exact)")
    print("  audit log: tools/log/rans_tables.txt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
