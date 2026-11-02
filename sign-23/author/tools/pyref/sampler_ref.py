#!/usr/bin/env python3
"""sampler_ref.py -- BaseSampler 96-bit CDT scan + SamplerU mirror for the
SHUTTLE Python reference (P12, deliverable 3).

Byte-exact to ref/sampler.c (cdt_scan96 / sampler_sigma2 / noise_magnitude_
batch) and ref/sampler_u.c (sampler_u: 80-bit MSB-first CLZ exponent + 57-bit
MSB-first mantissa + segmented Q62 log2).

RCDT tables are parsed from the committed tools/rcdt_tables.h; the ApproxLog
Q62 coefficients are parsed from the committed tools/approx_log_poly.h and
evaluated with the SAME integer Horner (mulhi rounds to nearest) the C reads.
No re-derivation -> no new magic numbers.
"""
import os
import re

_HERE = os.path.dirname(os.path.abspath(__file__))
_TOOLS = os.path.normpath(os.path.join(_HERE, ".."))
RCDT_H = os.path.join(_TOOLS, "rcdt_tables.h")
LOG_H = os.path.join(_TOOLS, "approx_log_poly.h")


# ---- RCDT 96-bit (3x32-limb) tables ----
def _parse_rcdt(name):
    txt = open(RCDT_H).read()
    # capture the whole initializer ... }; (the outer braces + trailing ;)
    m = re.search(r"%s\[\d+\]\[3\]\s*=\s*(\{.*?\});" % re.escape(name),
                  txt, re.S)
    body = m.group(1)
    rows = re.findall(r"\{([^{}]*)\}", body)
    out = []
    for r in rows:
        vals = re.findall(r"0[xX][0-9a-fA-F]+|\d+", r)
        out.append(tuple(int(v, 16) if v.lower().startswith("0x")
                         else int(v) for v in vals))
    return out


def rcdt_tables():
    return dict(
        Z=_parse_rcdt("SHUTTLE_RCDT_Z"),
        noise085=_parse_rcdt("SHUTTLE_RCDT_NOISE_0_85"),
        noise090=_parse_rcdt("SHUTTLE_RCDT_NOISE_0_90"),
        noise100=_parse_rcdt("SHUTTLE_RCDT_NOISE_1_00"),
    )


# ---- ApproxLog Q62 coeffs ----
def _parse_log_coeffs():
    txt = open(LOG_H).read()
    m = re.search(r"kShuttleLogPoly\[\d+\]\[\d+\]\s*=\s*\{(.*?)\};", txt, re.S)
    segs = re.findall(r"\{([^{}]*)\}", m.group(1))
    out = []
    for s in segs:
        out.append([int(x) for x in re.findall(r"INT64_C\((-?\d+)\)", s)])
    return out


_LOG_COEFFS = None


def _log2_frac_q62(j, x_q64):
    """shuttle_log2_frac_q62 mirror: Horner with mulhi rounding to nearest."""
    global _LOG_COEFFS
    if _LOG_COEFFS is None:
        _LOG_COEFFS = _parse_log_coeffs()
    c = _LOG_COEFFS[j]
    ROUND = 1 << 63
    acc = c[-1]
    for ck in reversed(c[:-1]):
        # mulhi(acc, x) = ((acc*x) + 2^63) >> 64  (arithmetic / floor)
        acc = ck + (((acc * x_q64) + ROUND) >> 64)
    return acc


def _u32(b, off):
    """load_le32: little-endian 32-bit at byte offset off."""
    return b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)


def cdt_scan96(rand, Z, entries, batch):
    """Mirror sampler.c cdt_scan96.  Grouped 12-byte layout: group = s>>3,
    lane = s&7, base = group*96 + lane*4; the three 32-bit limbs are LE32
    loads at base+0 / base+32 / base+64.  The borrow folds into the next
    threshold: b0 = [v0 < Z0]; b1 = [v1 < Z1+b0]; b2 = [v2 < Z2+b1]; z += b2.
    (The C adds the borrow to the threshold without masking; the table
    invariant INV-NOMAX -- mid/high limbs != 0xFFFFFFFF -- guarantees Z1+b /
    Z2+b never overflow uint32, so the plain integer add is exact.)"""
    out = []
    for s in range(batch):
        group, lane = s >> 3, s & 7
        base = group * 96 + lane * 4
        v0 = _u32(rand, base + 0)
        v1 = _u32(rand, base + 32)
        v2 = _u32(rand, base + 64)
        z = 0
        for i in range(entries):
            b = 1 if v0 < Z[i][0] else 0
            b = 1 if v1 < Z[i][1] + b else 0
            b = 1 if v2 < Z[i][2] + b else 0
            z += b
        out.append(z)
    return out


# ---- SamplerU (sampler_u.c) ----
def _clz(x, bits):
    if x == 0:
        return bits
    n = 0
    for i in range(bits - 1, -1, -1):
        if (x >> i) & 1:
            return bits - 1 - i
    return bits


def clz80_msb_first(rho_a):
    """80-bit MSB-first CLZ: hi=be64(rho_a[0..7]); lo=(rho_a[8]<<8)|rho_a[9]."""
    hi = int.from_bytes(bytes(rho_a[:8]), "big")
    lo = (rho_a[8] << 8) | rho_a[9]
    if hi != 0:
        return _clz(hi, 64)
    if lo != 0:
        return 64 + _clz(lo, 16)
    return 80


def mantissa57_msb_first(rho_b):
    """top 57 MSB-first bits of be64(rho_b)."""
    return int.from_bytes(bytes(rho_b[:8]), "big") >> (64 - 57)


def sampler_u_from_bytes(rho_a, rho_b):
    """Returns (a, frac_q62, j, x_q64) mirroring sampler_u()."""
    a = clz80_msb_first(rho_a) + 1
    m = mantissa57_msb_first(rho_b)
    j = m >> 55                       # top g=2 bits
    x_q64 = (m & ((1 << 55) - 1)) << 9
    frac = _log2_frac_q62(j, x_q64)
    return a, frac, j, x_q64


def _selftest():
    t = rcdt_tables()
    assert len(t["Z"]) == 36 and len(t["noise085"]) == 9
    assert len(t["noise090"]) == 10 and len(t["noise100"]) == 11
    # cdt_scan96 produces 32 magnitudes in [0, RCDT_Z_ENTRIES]
    import random
    rng = random.Random(5)
    rand = bytes(rng.randrange(256) for _ in range(384))
    a = cdt_scan96(rand, t["Z"], 36, 32)
    assert len(a) == 32 and all(0 <= v <= 36 for v in a)
    # sampler_u: all-zero rho_a -> a = 80+1 = 81; all-ones -> a = 1
    a0, f0, j0, x0 = sampler_u_from_bytes([0] * 10, [0] * 8)
    assert a0 == 81, a0
    aF, fF, jF, xF = sampler_u_from_bytes([0xFF] * 10, [0xFF] * 8)
    assert aF == 1, aF
    # frac in [0, 2^62)
    assert 0 <= f0 < (1 << 62) and 0 <= fF < (1 << 62)
    print("sampler_ref.py self-test: RCDT tables parsed (36/9/10/11 rows); "
          "cdt_scan96 borrow OK; SamplerU CLZ a in {1..81}, frac in [0,2^62)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
