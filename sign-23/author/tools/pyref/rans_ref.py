#!/usr/bin/env python3
"""rans_ref.py -- SHUTTLE-aware rANS codec wrapper for the Python reference
(P12, deliverable 2).

The byte-exact engine already lives in tools/rans.py (the P10 golden model
the C ref/rans.c matches byte-for-byte, asserted by test_rans.c).  This
module wraps it with the SHUTTLE block plan (Q0 ++ Qs ++ h, the logical
order pack_sig uses) and the committed-table loader, so the Python sigEncode
path encodes/decodes exactly as shuttle_rans_encode / shuttle_rans_decode.

  encode(set_id, q0, qs, h) -> com bytes  (== shuttle_rans_encode output)
  decode(set_id, com, nq0, nqs, nh)        (canonical decode + checks)
"""
import os
import sys

# reach tools/rans.py (the golden engine)
_TOOLS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                       ".."))
if _TOOLS not in sys.path:
    sys.path.insert(0, _TOOLS)
import rans as _rans  # tools/rans.py


def tables(set_id):
    return _rans.load_tables(set_id)


def encode(set_id, q0, qs, h):
    """Encode the merged symbol stream (Q0 ++ Qs ++ h) -> canonical com bytes.
    Matches shuttle_rans_encode's flat-stream order and N=2 interleave."""
    t = tables(set_id)
    return _rans.encode([(t["q0"], list(q0)), (t["qs"], list(qs)),
                         (t["hint"], list(h))])


def decode(set_id, com, nq0, nqs, nh):
    """Canonical decode (raises ValueError on any non-canonical stream)."""
    t = tables(set_id)
    out = _rans.decode(com, [(t["q0"], nq0), (t["qs"], nqs), (t["hint"], nh)])
    return out[0], out[1], out[2]


def _selftest():
    import random
    for s in (128, 256, 512):
        t = tables(s)
        q0lo, q0n = t["q0"]["sym_lo"], t["q0"]["n"]
        qslo, qsn = t["qs"]["sym_lo"], t["qs"]["n"]
        hlo, hn = t["hint"]["sym_lo"], t["hint"]["n"]
        rng = random.Random(99 + s)
        for _ in range(200):
            a = [rng.randrange(q0lo, q0lo + q0n) for _ in range(rng.randint(1, 40))]
            b = [rng.randrange(qslo, qslo + qsn) for _ in range(rng.randint(1, 40))]
            c = [rng.randrange(hlo, hlo + hn) for _ in range(rng.randint(1, 30))]
            com = encode(s, a, b, c)
            da, db, dc = decode(s, com, len(a), len(b), len(c))
            assert (da, db, dc) == (a, b, c), (s, "round-trip")
            # re-encode equality (the SHUTTLE injectivity addition, K15)
            assert encode(s, da, db, dc) == com, (s, "re-encode")
            # negatives: trailing byte / truncation must reject
            for bad in (com + b"\x42", com[:-1]):
                try:
                    decode(s, bad, len(a), len(b), len(c))
                except ValueError:
                    pass
                else:
                    raise AssertionError((s, "non-canonical accepted"))
    print("rans_ref.py self-test: round-trip + re-encode equality + negative "
          "reject over committed tables (3 sets)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
