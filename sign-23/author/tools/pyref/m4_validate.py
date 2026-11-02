#!/usr/bin/env python3
"""m4_validate.py -- M4 empirical rANS validation for SHUTTLE (P12).

Signs N real messages with the C ref (rANS path), and for each parses the
realized rANS com length (rlen) and the gathered Q0/Qs/h symbols.  Then:

  1. RESERVE check: max rlen over N signatures vs RANS_RESERVED_BYTES (does
     the 2^-35 reserve hold empirically?).  Also reports the mean/quantiles.
  2. MODEL check: builds empirical Q0/Qs/h histograms, compares to the
     theoretical PMFs the rANS tables assumed (rans_model.py q0/qs/hint_pmf),
     and reports the per-block empirical vs model entropy and the empirical
     mean rANS-com length predicted by the committed frequency tables.

Usage:  python3 m4_validate.py [N]   (default N = 10000)
"""
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.normpath(os.path.join(HERE, "..", "..", "ref"))
TOOLS = os.path.normpath(os.path.join(HERE, ".."))
QSET = {128: "q15361n256", 256: "q61441n512", 512: "q59393n1024"}

if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
import rans            # tools/rans.py (committed-table loader)
import rans_model      # tools/rans.py model PMFs

SCHEME = ("sign.c polyvec.c sampler.c sampler_u.c irs.c rounding.c packing.c "
          "poly.c poly_ntt.c reduce.c rans.c approx_exp.c approx_log.c "
          "symmetric.c drng.c auxfunc.c").split()


def build(mode):
    qs = QSET[mode]
    binp = os.path.join(HERE, "m4_dump_%d" % mode)
    cmd = ["gcc", "-O2", "-std=c99", "-I" + REF, "-I" + os.path.join(REF, "test"),
           "-I" + TOOLS, "-I" + os.path.join(REF, "ntt", qs),
           "-DDISABLE_NAMESPACE=1", "-DSHUTTLE_MODE=%d" % mode,
           os.path.join(HERE, "m4_dump.c")]
    cmd += [os.path.join(REF, s) for s in SCHEME]
    cmd += [os.path.join(REF, "ntt", qs, "ntt_ref.c"), "-o", binp]
    subprocess.run(cmd, check=True)
    return binp


def emp_entropy(hist):
    tot = sum(hist.values())
    h = 0.0
    for c in hist.values():
        if c:
            pf = c / tot
            h -= pf * math.log2(pf)
    return h


def model_entropy_from_table(table):
    """entropy implied by the committed rANS frequency table (bits/symbol)."""
    M = 1 << 10
    h = 0.0
    for f in table["freqs"]:
        p = f / M
        h -= p * math.log2(p)
    return h


def expected_com_bits(hist, table):
    """sum over symbols of count * (-log2(freq/M)) -- the rANS code length the
    committed table assigns to the EMPIRICAL symbol stream (bits)."""
    M = 1 << 10
    lo, n = table["sym_lo"], table["n"]
    freqs = table["freqs"]
    bits = 0.0
    oob = 0
    for sym, c in hist.items():
        slot = sym - lo
        if 0 <= slot < n:
            bits += c * (-math.log2(freqs[slot] / M))
        else:
            oob += c
    return bits, oob


def analyze(mode, N):
    binp = build(mode)
    out = subprocess.run([binp, str(N)], capture_output=True, text=True,
                         check=True).stdout.splitlines()
    reserved = b0 = bs = None
    rlens = []
    q0h, qsh, hh = {}, {}, {}
    nsig = 0
    for line in out:
        if line.startswith("MODE"):
            kv = line.split()
            reserved = int(kv[3])
        elif line.startswith("RLEN"):
            rlens.append(int(line.split()[1]))
        elif line.startswith("SYMS"):
            nsig += 1
            toks = line.split()
            # SYMS Q0 <...> QS <...> H <...>
            i = toks.index("Q0") + 1
            j = toks.index("QS")
            k = toks.index("H")
            for t in toks[i:j]:
                v = int(t); q0h[v] = q0h.get(v, 0) + 1
            for t in toks[j + 1:k]:
                v = int(t); qsh[v] = qsh.get(v, 0) + 1
            for t in toks[k + 1:]:
                v = int(t); hh[v] = hh.get(v, 0) + 1

    tables = rans.load_tables(mode)
    maxr = max(rlens) if rlens else 0
    meanr = sum(rlens) / len(rlens) if rlens else 0
    rlens_sorted = sorted(rlens)
    p99 = rlens_sorted[min(len(rlens) - 1, int(0.99 * len(rlens)))] if rlens else 0

    print("=== SHUTTLE-%d  M4 empirical rANS validation (N=%d real sigs) ==="
          % (mode, nsig))
    print("  RANS_RESERVED_BYTES = %d" % reserved)
    print("  rANS com length: mean=%.1f  p99=%d  MAX=%d  -> reserve %s "
          "(headroom %d B)" %
          (meanr, p99, maxr, "HOLDS" if maxr <= reserved else "OVERFLOW!!",
           reserved - maxr))
    reserve_ok = maxr <= reserved

    # model vs empirical per block
    for name, hist, tab in (("Q0", q0h, tables["q0"]),
                            ("Qs", qsh, tables["qs"]),
                            ("h", hh, tables["hint"])):
        He = emp_entropy(hist)
        Ht = model_entropy_from_table(tab)
        ebits, oob = expected_com_bits(hist, tab)
        lo, n = tab["sym_lo"], tab["n"]
        emin, emax = min(hist), max(hist)
        cover = "in-support" if (emin >= lo and emax < lo + n) else \
            "OUT-OF-SUPPORT(min=%d max=%d vs [%d,%d))" % (emin, emax, lo, lo + n)
        print("  %-3s emp-entropy=%.3f bit/sym  table-entropy=%.3f bit/sym  "
              "oob=%d  %s" % (name, He, Ht, oob, cover))

    return reserve_ok


def main():
    N = int(sys.argv[1]) if len(sys.argv) > 1 else 10000
    ok = True
    for mode in (128, 256, 512):
        ok = analyze(mode, N) and ok
    print()
    print("M4 verdict: reserve holds for all 3 sets over %d sigs each: %s"
          % (N, "YES" if ok else "NO"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
