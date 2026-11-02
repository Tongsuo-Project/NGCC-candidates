#!/usr/bin/env python3
"""rounding_ref.py -- CompressY / StretchS / RoundB / mod-2q lift / hint / norm
gates mirror for the SHUTTLE Python reference.  Value-exact to ref/rounding.c.

The CompressY round-to-nearest magic reciprocals (RCP_ALPHA_* / SH_ROUND_* /
ROUND_BIAS_* / LOG2_ALPHA_H) are parsed from the committed
tools/rounding_consts.h (no re-derivation -> no new magic numbers).  The
matrix products (mat_mul_2q / mat_mul_z1_2q) live here too, wired through the
NTT mirror (ntt_ref) and the Montgomery core (reduce_ref) exactly as the C.
"""
import os
import re

from params import params
from reduce_ref import Reduce
from ntt_ref import NTT

_HERE = os.path.dirname(os.path.abspath(__file__))
_TOOLS = os.path.normpath(os.path.join(_HERE, ".."))
ROUND_H = os.path.join(_TOOLS, "rounding_consts.h")


def _parse_round_consts(mode):
    """Parse the per-mode block of rounding_consts.h."""
    txt = open(ROUND_H).read()
    # split into the three #if SHUTTLE_MODE == ... blocks
    blocks = re.split(r"#if SHUTTLE_MODE == (\d+)", txt)
    out = {}
    for i in range(1, len(blocks), 2):
        m = int(blocks[i])
        body = blocks[i + 1]
        d = {}
        for name, val in re.findall(r"#\s*define\s+(\w+)\s+(?:INT(?:32|64)_C\()?(-?\d+)\)?", body):
            d[name] = int(val)
        out[m] = d
    return out[mode]


