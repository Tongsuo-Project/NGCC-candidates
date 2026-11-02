#!/usr/bin/env python3
"""rans.py -- byte-exact reference rANS engine for SHUTTLE.

GOLDEN reference the C ref/rans.c must match byte-for-byte; it is also the
KAT oracle seed.

Engine: 32-bit state x, L = 2^23 renorm lower bound, 8-bit renormalization,
prob_bits = 10 (each frequency table sums to 1024), RANS_N = 2 interleaved
streams.  The merged symbol stream is (Q0[.], Qs[.], h[.]) in logical order;
symbol t is coded on rANS state t % RANS_N.  Encode pushes in REVERSE t (so
decode pulls forward t); flush writes state 0 first (lands LAST) so decode
reads state N-1 first.  This is the same engine as the reference but
with THREE tables (Q0, Qs, hint) instead of two.

A "table" is dict(freqs, cdf, sym_lo, n) over a contiguous alphabet
[sym_lo, sym_lo+n-1]; cdf has n+1 prefix sums (cdf[-1] = 1024).

Run:  python3 rans.py     # self-test (3-table single-stream roundtrip)
"""
RANS_L = 1 << 23
PROB_BITS = 10
PROB_SCALE = 1 << PROB_BITS
RANS_N = 2


def make_table(freqs, sym_lo):
    cdf = [0]
    for f in freqs:
        cdf.append(cdf[-1] + f)
    assert cdf[-1] == PROB_SCALE, "freqs sum %d != %d" % (cdf[-1], PROB_SCALE)
    assert all(f >= 1 for f in freqs), "every in-support freq must be >= 1"
    return dict(freqs=freqs, cdf=cdf, sym_lo=sym_lo, n=len(freqs))


def encode(streams):
    """streams: list of (table, [symbols]) in logical order.  Flat sequence
    S[t] = concatenation of the streams' symbols; symbol t is coded on rANS
    state t % RANS_N, pushed in REVERSE t.  Returns the canonical `com` bytes
    (final states flushed state-0-first, then renorm bytes, all reversed)."""
    flat = [(table, s) for table, syms in streams for s in syms]
    x = [RANS_L] * RANS_N
    out = bytearray()
    for t in reversed(range(len(flat))):
        s = t % RANS_N
        table, sym = flat[t]
        slot = sym - table["sym_lo"]
        if slot < 0 or slot >= table["n"]:
            raise ValueError("symbol %d out of support [%d,%d)" %
                             (sym, table["sym_lo"], table["sym_lo"] + table["n"]))
        freq = table["freqs"][slot]
        start = table["cdf"][slot]
        x_max = ((RANS_L >> PROB_BITS) << 8) * freq
        while x[s] >= x_max:
            out.append(x[s] & 0xFF)
            x[s] >>= 8
        x[s] = ((x[s] // freq) << PROB_BITS) + (x[s] % freq) + start
    for s in range(RANS_N):          # flush state 0 first -> lands last
        for _ in range(4):
            out.append(x[s] & 0xFF)
            x[s] >>= 8
    out.reverse()
    return bytes(out)


def _slot(table, val):
    cdf = table["cdf"]
    for s in range(table["n"]):
        if cdf[s] <= val < cdf[s + 1]:
            return s
    raise ValueError("bad rANS slot")


def decode(data, plan):
    """plan: list of (table, count) in stream order.  Returns list of
    sym-lists.  STRICT canonical decode: initial-state range, full
    consumption, terminal state == L (raises ValueError otherwise)."""
    flat_tab = [table for table, count in plan for _ in range(count)]
    T = len(flat_tab)
    if len(data) < 4 * RANS_N:
        raise ValueError("truncated rANS stream")
    x = [0] * RANS_N
    bp = 0
    for s in range(RANS_N - 1, -1, -1):    # init: stream front is state N-1
        v = 0
        for _ in range(4):
            v = (v << 8) | data[bp]
            bp += 1
        x[s] = v
        if not (RANS_L <= x[s] < (RANS_L << 8)):
            raise ValueError("initial rANS state out of range")
    syms = [0] * T
    for t in range(T):
        s = t % RANS_N
        table = flat_tab[t]
        val = x[s] & (PROB_SCALE - 1)
        slot = _slot(table, val)
        syms[t] = table["sym_lo"] + slot
        x[s] = table["freqs"][slot] * (x[s] >> PROB_BITS) + val - \
            table["cdf"][slot]
        while x[s] < RANS_L:
            if bp >= len(data):
                raise ValueError("truncated rANS stream")
            x[s] = (x[s] << 8) | data[bp]
            bp += 1
    if bp != len(data):
        raise ValueError("trailing bytes in rANS stream")
    if any(v != RANS_L for v in x):
        raise ValueError("non-canonical final rANS state")
    out, off = [], 0
    for table, count in plan:
        out.append(syms[off:off + count])
        off += count
    return out


# ---- table loader: parse the committed ref/rans.h for a given mode -------

def load_tables(mode, rans_h_path=None):
    """Parse RANS_Q0/QS/HINT FREQ + LO out of ref/rans.h for `mode`."""
    import os
    import re
    if rans_h_path is None:
        here = os.path.dirname(os.path.abspath(__file__))
        rans_h_path = os.path.normpath(os.path.join(here, "..", "ref",
                                                    "rans.h"))
    txt = open(rans_h_path).read()
    m = re.search(r"@@AUTOGEN:rans_tables@@ BEGIN.*?@@AUTOGEN:rans_tables@@ END",
                  txt, re.S).group(0)
    blk = re.search(r"#if SHUTTLE_MODE == %d\b(.*?)#endif" % mode, m,
                    re.S).group(1)

    def get(name):
        fr = [int(x) for x in
              re.search(r"%s_FREQ\[\d+\] = \{([^}]*)\}" % name,
                        blk).group(1).split(",") if x.strip()]
        lo = int(re.search(r"%s_LO\s+\((-?\d+)\)" % name, blk).group(1))
        return make_table(fr, lo)
    return dict(q0=get("RANS_Q0"), qs=get("RANS_QS"), hint=get("RANS_HINT"))


def _selftest():
    import math
    import random

    def mk(n, lo):
        raw = [max(1, int(800 * math.exp(-((i - n // 2) ** 2) /
                                         (2 * (n / 6.0) ** 2))))
               for i in range(n)]
        s = sum(raw)
        raw = [max(1, round(r * PROB_SCALE / s)) for r in raw]
        raw[raw.index(max(raw))] += PROB_SCALE - sum(raw)
        return make_table(raw, lo)

    rng = random.Random(1234)
    t0 = mk(33, -16)
    ts = mk(25, -12)
    th = mk(13, 0)
    for trial in range(300):
        a = [rng.randint(-16, 16) for _ in range(rng.randint(1, 50))]
        b = [rng.randint(-12, 12) for _ in range(rng.randint(1, 50))]
        c = [rng.randint(0, 12) for _ in range(rng.randint(1, 40))]
        enc = encode([(t0, a), (ts, b), (th, c)])
        da, db, dc = decode(enc, [(t0, len(a)), (ts, len(b)), (th, len(c))])
        assert da == a and db == b and dc == c, (trial, a, da)
        for bad in (enc + b"\x42", enc[:-1]):
            try:
                decode(bad, [(t0, len(a)), (ts, len(b)), (th, len(c))])
            except ValueError:
                pass
            else:
                raise AssertionError("non-canonical stream accepted")
    print("rans.py self-test: roundtrip OK (300 trials, 3-table single stream)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
