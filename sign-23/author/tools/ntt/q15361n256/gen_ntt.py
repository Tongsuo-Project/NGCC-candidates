#!/usr/bin/env python3
"""
gen_ntt.py -- verified model + .S/constant generator for a COMPLETE n=256
negacyclic NTT over Z_q[x]/(x^256+1), q=15361, 16-bit AVX2, Kyber-style level
merging + the 4-tier shuffle network.  SIGNED 16-bit + LAZY REDUCTION.

q=15361:  q-1 = 2^10 * 3 * 5, so 2n=512 | q-1  =>  COMPLETE NTT (8 levels,
len 128..1), 256 zetas, brv width 8, POINTWISE (coefficient-wise) pointmul.

WHY SIGNED (the distinguishing choice vs the unsigned valley siblings):
  q = 15361 < 2^15, so coefficients fit a SIGNED int16 with ~2.13x headroom
  (2^15/q = 2.133).  The signed path follows pq-crystals/kyber:
    * Montgomery mul is the SIGNED `vpmulhw` form (Kyber fqmulprecomp): 4 ops, NO
      borrow-correction chain (the unsigned valley montmul needs vpsubusw +
      vpcmpeqw + vpandn + vpsubw on top -- 8 ops).
    * add/sub are PLAIN vpaddw/vpsubw (lazy) -- no overflow-aware addmod/submod.
    * a Barrett `red16` (4 ops, 2 mults; Kyber fq.inc) recentres a coefficient
      to |x| <~ q whenever a further lazy add would overflow int16.
  Net: ~2x fewer instructions per butterfly than the unsigned siblings.

LAZY HEADROOM (more conservative than Kyber, q=3329, whose headroom is 9.85x):
  Montgomery output |t| < q, so a CT butterfly add a +- t is safe iff |a| <=
  2^15-q = 17407.  Starting from normal-domain input [0,q) (|a| < q), level 0 is
  lazy (out < 2q = 30720 < 2^15); thereafter every accumulator reaches ~2q and
  must be red16'd before the next add.  An EXACT per-register bound tracker (the
  q30977-style machinery, here signed) derives the forward (CT) and inverse (GS)
  red16 schedules INDEPENDENTLY -- GS reduces its b-output for free (it passes
  through montmul), so the inverse needs fewer reductions.

Pipeline (mirrors q45569 build_half structure + q30977 lazy tracker):
  1. Build one 128-coeff half as an explicit op-list, baking per-lane CENTERED
     Montgomery zeta vectors by tracking coefficient indices through the shuffles.
  2. The inverse is the LITERAL REVERSE (CT->GS, shuffles are involutions).
  3. Verify with a pure-Python AVX2 interpreter that is BIT-EXACT to the asm
     (signed montmul/red16, lane permutations) AND tracks exact integer bounds,
     proving every value stays in (-2^15, 2^15) and every montmul input is valid.
  4. Emit ntt.S + ntt_consts.c (the proven sequence; transcription errors caught
     by test_ntt.c).
"""
import re
import sys

# SHUTTLE namespacing: every PUBLIC AVX2 symbol this generator emits is prefixed
# with SYM ("s256_") so all three vendored NTT configs co-link in one process
# (see SHUTTLE plan P03 / namespace.h).  The rename is a generator post-pass
# (_namespace), NOT a hand-patch of the .S, so it survives `make tables`
# deterministically.  Set SYM="" to reproduce the original un-prefixed output.
SYM = "s256_"

# Whole-word public AVX2 symbols (function labels that double as .global, plus
# the const tables -- including those the .S references RIP-relative by name,
# `ntt_qinv(%rip)` / `ntt_qdata(%rip)`, which are renamed in lock-step).
_NS_SYMS = ("ntt_avx", "invntt_tomont_avx", "pointwise_avx", "reduce_avx",
            "nttunpack_avx",
            "ntt_qdata", "ntt_qinv", "ntt_z0inv", "ntt_z0", "ntt_scale",
            "ntt_zetas_fwd", "ntt_zetas_inv")
_NS_PAT = re.compile(r"\b(" + "|".join(_NS_SYMS) + r")\b")

def _namespace(text):
    """Prefix every public AVX2 symbol with SYM (whole-word)."""
    if not SYM:
        return text
    return _NS_PAT.sub(lambda m: SYM + m.group(1), text)

Q = 15361
R = 1 << 16
MONT = R % Q                       # 4092  = 2^16 mod q
QINV = pow(Q, -1, R)               # 50177 = q^-1 mod 2^16
RINV = pow(R, -1, Q)
N = 256
LEVELS = 8                         # COMPLETE: len 128..1
BRV = 8

# CENTERED Barrett red16 constants (brute-forced + verified over the whole int16
# range):  q1 = mulhrsw(RND, mulhw(V, r));  red = r - q*q1   in [-RB, RB],
# RB ~ q/2 (rounding => centred).  RND = 2^(15-S) encodes the shift inside the
# rounding multiply (one vpmulhrsw instead of a plain vpsraw), so red16 stays a
# 4-op kernel while recentering tightly enough that a CT/GS accumulator can take
# TWO further lazy adds between reductions (vs one for a one-sided reduce).
RED_V = 273
RED_S = 6
RED_RND = 1 << (15 - RED_S)         # 512

def _factorize(m):
    f = []; d = 2
    while d * d <= m:
        if m % d == 0:
            f.append(d)
            while m % d == 0: m //= d
        d += 1
    if m > 1: f.append(m)
    return f
