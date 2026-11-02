#!/usr/bin/env python3
"""xcheck_sampler.py -- sampler byte-schedule cross-check of the SHUTTLE
Python reference against the C oracle (P12, deliverable 3).

Per set, builds + runs sampler_oracle.c and asserts:
  - sampler_sigma2 / cdt_scan96 : Python cdt_scan96 over the dumped 384-byte
    buffer + the committed SHUTTLE_RCDT_Z table == the C SIGMA2 output
    (byte-exact: the 96-bit 3x32-limb grouped-LE layout + borrow-fold).
  - sampler_u : for each dumped raw 18-byte (rho_a||rho_b) chunk, the Python
    (a, frac_q62) re-derivation == the C (a, frac) -- proving the MSB-first
    80-bit CLZ exponent + 57-bit mantissa + segmented Q62 log2 byte
    interpretation is byte-exact (K3).  The 18 bytes/call consumption is
    fixed by construction.

Usage:  python3 xcheck_sampler.py
"""
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.normpath(os.path.join(HERE, "..", "..", "ref"))
TOOLS = os.path.normpath(os.path.join(HERE, ".."))
QSET = {128: "q15361n256", 256: "q61441n512", 512: "q59393n1024"}

import sampler_ref
from params import params

# RCDT_Z entry count (RCDT_Z_ENTRIES) and GAUSS_BATCH; shared across sets.
RCDT_Z_ENTRIES = 36
GAUSS_BATCH = 32


def build(mode):
    binp = os.path.join(HERE, "sampler_oracle_%d" % mode)
    srcs = ["sampler.c", "sampler_u.c", "approx_log.c", "approx_exp.c",
            "reduce.c", "symmetric.c", "drng.c", "auxfunc.c"]
    cmd = ["gcc", "-O2", "-std=c99", "-I" + REF, "-I" + TOOLS,
           "-DDISABLE_NAMESPACE=1", "-DSHUTTLE_MODE=%d" % mode,
           os.path.join(HERE, "sampler_oracle.c")]
    cmd += [os.path.join(REF, s) for s in srcs]
    cmd += ["-o", binp]
    subprocess.run(cmd, check=True)
    return binp


def main():
    rcdt = sampler_ref.rcdt_tables()
    Z = rcdt["Z"]
    rc = 0
    for mode in (128, 256, 512):
        binp = build(mode)
        out = subprocess.run([binp], capture_output=True, text=True,
                             check=True).stdout.splitlines()
        sigma2 = None
        cdtbuf = None
        su = []        # C (a, frac)
        suraw = []     # (rho_a bytes, rho_b bytes)
        for line in out:
            p = line.split()
            if p[0] == "SIGMA2":
                sigma2 = [int(x) for x in p[1:]]
            elif p[0] == "CDTBUF":
                cdtbuf = bytes.fromhex(p[1])
            elif p[0] == "SU":
                su.append((int(p[1]), int(p[2])))
            elif p[0] == "SURAW":
                suraw.append((bytes.fromhex(p[1]), bytes.fromhex(p[2])))

        fails = []
        # (1) cdt_scan96 / sampler_sigma2 byte-exact
        py_sigma2 = sampler_ref.cdt_scan96(cdtbuf, Z, RCDT_Z_ENTRIES,
                                           GAUSS_BATCH)
        if py_sigma2 != sigma2:
            fails.append(("sampler_sigma2/cdt_scan96", sigma2, py_sigma2))

        # (2) sampler_u byte interpretation byte-exact
        su_ok = 0
        for (ca, cf), (ra, rb) in zip(su, suraw):
            pa, pf, j, x = sampler_ref.sampler_u_from_bytes(list(ra), list(rb))
            if (pa, pf) != (ca, cf):
                fails.append(("sampler_u#%d" % su_ok, (ca, cf), (pa, pf)))
            else:
                su_ok += 1

        if not fails:
            print("SHUTTLE-%d sampler xcheck: cdt_scan96/sampler_sigma2 "
                  "byte-exact (32 samples); sampler_u (a,frac) byte-exact "
                  "from %d raw 18-byte streams (18 B/call fixed)"
                  % (mode, su_ok))
        else:
            rc = 1
            print("=== SHUTTLE-%d sampler xcheck FAIL ===" % mode)
            for tag, c, py in fails[:10]:
                print("  BUG %s  C=%s  PY=%s" % (tag, c, py))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
