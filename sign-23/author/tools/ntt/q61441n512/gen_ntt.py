#!/usr/bin/env python3
"""
gen_ntt.py -- verified model + .S/constant generator for a COMPLETE n=512
negacyclic NTT over Z_q[x]/(x^512+1), q=61441, 16-bit AVX2, Kyber-style level
merging + the 4-tier shuffle network.

q=61441:  q-1 = 2^12 * 3 * 5, so 2n=1024 | q-1  =>  COMPLETE NTT (9 levels,
len 256..1), 512 zetas, brv width 9, POINTWISE (coefficient-wise) pointmul.
q > 2^15  =>  VALLEY: unsigned [0,q) path, vpmulhuw + borrow correction, and
2q > 2^16 so ZERO lazy-reduction headroom (every add/sub overflow-aware).

LAYOUT (the only structural difference from q=45569 n=256):
  512 coeffs = 32 YMM registers; we cannot hold all live.  We split into 4
  BLOCKS of 128 coeffs (8 YMM each), exactly like the q=45569 "half" but four
  of them.  The two OUTERMOST levels are cross-block and go through memory:
    level 0 (len=256): partners 256 apart -> B0<->B2, B1<->B3
    level 1 (len=128): partners 128 apart -> B0<->B1, B2<->B3
  Then levels 2..8 (len=64,32,16,8,4,2,1) are entirely WITHIN a 128-coeff block
  and run register-resident with the shuf8/4/2/1 network (identical to the
  q=45569 per-half levels 1..7).

Pipeline (mirrors q=45569 gen_ntt.py exactly):
  1. Build the forward transform of one 128-coeff block as an explicit op-list,
     baking per-lane Montgomery zeta vectors by tracking coefficient indices
     through the exact shuffles.
  2. The inverse is the LITERAL REVERSE of that program (CT->GS, shuffle stays),
     so a forward/inverse roundtrip is correct by construction.
  3. Verify both with a pure-Python AVX2 lane-permutation interpreter: roundtrip
     and full negacyclic convolution vs schoolbook.
  4. Emit ntt.S and ntt_consts.c ({q, 2^16-q, 0} + the Seiler-precomputed
     (zl,zh) twiddle tables in exact consumption order).
"""
import re
import sys

# SHUTTLE namespacing: every PUBLIC AVX2 symbol this generator emits is prefixed
# with SYM ("s512_") so all three vendored NTT configs co-link in one process
# (see SHUTTLE plan P03 / namespace.h).  The rename is a generator post-pass
# (_namespace), NOT a hand-patch of the .S, so it survives `make tables`
# deterministically.  Set SYM="" to reproduce the original un-prefixed output.
SYM = "s512_"

# Whole-word public AVX2 symbols (function labels that double as .global, plus
# the const tables -- including those the .S references RIP-relative by name,
# `ntt_qinv(%rip)`, which is renamed in lock-step).  UNSIGNED valley config:
# no reduce_avx; has ntt_ninv/ntt_r2 and the cross-block ntt_cross_fwd/inv tables.
_NS_SYMS = ("ntt_avx", "invntt_tomont_avx", "pointwise_avx", "nttunpack_avx",
            "ntt_qdata", "ntt_qinv", "ntt_ninv", "ntt_r2",
            "ntt_cross_fwd", "ntt_cross_inv",
            "ntt_zetas_fwd", "ntt_zetas_inv")
_NS_PAT = re.compile(r"\b(" + "|".join(_NS_SYMS) + r")\b")

def _namespace(text):
    """Prefix every public AVX2 symbol with SYM (whole-word)."""
    if not SYM:
        return text
    return _NS_PAT.sub(lambda m: SYM + m.group(1), text)

Q = 61441
N = 512
LOGN = 9                            # log2(N) = number of butterfly levels
R = 1 << 16
MONT = R % Q                        # 4095
QINV = pow(Q, -1, R)               # 4097
RMQ  = R - Q                       # 4095  (2^16 - q, reduction constant)
NINV = pow(N, Q - 2, Q)            # N^-1 mod q
def _factorize(m):
    f = []; d = 2
    while d * d <= m:
        if m % d == 0:
            f.append(d)
            while m % d == 0: m //= d
        d += 1
    if m > 1: f.append(m)
    return f
