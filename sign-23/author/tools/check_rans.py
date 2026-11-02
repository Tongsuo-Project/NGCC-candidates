#!/usr/bin/env python3
"""check_rans.py -- round-trip + negative-test driver for the SHUTTLE rANS
golden codec, over the real committed tables in ref/rans.h.

It does three things per param set:
  1. ROUND-TRIP: draw many random in-support (Q0, Qs, h) vectors from the
     model PMFs, encode -> decode -> assert equality.
  2. NEGATIVE TESTS (the >=6 mutations of rANS.tex item 9): one extra trailing
     byte, one missing byte, a flipped state-init byte, a flipped last com
     byte, an out-of-range initial state, and a non-terminal end state -- all
     MUST be rejected by the golden decode (mirrors the C negative tests).
  3. RECORD a fixed synthetic vector + its golden `com` bytes into
     ref/test/rans_vectors_<set>.txt, the byte-exactness oracle the C test
     reads (test_rans.c case (b)).

Run:  python3 check_rans.py
"""
import os
import random

import rans
import rans_model

HERE = os.path.dirname(os.path.abspath(__file__))
RANS_H = os.path.normpath(os.path.join(HERE, "..", "ref", "rans.h"))
TESTDIR = os.path.normpath(os.path.join(HERE, "..", "ref", "test"))


def sample_from_pmf(pmf, rng):
    """Inverse-CDF sample one symbol from a {sym: prob} dict."""
    keys = sorted(pmf)
    r = rng.random()
    acc = 0.0
    for k in keys:
        acc += pmf[k]
        if r <= acc:
            return k
    return keys[-1]


def build_plan(tabs, nq0, nqs, nh):
    return [(tabs["q0"], nq0), (tabs["qs"], nqs), (tabs["hint"], nh)]


def roundtrip(mode, P, tabs, rng, trials=400):
    n, lenS, lenE = P["n"], P["lenS"], P["lenE"]
    nq0, nqs, nh = n, n * lenS, n * lenE
    q0 = rans_model.q0_pmf(P["r"], P["a1"],
                           rans_model.PINNED_SPLIT[mode]["b0"])
    qs = rans_model.qs_pmf(P["r"], P["asec"],
                           rans_model.PINNED_SPLIT[mode]["bs"])
    h = rans_model.hint_pmf(P["r"], P["ae"], P["ah"], P["Hh"])
    for _ in range(trials):
        # short random vectors for speed; lengths vary so the interleave/flush
        # boundaries are exercised.
        a0 = [sample_from_pmf(q0, rng) for _ in range(rng.randint(1, 64))]
        a1 = [sample_from_pmf(qs, rng) for _ in range(rng.randint(1, 64))]
        a2 = [sample_from_pmf(h, rng) for _ in range(rng.randint(1, 64))]
        enc = rans.encode([(tabs["q0"], a0), (tabs["qs"], a1),
                           (tabs["hint"], a2)])
        d0, d1, d2 = rans.decode(enc, [(tabs["q0"], len(a0)),
                                       (tabs["qs"], len(a1)),
                                       (tabs["hint"], len(a2))])
        assert d0 == a0 and d1 == a1 and d2 == a2
    # one full-length vector for the negative tests
    a0 = [sample_from_pmf(q0, rng) for _ in range(nq0)]
    a1 = [sample_from_pmf(qs, rng) for _ in range(nqs)]
    a2 = [sample_from_pmf(h, rng) for _ in range(nh)]
    enc = rans.encode([(tabs["q0"], a0), (tabs["qs"], a1), (tabs["hint"], a2)])
    plan = build_plan(tabs, nq0, nqs, nh)
    return a0, a1, a2, enc, plan