def _mod_order(g, q):
    n = q - 1
    for p in _factorize(n):
        while n % p == 0 and pow(g, n // p, q) == 1: n //= p
    return n
def _primitive_root(q, order):
    for g in range(2, q):
        if _mod_order(g, q) == order: return g
    raise ValueError("no primitive %d-th root mod %d" % (order, q))
OMEGA = _primitive_root(Q, 2 * N)  # 98: smallest primitive 2N-th root, omega^N == -1

NINV = pow(N, Q - 2, Q)
R2_MOD_Q = MONT * MONT % Q
NINV_TOMONT = NINV * R2_MOD_Q % Q

def cent(x):
    """centered representative in (-q/2, q/2]."""
    c = x % Q
    return c - Q if c > Q // 2 else c

def s16(v):
    v %= R
    return v - R if v >= (R >> 1) else v

def brv(x, w=BRV):
    r = 0
    for i in range(w):
        r |= ((x >> i) & 1) << (w - 1 - i)
    return r

# CENTERED Montgomery zetas: |zeta| <= q/2, so any int16 montmul input is valid.
ZETA = [cent(pow(OMEGA, brv(k), Q) * MONT % Q) for k in range(N)]

def fqmul(a, b):
    return (a * b * RINV) % Q

# (len,start) -> global zeta index, standard CT loop order, len 128..1.
FWD_K = {}
_k = 1; _len = N // 2
while _len >= 1:
    _s = 0
    while _s < N:
        FWD_K[(_len, _s)] = _k; _k += 1; _s += 2 * _len
    _len >>= 1

def zeta_of(length, coeff_low):
    start = (coeff_low // (2 * length)) * (2 * length)
    return ZETA[FWD_K[(length, start)]]

# --------------------------------------------------------------------------
# BIT-EXACT signed 16-bit arithmetic, mirroring ntt.S instruction by instruction.
# A "cell" is (signed_value, max_abs_bound).  Permutations move whole cells.
# --------------------------------------------------------------------------
def mullw(a, b):
    return s16((a * b) & 0xFFFF)            # vpmullw: low 16 (signed view)
def mulhw(a, b):
    return (a * b) >> 16                     # vpmulhw: signed high (arithmetic >>)

def montmul_s(inp, zeta):
    """signed Montgomery mul (Kyber fqmulprecomp): t = inp*zeta*R^-1 mod q, |t|<q.
       zl = zeta*qinv mod 2^16 (signed)."""
    zl = s16((zeta * QINV) & 0xFFFF)
    m  = mullw(inp, zl)                      # low(inp*zeta*qinv)
    hi = mulhw(zeta, inp)                    # high(zeta*inp)
    mq = mulhw(Q, m)                        # high(q*m)
    return hi - mq

def mulhrsw(a, b):
    return (a * b + (1 << 14)) >> 15        # vpmulhrsw: signed rounding mul high

def red16_s(r):
    """CENTERED signed Barrett reduce: r - q*round(V*r/2^(16+S)), |.| <= RB ~ q/2.
       q1 = mulhrsw(RND, mulhw(V,r)) rounds the quotient (vpmulhw then vpmulhrsw)."""
    hi = mulhw(RED_V, r)
    q1 = mulhrsw(RED_RND, hi)
    return s16((r - s16((Q * q1) & 0xFFFF)) & 0xFFFF)

# proven bounds the tracker enforces
HALF   = 1 << 15
MONT_T = 3 * Q // 4                         # |montmul output| < 3q/4 (centered zeta, |in|<2^15)
LAZY_CAP = HALF - Q                         # |a| <= 2^15-q => a+-t fits int16   (=17407)
RB = max(abs(red16_s(r)) for r in range(-HALF, HALF))   # exact red16 output bound

def montmul_cell(z, b):
    vb, _ = b
    return (montmul_s(vb, z), MONT_T)

# CT butterfly (signed).  a'=a+t, b'=a-t, t=montmul(b,zeta).
def ct_lazy(a, t):
    va, ba = a; vt, _ = t
    assert ba + MONT_T <= HALF - 1, "lazy CT overflow: |a|<=%d + (q-1)" % ba
    return (va + vt, ba + MONT_T), (va - vt, ba + MONT_T)
def ct_red(a, t):
    va, ba = a; vt, _ = t
    var = red16_s(va)
    return (var + vt, RB + MONT_T), (var - vt, RB + MONT_T)

# GS butterfly (signed).  s=u+v, d=u-v, b'=montmul(d,zinv).  Only s grows; b' free.
def gs_lazy(u, v, zi):
    vu, bu = u; vv, bv = v
    assert bu + bv <= HALF - 1, "lazy GS overflow: %d + %d" % (bu, bv)
    s = (vu + vv, bu + bv)
    d = (vu - vv)
    b = (montmul_s(d, zi), MONT_T)
    return s, b
def gs_red(u, v, zi):
    vu, bu = u; vv, bv = v
    ur = red16_s(vu); vr = red16_s(vv)
    s = (ur + vr, 2 * RB)
    b = (montmul_s(ur - vr, zi), MONT_T)
    return s, b

# --------------------------------------------------------------------------
# AVX2 lane permutations on CELLS (16 lanes; 0-7 low 128-bit, 8-15 high).
# --------------------------------------------------------------------------
Z0CELL = (0, 0)
def _dwords(x):  return [x[2*i:2*i+2] for i in range(8)]
def _undw(dws):  return [v for dw in dws for v in dw]
def vperm2i128(a, b, imm):
    lo = {0: a[0:8], 1: a[8:16], 2: b[0:8], 3: b[8:16]}
    return lo[imm & 3] + lo[(imm >> 4) & 3]
def vpunpcklqdq(a, b): return a[0:4] + b[0:4] + a[8:12] + b[8:12]
def vpunpckhqdq(a, b): return a[4:8] + b[4:8] + a[12:16] + b[12:16]
def vmovsldup(x):
    d = _dwords(x); return _undw([d[0],d[0],d[2],d[2],d[4],d[4],d[6],d[6]])
def vpsrlq(x, n):
    assert n == 32
    d = _dwords(x); z = [Z0CELL, Z0CELL]
    return _undw([d[1],z,d[3],z,d[5],z,d[7],z])
def vpblendd(a, b, mask):
    da, db = _dwords(a), _dwords(b)
    return _undw([db[i] if (mask>>i)&1 else da[i] for i in range(8)])
def vpslld(x, n):
    assert n == 16
    return _undw([[Z0CELL, x[2*i]] for i in range(8)])
def vpsrld(x, n):
    assert n == 16
    return _undw([[x[2*i+1], Z0CELL] for i in range(8)])
def vpblendw(a, b, mask):
    return [b[i] if (mask >> (i % 8)) & 1 else a[i] for i in range(16)]

def shuffle8(a, b):
    return vperm2i128(a, b, 0x20), vperm2i128(a, b, 0x31)
def shuffle4(a, b):
    return vpunpcklqdq(a, b), vpunpckhqdq(a, b)
def shuffle2(a, b):
    c = vpblendd(a, vmovsldup(b), 0xAA)
    d = vpblendd(vpsrlq(a, 32), b, 0xAA)
    return c, d
def shuffle1(a, b):
    c = vpblendw(a, vpslld(b, 16), 0xAA)
    d = vpblendw(vpsrld(a, 16), b, 0xAA)
    return c, d
SHUF = {8: shuffle8, 4: shuffle4, 2: shuffle2, 1: shuffle1}

# --------------------------------------------------------------------------
# Build the forward op-list for one half (base = 0 or 128).  COMPLETE: shuf1
# is used (len down to 1).  Ops carry the NTT level for annotation/scheduling.
#   ('xb', ra, rb, zvec, lvl) cross-register vertical CT butterfly
#   ('sh', k, r2m, r2m1, lvl) in-place shuffle of a register pair
#   ('ib', r2m, r2m1, zvec, lvl) vertical CT butterfly after a shuffle
# --------------------------------------------------------------------------
LVL = {64: 1, 32: 2, 16: 3, 8: 4, 4: 5, 2: 6, 1: 7}   # level 0 = len128 cross-half

def build_half(base):
    idx = [[base + 16*r + l for l in range(16)] for r in range(8)]
    prog = []
    for length in (64, 32, 16):
        step = length // 16
        for r in range(8):
            if (r // step) % 2 == 0:
                p = r + step
                z = zeta_of(length, idx[r][0])
                prog.append(('xb', r, p, [z]*16, LVL[length]))
    for length in (8, 4, 2, 1):
        sh = SHUF[length]
        for m in range(4):
            a, b = idx[2*m], idx[2*m+1]
            c, d = sh(a, b)
            zvec = [zeta_of(length, c[l]) for l in range(16)]
            prog.append(('sh', length, 2*m, 2*m+1, LVL[length]))
            prog.append(('ib', 2*m, 2*m+1, zvec, LVL[length]))
            idx[2*m], idx[2*m+1] = c, d
    return prog, idx

FWD0, IDX0 = build_half(0)
FWD1, IDX1 = build_half(128)
Z0 = zeta_of(128, 0)               # level-0 (cross-half) zeta

def zinv_mont(zm):
    plain = zm * RINV % Q
    return cent(pow(plain, -1, Q) * MONT % Q)

# --------------------------------------------------------------------------
# Forward / inverse half interpreters with schedule derivation (q30977 style).
# --------------------------------------------------------------------------
def run_fwd_half(regs, prog, fixed=None):
    sched = []; fi = 0
    for op in prog:
        if op[0] in ('xb', 'ib'):
            _, ra, rb, z, lvl = op
            if fixed is not None:
                do_red = fixed[fi][1]; fi += 1
            else:
                ba = max(c[1] for c in regs[ra])
                do_red = not (ba + MONT_T <= HALF - 1)
            sched.append((lvl, do_red))
            for l in range(16):
                t = montmul_cell(z[l], regs[rb][l])
                if do_red:
                    regs[ra][l], regs[rb][l] = ct_red(regs[ra][l], t)
                else:
                    regs[ra][l], regs[rb][l] = ct_lazy(regs[ra][l], t)
        else:
            _, k, r0, r1, _ = op
            regs[r0], regs[r1] = SHUF[k](regs[r0], regs[r1])
    return regs, sched

def run_inv_half(regs, prog, fixed=None):
    sched = []; fi = 0
    for op in reversed(prog):
        if op[0] in ('xb', 'ib'):
            _, r0, r1, z, lvl = op
            if fixed is not None:
                do_red = fixed[fi][1]; fi += 1
            else:
                bu = max(c[1] for c in regs[r0]); bv = max(c[1] for c in regs[r1])
                do_red = not (bu + bv <= HALF - 1)
            sched.append((lvl, do_red))
            for l in range(16):
                zi = zinv_mont(z[l])
                if do_red:
                    regs[r0][l], regs[r1][l] = gs_red(regs[r0][l], regs[r1][l], zi)
                else:
                    regs[r0][l], regs[r1][l] = gs_lazy(regs[r0][l], regs[r1][l], zi)
        else:
            _, k, r0, r1, _ = op
            regs[r0], regs[r1] = SHUF[k](regs[r0], regs[r1])
    return regs, sched

def _bound_regs(init_bound):
    return [[(0, init_bound) for _ in range(16)] for _ in range(8)]

# --- forward schedule (level 0 cross-half: inputs [0,q), |a|<q<=LAZY_CAP -> lazy) ---
FWD_L0_RED = (Q - 1 + MONT_T > HALF - 1)          # False: level 0 lazy
_fwd_regs, FWD_SCHED = run_fwd_half(_bound_regs(2*Q - 2), FWD0)   # half sees [0,2q) from L0
FWD_OUT_BOUND = max(c[1] for reg in _fwd_regs for c in reg)

# --- inverse schedule (fed the forward output bound) ---
INV_IN_BOUND = FWD_OUT_BOUND
_inv_regs, INV_SCHED = run_inv_half(_bound_regs(INV_IN_BOUND), FWD0)
INV_OUT_BOUND = max(c[1] for reg in _inv_regs for c in reg)

def _inv_l0_red():
    bu = INV_OUT_BOUND
    return not (bu + INV_OUT_BOUND <= HALF - 1)
INV_L0_RED = _inv_l0_red()

def levels_red(sched):
    d = {}
    for lvl, c in sched:
        d[lvl] = d.get(lvl, False) or c
    return d
FWD_LEVEL_RED = levels_red(FWD_SCHED); FWD_LEVEL_RED[0] = FWD_L0_RED
INV_LEVEL_RED = levels_red(INV_SCHED)

# --------------------------------------------------------------------------
# Full transform model (cells), used by verify().
# --------------------------------------------------------------------------
def ntt(poly):
    a = [(x % Q, Q - 1) for x in poly]          # normal domain [0,q), |a|<q
    for j in range(128):                         # level 0 cross-half
        t = montmul_cell(Z0, a[j+128])
        if FWD_L0_RED:
            a[j], a[j+128] = ct_red(a[j], t)
        else:
            a[j], a[j+128] = ct_lazy(a[j], t)
    out = [c for c in a]
    for h, prog in ((0, FWD0), (1, FWD1)):
        regs = [[out[128*h+16*r+l] for l in range(16)] for r in range(8)]
        regs, _ = run_fwd_half(regs, prog, fixed=FWD_SCHED)
        for r in range(8):
            for l in range(16):
                out[128*h+16*r+l] = regs[r][l]
    return out                                   # list of cells

def intt(cells):
    a = list(cells)
    for h, prog in ((0, FWD0), (1, FWD1)):
        regs = [[a[128*h+16*r+l] for l in range(16)] for r in range(8)]
        regs, _ = run_inv_half(regs, prog, fixed=INV_SCHED)
        for r in range(8):
            for l in range(16):
                a[128*h+16*r+l] = regs[r][l]
    zi0 = zinv_mont(Z0)
    for j in range(128):                         # level 0 cross-half GS
        if INV_L0_RED:
            a[j], a[j+128] = gs_red(a[j], a[j+128], zi0)
        else:
            a[j], a[j+128] = gs_lazy(a[j], a[j+128], zi0)
    # final *1/n and to-Montgomery: red16 then montmul(NINV_TOMONT).
    return [montmul_s(red16_s(c[0]), cent(NINV_TOMONT)) % Q for c in a]

def pointwise(na, nb):
    """red16 inputs (forward outputs ~2q) then signed data*data Montgomery mul."""
    qinv = QINV
    out = []
    for ca, cb in zip(na, nb):
        x = red16_s(ca[0]); y = red16_s(cb[0])
        lo = mullw(x, y); m = mullw(lo, s16(qinv & 0xFFFF))
        hi = mulhw(x, y); mq = mulhw(Q, m)
        out.append((hi - mq, MONT_T))
    return out

def schoolbook(a, b):
    c = [0]*N
    for i in range(N):
        for j in range(N):
            p = a[i]*b[j] % Q; k = i+j
            if k < N: c[k] = (c[k]+p) % Q
            else:     c[k-N] = (c[k-N]-p) % Q
    return c

# --------------------------------------------------------------------------
def verify():
    import random
    random.seed(2026)
    out = []
    ok = True
    for _ in range(400):
        a = [random.randrange(Q) for _ in range(N)]
        want = [(x * MONT) % Q for x in a]
        if intt(ntt(a)) != want:
            ok = False; break
    out.append("roundtrip-to-mont(400): " + ("PASS" if ok else "FAIL"))
    ok2 = True
    for _ in range(120):
        a = [random.randrange(Q) for _ in range(N)]
        b = [random.randrange(Q) for _ in range(N)]
        nc = pointwise(ntt(a), ntt(b))
        if intt(nc) != schoolbook(a, b):
            ok2 = False; break
    out.append("pointwise/convolution(120): " + ("PASS" if ok2 else "FAIL"))
    fl = sorted(k for k, v in FWD_LEVEL_RED.items() if v)
    fz = sorted(k for k, v in FWD_LEVEL_RED.items() if not v)
    il = sorted(k for k, v in INV_LEVEL_RED.items() if v)
    iz = sorted(k for k, v in INV_LEVEL_RED.items() if not v)
    out.append("forward schedule: RED levels %s, LAZY levels %s" % (fl, fz))
    out.append("inverse schedule: RED levels %s, LAZY levels %s (cross-half L0 red=%s)"
               % (il, iz, INV_L0_RED))
    out.append("bounds: RB=%d (%.4fq); fwd-out max %d < 2^15 (%s); inv-out max %d < 2^15 (%s)"
               % (RB, RB/Q, FWD_OUT_BOUND, FWD_OUT_BOUND < HALF, INV_OUT_BOUND, INV_OUT_BOUND < HALF))
    return ok and ok2, out

# --------------------------------------------------------------------------
# Emit constants.
# --------------------------------------------------------------------------
def emit_consts(path):
    fwd = []
    for prog in (FWD0, FWD1):
        for op in prog:
            if op[0] in ('xb', 'ib'):
                fwd.append(op[3])
    inv = []
    for prog in (FWD0, FWD1):
        for op in reversed(prog):
            if op[0] in ('xb', 'ib'):
                inv.append([zinv_mont(z) for z in op[3]])

    def zl16(z): return s16((z * QINV) & 0xFFFF)
    def pair_table(table):
        o = []
        for zvec in table:
            o.append([zl16(z) for z in zvec]); o.append([s16(z) for z in zvec])
        return o
    def vecs(name, table):
        lines = [f"const int16_t {name}[{len(table)}][16] = {{"]
        for v in table:
            lines.append("  {" + ",".join(str(int(x)) for x in v) + "},")
        lines.append("};")
        return "\n".join(lines)
    def pair32(name, z):
        zl = ",".join([str(zl16(z))]*16)
        zh = ",".join([str(s16(z))]*16)
        return ("const int16_t %s[32] __attribute__((aligned(32))) = {\n  %s,  /* zl */\n  %s   /* zh */\n};\n"
                % (name, zl, zh))
    fwd_p, inv_p = pair_table(fwd), pair_table(inv)
    import io
    f = io.StringIO()
    if True:
        f.write("#include <stdint.h>\n")
        f.write("/* Auto-generated by gen_ntt.py -- do not edit. q=%d, COMPLETE n=256,\n"
                "   SIGNED 16-bit + LAZY reduction.  Twiddles use the Seiler precompute:\n"
                "   each butterfly stores zl=zeta*qinv mod 2^16 then zh=zeta (CENTERED),\n"
                "   as two 16-lane vectors. */\n\n" % Q)
        # qdata: [0]=q [1]=V(Barrett) [2]=RND(rounding) -- ymm0=q, ymm2=V, ymm3=RND
        f.write("const int16_t ntt_qdata[48] __attribute__((aligned(32))) = {\n")
        f.write("  " + ",".join([str(Q)]*16) + ",  /* _16XQ   (off 0)  */\n")
        f.write("  " + ",".join([str(RED_V)]*16) + ",  /* _16XV   (off 32, Barrett red16 const) */\n")
        f.write("  " + ",".join([str(RED_RND)]*16) + "   /* _16XRND (off 64, red16 rounding const) */\n};\n\n")
        f.write("const int16_t ntt_qinv[16] __attribute__((aligned(32))) = {\n  "
                + ",".join([str(s16(QINV))]*16) + "\n};\n\n")
        f.write(pair32("ntt_z0", Z0))
        f.write(pair32("ntt_z0inv", zinv_mont(Z0)))
        f.write(pair32("ntt_scale", cent(NINV_TOMONT)) + "\n")
        f.write("__attribute__((aligned(32)))\n" + vecs("ntt_zetas_fwd", fwd_p) + "\n\n")
        f.write("__attribute__((aligned(32)))\n" + vecs("ntt_zetas_inv", inv_p) + "\n")
    with open(path, "w") as out:
        out.write(_namespace(f.getvalue()))
    return len(fwd_p), len(inv_p)

# --------------------------------------------------------------------------
# Emit ntt.S (SIGNED + LAZY).  Register map:
#   ymm0 = q   ymm1 = zh = zeta   ymm2 = V (Barrett)   ymm15 = zl
#   ymm4..ymm11 = 8 data regs of one half   ymm12,13 = scratch   ymm14 = t
# --------------------------------------------------------------------------
ASM_PREAMBLE = r"""/* ==========================================================================
 * Auto-generated by gen_ntt.py -- DO NOT EDIT (edit gen_ntt.py, run `make gen`).
 *
 * COMPLETE n=256 negacyclic NTT / INTT over Z_q[x]/(x^256+1), q = 15361,
 * 16-bit AVX2, SIGNED + LAZY REDUCTION.  8 butterfly levels (len 128..1), then
 * a coefficient-wise POINTWISE multiply.
 *
 * Entry points (System V AMD64 ABI):
 *   ntt_avx     (rdi=poly[256] i16, rsi=qdata, rdx=zetas_fwd, rcx=z0,    r8=scale)
 *   invntt_tomont_avx(rdi=poly[256] i16, rsi=qdata, rdx=zetas_inv, rcx=z0inv, r8=scale)
 *   pointwise_avx(rdi=c, rsi=a, rdx=b, rcx=qdata)
 *   nttunpack_avx(rdi=dst i16, rsi=src i32)
 *
 * Register map (held live across a half-transform):
 *   ymm0 = q (16x)   ymm1 = zh = zeta vec   ymm2 = V (Barrett red16 const)
 *   ymm4..ymm11 = the 8 data registers of one 128-coeff half
 *   ymm12,ymm13 = scratch   ymm14 = butterfly temp t
 *   ymm15 = zl = zeta*q^-1 mod 2^16 (precomputed Montgomery-factor twiddle)
 *
 * DOMAIN: SIGNED int16, q = 15361 < 2^15 (headroom 2^15/q = 2.13x).  Twiddles
 * are CENTERED *R mod q (|zeta| <= q/2), so any int16 montmul input is valid.
 *   montmul (Kyber fqmulprecomp, 4 ops, signed vpmulhw, no borrow correction):
 *     m  = in*zl (vpmullw);  hi = mulhw_s(zh,in);  mq = mulhw_s(q,m);  out=hi-mq
 *   red16 (CENTERED Barrett, 4 ops, recentre to |x| <~ q/2):
 *     h = mulhw_s(V,x); h = mulhrsw(RND,h); h = mullw(q,h); x = x - h
 * LAZY: a +- t is a plain vpaddw/vpsubw; |montmul out| < 3q/4 so it is safe
 * while |a| <= 2^15 - 3q/4 = 21247.  Because red16 recentres to |x| <~ q/2, an
 * accumulator can take TWO lazy adds before it must be reduced again -- so the
 * proven schedule reduces only every other level (vs every level for a
 * one-sided reduce), which is the bulk of the signed-path speedup.
 * ========================================================================== */

/* montmul: out = (in * zeta) * R^-1 mod q, |out| < 3q/4.  zl=ymm15, zh=ymm1. */
.macro montmul out,in
  vpmullw  %ymm15,\in,%ymm12
  vpmulhw  %ymm1,\in,\out
  vpmulhw  %ymm0,%ymm12,%ymm12
  vpsubw   %ymm12,\out,\out
.endm

/* red16: a = a - q*mulhrsw(RND, mulhw(V,a))  (centered Barrett).  V=ymm2,
 * RND=ymm3.  Clobbers ymm13. */
.macro red16 a
  vpmulhw   %ymm2,\a,%ymm13
  vpmulhrsw %ymm3,%ymm13,%ymm13
  vpmullw   %ymm0,%ymm13,%ymm13
  vpsubw    %ymm13,\a,\a
.endm

/* dmul: GENERAL signed Montgomery mul of two DATA vectors.  qinv in ymm9. */
.macro dmul out,x,y
  vpmullw  \x,\y,%ymm12
  vpmullw  %ymm9,%ymm12,%ymm12
  vpmulhw  \x,\y,\out
  vpmulhw  %ymm0,%ymm12,%ymm12
  vpsubw   %ymm12,\out,\out
.endm

/* ctbf_lazy: forward CT, LAZY.  t=montmul(b,zeta); b=a-t; a=a+t. */
.macro ctbf_lazy a,b
  montmul %ymm14,\b
  vpsubw  %ymm14,\a,\b
  vpaddw  %ymm14,\a,\a
.endm

/* ctbf_red: forward CT, recentre a first (red16), then the lazy butterfly. */
.macro ctbf_red a,b
  red16   \a
  montmul %ymm14,\b
  vpsubw  %ymm14,\a,\b
  vpaddw  %ymm14,\a,\a
.endm

/* gsbf_lazy: inverse GS, LAZY.  d=u-v (ymm14); s=u+v; b=montmul(d,zinv).
 *   the b-output is reduced |.|<q for free by montmul; only s stays lazy.
 *   d in ymm14 (montmul clobbers ymm12, so d must not live there). */
.macro gsbf_lazy a,b
  vpsubw  \b,\a,%ymm14
  vpaddw  \b,\a,\a
  montmul \b,%ymm14
.endm

/* gsbf_red: inverse GS, recentre u,v first (red16), then the lazy butterfly. */
.macro gsbf_red a,b
  red16   \a
  red16   \b
  vpsubw  \b,\a,%ymm14
  vpaddw  \b,\a,\a
  montmul \b,%ymm14
.endm

/* ---- the 4-tier shuffle network (granularity halves each tier) ---- */
.macro shuf8 r0,r1
  vperm2i128 $0x20,\r1,\r0,%ymm12
  vperm2i128 $0x31,\r1,\r0,\r1
  vmovdqa  %ymm12,\r0
.endm
.macro shuf4 r0,r1
  vpunpcklqdq \r1,\r0,%ymm12
  vpunpckhqdq \r1,\r0,\r1
  vmovdqa  %ymm12,\r0
.endm
.macro shuf2 r0,r1
  vmovsldup \r1,%ymm12
  vpblendd $0xAA,%ymm12,\r0,%ymm12
  vpsrlq   $32,\r0,\r0
  vpblendd $0xAA,\r1,\r0,\r1
  vmovdqa  %ymm12,\r0
.endm
.macro shuf1 r0,r1
  vpslld   $16,\r1,%ymm12
  vpblendw $0xAA,%ymm12,\r0,%ymm12
  vpsrld   $16,\r0,\r0
  vpblendw $0xAA,\r1,\r0,\r1
  vmovdqa  %ymm12,\r0
.endm
"""

def emit_asm(path):
    ymm = lambda r: "%%ymm%d" % (4 + r)
    def load_consts():
        return ["  vmovdqa   0(%rsi),%ymm0",      # q
                "  vmovdqa  32(%rsi),%ymm2",      # V (Barrett)
                "  vmovdqa  64(%rsi),%ymm3"]      # RND (rounding)
    def load_tw(reg, disp=0):
        return ["  vmovdqa  %d(%s),%%ymm15" % (disp, reg),
                "  vmovdqa  %d(%s),%%ymm1"  % (disp + 32, reg)]
    def load_half(h):
        return ["  vmovdqu  %d(%%rdi),%s" % (256*h + 32*r, ymm(r)) for r in range(8)]
    def store_half(h):
        return ["  vmovdqu  %s,%d(%%rdi)" % (ymm(r), 256*h + 32*r) for r in range(8)]
    XB_LENS = [64]*4 + [32]*4 + [16]*4

    # ---- forward ----
    L = list(ASM_PREAMBLE.splitlines())
    L += [".text", ".global ntt_avx", ".global invntt_tomont_avx",
          ".global pointwise_avx", ".global reduce_avx",
          "", "/* ============ forward NTT (8 levels, len 128..1), SIGNED+LAZY ============ */",
          "ntt_avx:"]
    L += load_consts()
    L += ["", "/* --- level 0 (len=128): cross-HALF CT, partners 128 apart.",
          "       inputs [0,q) -> LAZY add (out in (-q,2q)). --- */"]
    L += load_tw("%rcx")
    bf0 = "ctbf_red" if FWD_L0_RED else "ctbf_lazy"
    for r in range(8):
        L += ["  vmovdqu  %d(%%rdi),%%ymm4" % (32*r),
              "  vmovdqu  %d(%%rdi),%%ymm5" % (256 + 32*r),
              "  %s %%ymm4,%%ymm5" % bf0,
              "  vmovdqu  %%ymm4,%d(%%rdi)" % (32*r),
              "  vmovdqu  %%ymm5,%d(%%rdi)" % (256 + 32*r)]
    ki = 0
    half_sched = list(FWD_SCHED)
    for h, prog in ((0, FWD0), (1, FWD1)):
        lo, hi = 128*h, 128*h + 127
        L += ["", "/* ===== half %d (coeffs %d..%d): levels 1-7 merged ===== */" % (h, lo, hi)]
        L += load_half(h)
        xi = 0; cur = None; si = 0
        for op in prog:
            if op[0] == 'xb':
                ln = XB_LENS[xi]; xi += 1; lvl, do_red = half_sched[si]; si += 1
                bf = "ctbf_red" if do_red else "ctbf_lazy"
                tag = "" if do_red else " [LAZY]"
                if ln != cur:
                    cur = ln
                    L += ["  /* level %d (len=%d): cross-REGISTER CT%s */" % (lvl, ln, tag)]
                _, r, p, _, _ = op
                L += load_tw("%rdx", ki*64) + ["  %s %s,%s" % (bf, ymm(r), ymm(p))]
                ki += 1
            elif op[0] == 'sh':
                _, k, r0, r1, lvl = op
                if k != cur:
                    cur = k
                    L += ["  /* level %d (len=%d): IN-REGISTER shuf%d then CT */" % (lvl, k, k)]
                L += ["  shuf%d %s,%s" % (k, ymm(r0), ymm(r1))]
            else:  # ib
                _, r0, r1, _, lvl = op
                _, do_red = half_sched[si]; si += 1
                bf = "ctbf_red" if do_red else "ctbf_lazy"
                tag = "" if do_red else "  /* [LAZY] */"
                L += load_tw("%rdx", ki*64) + ["  %s %s,%s%s" % (bf, ymm(r0), ymm(r1), tag)]
                ki += 1
        L += store_half(h)
    L += ["  ret", ""]

    # ---- inverse ----
    L += ["/* ============ inverse NTT (GS, levels 7..0), SIGNED+LAZY ============ */",
          "invntt_tomont_avx:"]
    L += load_consts()
    ki = 0
    inv_half_sched = list(INV_SCHED)
    for h, prog in ((0, FWD0), (1, FWD1)):
        lo, hi = 128*h, 128*h + 127
        L += ["", "/* ===== half %d (coeffs %d..%d): levels 7-1 reversed ===== */" % (h, lo, hi)]
        L += load_half(h)
        rev = list(reversed(prog))
        fl = []; xi = 0; last_sh = None
        for op in prog:
            if op[0] == 'xb': fl.append(XB_LENS[xi]); xi += 1
            elif op[0] == 'sh': last_sh = op[1]; fl.append(op[1])
            else: fl.append(last_sh)
        rl = list(reversed(fl)); cur = None; si = 0
        for op, ln in zip(rev, rl):
            if op[0] in ('xb', 'ib'):
                lvl, do_red = inv_half_sched[si]; si += 1
                bf = "gsbf_red" if do_red else "gsbf_lazy"
                tag = "" if do_red else " [LAZY]"
                kind = "cross-REGISTER GS" if op[0] == 'xb' else "IN-REGISTER GS (after shuf%d)" % ln
                if ln != cur:
                    cur = ln
                    L += ["  /* level %d (len=%d): %s%s */" % (lvl, ln, kind, tag)]
                _, r0, r1, _, _ = op
                L += load_tw("%rdx", ki*64) + ["  %s %s,%s" % (bf, ymm(r0), ymm(r1))]
                ki += 1
            else:
                _, k, r0, r1, _ = op
                L += ["  shuf%d %s,%s" % (k, ymm(r0), ymm(r1))]
        L += store_half(h)
    L += ["", "/* --- level 0 (len=128): cross-HALF GS (z0inv) --- */"]
    L += load_tw("%rcx")
    bfl0 = "gsbf_red" if INV_L0_RED else "gsbf_lazy"
    for r in range(8):
        L += ["  vmovdqu  %d(%%rdi),%%ymm4" % (32*r),
              "  vmovdqu  %d(%%rdi),%%ymm5" % (256 + 32*r),
              "  %s %%ymm4,%%ymm5" % bfl0,
              "  vmovdqu  %%ymm4,%d(%%rdi)" % (32*r),
              "  vmovdqu  %%ymm5,%d(%%rdi)" % (256 + 32*r)]
    L += ["", "/* --- final scaling: red16 then montmul by 128^-1*R^2 (-> a*R) --- */"]
    L += load_tw("%r8")
    for r in range(16):
        L += ["  vmovdqu  %d(%%rdi),%%ymm4" % (32*r),
              "  red16 %ymm4",
              "  montmul %ymm5,%ymm4",
              "  vmovdqu  %%ymm5,%d(%%rdi)" % (32*r)]
    L += ["  ret", ""]

    # ---- pointwise ----  c_i = a_i*b_i*R^-1 (red16 inputs, then signed dmul).
    L += ["/* ============ pointwise: c = a*b*R^-1 coefficient-wise ============",
          "   red16 the (lazy ~2q) NTT outputs, then signed data*data Montgomery mul. */",
          "pointwise_avx:",
          "  vmovdqa   0(%rcx),%ymm0",            # q
          "  vmovdqa  32(%rcx),%ymm2",            # V
          "  vmovdqa  64(%rcx),%ymm3",            # RND
          "  vmovdqa  ntt_qinv(%rip),%ymm9"]      # qinv for dmul
    for r in range(16):
        off = 32*r
        L += ["  vmovdqu  %d(%%rsi),%%ymm4" % off,
              "  vmovdqu  %d(%%rdx),%%ymm5" % off,
              "  red16 %ymm4",
              "  red16 %ymm5",
              "  dmul %ymm7,%ymm4,%ymm5",
              "  vmovdqu  %%ymm7,%d(%%rdi)" % off]
    L += ["  ret", ""]

    # ---- reduce_avx: red16 every coeff to |x| <~ q (signed canonicalize) ----
    L += ["/* ============ reduce_avx: red16 each coeff (signed, |x| <~ q) ============ */",
          "reduce_avx:",
          "  vmovdqa   ntt_qdata(%rip),%ymm0",
          "  vmovdqa   ntt_qdata+32(%rip),%ymm2",
          "  vmovdqa   ntt_qdata+64(%rip),%ymm3"]
    for r in range(16):
        off = 32*r
        L += ["  vmovdqu  %d(%%rdi),%%ymm4" % off,
              "  red16 %ymm4",
              "  vmovdqu  %%ymm4,%d(%%rdi)" % off]
    L += ["  ret", ""]

    # ---- nttunpack: standard-order int32 samples -> avx NTT layout ----
    L += ["/* ============ nttunpack: standard int32 samples -> avx NTT layout ============",
          " * Kyber-style: forward shuffle ladder, no butterflies.",
          " *   rdi = dst (int16[256]), rsi = src (int32[256]). */",
          ".global nttunpack_avx", "nttunpack_avx:"]
    for h, prog in ((0, FWD0), (1, FWD1)):
        for r in range(8):
            so = (128 * h + 16 * r) * 4
            L += ["  vmovdqu  %d(%%rsi),%%ymm12" % so,
                  "  vmovdqu  %d(%%rsi),%%ymm13" % (so + 32),
                  "  vpackusdw %%ymm13,%%ymm12,%s" % ymm(r),
                  "  vpermq   $0xD8,%s,%s" % (ymm(r), ymm(r))]
        for op in prog:
            if op[0] == 'sh':
                k, r0, r1 = op[1], op[2], op[3]
                L += ["  shuf%d %s,%s" % (k, ymm(r0), ymm(r1))]
        for r in range(8):
            L += ["  vmovdqu  %s,%d(%%rdi)" % (ymm(r), (128 * h + 16 * r) * 2)]
    L += ["  ret", ""]

    L += ['.section .note.GNU-stack,"",@progbits']
    body = _namespace("\n".join(L) + "\n")
    with open(path, "w") as f:
        f.write(body)
    return len(body.splitlines())

if __name__ == "__main__":
    from pathlib import Path
    # SHUTTLE vendored output layout: AVX2 .S/consts -> SHUTTLE/avx2/<qset>/,
    # AVX-512 -> SHUTTLE/avx512/<qset>/.  Paths are relative to this generator
    # (SHUTTLE/tools/ntt/<qset>/gen_ntt.py): ../../../{avx2,avx512}/<qset>/.
    # An optional argv[1] overrides the base dir (used by check-consts to emit
    # into a scratch tree before cmp).
    HERE = Path(__file__).resolve().parent
    QSET = HERE.name                                  # "q15361n256"
    ROOT = HERE.parents[2]                            # SHUTTLE/
    base = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT
    avx2_dir = base / "avx2" / QSET
    avx512_dir = base / "avx512" / QSET
    avx2_dir.mkdir(parents=True, exist_ok=True)
    avx512_dir.mkdir(parents=True, exist_ok=True)

    ok, lines = verify()
    print("q=%d omega=%d (omega^%d==-1:%s) MONT=%d QINV=%d NINV_TOMONT=%d RED_V=%d RED_RND=%d"
          % (Q, OMEGA, N, pow(OMEGA, N, Q) == Q-1, MONT, QINV, NINV_TOMONT, RED_V, RED_RND))
    for l in lines:
        print(l)
    nf, ni = emit_consts(str(avx2_dir / "ntt_consts.c"))
    print("emitted ntt_consts.c  fwd_vecs=%d inv_vecs=%d" % (nf, ni))
    nl = emit_asm(str(avx2_dir / "ntt.S"))
    print("emitted ntt.S (%d lines)" % nl)
    print("FWD red butterflies=%d/%d, INV red butterflies=%d/%d"
          % (sum(1 for _,c in FWD_SCHED if c), len(FWD_SCHED),
             sum(1 for _,c in INV_SCHED if c), len(INV_SCHED)))
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from avx512_codegen import Avx512Config, emit_avx512
    cfg512 = Avx512Config(q=Q, n=N, root=OMEGA, brv_width=BRV,
                          levels=(128, 64, 32, 16, 8, 4, 2, 1),
                          scale_mont=NINV_TOMONT, incomplete=False, lazy=False,
                          signed=True, red_v=RED_V, red_rnd=RED_RND,
                          sym_prefix=SYM, prefix=SYM + "ntt512")
    ok512, lines512 = emit_avx512(cfg512, out_dir=str(avx512_dir))
    for l in lines512:
        print(l)
    sys.exit(0 if (ok and ok512) else 1)
