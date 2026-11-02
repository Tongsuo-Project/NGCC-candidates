#!/usr/bin/env python3
"""reduce_ref.py -- modular-reduction substrate mirror for the SHUTTLE Python
reference (P12).  Byte/value-exact to ref/reduce.{h,c}.

Two domains (mirror reduce.h):
  (1) uint16 Montgomery NTT core (R = 2^16, canonical [0,q)):
        montgomery_reduce16 / fqmul16 / addm16 / subm16
  (2) int32 signed scheme-domain Barrett helpers:
        reduce32 / caddq / freeze / caddq2 / reduce_mod_2q

These are integer functions of (q, set).  Validated against the C in
xcheck_reduce (driven by the test dumper).
"""
from params import params

BARRETT_SH = 46  # reduce.c (MS-C6: 46 not Lithium's 47, avoids uint64 overflow)


class Reduce:
    def __init__(self, set_id):
        p = params(set_id)
        self.Q = p["Q"]
        self.DQ = p["DQ"]
        self.QINV = p["QINV"]            # -(q^-1) mod 2^16
        self.MONT = p["MONT"]            # 2^16 mod q
        self.NINV_TOMONT = p["NINV_TOMONT"]
        self.REC_Q = (1 << BARRETT_SH) // self.Q
        self.REC_2Q = (1 << BARRETT_SH) // self.DQ

    # ---- (1) uint16 Montgomery core (R = 2^16) ----
    def montgomery_reduce16(self, a):
        """a*2^-16 mod q in [0,q), valid for 0 <= a < q*2^16."""
        a &= 0xFFFFFFFF
        m = (a * self.QINV) & 0xFFFF
        t = (a + m * self.Q) >> 16  # exact; 0 <= t < 2q
        return t - self.Q if t >= self.Q else t

    def fqmul16(self, a, b):
        return self.montgomery_reduce16((a & 0xFFFF) * (b & 0xFFFF))

    def addm16(self, a, b):
        s = (a & 0xFFFF) + (b & 0xFFFF)
        return s - self.Q if s >= self.Q else s

    def subm16(self, a, b):
        a &= 0xFFFF
        b &= 0xFFFF
        return a - b if a >= b else a + self.Q - b

    # ---- (2) int32 signed scheme-domain Barrett ----
    @staticmethod
    def _to_int32(a):
        a &= 0xFFFFFFFF
        return a - (1 << 32) if a >= (1 << 31) else a

    def _barrett_mod_u(self, au, d, rec):
        qh = (au * rec) >> BARRETT_SH
        r = (au - qh * d) & 0xFFFFFFFF
        if r >= d:
            r -= d
        return r

    def reduce32(self, a):
        """r congruent to a mod q in (-q,q) == a % q (truncate toward 0)."""
        a = self._to_int32(a)
        au = abs(a)
        r = self._barrett_mod_u(au, self.Q, self.REC_Q)
        return -r if a < 0 else r

    def caddq(self, a):
        a = self._to_int32(a)
        return a + self.Q if a < 0 else a

    def freeze(self, a):
        return self.caddq(self.reduce32(a))

    def caddq2(self, a):
        a = self._to_int32(a)
        return a + self.DQ if a < 0 else a

    def reduce_mod_2q(self, a):
        """canonical residue in [0,2q)."""
        a = self._to_int32(a)
        au = abs(a)
        ru = self._barrett_mod_u(au, self.DQ, self.REC_2Q)
        r = -ru if a < 0 else ru
        if r < 0:
            r += self.DQ
        return r


def _selftest():
    import random
    for s in (128, 256, 512):
        red = Reduce(s)
        q, dq = red.Q, red.DQ
        rng = random.Random(0xBEEF + s)
        for _ in range(20000):
            a = rng.randint(-(1 << 31), (1 << 31) - 1)
            # reduce32 == truncated a%q (C truncates toward zero)
            tr = abs(a) % q
            tr = -tr if a < 0 else tr
            assert red.reduce32(a) == tr, (s, a, red.reduce32(a), tr)
            assert 0 <= red.freeze(a) < q
            assert red.freeze(a) == a % q, (s, a)
            assert 0 <= red.reduce_mod_2q(a) < dq
            assert red.reduce_mod_2q(a) == a % dq, (s, a)
        # Montgomery core: fqmul16(a,b) == a*b*2^-16 mod q for a,b in [0,q)
        for _ in range(20000):
            a = rng.randint(0, q - 1)
            b = rng.randint(0, q - 1)
            exp = (a * b * pow(1 << 16, -1, q)) % q
            assert red.fqmul16(a, b) == exp, (s, a, b)
            assert red.addm16(a, b) == (a + b) % q
            assert red.subm16(a, b) == (a - b) % q
    print("reduce_ref.py self-test: reduce32/freeze/reduce_mod_2q == a%%q/%%2q; "
          "fqmul16/addm16/subm16 == Montgomery (3 sets, 20k each)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
