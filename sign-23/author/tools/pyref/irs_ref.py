#!/usr/bin/env python3
"""irs_ref.py -- SHUTTLE RejectSample / R-transition mirror for the Python
reference.  Byte/value-exact to ref/irs.c + ref/sampler_u.c.

The IRS path is rejection-FREE: exactly TAU transitions, each consuming a
FIXED 18 bytes (10 exponent + 8 mantissa) off the single 0x09||seed_y bulk
buffer (drawn ONCE, sliced 18 B/transition in ascending-j order, K2/K3/K5).

R-transition (alg:Ryv): form u = (2 r^2 ln2)*log2(U) at Q44, sign-normalize v
so <z,v> > 0, run the 15 boundary-pair interval tests, apply z -= flag*v
(interval hit flag=+1 => y-v, matching pv).

The R2LN2 fixed-point constant (R2LN2_QSHIFT / R2LN2_QF) is parsed from the
committed ref/irs.c (no re-derivation -> no new magic numbers).
"""
import os
import re

from params import params
from sampler_ref import (clz80_msb_first, mantissa57_msb_first,
                         _log2_frac_q62)

_HERE = os.path.dirname(os.path.abspath(__file__))
_REF = os.path.normpath(os.path.join(_HERE, "..", "..", "ref"))
IRS_C = os.path.join(_REF, "irs.c")


# ---- R2LN2 fixed-point constant (parsed from irs.c) ----
_R2LN2 = None


def _r2ln2():
    global _R2LN2
    if _R2LN2 is None:
        txt = open(IRS_C).read()
        sh = int(re.search(r"#define\s+R2LN2_QSHIFT\s+(\d+)", txt).group(1))
        qf = int(re.search(r"#define\s+R2LN2_QF\s+UINT64_C\((\d+)\)", txt).group(1))
        _R2LN2 = (sh, qf)
    return _R2LN2


# ---- SamplerU decode (sampler_u.c sampler_u_decode) ----
def sampler_u_decode(rho_a, rho_b):
    """Pure (no-XOF) decode of one 18-byte block -> (a, frac_q62).
    a = clz80(rho_a)+1; m = top-57(rho_b); j = m>>55; x = (m & 2^55-1)<<9;
    frac = ApproxLog2(j, x)."""
    a = clz80_msb_first(rho_a) + 1
    m = mantissa57_msb_first(rho_b)            # top 57 MSB-first bits
    j = m >> 55                                 # top g=2 bits
    x_q64 = (m & ((1 << 55) - 1)) << 9
    frac = _log2_frac_q62(j, x_q64)
    return a, frac


# ---- u_q44 (irs.c sampler_u_to_u_q44) ----
def _sampler_u_to_u_q44(a, frac_q62):
    """u_q44 = (2 r^2 ln2)*log2(U) at Q44.  log2(U) = frac/2^62 - a."""
    _sh, qf = _r2ln2()
    prod = qf * (frac_q62 & ((1 << 64) - 1))           # frac_q62 as uint64
    u_frac = (prod + (1 << 61)) >> 62                    # round >> 62
    u_a = a * qf
    return u_frac - u_a                                  # signed Q44


class IRS:
    def __init__(self, set_id):
        p = params(set_id)
        self.N = p["N"]
        self.KVEC = p["KVEC"]
        self.IRS_BDRY = p["IRS_BDRY"]      # 15
        self.TAU = p["TAU"]
        self.R2LN2_QSHIFT = _r2ln2()[0]    # 44

    # ---- inner products + negacyclic shift (irs.c) ----
    def _sk_tilde_norm2(self, sk_tilde):
        acc = 0
        for poly in sk_tilde:
            for c in poly:
                acc += c * c
        return acc

    def _inner(self, a, b):
        acc = 0
        for pa, pb in zip(a, b):
            for x, y in zip(pa, pb):
                acc += x * y
        return acc

    def _shift_negacyclic(self, src, j):
        """v[i].coeffs[k] = (k>=j) ? src[i][k-j] : -src[i][n+k-j]."""
        n = self.N
        out = []
        for poly in src:
            v = [0] * n
            for k in range(j):
                v[k] = -poly[n + k - j]
            for k in range(j, n):
                v[k] = poly[k - j]
            out.append(v)
        return out

    # ---- one R transition (irs.c R_transition) ----
    def _r_transition(self, z, v, V, a, frac_q62):
        u = _sampler_u_to_u_q44(a, frac_q62)
        t = self._inner(z, v)
        # sign-normalize: flip iff t <= 0.  flagmask = (t-1)>>63 (arith) is
        # -1 iff t<=0.  Use 64-bit two's-complement arithmetic shift.
        flagmask = _arsh64(t - 1, 63)              # 0 or -1
        if flagmask:
            for poly in v:
                for k in range(len(poly)):
                    poly[k] = -poly[k]
            t = -t
        # 15 boundary pairs.  flag starts -1; set 1 iff (lo<<F) < u <= (hi<<F).
        F = self.R2LN2_QSHIFT
        flag = -1
        for i in range(self.IRS_BDRY):
            two_i1 = 2 * i + 1
            four_i = 4 * i
            lo = -2 * two_i1 * t - two_i1 * two_i1 * V
            hi = -four_i * t - (4 * i * i) * V
            lo_s = lo << F
            hi_s = hi << F
            if (lo_s < u) and (u <= hi_s):
                flag = 1
        f = flag
        for zi, vi in zip(z, v):
            for k in range(len(zi)):
                zi[k] -= f * vi[k]

    # ---- RejectSample (irs.c reject_sample) ----
    def reject_sample(self, buf, y, c_coeffs, sk_tilde):
        """buf = the TAU*18 bulk IRS byte buffer (already squeezed).
        y = KVEC polys (uncompressed); c_coeffs = challenge poly (list of N).
        sk_tilde = StretchS(sk_full).  Returns z (KVEC polys)."""
        z = [list(poly) for poly in y]                # z <- y
        V = self._sk_tilde_norm2(sk_tilde)
        cur = 0
        for j in range(self.N):
            if c_coeffs[j] == 1:
                rho_a = buf[cur:cur + 10]
                rho_b = buf[cur + 10:cur + 18]
                a, frac = sampler_u_decode(rho_a, rho_b)
                cur += 18
                v = self._shift_negacyclic(sk_tilde, j)
                self._r_transition(z, v, V, a, frac)
        return z


def _arsh64(x, n):
    """arithmetic right shift of a signed value reduced to int64 width."""
    x &= (1 << 64) - 1
    if x >= (1 << 63):
        x -= (1 << 64)
    return x >> n


def _selftest():
    for s in (128, 256, 512):
        irs = IRS(s)
        # sampler_u_decode determinism on all-zero / all-ones blocks.
        a0, f0 = sampler_u_decode([0] * 10, [0] * 8)
        assert a0 == 81 and f0 == 0, (a0, f0)
        aF, fF = sampler_u_decode([0xFF] * 10, [0xFF] * 8)
        assert aF == 1
        # u_q44 sign: log2(U) <= 0 so u <= 0 (a>=1).
        assert _sampler_u_to_u_q44(a0, f0) <= 0
    sh, qf = _r2ln2()
    assert sh == 44 and qf == 16599047320634951608, (sh, qf)
    print("irs_ref.py self-test: sampler_u_decode (a in {1..81}, frac>=0); "
          "u_q44<=0; R2LN2 const parsed (Q44, 0xE65BA997B45887B8)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