class Rounding:
    def __init__(self, set_id):
        self.p = params(set_id)
        self.red = Reduce(set_id)
        self.ntt = NTT(set_id)
        self.Q = self.p["Q"]
        self.DQ = self.p["DQ"]
        self.N = self.p["N"]
        self.ELL = self.p["ELL"]
        self.EM = self.p["EM"]
        self.KVEC = self.p["KVEC"]
        self.Z1LEN = self.p["Z1LEN"]
        self.HH = self.p["HH"]
        self.ALPHA_H = self.p["ALPHA_H"]
        self.ALPHA_B = self.p["ALPHA_B"]
        self.ALPHA_1 = self.p["ALPHA_1"]
        self.ALPHA_S = self.p["ALPHA_S"]
        self.ALPHA_E = self.p["ALPHA_E"]
        rc = _parse_round_consts(set_id)
        self.RCP_1, self.SH_1, self.BIAS_1, self.K_1 = (
            rc["RCP_ALPHA_1"], rc["SH_ROUND_1"], rc["ROUND_BIAS_1"], rc["ROUND_K_1"])
        self.RCP_S, self.SH_S, self.BIAS_S, self.K_S = (
            rc["RCP_ALPHA_S"], rc["SH_ROUND_S"], rc["ROUND_BIAS_S"], rc["ROUND_K_S"])
        self.RCP_E, self.SH_E, self.BIAS_E, self.K_E = (
            rc["RCP_ALPHA_E"], rc["SH_ROUND_E"], rc["ROUND_BIAS_E"], rc["ROUND_K_E"])
        self.LOG2_ALPHA_H = rc["LOG2_ALPHA_H"]

    # ---- internal helpers (rounding.c) ----
    def _round_div_hup(self, v, recip, shift, bias, koff):
        """round v/alpha half-up toward +inf (magic reciprocal).  Matches C
        round_div_hup exactly: the shifted operand 2*v*recip+bias is always
        >= 0, so Python's arithmetic >> is the same floor as the C kernel."""
        q = (2 * v * recip + bias) >> shift
        return q - koff

    def _bmodpm_pow2(self, v, alpha):
        r = v & (alpha - 1)
        half = alpha >> 1
        if r >= half:
            r -= alpha
        return r

    def _bmodpm_q(self, a):
        r = self.red.reduce32(a)                   # (-q, q)
        hi = (self.Q - 1) // 2
        if r > hi:
            r -= self.Q
        if r < -hi:
            r += self.Q
        return r

    def _addmod_Hh(self, x):
        return x - self.HH if x >= self.HH else x

    def _centermod_Hh(self, x):
        return x + self.HH if x < 0 else x

    # ---- LSB / lift ----
    def lsb_coeff(self, x):
        return self.red.reduce_mod_2q(x) & 1

    def lift_to_mod2q_coeff(self, xbar, b):
        diff = (xbar & 1) ^ (b & 1)
        return xbar + (self.Q if diff else 0)

    # ---- CompressY / StretchS ----
    def compress_y(self, inp):
        """inp = KVEC polys (list of int lists).  Returns KVEC compressed."""
        out = []
        p = self.p
        for idx in range(self.KVEC):
            if idx == 0:
                rcp, sh, bias, koff = self.RCP_1, self.SH_1, self.BIAS_1, self.K_1
            elif idx < 1 + self.ELL:
                rcp, sh, bias, koff = self.RCP_S, self.SH_S, self.BIAS_S, self.K_S
            else:
                rcp, sh, bias, koff = self.RCP_E, self.SH_E, self.BIAS_E, self.K_E
            out.append([self._round_div_hup(v, rcp, sh, bias, koff) for v in inp[idx]])
        return out

    def stretch_s(self, inp):
        out = []
        for idx in range(self.KVEC):
            if idx == 0:
                a = self.ALPHA_1
            elif idx < 1 + self.ELL:
                a = self.ALPHA_S
            else:
                a = self.ALPHA_E
            out.append([a * v for v in inp[idx]])
        return out

    # ---- RoundB fused e' update ----
    def roundB_update_s2(self, b0_vec, e_vec):
        """Returns (b_vec, ep_vec): b = RoundB(b0); e' = e + (b - b0) bmodpm q."""
        b_out, ep_out = [], []
        for p in range(self.EM):
            bi, ei = [], []
            for k in range(self.N):
                b0 = b0_vec[p][k]
                vp = self.red.freeze(b0)
                v0 = self._bmodpm_pow2(vp, self.ALPHA_B)
                b = vp - v0
                delta = self._bmodpm_q(b - b0)
                bi.append(b)
                ei.append(e_vec[p][k] + delta)
            b_out.append(bi)
            ep_out.append(ei)
        return b_out, ep_out

    # ---- highbits / hint ----
    def highbits_reduced(self, x):
        b = (x + (self.ALPHA_H >> 1)) >> self.LOG2_ALPHA_H
        if b >= self.HH:
            b -= self.HH
        return b

    def hbvalue(self, bucket):
        return self.ALPHA_H * bucket

    def make_hint(self, comY, z2):
        """comY, z2 = EM polys.  Returns hint EM polys."""
        out = []
        for p in range(self.EM):
            hi = []
            for k in range(self.N):
                w = comY[p][k]
                wt = self.red.reduce_mod_2q(w - 2 * z2[p][k])
                hb = self.highbits_reduced(w) - self.highbits_reduced(wt)
                hi.append(self._centermod_Hh(hb))
            out.append(hi)
        return out

    def use_hint(self, hint, comY_tilde, comY0p0):
        """Returns (comY_h EM polys, z2p EM polys).  comY0p0 = first-slot
        comY0p poly (others are 0)."""
        comY_h, z2p = [], []
        for p in range(self.EM):
            chi, zi = [], []
            for k in range(self.N):
                wt = comY_tilde[p][k]
                c0 = comY0p0[k] if p == 0 else 0
                cyh = self._addmod_Hh(hint[p][k] + self.highbits_reduced(wt))
                chi.append(cyh)
                app = self.hbvalue(cyh) + c0
                even = app - wt
                zi.append(self._bmodpm_q(even >> 1))
            comY_h.append(chi)
            z2p.append(zi)
        return comY_h, z2p

    def recon_comY0p(self, z0, c):
        """comY0p = LSB(z0 - c) per coeff (first slot)."""
        return [self.lsb_coeff(z0[k] - c[k]) for k in range(self.N)]

    # ---- NTT-domain bridge + matrix products ----
    def _poly_to_ntt_dom(self, poly):
        """freeze each coeff to [0,q), then forward NTT (poly16 domain)."""
        return self.ntt.ntt([self.red.freeze(v) for v in poly])

    def _compute_t(self, bh_i, Ahat_row, x0h, xsh):
        """t = -bhat_i . x0 + sum_j Ahat[i][j] . xs_j (mod q), normal domain."""
        red = self.red
        acc = self.ntt.pointwise(bh_i, x0h)
        acc = [red.subm16(0, c) for c in acc]                # -bhat_i . x0
        for j in range(self.ELL):
            prod = self.ntt.pointwise(Ahat_row[j], xsh[j])
            acc = [red.addm16(acc[k], prod[k]) for k in range(self.N)]
        th = self.ntt.invntt_tomont(acc)                     # -> [0,q)
        return th

    def mat_mul_2q(self, yp, bhat, Ahat):
        """Sign commitment.  yp = KVEC compressed polys; bhat = EM NTT polys;
        Ahat = EM*ELL NTT polys.  Returns comY (EM polys, in [0,2q))."""
        x0h = self._poly_to_ntt_dom(yp[0])
        xsh = [self._poly_to_ntt_dom(yp[1 + j]) for j in range(self.ELL)]
        comY = []
        for i in range(self.EM):
            Ahat_row = [Ahat[i * self.ELL + j] for j in range(self.ELL)]
            t = self._compute_t(bhat[i], Ahat_row, x0h, xsh)
            # + e-block (the 2*I_m contribution), then freeze, then lift.
            t = [t[k] + yp[1 + self.ELL + i][k] for k in range(self.N)]
            t = [self.red.freeze(v) for v in t]
            ci = []
            for k in range(self.N):
                v = 2 * t[k]
                if i == 0:
                    v += self.Q * (yp[0][k] & 1)            # RAW parity (K13)
                # v in [0,3q): mod-2q via one masked subtract.
                if self.DQ - 1 - v < 0:                      # (DQ-1-v)>>31 == -1
                    v -= self.DQ
                ci.append(v)
            comY.append(ci)
        return comY

    def mat_mul_z1_2q(self, z1, c, bhat, Ahat):
        """Verifier reconstruction.  z1 = Z1LEN polys; c = challenge poly;
        returns comY_tilde (EM polys, [0,2q))."""
        z0h = self._poly_to_ntt_dom(z1[0])
        zsh = [self._poly_to_ntt_dom(z1[1 + j]) for j in range(self.ELL)]
        comY_tilde = []
        for i in range(self.EM):
            Ahat_row = [Ahat[i * self.ELL + j] for j in range(self.ELL)]
            t = self._compute_t(bhat[i], Ahat_row, z0h, zsh)
            ci = []
            for k in range(self.N):
                v = 2 * t[k]
                if i == 0:
                    ck = c[k] & 1
                    v += self.Q * (z1[0][k] & 1)
                    v -= self.Q * ck
                # v in [-q,3q): one conditional ADD then one conditional SUB.
                if v < 0:
                    v += self.DQ
                if self.DQ - 1 - v < 0:
                    v -= self.DQ
                ci.append(v)
            comY_tilde.append(ci)
        return comY_tilde

    # ---- norm gates (centered sqnorm via poly_sqnorm window) ----
    def _poly_sqnorm(self, poly):
        red = self.red
        hi = (self.Q - 1) // 2
        acc = 0
        for x in poly:
            r = red.reduce32(x)
            if r > hi:
                r -= self.Q
            if r < -hi:
                r += self.Q
            acc += r * r
        return acc

    def keygen_norm_ok(self, stretched):
        nsq = sum(self._poly_sqnorm(p) for p in stretched)
        return self.p["BK_LOW_SQ"] <= nsq <= self.p["BK_SQ"]

    def response_norm_ok(self, z1, z2p):
        nsq = sum(self._poly_sqnorm(p) for p in z1)
        nsq += sum(self._poly_sqnorm(p) for p in z2p)
        return nsq <= self.p["BV_SQ"]


