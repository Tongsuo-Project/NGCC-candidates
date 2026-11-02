#!/usr/bin/env python3
"""xcheck.py -- byte-for-byte cross-check of the SHUTTLE Python reference
against the C oracle (P12, deliverable 2).

Builds + runs tools/pyref/xcheck_dump.c (the C dumper) per mode, mirrors its
deterministic xorshift64 input RNG, recomputes every tagged line with the
Python ref (reduce_ref / ntt_ref / packing_ref / drng_ref), and ASSERTS
Python == C.  Any divergence is a BUG and is printed prominently.

Usage:  python3 xcheck.py [--mode 128|256|512|all]
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.normpath(os.path.join(HERE, "..", "..", "ref"))
QSET = {128: "q15361n256", 256: "q61441n512", 512: "q59393n1024"}

from params import params
from reduce_ref import Reduce
from ntt_ref import NTT
from packing_ref import Pack
import drng_ref
import rans_ref


# ---- mirror of the C xorshift64 in xcheck_dump.c ----
class XS:
    def __init__(self, seed):
        self.x = seed & 0xFFFFFFFFFFFFFFFF
        if self.x == 0:
            self.x = 0x123456789abcdef0

    def next(self):
        x = self.x
        x ^= (x << 13) & 0xFFFFFFFFFFFFFFFF
        x ^= x >> 7
        x ^= (x << 17) & 0xFFFFFFFFFFFFFFFF
        self.x = x
        return x

    def u32(self):
        return self.next() & 0xFFFFFFFF


def build_and_run(mode):
    qs = QSET[mode]
    binp = os.path.join(HERE, "xcheck_dump_%d" % mode)
    srcs = ["reduce.c", "poly.c", "poly_ntt.c", "packing.c", "rans.c",
            "drng.c", "ntt/%s/ntt_ref.c" % qs]
    cmd = ["gcc", "-O2", "-std=c99",
           "-I" + REF, "-I" + os.path.join(REF, "test"),
           "-I" + os.path.normpath(os.path.join(HERE, "..", "..", "tools")),
           "-I" + os.path.join(REF, "ntt", qs),
           "-DDISABLE_NAMESPACE=1", "-DSHUTTLE_MODE=%d" % mode,
           os.path.join(HERE, "xcheck_dump.c")]
    cmd += [os.path.join(REF, s) for s in srcs]
    cmd += ["-o", binp]
    subprocess.run(cmd, check=True)
    out = subprocess.run([binp], check=True, capture_output=True, text=True)
    return out.stdout.splitlines()


def s16le(b):
    """parse a hex string of int16-LE uint16 -> list of ints."""
    raw = bytes.fromhex(b)
    return [raw[2 * k] | (raw[2 * k + 1] << 8) for k in range(len(raw) // 2)]


# The packing inputs depend on the full xs consumption order across the whole
# dumper.  We re-derive every input in one pass that exactly mirrors
# xcheck_dump.c's draw order, then assert Python == C line by line.

def check_mode_full(mode):
    lines = build_and_run(mode)
    p = params(mode)
    red, nt, pk = Reduce(mode), NTT(mode), Pack(mode)
    n, q, ell, em = p["N"], p["Q"], p["ELL"], p["EM"]
    xs = XS(0xC0FFEE ^ mode)

    # The C dumper consumes xs ONLY in sections (2),(3),(4).  Section (1) DRNG
    # uses no xs.  Replay the exact order:
    # (2) reduce probes: 16 x xs32() as int32
    probes = [xs.u32() for _ in range(16)]
    probes = [(v - (1 << 32) if v >= (1 << 31) else v) for v in probes]
    # (3) NTT input: N x (xs32() % q)
    ntt_in = [xs.u32() % q for _ in range(n)]
    # (4) packing inputs, in the dumper's order:
    seedA = bytes(xs.u32() & 0xFF for _ in range(p["SEEDBYTES"]))
    ceilqab = (q + p["ALPHA_B"] - 1) // p["ALPHA_B"]
    b = [[(xs.u32() % ceilqab) * p["ALPHA_B"] for _ in range(n)]
         for _ in range(em)]
    py_pk = pk.pack_pk(seedA, b)
    cs = p["CHALLENGESEEDBYTES"]
    masterSeed = bytearray(cs)
    tr = bytearray(cs)
    for i in range(cs):
        masterSeed[i] = xs.u32() & 0xFF
        tr[i] = xs.u32() & 0xFF
    s = [[(xs.u32() % (2 * p["BS_ENC"] + 1)) - p["BS_ENC"] for _ in range(n)]
         for _ in range(ell)]
    ep = [[(xs.u32() % (2 * p["BE_ENC"] + 1)) - p["BE_ENC"] for _ in range(n)]
          for _ in range(em)]
    py_sk = pk.pack_sk(seedA, b, bytes(masterSeed), bytes(tr), s, ep)
    comH = [0] * n
    com0 = [0] * n
    for k in range(n):                 # interleaved, matching the C k-loop
        comH[k] = xs.u32() % p["HH"]
        com0[k] = xs.u32() & 1
    py_com = pk.pack_com(comH, com0)
    # rANS byte-exactness is cross-checked separately (xcheck_rans.py) on real
    # model-distributed response vectors -- the synthetic uniform draws here
    # would exceed the tight 2^-35 reserve.

    # DRNG replay (independent of xs)
    drng = drng_ref.DRNG.instantiate(bytes((0x30 + (i % 16)) for i in range(64)))

    fails = []
    npass = 0

    def eq(tag, got, exp):
        nonlocal npass
        if got != exp:
            fails.append((tag, repr(exp)[:80], repr(got)[:80]))
        else:
            npass += 1

    pi = 0  # probe index
    for line in lines:
        parts = line.split()
        if not parts:
            continue
        tag = parts[0]
        if tag.startswith("DRNG["):
            nb = int(tag[5:-1])
            eq(tag, drng_ref.get_random_number(drng, nb * 8).hex(), parts[1])
        elif tag == "RED32":
            eq("RED32", red.reduce32(int(parts[1])), int(parts[2]))
        elif tag == "FREEZE":
            eq("FREEZE", red.freeze(int(parts[1])), int(parts[2]))
        elif tag == "RED2Q":
            eq("RED2Q", red.reduce_mod_2q(int(parts[1])), int(parts[2]))
        elif tag == "NTTIN":
            assert s16le(parts[1]) == ntt_in, "NTT input mismatch (xs drift)"
        elif tag == "NTTOUT":
            eq("NTT-forward", nt.ntt(ntt_in), s16le(parts[1]))
        elif tag == "INVNTT":
            eq("NTT-inverse", nt.invntt_tomont(nt.ntt(ntt_in)), s16le(parts[1]))
        elif tag == "PK":
            eq("PK", py_pk, bytes.fromhex(parts[1]))
        elif tag == "SK":
            eq("SK", py_sk, bytes.fromhex(parts[1]))
        elif tag == "COM":
            eq("COM", py_com, bytes.fromhex(parts[1]))
    return npass, fails


def main():
    modes = [128, 256, 512]
    if "--mode" in sys.argv:
        v = sys.argv[sys.argv.index("--mode") + 1]
        if v != "all":
            modes = [int(v)]
    rc = 0
    for m in modes:
        npass, fails = check_mode_full(m)
        if fails:
            rc = 1
            print("=== SHUTTLE-%d xcheck: %d PASS, %d FAIL ===" %
                  (m, npass, len(fails)))
            for tag, exp, got in fails[:20]:
                print("  BUG  %s  C=%s  PY=%s" % (tag, exp, got))
        else:
            print("SHUTTLE-%d xcheck: ALL %d byte-exact (DRNG, reduce32/"
                  "freeze/reduce_mod_2q, NTT fwd/inv, pk/sk/com)" % (m, npass))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
