#!/usr/bin/env python3
"""ntt_ref.py -- complete negacyclic NTT mirror for the SHUTTLE Python
reference (P12).  Byte/value-exact to ref/ntt/<qset>/ntt_ref.c.

Complete NTT over Z_q[x]/(x^n+1): log2(n) butterfly levels then pointwise.
16-bit Montgomery (R = 2^16).  ZMONT[k] = omega^{brv(k)} * R mod q where
omega = smallest primitive 2n-th root (== params ZETA).  Forward CT with k
ascending from 1; inverse GS with k descending from n-1, then * NINVTOMONT.

Validates ntt -> pointwise -> invntt == schoolbook (a*b) mod q.
"""
from params import params
from reduce_ref import Reduce


def _brv(x, bits):
    r = 0
    for i in range(bits):
        r |= ((x >> i) & 1) << (bits - 1 - i)
    return r


def _powmod(b, e, q):
    return pow(b % q, e, q)


def _mod_order(g, q):
    """smallest e with g^e == 1, the multiplicative order of g mod q
    (mirrors ntt_ref.c mod_order: peel q-1 by prime factors)."""
    n = q - 1
    m = q - 1
    p = 2
    while p * p <= m:
        if m % p == 0:
            while m % p == 0:
                m //= p
            while n % p == 0 and _powmod(g, n // p, q) == 1:
                n //= p
        p += 1
    if m > 1:
        while n % m == 0 and _powmod(g, n // m, q) == 1:
            n //= m
    return n


def _prim_root(order, q):
    for g in range(2, q):
        if _mod_order(g, q) == order:
            return g
    return 0


class NTT:
    def __init__(self, set_id):
        p = params(set_id)
        self.N = p["N"]
        self.Q = p["Q"]
        self.bits = p["NTT_LEVELS"]
        self.red = Reduce(set_id)
        R = 1 << 16
        q = self.Q
        w = _prim_root(2 * self.N, q)          # smallest primitive 2n-th root
        assert w == p["ZETA"], (set_id, w, p["ZETA"])
        Rmodq = R % q
        self.ZMONT = [(_powmod(w, _brv(k, self.bits), q) * Rmodq) % q
                      for k in range(self.N)]
        self.R2 = (Rmodq * Rmodq) % q
        ninv = _powmod(self.N, q - 2, q)
        self.NINVTOMONT = (ninv * self.R2) % q

    def ntt(self, r):
        """forward, in place on a list of N uint16 in [0,q); returns new list
        in bit-reversed NTT order."""
        a = list(r)
        red, N = self.red, self.N
        k = 1
        length = N // 2
        while length >= 1:
            s = 0
            while s < N:
                z = self.ZMONT[k]
                k += 1
                for j in range(s, s + length):
                    t = red.fqmul16(z, a[j + length])
                    a[j + length] = red.subm16(a[j], t)
                    a[j] = red.addm16(a[j], t)
                s += 2 * length
            length >>= 1
        return a

    def invntt_tomont(self, r):
        """inverse + to-Montgomery, in place; bare round-trip = a*R mod q."""
        a = list(r)
        red, N = self.red, self.N
        k = N - 1
        length = 1
        while length <= N // 2:
            s = 0
            while s < N:
                z = self.ZMONT[k]
                k -= 1
                for j in range(s, s + length):
                    t = a[j]
                    a[j] = red.addm16(t, a[j + length])
                    a[j + length] = red.fqmul16(z, red.subm16(a[j + length], t))
                s += 2 * length
            length <<= 1
        return [red.fqmul16(x, self.NINVTOMONT) for x in a]

    def pointwise(self, a, b):
        red = self.red
        return [red.fqmul16(a[i], b[i]) for i in range(self.N)]

    def schoolbook(self, a, b):
        """negacyclic a*b mod q in [0,q)."""
        N, q = self.N, self.Q
        c = [0] * N
        for i in range(N):
            for j in range(N):
                p = (a[i] * b[j]) % q
                k = i + j
                if k < N:
                    c[k] = (c[k] + p) % q
                else:
                    c[k - N] = (c[k - N] - p) % q
        return c


def _selftest():
    import random
    for s in (128, 256, 512):
        nt = NTT(s)
        q, N = nt.Q, nt.N
        rng = random.Random(7 + s)
        trials = 3 if N >= 512 else 8   # schoolbook is O(n^2)
        for _ in range(trials):
            a = [rng.randint(0, q - 1) for _ in range(N)]
            b = [rng.randint(0, q - 1) for _ in range(N)]
            # ntt -> pointwise -> invntt_tomont gives PLAIN a*b mod q: the
            # pointwise owes one R^-1 (value a*b*R^-1), and invntt_tomont's
            # n^-1*R^2 scale pays it back exactly, landing at a*b.
            ah, bh = nt.ntt(a), nt.ntt(b)
            ch = nt.pointwise(ah, bh)
            c = nt.invntt_tomont(ch)
            sb = nt.schoolbook(a, b)
            assert c == sb, (s, "ntt*pointwise*invntt != schoolbook")
        # round-trip identity: invntt_tomont(ntt(a)) == a*R mod q
        a = [rng.randint(0, q - 1) for _ in range(N)]
        rt = nt.invntt_tomont(nt.ntt(a))
        Rmodq = (1 << 16) % q
        assert rt == [(x * Rmodq) % q for x in a], (s, "round-trip != a*R")
    print("ntt_ref.py self-test: ntt->pointwise->invntt_tomont == schoolbook; "
          "round-trip == a*R (3 sets)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