def _selftest():
    for s in (128, 256, 512):
        r = Rounding(s)
        # round_div_hup: round-half-up on a few probes (0->0; the half-up tie
        # +alpha/2 -> +1, -alpha/2 -> 0; and the translation identity holds).
        assert r._round_div_hup(0, r.RCP_1, r.SH_1, r.BIAS_1, r.K_1) == 0
        half = r.ALPHA_1 // 2
        if r.ALPHA_1 % 2 == 0:
            assert r._round_div_hup(half, r.RCP_1, r.SH_1, r.BIAS_1, r.K_1) == 1
            assert r._round_div_hup(-half, r.RCP_1, r.SH_1, r.BIAS_1, r.K_1) == 0
        # translation identity: round((v+alpha*x)/alpha) == round(v/alpha)+x.
        cy = r.compress_y([[v for v in range(-200, 200)]] * r.KVEC)
        cyx = r.compress_y([[v + r.ALPHA_1 * 3 for v in range(-200, 200)]]
                           + [[v for v in range(-200, 200)]] * (r.KVEC - 1))
        assert all(cyx[0][i] == cy[0][i] + 3 for i in range(400))
        # lift parity: result parity matches the bit b.
        for xbar in (0, 1, 5, r.Q - 1):
            for b in (0, 1):
                v = r.lift_to_mod2q_coeff(xbar, b)
                assert (v & 1) == (b & 1) or (xbar & 1) == (b & 1)
        # highbits in [0, HH)
        for x in (0, r.Q, 2 * r.Q - 1):
            assert 0 <= r.highbits_reduced(x) < r.HH
    print("rounding_ref.py self-test: round_div_hup(0)=0 + half-up ties + "
          "translation identity, lift parity, highbits in [0,HH) (3 sets)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