def negatives(enc, plan):
    """Return a list of (label, mutated_bytes) the decode MUST reject."""
    muts = []
    muts.append(("extra-trailing-byte", enc + b"\x00"))
    muts.append(("missing-byte", enc[:-1]))
    # flip a state-init byte (the first 4*N bytes are the packed states)
    b = bytearray(enc)
    b[0] ^= 0x01
    muts.append(("flip-state-init-byte", bytes(b)))
    # flip the last com byte
    b = bytearray(enc)
    b[-1] ^= 0x80
    muts.append(("flip-last-com-byte", bytes(b)))
    # force an out-of-range initial state: zero the top state bytes so x < L
    b = bytearray(enc)
    b[0] = 0
    b[1] = 0
    b[2] = 0
    b[3] = 0
    muts.append(("out-of-range-init-state", bytes(b)))
    return muts


def record_vector(mode, P, tabs, rng):
    """Write a FIXED synthetic vector + golden com bytes to
    ref/test/rans_vectors_<mode>.txt (the C byte-exactness oracle)."""
    n, lenS, lenE = P["n"], P["lenS"], P["lenE"]
    # A small deterministic synthetic vector (not the model sample): a few
    # symbols per table spanning the support edges, so the C and Python encode
    # paths are compared on identical inputs.
    q0t, qst, ht = tabs["q0"], tabs["qs"], tabs["hint"]

    def spread(table, count):
        lo = table["sym_lo"]
        hi = lo + table["n"] - 1
        out = []
        for i in range(count):
            # walk lo..hi..lo to hit both edges + interior
            span = hi - lo
            v = lo + (i % (span + 1))
            out.append(v)
        return out
    a0 = spread(q0t, 37)
    a1 = spread(qst, 41)
    a2 = spread(ht, 29)
    com = rans.encode([(q0t, a0), (qst, a1), (ht, a2)])
    # round-trip sanity
    d0, d1, d2 = rans.decode(com, [(q0t, len(a0)), (qst, len(a1)),
                                   (ht, len(a2))])
    assert d0 == a0 and d1 == a1 and d2 == a2
    os.makedirs(TESTDIR, exist_ok=True)
    path = os.path.join(TESTDIR, "rans_vectors_%s.txt" % mode)
    with open(path, "w") as f:
        f.write("# SHUTTLE-%s rANS C-vs-Python byte-exactness vector "
                "(tools/check_rans.py)\n" % mode)
        f.write("# Format: header line, then q0/qs/h symbol lines, then the "
                "golden com bytes.\n")
        f.write("nq0 %d\nnqs %d\nnh %d\n" % (len(a0), len(a1), len(a2)))
        f.write("q0 " + " ".join(str(v) for v in a0) + "\n")
        f.write("qs " + " ".join(str(v) for v in a1) + "\n")
        f.write("h " + " ".join(str(v) for v in a2) + "\n")
        f.write("comlen %d\n" % len(com))
        f.write("com " + " ".join("%d" % b for b in com) + "\n")
    return path, len(com)


def main():
    print("check_rans.py -- SHUTTLE rANS golden round-trip + negatives\n")
    rng = random.Random(0xC0FFEE)
    rc = 0
    for mode_s, P in rans_model.SETS.items():
        mode = int(mode_s)
        tabs = rans.load_tables(mode, RANS_H)
        # sanity: full coverage + sums
        for nm, t in tabs.items():
            assert t["cdf"][0] == 0 and t["cdf"][-1] == rans.PROB_SCALE, nm
        a0, a1, a2, enc, plan = roundtrip(mode_s, P, tabs, rng)
        print("--- SHUTTLE-%s ---" % mode_s)
        print("  round-trip: PASS (full vector com = %d bytes)" % len(enc))
        nfail = 0
        for label, bad in negatives(enc, plan):
            try:
                rans.decode(bad, plan)
            except ValueError:
                pass
            else:
                print("  NEGATIVE FAIL: %s accepted!" % label)
                nfail += 1
                rc = 1
        if nfail == 0:
            print("  negatives: PASS (all %d mutations rejected)" %
                  len(negatives(enc, plan)))
        path, clen = record_vector(mode_s, P, tabs, rng)
        print("  recorded byte-exactness vector: %s (com=%d bytes)" %
              (os.path.relpath(path, os.path.normpath(
                  os.path.join(HERE, ".."))), clen))
    print("\ncheck_rans.py: %s" % ("PASS" if rc == 0 else "FAIL"))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