def _mod_order(g, q):                # multiplicative order of g mod q
    n = q - 1
    for p in _factorize(n):
        while n % p == 0 and pow(g, n // p, q) == 1: n //= p
    return n
def _primitive_root(q, order):       # SMALLEST primitive order-th root (matches gen_zetas.py)
    for g in range(2, q):
        if _mod_order(g, q) == order: return g
    raise ValueError("no primitive %d-th root mod %d" % (order, q))
# Use ref's root convention (gen_zetas.py: smallest primitive root) so the avx
# NTT is a true transpose of the scalar ntt16 -- this lets the A0 layout
# permutation be a clean shuffle ladder (nttunpack) instead of a scramble.
OMEGA = _primitive_root(Q, 2 * N)  # 156: smallest primitive 2N-th root, omega^N == -1
RINV = pow(R, -1, Q)

NB   = 4                            # number of 128-coeff blocks
BLK  = 128                          # coeffs per block (8 YMM)

def brv(x, w=LOGN):
    r = 0
    for i in range(w):
        r |= ((x >> i) & 1) << (w - 1 - i)
    return r

ZETA_MONT = [pow(OMEGA, brv(k), Q) * MONT % Q for k in range(N)]
R2_MOD_Q  = MONT * MONT % Q
NINV_TOMONT = NINV * R2_MOD_Q % Q

def fqmul(a, b):
    return (a * b * RINV) % Q

# (len,start) -> global zeta index, standard CT loop order over the whole array.
FWD_K = {}
_k = 1; _len = N // 2
while _len >= 1:
    _s = 0
    while _s < N:
        FWD_K[(_len, _s)] = _k; _k += 1; _s += 2 * _len
    _len >>= 1

def zeta_of(length, coeff_low):
    start = (coeff_low // (2 * length)) * (2 * length)
    return ZETA_MONT[FWD_K[(length, start)]]

# --------------------------------------------------------------------------
# AVX2 lane permutations, modelled INSTRUCTION-ACCURATELY (16 int16 lanes,
# lanes 0-7 = low 128-bit, lanes 8-15 = high 128-bit).
# --------------------------------------------------------------------------
def _dwords(x):  return [x[2*i:2*i+2] for i in range(8)]
def _undw(dws):  return [v for dw in dws for v in dw]

def vperm2i128(a, b, imm):
    lo = {0: a[0:8], 1: a[8:16], 2: b[0:8], 3: b[8:16]}
    return lo[imm & 3] + lo[(imm >> 4) & 3]
def vpunpcklqdq(a, b):
    return a[0:4] + b[0:4] + a[8:12] + b[8:12]
def vpunpckhqdq(a, b):
    return a[4:8] + b[4:8] + a[12:16] + b[12:16]
def vmovsldup(x):
    d = _dwords(x); return _undw([d[0],d[0],d[2],d[2],d[4],d[4],d[6],d[6]])
def vpsrlq(x, n):
    assert n == 32
    d = _dwords(x); z = [0,0]
    return _undw([d[1],z,d[3],z,d[5],z,d[7],z])
def vpblendd(a, b, mask):
    da, db = _dwords(a), _dwords(b)
    return _undw([db[i] if (mask>>i)&1 else da[i] for i in range(8)])
def vpslld(x, n):
    assert n == 16
    return _undw([[0, x[2*i]] for i in range(8)])
def vpsrld(x, n):
    assert n == 16
    return _undw([[x[2*i+1], 0] for i in range(8)])
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
# Build the forward op-list for one 128-coeff block (base = 0,128,256,384),
# baking zeta vectors by tracking coefficient indices through the exact
# shuffles.  Identical structure to the q=45569 build_half.
#   ('xb', ra, rb, zvec)   cross-register vertical CT butterfly
#   ('sh', k, r2m, r2m1)   in-place shuffle of a register pair
#   ('ib', r2m, r2m1, zvec) vertical CT butterfly after a shuffle
# --------------------------------------------------------------------------
def build_block(base):
    idx = [[base + 16*r + l for l in range(16)] for r in range(8)]
    prog = []
    for length in (64, 32, 16):                 # cross-register
        step = length // 16
        for r in range(8):
            if (r // step) % 2 == 0:
                p = r + step
                z = zeta_of(length, idx[r][0])
                prog.append(('xb', r, p, [z]*16))
    for length in (8, 4, 2, 1):                  # in-register
        sh = SHUF[length]
        for m in range(4):
            a, b = idx[2*m], idx[2*m+1]
            c, d = sh(a, b)
            zvec = [zeta_of(length, c[l]) for l in range(16)]
            prog.append(('sh', length, 2*m, 2*m+1))
            prog.append(('ib', 2*m, 2*m+1, zvec))
            idx[2*m], idx[2*m+1] = c, d
    return prog

BLOCKS = [build_block(BLK*b) for b in range(NB)]

# cross-block levels: level 0 (len=256) partners 256 apart, level 1 (len=128)
# partners 128 apart.  These zetas vary per partner pair.
def cross_zeta(length, low):    # one scalar zeta vector for the cross-block op
    return zeta_of(length, low)

# level-0 pairs (len=256): (B0,B2),(B1,B3)  partner = blk+2
L0_PAIRS = [(0, 2), (1, 3)]
# level-1 pairs (len=128): (B0,B1),(B2,B3)  partner = blk+1
L1_PAIRS = [(0, 1), (2, 3)]

# --------------------------------------------------------------------------
# Interpreter: run a forward program on 8 value-registers of a block.
# --------------------------------------------------------------------------
def run_fwd_block(regs, prog):
    for op in prog:
        if op[0] in ('xb', 'ib'):
            _, ra, rb, z = op
            a, b = regs[ra], regs[rb]
            for l in range(16):
                t = fqmul(z[l], b[l])
                a[l], b[l] = (a[l]+t) % Q, (a[l]-t) % Q
        else:  # 'sh'
            _, k, r0, r1 = op
            regs[r0], regs[r1] = SHUF[k](regs[r0], regs[r1])
    return regs

def zinv_mont(zm):
    plain = zm * RINV % Q
    return pow(plain, -1, Q) * MONT % Q

def run_inv_block(regs, prog):
    for op in reversed(prog):
        if op[0] in ('xb', 'ib'):
            tag, r0, r1, z = op
            a, b = regs[r0], regs[r1]
            for l in range(16):
                zi = zinv_mont(z[l])
                u, v = a[l], b[l]
                a[l] = (u + v) % Q
                b[l] = fqmul(zi, (u - v) % Q)
        else:  # 'sh'
            _, k, r0, r1 = op
            regs[r0], regs[r1] = SHUF[k](regs[r0], regs[r1])
    return regs

def to_regs(poly, base):
    return [[poly[base+16*r+l] for l in range(16)] for r in range(8)]
def from_regs(regs, poly, base):
    for r in range(8):
        for l in range(16):
            poly[base+16*r+l] = regs[r][l]

def ntt(poly):
    a = poly[:]
    # level 0 (len=256): cross-block B0<->B2, B1<->B3
    for (b0, b1) in L0_PAIRS:
        lo = BLK*b0
        z = cross_zeta(256, lo)
        for off in range(BLK):
            j = lo + off; jp = BLK*b1 + off
            t = fqmul(z, a[jp]); a[jp] = (a[j]-t) % Q; a[j] = (a[j]+t) % Q
    # level 1 (len=128): cross-block B0<->B1, B2<->B3
    for (b0, b1) in L1_PAIRS:
        lo = BLK*b0
        z = cross_zeta(128, lo)
        for off in range(BLK):
            j = lo + off; jp = BLK*b1 + off
            t = fqmul(z, a[jp]); a[jp] = (a[j]-t) % Q; a[j] = (a[j]+t) % Q
    # levels 2..8: per-block register-resident
    for b in range(NB):
        regs = run_fwd_block(to_regs(a, BLK*b), BLOCKS[b])
        from_regs(regs, a, BLK*b)
    return a

def intt(poly):
    a = poly[:]
    # levels 8..2: per-block inverse (reversed program)
    for b in range(NB):
        regs = run_inv_block(to_regs(a, BLK*b), BLOCKS[b])
        from_regs(regs, a, BLK*b)
    # level 1 inverse (len=128) GS, then level 0 (len=256) GS
    for (b0, b1) in L1_PAIRS:
        lo = BLK*b0
        zi = zinv_mont(cross_zeta(128, lo))
        for off in range(BLK):
            j = lo + off; jp = BLK*b1 + off
            A, B = a[j], a[jp]
            a[j] = (A + B) % Q
            a[jp] = fqmul(zi, (A - B) % Q)
    for (b0, b1) in L0_PAIRS:
        lo = BLK*b0
        zi = zinv_mont(cross_zeta(256, lo))
        for off in range(BLK):
            j = lo + off; jp = BLK*b1 + off
            A, B = a[j], a[jp]
            a[j] = (A + B) % Q
            a[jp] = fqmul(zi, (A - B) % Q)
    # final *1/n and to-Montgomery: invntt_tomont(ntt(a)) = a*R.
    return [fqmul(x, NINV_TOMONT) for x in a]

def schoolbook(u, v):
    c = [0]*N
    for i in range(N):
        for j in range(N):
            p = u[i]*v[j] % Q; k = i+j
            if k < N: c[k] = (c[k]+p) % Q
            else:     c[k-N] = (c[k-N]-p) % Q
    return c

# --------------------------------------------------------------------------
def verify():
    import random
    random.seed(2026)
    out = []
    ok = all(intt(ntt(a := [random.randrange(Q) for _ in range(N)])) == [(x * MONT) % Q for x in a]
             for _ in range(300))
    out.append("roundtrip-to-mont(300): " + ("PASS" if ok else "FAIL"))
    ok2 = True
    for _ in range(60):
        a = [random.randrange(Q) for _ in range(N)]
        b = [random.randrange(Q) for _ in range(N)]
        na, nb = ntt(a), ntt(b)
        nc = [fqmul(na[i], nb[i]) for i in range(N)]
        if intt(nc) != schoolbook(a, b):
            ok2 = False; break
    out.append("convolution(60): " + ("PASS" if ok2 else "FAIL"))
    return ok and ok2, out

# --------------------------------------------------------------------------
# Emit constant table (consumption order) and the generated .S body.
# --------------------------------------------------------------------------
def s16(v):
    v %= 65536
    return v - 65536 if v >= 32768 else v

def emit_consts(path):
    # forward zeta vectors in execution order: B0,B1,B2,B3 (xb + ib carry zvec)
    fwd = []
    for prog in BLOCKS:
        for op in prog:
            if op[0] in ('xb', 'ib'):
                fwd.append(op[-1])
    # inverse consumes the reversed program per block, INVERTED twiddles
    inv = []
    for prog in BLOCKS:
        for op in reversed(prog):
            if op[0] in ('xb', 'ib'):
                inv.append([zinv_mont(z) for z in op[-1]])
    def zl16(z): return (z * QINV) % R
    def vec_zl(zvec): return [zl16(z) for z in zvec]
    def vec_zh(zvec): return list(zvec)
    def pair_table(table):
        out = []
        for zvec in table:
            out.append(vec_zl(zvec)); out.append(vec_zh(zvec))
        return out
    def vecs(name, table):
        lines = [f"const int16_t {name}[{len(table)}][16] = {{"]
        for v in table:
            lines.append("  {" + ",".join(str(s16(x)) for x in v) + "},")
        lines.append("};")
        return "\n".join(lines)
    def pair32(name, z):
        zl = ",".join([str(s16(zl16(z)))]*16)
        zh = ",".join([str(s16(z))]*16)
        return ("const int16_t %s[32] __attribute__((aligned(32))) = {\n  %s,  /* zl */\n  %s   /* zh */\n};\n"
                % (name, zl, zh))
    # cross-block twiddle tables (one (zl,zh) pair per cross-block butterfly group)
    def cross_pairs_fwd(level, pairs):
        out = []
        for (b0, b1) in pairs:
            z = cross_zeta(level, BLK*b0)
            out.append(vec_zl([z]*16)); out.append(vec_zh([z]*16))
        return out
    def cross_pairs_inv(level, pairs):
        out = []
        for (b0, b1) in pairs:
            z = zinv_mont(cross_zeta(level, BLK*b0))
            out.append(vec_zl([z]*16)); out.append(vec_zh([z]*16))
        return out

    fwd_p, inv_p = pair_table(fwd), pair_table(inv)
    l0_fwd = cross_pairs_fwd(256, L0_PAIRS)
    l1_fwd = cross_pairs_fwd(128, L1_PAIRS)
    l1_inv = cross_pairs_inv(128, L1_PAIRS)
    l0_inv = cross_pairs_inv(256, L0_PAIRS)
    import io
    f = io.StringIO()
    if True:
        f.write("#include <stdint.h>\n")
        f.write("/* Auto-generated by gen_ntt.py -- do not edit. q=%d n=%d.\n"
                "   Twiddles use the Seiler precompute: each butterfly stores zl=z*qinv\n"
                "   mod 2^16 then zh=z, as two consecutive 16-lane vectors. */\n\n" % (Q, N))
        f.write("const int16_t ntt_qdata[48] __attribute__((aligned(32))) = {\n")
        # q=61441 does not fit a signed int16; emit its int16 two's-complement
        # value (identical 16-bit pattern, no -Woverflow on the literal).
        f.write("  " + ",".join([str(s16(Q))]*16) + ",  /* _16XQ    */\n")
        f.write("  " + ",".join([str(RMQ)]*16) + ",  /* _16XRMQ  */\n")
        f.write("  " + ",".join(["0"]*16) + "   /* _16XZERO */\n};\n\n")
        f.write(pair32("ntt_ninv", NINV_TOMONT) + "\n")
        f.write("const int16_t ntt_qinv[16] __attribute__((aligned(32))) = {\n")
        f.write("  " + ",".join([str(s16(QINV))]*16) + "\n};\n\n")
        f.write(pair32("ntt_r2", R2_MOD_Q) + "\n")
        # cross-block twiddles, emitted CONTIGUOUS as the asm consumes them:
        #   cross_fwd = level0 (2 pairs) then level1 (2 pairs)
        #   cross_inv = level1 (2 pairs) then level0 (2 pairs)
        f.write("__attribute__((aligned(32)))\n" + vecs("ntt_cross_fwd", l0_fwd + l1_fwd) + "\n\n")
        f.write("__attribute__((aligned(32)))\n" + vecs("ntt_cross_inv", l1_inv + l0_inv) + "\n\n")
        f.write("__attribute__((aligned(32)))\n" + vecs("ntt_zetas_fwd", fwd_p) + "\n\n")
        f.write("__attribute__((aligned(32)))\n" + vecs("ntt_zetas_inv", inv_p) + "\n")
    with open(path, "w") as out:
        out.write(_namespace(f.getvalue()))
    return len(fwd_p), len(inv_p)

# --------------------------------------------------------------------------
# Emit ntt.S from the proven op-list (correct by construction).
# Register map: ymm0=q ymm2=2^16-q ymm3=zero ; ymm1=zh ymm15=zl ;
# ymm4..11 = 8 data regs of a block ; ymm12,13 scratch ; ymm14 = temp t.
# C ABI (System V): rdi=poly, rsi=qdata[48], rdx=ztab[][16] (per-block fwd/inv),
#   rcx=cross-block twiddle table base, r8=ninv pair.
# --------------------------------------------------------------------------
ASM_PREAMBLE = r"""/* ==========================================================================
 * Auto-generated by gen_ntt.py -- DO NOT EDIT (edit gen_ntt.py, run `make gen`).
 *
 * Complete n=512 negacyclic NTT / INTT over Z_q[x]/(x^512+1), q = 61441,
 * 16-bit AVX2, Kyber-style level merging + 4-tier shuffle network.
 *
 * LAYOUT: 512 coeffs = 32 YMM (cannot hold live), so we use 4 BLOCKS of 128
 * coeffs (8 YMM each).  The 2 outermost levels are cross-block (through memory):
 *   level 0 (len=256): B0<->B2, B1<->B3
 *   level 1 (len=128): B0<->B1, B2<->B3
 * Levels 2..8 (len=64..1) run register-resident inside each 128-coeff block,
 * with the shuf8/4/2/1 network (identical to the q=45569 per-half levels 1..7).
 *
 * Entry points (System V AMD64 ABI):
 *   ntt_avx   (rdi=poly[512] int16, rsi=qdata, rdx=zetas_fwd, rcx=cross_fwd, r8=ninv)
 *   invntt_tomont_avx(rdi=poly[512] int16, rsi=qdata, rdx=zetas_inv, rcx=cross_inv, r8=ninv)
 *   pointwise_avx(rdi=c[512], rsi=a[512], rdx=b[512], rcx=qdata)
 *   (cross_fwd = ntt_l0_fwd followed by ntt_l1_fwd; cross_inv = ntt_l1_inv then ntt_l0_inv)
 *
 * Register map (held live across a whole block transform):
 *   ymm0 = q (16x)   ymm1 = zh = current zeta vec   ymm2 = 2^16 - q   ymm3 = 0
 *   ymm4..ymm11 = the 8 data registers of one 128-coeff block
 *   ymm12,ymm13 = scratch   ymm14 = butterfly temp t
 *   ymm15 = zl = (zeta * q^-1) mod 2^16 = the PRECOMPUTED Montgomery-factor twiddle
 *
 * DOMAIN: coefficients stay in PLAIN [0,q); twiddles are *R mod q (Montgomery).
 * q > 2^15 => ZERO lazy headroom: every add/sub is a full overflow-aware
 * reduction (2q>2^16 so even a+b does not fit), Montgomery hi word UNSIGNED.
 * ========================================================================== */

/* montmul: out = (in * zeta) * R^-1 mod q, result in [0,q).
 *   zl in ymm15 (= zeta*qinv mod 2^16),  zh in ymm1 (= zeta). */
.macro montmul out,in
  vpmullw  %ymm15,\in,%ymm12
  vpmulhuw %ymm1,\in,\out
  vpmulhuw %ymm0,%ymm12,%ymm12
  vpsubusw \out,%ymm12,%ymm13
  vpsubw   %ymm12,\out,\out
  vpcmpeqw %ymm3,%ymm13,%ymm13
  vpandn   %ymm2,%ymm13,%ymm13
  vpsubw   %ymm13,\out,\out
.endm

/* dmul: GENERAL Montgomery multiply of two DATA vectors (no Seiler precompute).
 * qinv is kept in ymm9. */
.macro dmul out,x,y
  vpmullw  \x,\y,%ymm12
  vpmullw  %ymm9,%ymm12,%ymm12
  vpmulhuw \x,\y,\out
  vpmulhuw %ymm0,%ymm12,%ymm12
  vpsubusw \out,%ymm12,%ymm13
  vpsubw   %ymm12,\out,\out
  vpcmpeqw %ymm3,%ymm13,%ymm13
  vpandn   %ymm2,%ymm13,%ymm13
  vpsubw   %ymm13,\out,\out
.endm

/* addmod: dst = (a + t) mod q via a-(q-t) with borrow-driven +(2^16-q). */
.macro addmod dst,a,t
  vpsubw   \t,%ymm0,%ymm12
  vpsubusw \a,%ymm12,%ymm13
  vpsubw   %ymm12,\a,%ymm12
  vpcmpeqw %ymm3,%ymm13,%ymm13
  vpandn   %ymm2,%ymm13,%ymm13
  vpsubw   %ymm13,%ymm12,\dst
.endm

/* submod: dst = (a - t) mod q with borrow-driven correction. */
.macro submod dst,a,t
  vpsubusw \a,\t,%ymm13
  vpsubw   \t,\a,%ymm12
  vpcmpeqw %ymm3,%ymm13,%ymm13
  vpandn   %ymm2,%ymm13,%ymm13
  vpsubw   %ymm13,%ymm12,\dst
.endm

/* ctbf: Cooley-Tukey butterfly (forward).  t = b*zeta; b = a-t; a = a+t. */
.macro ctbf a,b
  montmul %ymm14,\b
  submod  \b,\a,%ymm14
  addmod  \a,\a,%ymm14
.endm

/* gsbf: Gentleman-Sande butterfly (inverse).  t = a-b; a = a+b; b = t*zeta. */
.macro gsbf a,b
  submod  %ymm14,\a,\b
  addmod  \a,\a,\b
  montmul \b,%ymm14
.endm

/* ---- the 4-tier shuffle network (granularity halves each tier) ---- */
.macro shuf8 r0,r1            /* granularity 8 coeffs; the ONLY cross-128-bit tier */
  vperm2i128 $0x20,\r1,\r0,%ymm12
  vperm2i128 $0x31,\r1,\r0,\r1
  vmovdqa  %ymm12,\r0
.endm
.macro shuf4 r0,r1           /* granularity 4 coeffs (64-bit); in-128 unpack */
  vpunpcklqdq \r1,\r0,%ymm12
  vpunpckhqdq \r1,\r0,\r1
  vmovdqa  %ymm12,\r0
.endm
.macro shuf2 r0,r1           /* granularity 2 coeffs (32-bit dword) */
  vmovsldup \r1,%ymm12
  vpblendd $0xAA,%ymm12,\r0,%ymm12
  vpsrlq   $32,\r0,\r0
  vpblendd $0xAA,\r1,\r0,\r1
  vmovdqa  %ymm12,\r0
.endm
.macro shuf1 r0,r1           /* granularity 1 coeff (16-bit word) */
  vpslld   $16,\r1,%ymm12
  vpblendw $0xAA,%ymm12,\r0,%ymm12
  vpsrld   $16,\r0,\r0
  vpblendw $0xAA,\r1,\r0,\r1
  vmovdqa  %ymm12,\r0
.endm
"""

def emit_asm(fwd_path):
    ymm = lambda r: "%%ymm%d" % (4 + r)
    def load_consts():
        return ["  vmovdqa   0(%rsi),%ymm0", "  vmovdqa  32(%rsi),%ymm2",
                "  vmovdqa  64(%rsi),%ymm3"]
    def load_tw(reg, disp=0):
        return ["  vmovdqa  %d(%s),%%ymm15" % (disp, reg),
                "  vmovdqa  %d(%s),%%ymm1"  % (disp + 32, reg)]
    def load_block(b):
        return ["  vmovdqu  %d(%%rdi),%s" % (256*b + 32*r, ymm(r)) for r in range(8)]
    def store_block(b):
        return ["  vmovdqu  %s,%d(%%rdi)" % (ymm(r), 256*b + 32*r) for r in range(8)]

    # length -> NTT level number (level 0 = len=256, level 1 = len=128 cross-block)
    LVL = {256: 0, 128: 1, 64: 2, 32: 3, 16: 4, 8: 5, 4: 6, 2: 7, 1: 8}
    XB_LENS = [64]*4 + [32]*4 + [16]*4

    # ---------- forward ----------
    L = list(ASM_PREAMBLE.splitlines())
    L += [".text", ".global ntt_avx", ".global invntt_tomont_avx", ".global pointwise_avx",
          "", "/* ================= forward NTT (9 levels, len 256..1) ================= */",
          "ntt_avx:"]
    L += load_consts()

    # level 0 (len=256): B0<->B2, B1<->B3  (cross-block, through memory)
    L += ["",
          "/* --- level 0 (len=256): cross-BLOCK CT, partners 256 apart (B0<->B2,B1<->B3) --- */"]
    for pi, (b0, b1) in enumerate(L0_PAIRS):
        L += load_tw("%rcx", pi*64)
        for r in range(8):
            o0 = 256*b0 + 32*r; o1 = 256*b1 + 32*r
            L += ["  vmovdqu  %d(%%rdi),%%ymm4" % o0,
                  "  vmovdqu  %d(%%rdi),%%ymm5" % o1,
                  "  ctbf %ymm4,%ymm5",
                  "  vmovdqu  %%ymm4,%d(%%rdi)" % o0,
                  "  vmovdqu  %%ymm5,%d(%%rdi)" % o1]
    # level 1 (len=128): B0<->B1, B2<->B3  (cross-block, through memory)
    L += ["",
          "/* --- level 1 (len=128): cross-BLOCK CT, partners 128 apart (B0<->B1,B2<->B3) --- */"]
    l1_base = len(L0_PAIRS)*64
    for pi, (b0, b1) in enumerate(L1_PAIRS):
        L += load_tw("%rcx", l1_base + pi*64)
        for r in range(8):
            o0 = 256*b0 + 32*r; o1 = 256*b1 + 32*r
            L += ["  vmovdqu  %d(%%rdi),%%ymm4" % o0,
                  "  vmovdqu  %d(%%rdi),%%ymm5" % o1,
                  "  ctbf %ymm4,%ymm5",
                  "  vmovdqu  %%ymm4,%d(%%rdi)" % o0,
                  "  vmovdqu  %%ymm5,%d(%%rdi)" % o1]

    # per-block merged levels 2..8 (3 cross-register + 4 in-register-with-shuffle)
    ki = 0
    for b in range(NB):
        prog = BLOCKS[b]
        lo, hi = BLK*b, BLK*b + BLK - 1
        L += ["",
              "/* ===== block %d (coeffs %d..%d): levels 2-8 merged, NO stores until done ===== */" % (b, lo, hi)]
        L += load_block(b)
        xi = 0; cur = None
        for op in prog:
            if op[0] == 'xb':
                ln = XB_LENS[xi]; xi += 1
                if ln != cur:
                    cur = ln
                    L += ["  /* level %d (len=%d): cross-REGISTER CT, partners %d lanes apart, no shuffle */"
                          % (LVL[ln], ln, ln)]
                _, r, p, _ = op
                L += load_tw("%rdx", ki*64) + ["  ctbf %s,%s" % (ymm(r), ymm(p))]
                ki += 1
            elif op[0] == 'sh':
                _, k, r0, r1 = op
                if k != cur:
                    cur = k
                    L += ["  /* level %d (len=%d): IN-REGISTER, shuf%d interleave then vertical CT */"
                          % (LVL[k], k, k)]
                L += ["  shuf%d %s,%s" % (k, ymm(r0), ymm(r1))]
            else:  # ib
                _, r0, r1, _ = op
                L += load_tw("%rdx", ki*64) + ["  ctbf %s,%s" % (ymm(r0), ymm(r1))]
                ki += 1
        L += store_block(b)
    L += ["  ret", ""]

    # ---------- inverse ----------  (rdx=zinv table, rcx=cross_inv = l1_inv then l0_inv)
    L += ["/* ================= inverse NTT (GS butterflies, levels 8..0) ================= */",
          "invntt_tomont_avx:"]
    L += load_consts()
    ki = 0
    for b in range(NB):
        prog = BLOCKS[b]
        lo, hi = BLK*b, BLK*b + BLK - 1
        L += ["",
              "/* ===== block %d (coeffs %d..%d): levels 8-2 merged (forward order reversed) ===== */" % (b, lo, hi)]
        L += load_block(b)
        rev = list(reversed(prog))
        fl = []; xi = 0; last_sh = None
        for op in prog:
            if op[0] == 'xb': fl.append(XB_LENS[xi]); xi += 1
            elif op[0] == 'sh': last_sh = op[1]; fl.append(op[1])
            else: fl.append(last_sh)
        rl = list(reversed(fl)); cur = None
        for op, ln in zip(rev, rl):
            if op[0] in ('xb', 'ib'):
                kind = "cross-REGISTER GS" if op[0] == 'xb' else "IN-REGISTER GS (after shuf%d)" % ln
                if ln != cur:
                    cur = ln
                    L += ["  /* level %d (len=%d): %s */" % (LVL[ln], ln, kind)]
                _, r0, r1, _ = op
                L += load_tw("%rdx", ki*64) + ["  gsbf %s,%s" % (ymm(r0), ymm(r1))]
                ki += 1
            else:  # sh
                _, k, r0, r1 = op
                L += ["  shuf%d %s,%s" % (k, ymm(r0), ymm(r1))]
        L += store_block(b)

    # level 1 GS (len=128, cross-block), then level 0 GS (len=256) with
    # final scaling folded in.  The old version stored level-0 outputs and then
    # made a second full memory pass for *1/n; processing 4 YMM-pairs at a time
    # lets the 8 outputs fit in ymm4..ymm11 before scaling and storing.
    L += ["",
          "/* --- level 1 (len=128): cross-BLOCK GS (B0<->B1,B2<->B3) --- */"]
    for pi, (b0, b1) in enumerate(L1_PAIRS):
        L += load_tw("%rcx", pi*64)
        for r in range(8):
            o0 = 256*b0 + 32*r; o1 = 256*b1 + 32*r
            L += ["  vmovdqu  %d(%%rdi),%%ymm4" % o0,
                  "  vmovdqu  %d(%%rdi),%%ymm5" % o1,
                  "  gsbf %ymm4,%ymm5",
                  "  vmovdqu  %%ymm4,%d(%%rdi)" % o0,
                  "  vmovdqu  %%ymm5,%d(%%rdi)" % o1]
    L += ["",
          "/* --- level 0 (len=256): cross-BLOCK GS + final 1/n scale (B0<->B2,B1<->B3) --- */"]
    l0_inv_base = len(L1_PAIRS)*64
    for pi, (b0, b1) in enumerate(L0_PAIRS):
        for g in (0, 4):
            L += load_tw("%rcx", l0_inv_base + pi*64)
            for j, r in enumerate(range(g, g + 4)):
                y0 = 4 + 2*j
                y1 = y0 + 1
                o0 = 256*b0 + 32*r; o1 = 256*b1 + 32*r
                L += ["  vmovdqu  %d(%%rdi),%%ymm%d" % (o0, y0),
                      "  vmovdqu  %d(%%rdi),%%ymm%d" % (o1, y1),
                      "  gsbf %%ymm%d,%%ymm%d" % (y0, y1)]
            L += load_tw("%r8")
            for j, r in enumerate(range(g, g + 4)):
                y0 = 4 + 2*j
                y1 = y0 + 1
                o0 = 256*b0 + 32*r; o1 = 256*b1 + 32*r
                L += ["  montmul %%ymm%d,%%ymm%d" % (y0, y0),
                      "  montmul %%ymm%d,%%ymm%d" % (y1, y1),
                      "  vmovdqu  %%ymm%d,%d(%%rdi)" % (y0, o0),
                      "  vmovdqu  %%ymm%d,%d(%%rdi)" % (y1, o1)]
    L += ["  ret", ""]

    # pointwise multiply for the complete NTT: c_i = a_i*b_i*R^-1.
    L += ["/* ================= pointwise: c = a*b*R^-1 coefficient-wise ================= */",
          "pointwise_avx:"]
    L += ["  vmovdqa   0(%rcx),%ymm0",
          "  vmovdqa  32(%rcx),%ymm2",
          "  vmovdqa  64(%rcx),%ymm3",
          "  vmovdqa  ntt_qinv(%rip),%ymm9"]
    for r in range(N // 16):
        off = 32*r
        L += ["  vmovdqu  %d(%%rsi),%%ymm4" % off,
              "  vmovdqu  %d(%%rdx),%%ymm5" % off,
              "  dmul     %ymm7,%ymm4,%ymm5",
              "  vmovdqu  %%ymm7,%d(%%rdi)" % off]
    L += ["  ret", ""]

    # ---- nttunpack: standard-order int32 samples -> avx NTT layout ----
    # Kyber/Dilithium-style (poly_nttunpack / nttunpack_avx): replay the forward
    # shuffle network with NO butterflies, so out[s] = src[FINAL_IDX[s]].  The A0
    # seed-to-NTT samples arrive int32 in [0,q) and standard order; this narrows
    # them to uint16 and lays them out in the avx NTT slot order.  4 blocks of 128.
    L += ["/* ============ nttunpack: standard int32 samples -> avx NTT layout ============",
          " * Kyber-style: forward shuffle ladder, no butterflies, so",
          " * out[s] = src[FINAL_IDX[s]].  Narrows int32 (in [0,q)) -> uint16 on load.",
          " *   rdi = dst (int16[512]), rsi = src (int32[512]). */",
          ".global nttunpack_avx", "nttunpack_avx:"]
    for b in range(NB):
        for r in range(8):
            so = (128 * b + 16 * r) * 4
            L += ["  vmovdqu  %d(%%rsi),%%ymm12" % so,
                  "  vmovdqu  %d(%%rsi),%%ymm13" % (so + 32),
                  "  vpackusdw %%ymm13,%%ymm12,%s" % ymm(r),
                  "  vpermq   $0xD8,%s,%s" % (ymm(r), ymm(r))]
        for op in BLOCKS[b]:
            if op[0] == 'sh':
                k, r0, r1 = op[1], op[2], op[3]
                L += ["  shuf%d %s,%s" % (k, ymm(r0), ymm(r1))]
        for r in range(8):
            L += ["  vmovdqu  %s,%d(%%rdi)" % (ymm(r), (128 * b + 16 * r) * 2)]
    L += ["  ret", ""]

    L += ['.section .note.GNU-stack,"",@progbits']

    body = _namespace("\n".join(L) + "\n")
    with open(fwd_path, "w") as f:
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
    QSET = HERE.name                                  # "q61441n512"
    ROOT = HERE.parents[2]                            # SHUTTLE/
    base = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT
    avx2_dir = base / "avx2" / QSET
    avx512_dir = base / "avx512" / QSET
    avx2_dir.mkdir(parents=True, exist_ok=True)
    avx512_dir.mkdir(parents=True, exist_ok=True)

    ok, lines = verify()
    print("q=%d n=%d omega=%d (omega^%d==-1:%s) MONT=%d QINV=%d RMQ=%d NINV_TOMONT=%d R2=%d"
          % (Q, N, OMEGA, N, pow(OMEGA, N, Q) == Q-1, MONT, QINV, RMQ, NINV_TOMONT, R2_MOD_Q))
    for l in lines:
        print(l)
    nf, ni = emit_consts(str(avx2_dir / "ntt_consts.c"))
    print("emitted ntt_consts.c  fwd_vecs=%d inv_vecs=%d" % (nf, ni))
    nl = emit_asm(str(avx2_dir / "ntt.S"))
    print("emitted ntt.S (%d lines, forward+inverse)" % nl)
    print("block_ops=%d (x%d blocks)" % (len(BLOCKS[0]), NB))
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from avx512_codegen import Avx512Config, emit_avx512
    cfg512 = Avx512Config(q=Q, n=N, root=OMEGA, brv_width=LOGN,
                          levels=tuple(1 << i for i in range(LOGN - 1, -1, -1)),
                          scale_mont=NINV_TOMONT, incomplete=False, lazy=False,
                          sym_prefix=SYM, prefix=SYM + "ntt512")
    ok512, lines512 = emit_avx512(cfg512, out_dir=str(avx512_dir))
    for l in lines512:
        print(l)
    sys.exit(0 if (ok and ok512) else 1)
