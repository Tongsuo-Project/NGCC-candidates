#!/usr/bin/env python3
"""Generate and audit the branchless, division-free, segmented base-2 logarithm
approximation used by SHUTTLE's SamplerU / ApproxLog.

Background
----------
SamplerU represents a uniform sample u in (0,1) as u = 2^{-a} * b with the
exponent a >= 1 (number of leading zero bits of the mantissa stream) and the
mantissa b = 1 + m/2^{kappa_b} in [1,2).  Working in base two,

    log2(u) = log2(b) - a,

so the integer exponent term -a is *exact* and only log2(b), b in [1,2), needs
a polynomial approximation.  The transition test of R(.) only ever compares
log2(u) against fixed thresholds, so an additive error budget eta_log applies:
the spec fixes eta_log <= 2^-57 (absolute, in base-2 log units).

Current (deployed) method
-------------------------
A single degree-21 minimax polynomial P(u), u = b-1 in [0,1), evaluated by
Horner in Q62.  21 wide multiplies on the secret-dependent inner loop.

New method (this script): top-g-bit segmentation
------------------------------------------------
Split [1,2) into 2^g equal segments selected by the top g mantissa bits
j = floor(m / 2^{kappa_b-g}).  On segment j the left endpoint is
beta_j = 1 + j/2^g and we normalise the reduced argument to fill [0,1):

    b = beta_j + w,  w in [0, 2^-g),   x = w * 2^g in [0,1),
    P_j(x) ~= log2(beta_j + x*2^-g).

Each segment carries its own degree-d minimax polynomial (the offset
log2(beta_j) is baked into c_{j,0}).  Because each segment has endpoint ratio
1 + 2^-g instead of 2, the required degree collapses (~ g+2 accuracy bits per
degree), trading wide multiplies for one constant-time table scan.

Runtime semantics reproduced here EXACTLY (so the audit is bit-faithful to the
generated C, cf. gen_approx_exp_poly.py):
  * coefficients are signed Q62 int64;
  * x is an unsigned Q64 multiplier in [0,1);
  * Horner step:  acc <- c_k + ((acc * x) >> 64)   (arithmetic shift, __int128);
  * no branches, no division, no data-dependent memory access in the body;
  * the segment's coefficient row is fetched by a full-table constant-time scan.
"""
import argparse
from dataclasses import dataclass, field
from pathlib import Path

import mpmath as mp

mp.mp.dps = 60

# ------------------------------------------------------------------ constants
W = 62                      # coefficient / accumulator fractional bits (Q62)
SCALE = 1 << W
TARGET_BITS = mp.mpf(57)    # eta_log <= 2^-57 absolute (spec)
MARGIN_BITS = mp.mpf(57.5)  # require >= ~0.5 bit head-room when *selecting*
GRID_PER_SEG = 2049         # audit samples per segment (the C __float128
                            # verifier at 2^18/seg is the authoritative gate)

SELECTED_G = 2
SELECTED_DEGREE = 13


def f_seg(g: int, j: int):
    """Exact target on segment j, in the normalised variable x in [0,1)."""
    beta = 1 + mp.mpf(j) / (1 << g)
    step = mp.mpf(1) / (1 << g)

    def f(x):
        return mp.log(beta + x * step) / mp.log(2)

    return f


# --------------------------------------------------------- minimax (Remez)
def remez(f, d, lo=mp.mpf(0), hi=mp.mpf(1), iters=40, grid=2049, pin0=False):
    """Degree-d minimax polynomial of f on [lo,hi]; returns monomial coeffs
    c_0..c_d (low-to-high) as mpmath reals.  Standard Remez exchange with a
    dense-grid extremum search (robust for the smooth, monotone log).

    pin0=True forces c_0 == 0 (used on segment 0 so that ApproxLog(0,1)=0
    holds bit-exactly; legitimate because f(0)=log2(1)=0 there, so the error
    already vanishes at x=0 and no equioscillation point is spent on it)."""
    # number of free polynomial coefficients
    nc = d if pin0 else d + 1            # pin0: coeffs c_1..c_d
    # initial references: Chebyshev-Lobatto points (nc+1 of them)
    refs = [(lo + hi) / 2 - (hi - lo) / 2 * mp.cos(mp.pi * i / (nc))
            for i in range(nc + 1)]
    xs = [lo + (hi - lo) * mp.mpf(i) / (grid - 1) for i in range(grid)]
    fxs = [f(x) for x in xs]
    coeffs = None
    for _ in range(iters):
        n = nc + 1
        A = mp.matrix(n, n)
        rhs = mp.matrix(n, 1)
        for i, r in enumerate(refs):
            if pin0:
                xp = r                   # basis x^1..x^d
                for k in range(nc):
                    A[i, k] = xp
                    xp *= r
            else:
                xp = mp.mpf(1)           # basis x^0..x^d
                for k in range(nc):
                    A[i, k] = xp
                    xp *= r
            A[i, nc] = mp.mpf((-1) ** i)
            rhs[i] = f(r)
        sol = mp.lu_solve(A, rhs)
        if pin0:
            coeffs = [mp.mpf(0)] + [sol[k] for k in range(nc)]
        else:
            coeffs = [sol[k] for k in range(nc)]

        def err(x, fx):
            p = mp.mpf(0)
            for c in reversed(coeffs):
                p = p * x + c
            return fx - p

        es = [err(x, fx) for x, fx in zip(xs, fxs)]
        # locate the d+2 alternating extrema of the error on the dense grid
        ext_idx = [0]
        for i in range(1, grid - 1):
            if (es[i] - es[i - 1]) * (es[i + 1] - es[i]) <= 0:
                ext_idx.append(i)
        ext_idx.append(grid - 1)
        # collapse to the strongest extremum within each alternation run
        pruned = []
        for i in ext_idx:
            if pruned and (es[i] >= 0) == (es[pruned[-1]] >= 0):
                if abs(es[i]) > abs(es[pruned[-1]]):
                    pruned[-1] = i
            else:
                pruned.append(i)
        if len(pruned) >= nc + 1:
            # keep the nc+1 with the largest magnitude, preserving order
            strongest = sorted(sorted(pruned, key=lambda i: -abs(es[i]))[:nc + 1])
            refs = [xs[i] for i in strongest]
        emax = max(abs(e) for e in es)
        emin = min(abs(es[i]) for i in pruned) if pruned else mp.mpf(0)
        if emax > 0 and (emax - emin) / emax < mp.mpf('1e-6'):
            break
    return coeffs


# --------------------------------------------------------- fixed-point audit
def quantize(coeffs):
    return [int(mp.nint(c * SCALE)) for c in coeffs]


ROUND = 1 << 63             # round-to-nearest bias for the high-half multiply


def horner_int(X_q64: int, cq) -> int:
    """Bit-faithful image of the generated C inner loop.  X_q64 is the Q64
    unsigned multiplier (x in [0,1)); cq are signed Q62 coefficients; the
    accumulator is a 128-bit signed integer.  The high-half multiply rounds to
    nearest: ((acc*x) + 2^63) >> 64 (arithmetic shift; Python '>>' on the true
    integer == floor == gcc signed __int128 >>).  Rounding is unbiased and
    halves the per-step error vs truncation -- important because each Horner
    step otherwise adds a truncation bias."""
    acc = cq[-1]
    for c in reversed(cq[:-1]):
        acc = c + (((acc * X_q64) + ROUND) >> 64)
    return acc


@dataclass
class SegFit:
    j: int
    coeffs_real: list
    coeffs_q62: list
    max_abs: mp.mpf = mp.mpf(0)
    worst_b: mp.mpf = mp.mpf(0)


@dataclass
class Audit:
    g: int
    degree: int
    segs: list = field(default_factory=list)
    max_abs: mp.mpf = mp.mpf(0)
    worst_b: mp.mpf = mp.mpf(0)
    max_coeff_bits: int = 0
    fits_i64: bool = True
    monotonic: bool = True
    pins_zero: bool = True
    no_i128_overflow: bool = True

    @property
    def bits(self):
        return -mp.log(self.max_abs, 2) if self.max_abs > 0 else mp.inf

    @property
    def table_entries(self):
        return (1 << self.g) * (self.degree + 1)

    @property
    def table_bytes(self):
        return self.table_entries * 8

    @property
    def multiplies(self):
        return self.degree


def audit(g: int, degree: int) -> Audit:
    res = Audit(g=g, degree=degree)
    step = mp.mpf(1) / (1 << g)
    prev_val = None  # for global monotonicity across segment boundaries
    for j in range(1 << g):
        f = f_seg(g, j)
        cr = remez(f, degree, pin0=(j == 0))
        cq = quantize(cr)
        sf = SegFit(j=j, coeffs_real=cr, coeffs_q62=cq)
        beta = 1 + mp.mpf(j) / (1 << g)
        for s in range(GRID_PER_SEG + 1):
            x = mp.mpf(s) / GRID_PER_SEG
            if x >= 1:
                x = mp.mpf(GRID_PER_SEG - 1) / GRID_PER_SEG
            X = int(x * (mp.mpf(2) ** 64))
            if X >= (1 << 64):
                X = (1 << 64) - 1
            zi = horner_int(X, cq)
            approx = mp.mpf(zi) / SCALE
            b = beta + x * step
            ref = mp.log(b) / mp.log(2)
            e = abs(approx - ref)
            if e > sf.max_abs:
                sf.max_abs = e
                sf.worst_b = b
            # 128-bit product safety: |acc|*2^64 + 2^63 must stay < 2^127
            if abs(zi) * (1 << 64) + ROUND >= (1 << 127):
                res.no_i128_overflow = False
            # global monotonicity of the represented value z (a is fixed here)
            if prev_val is not None and zi < prev_val:
                res.monotonic = False
            prev_val = zi
        if sf.max_abs > res.max_abs:
            res.max_abs = sf.max_abs
            res.worst_b = sf.worst_b
        res.segs.append(sf)
    # global properties
    res.max_coeff_bits = max(abs(c).bit_length()
                             for sf in res.segs for c in sf.coeffs_q62)
    res.fits_i64 = all(-(1 << 63) <= c <= (1 << 63) - 1
                       for sf in res.segs for c in sf.coeffs_q62)
    # ApproxLog(0,1)=0 : segment 0 at x=0 must read exactly 0  (c_{0,0} == 0)
    res.pins_zero = (res.segs[0].coeffs_q62[0] == 0)
    return res


# --------------------------------------------------------------- code emit
def emit_header(path: Path, res: Audit):
    g, d = res.g, res.degree
    nseg = 1 << g
    rows = []
    for sf in res.segs:
        rows.append("    {" + ", ".join(f"INT64_C({c})" for c in sf.coeffs_q62) + "},")
    L = []
    L += ["#ifndef SHUTTLE_APPROX_LOG_POLY_H",
          "#define SHUTTLE_APPROX_LOG_POLY_H",
          "",
          "#include <stdint.h>",
          "",
          "#if defined(__GNUC__) || defined(__clang__)",
          "#define SHUTTLE_ALWAYS_INLINE static inline __attribute__((always_inline))",
          "#else",
          "#define SHUTTLE_ALWAYS_INLINE static inline",
          "#endif",
          "",
          f"/* Segmented base-2 log: log2(b), b in [1,2), to absolute error < 2^-57.",
          f"   g = {g} segment-index bits, degree {d}, Q{W} coefficients.",
          f"   measured max abs error = 2^-{mp.nstr(res.bits, 6)}",
          f"   table = {nseg} x {d + 1} int64 = {res.table_bytes} bytes. */",
          f"#define SHUTTLE_LOG_POLY_G {g}",
          f"#define SHUTTLE_LOG_POLY_SEGMENTS {nseg}",
          f"#define SHUTTLE_LOG_POLY_DEGREE {d}",
          f"#define SHUTTLE_LOG_POLY_QBITS {W}",
          "",
          f"/* kShuttleLogPoly[j][k] : Q{W} coefficient of x^k on segment j,",
          "   with x = (low mantissa bits) normalised to [0,1) as a Q64 value. */",
          f"static const int64_t kShuttleLogPoly[{nseg}][{d + 1}] = {{"]
    L += rows
    L += ["};",
          "",
          "/* High half of a signed 128-bit product, rounded to nearest:",
          "   ((a*x) + 2^63) >> 64  (arithmetic shift).  Rounding is unbiased and",
          "   halves the per-step error vs truncation. */",
          "SHUTTLE_ALWAYS_INLINE int64_t shuttle_log_mulhi(__int128 a, uint64_t x)",
          "{",
          "    return (int64_t)(((a * (__int128)(__uint128_t)x) + ((__int128)1 << 63)) >> 64);",
          "}",
          "",
          "/* Branch-free constant-time equality mask: all-ones iff a==b, else 0.",
          "   Data-independent: no branch and no cmov.  (At -O2/-O3 the compiler",
          "   may re-fold this into a cmp+sbb/sete, all single-cycle, flag consumed",
          "   arithmetically -- never by a conditional jump.) */",
          "SHUTTLE_ALWAYS_INLINE uint64_t shuttle_log_eqmask(uint32_t a, uint32_t b)",
          "{",
          "    uint64_t z = (uint64_t)(a ^ b);          /* 0 iff a==b */",
          "    uint64_t nz = (z | (~z + 1)) >> 63;       /* 1 iff z!=0, else 0 */",
          "    return nz - 1;                            /* 0xFF..FF iff a==b, else 0 */",
          "}",
          "",
          "/* Constant-time fetch of segment row `sel` into c[0..DEGREE]:",
          "   touches EVERY table entry, selects by arithmetic mask (no branch,",
          "   no data-dependent address). */",
          "SHUTTLE_ALWAYS_INLINE void shuttle_log_fetch_row(uint32_t sel, int64_t c[SHUTTLE_LOG_POLY_DEGREE + 1])",
          "{",
          "    for (int k = 0; k <= SHUTTLE_LOG_POLY_DEGREE; k++) c[k] = 0;",
          "    for (uint32_t j = 0; j < SHUTTLE_LOG_POLY_SEGMENTS; j++) {",
          "        uint64_t m = shuttle_log_eqmask(j, sel); /* all-ones iff j==sel */",
          "        for (int k = 0; k <= SHUTTLE_LOG_POLY_DEGREE; k++)",
          "            c[k] |= (int64_t)(m & (uint64_t)kShuttleLogPoly[j][k]);",
          "    }",
          "}",
          "",
          "/* Approximate log2(b) in Q62 for b = 1 + m/2^kappa_b in [1,2).",
          "   `j` = top g bits of m (segment), `x_q64` = remaining low mantissa",
          "   bits left-justified into a Q64 fraction in [0,1).  Branch-free,",
          "   division-free, constant-time. */",
          "SHUTTLE_ALWAYS_INLINE int64_t shuttle_log2_frac_q62(uint32_t j, uint64_t x_q64)",
          "{",
          "    int64_t c[SHUTTLE_LOG_POLY_DEGREE + 1];",
          "    shuttle_log_fetch_row(j, c);",
          "    __int128 acc = c[SHUTTLE_LOG_POLY_DEGREE];",
          "    for (int k = SHUTTLE_LOG_POLY_DEGREE - 1; k >= 0; k--)",
          "        acc = (__int128)c[k] + (__int128)shuttle_log_mulhi(acc, x_q64);",
          "    return (int64_t)acc;",
          "}",
          "",
          "/* PERFORMANCE-OPTIMAL variant (see ApproxLog.tex, Table tab:batch):",
          "   2-way batched evaluator, ~69 cyc/output vs ~78 for the scalar above.",
          "   Computes two INDEPENDENT log2 fractions at once -- each kShuttleLogPoly",
          "   entry is loaded once and shared by both lanes, and the two Horner chains",
          "   interleave to hide the multiply latency.  Register-lean fused form (only",
          "   the 2*SEGMENTS masks + two accumulators are live).  Same constant-time",
          "   guarantees as the scalar.  Use this when the caller can supply two",
          "   independent inputs; otherwise use shuttle_log2_frac_q62. */",
          "SHUTTLE_ALWAYS_INLINE void shuttle_log2_frac_q62_x2(const uint32_t sel[2], const uint64_t x_q64[2], int64_t out[2])",
          "{",
          "    uint64_t M0[SHUTTLE_LOG_POLY_SEGMENTS], M1[SHUTTLE_LOG_POLY_SEGMENTS];",
          "    for (uint32_t j = 0; j < SHUTTLE_LOG_POLY_SEGMENTS; j++) {",
          "        M0[j] = shuttle_log_eqmask(j, sel[0]);",
          "        M1[j] = shuttle_log_eqmask(j, sel[1]);",
          "    }",
          "    int64_t h0 = 0, h1 = 0;",
          "    for (uint32_t j = 0; j < SHUTTLE_LOG_POLY_SEGMENTS; j++) {",
          "        int64_t v = kShuttleLogPoly[j][SHUTTLE_LOG_POLY_DEGREE];",
          "        h0 |= (int64_t)(M0[j] & (uint64_t)v);",
          "        h1 |= (int64_t)(M1[j] & (uint64_t)v);",
          "    }",
          "    __int128 a0 = h0, a1 = h1;",
          "    for (int k = SHUTTLE_LOG_POLY_DEGREE - 1; k >= 0; k--) {",
          "        int64_t c0 = 0, c1 = 0;",
          "        for (uint32_t j = 0; j < SHUTTLE_LOG_POLY_SEGMENTS; j++) {",
          "            int64_t v = kShuttleLogPoly[j][k];",
          "            c0 |= (int64_t)(M0[j] & (uint64_t)v);",
          "            c1 |= (int64_t)(M1[j] & (uint64_t)v);",
          "        }",
          "        a0 = (__int128)c0 + (__int128)shuttle_log_mulhi(a0, x_q64[0]);",
          "        a1 = (__int128)c1 + (__int128)shuttle_log_mulhi(a1, x_q64[1]);",
          "    }",
          "    out[0] = (int64_t)a0;",
          "    out[1] = (int64_t)a1;",
          "}",
          "",
          "#undef SHUTTLE_ALWAYS_INLINE",
          "#endif",
          ""]
    path.write_text("\n".join(L))


def emit_log(path: Path, res: Audit, sweep):
    L = []
    L.append("SHUTTLE SamplerU / ApproxLog : segmented base-2 logarithm")
    L.append("target log2(b), b in [1,2);  eta_log <= 2^-57 absolute")
    L.append(f"coefficient format = signed Q{W};  x = Q64 multiplier in [0,1)")
    L.append("runtime = no branch, no division, full-table constant-time row scan")
    L.append("wide multiply use = (acc*x)>>64 high half only (>>64), __int128 acc")
    L.append("")
    L.append(f"selected scheme : g={res.g} ({1 << res.g} segments), degree={res.degree}")
    L.append(f"  multiplies (Horner)      = {res.multiplies}")
    L.append(f"  table                    = {1 << res.g} x {res.degree + 1} int64 = {res.table_bytes} bytes")
    L.append(f"  measured max abs error   = {mp.nstr(res.max_abs, 8)} = 2^-{mp.nstr(res.bits, 8)}")
    L.append(f"  worst b                  = {mp.nstr(res.worst_b, 12)}")
    L.append(f"  max coeff bits           = {res.max_coeff_bits};  fits int64 = {res.fits_i64}")
    L.append(f"  strictly increasing      = {res.monotonic}")
    L.append(f"  ApproxLog(0,1)=0 pinned   = {res.pins_zero}")
    L.append(f"  128-bit product safe      = {res.no_i128_overflow}")
    L.append("")
    L.append("comparison sweep (min degree to reach 2^-57 with >=0.5-bit margin):")
    L.append("g segments degree mults table_entries table_bytes scan_ops abs_bits fits_i64 mono note")
    for r in sweep:
        note = "selected" if (r.g == res.g and r.degree == res.degree) else ""
        if r.g == 0:
            note = (note + " baseline(deployed-style,single-segment)").strip()
        L.append(f"{r.g} {1 << r.g} {r.degree} {r.multiplies} {r.table_entries} "
                 f"{r.table_bytes} {r.table_entries} {mp.nstr(r.bits, 7)} "
                 f"{r.fits_i64} {r.monotonic} {note}")
    L.append("")
    L.append(f"selected coefficients kShuttleLogPoly[{1 << res.g}][{res.degree + 1}] (Q{W}, low->high):")
    for sf in res.segs:
        beta = 1 + mp.mpf(sf.j) / (1 << res.g)
        L.append(f"  j={sf.j:3d}  beta={mp.nstr(beta, 10)}  err=2^-{mp.nstr(-mp.log(sf.max_abs,2),6)}")
        L.append("    " + ", ".join(str(c) for c in sf.coeffs_q62))
    text = "\n".join(L) + "\n"
    path.write_text(text)
    print(text, end="")


def sweep_table():
    rows = []
    for g in range(0, 8):
        d_start = max(1, int(mp.floor(58 / (g + 2))) - 3)
        chosen = None
        for d in range(d_start, 40):
            r = audit(g, d)
            if r.bits >= MARGIN_BITS and r.fits_i64 and r.monotonic and r.no_i128_overflow:
                chosen = r
                break
        if chosen is None:
            chosen = r
        rows.append(chosen)
    return rows


# deployed single-segment baseline: P(u)=2^-62 sum c_i u^i ~ log2(1+u), u=b-1
# (tools/LogPolyApprox/log2_b_in_1_2_abs_57_64.txt, degree 21, Q62)
DEPLOYED_BASELINE = [
    12, 6653256548922149505, -3326628274459181085, 2217752182851402654,
    -1663314133014378403, 1330651220520872915, -1108874821083789136,
    950452344052204895, -831560255485224283, 738694605269400153,
    -662829495809222924, 595932539625226929, -528794318160472007,
    451527267200338167, -358255448212785553, 253529021404104913,
    -153211958011988634, 75505384931375114, -28766651603269161,
    7878760815328986, -1372175949002259, 113674624297114,
]
# schemes to emit for the performance exploration: (g, degree) at >=1-bit margin
# (g3 uses degree 11, not the thin-margin 10, since it is the selected scheme)
EXPLORE_SCHEMES = [(1, 16), (2, 13), (3, 11), (4, 9), (5, 8), (6, 7), (7, 6)]


def emit_nway(g, d, nseg, N):
    """Emit an N-way batched evaluator log2_frac_g{g}_x{N} that runs N
    independent inputs together.  Two goals: (i) amortize the constant-time table
    loads (each kLogPoly entry is loaded once and shared across the N lanes), and
    (ii) hide the Horner multiply latency by interleaving N independent dependency
    chains.  To keep register pressure low we DO NOT materialize N full
    coefficient rows; instead we precompute only the N*2^g equality masks once and
    FUSE the per-coefficient column scan into each Horner step -- so only N
    accumulators (+ N small temporaries + the mask array) are live."""
    L = [f"AL_INLINE void log2_frac_g{g}_x{N}(const uint32_t sel[{N}], "
         f"const uint64_t xx[{N}], int64_t out[{N}]){{"]
    # N x 2^g equality masks, precomputed once
    L.append(f"    uint64_t M[{N}][{nseg}];")
    L.append(f"    for(uint32_t j=0;j<{nseg};j++){{ "
             + " ".join(f"M[{n}][j]=al_eqmask(j,sel[{n}]);" for n in range(N)) + " }")
    # highest coefficient (column d) -> Horner init
    L.append("    " + " ".join(f"int64_t h{n}=0;" for n in range(N)))
    L.append(f"    for(uint32_t j=0;j<{nseg};j++){{ int64_t v=kLogPoly_g{g}[j][{d}];")
    L.append("        " + " ".join(f"h{n}|=(int64_t)(M[{n}][j]&(uint64_t)v);" for n in range(N)) + " }")
    L.append("    " + " ".join(f"__int128 a{n}=h{n};" for n in range(N)))
    # fused column-scan + interleaved Horner, high coefficient to low
    L.append(f"    for(int k={d-1};k>=0;k--){{")
    L.append("        " + " ".join(f"int64_t c{n}=0;" for n in range(N)))
    L.append(f"        for(uint32_t j=0;j<{nseg};j++){{ int64_t v=kLogPoly_g{g}[j][k];")
    L.append("            " + " ".join(f"c{n}|=(int64_t)(M[{n}][j]&(uint64_t)v);" for n in range(N)) + " }")
    L.append("        " + " ".join(f"a{n}=(__int128)c{n}+(__int128)al_mulhi(a{n},xx[{n}]);" for n in range(N)))
    L.append("    }")
    L.append("    " + " ".join(f"out[{n}]=(int64_t)a{n};" for n in range(N)))
    L.append("}")
    return L


def emit_explore(path: Path):
    """Emit one header with every scheme (baseline + g=1..7) for benchmarking:
    per-scheme coefficient table and an inline evaluator, sharing the rounded
    high-half multiply and the constant-time equality mask."""
    L = []
    L += ["#ifndef SHUTTLE_APPROX_LOG_EXPLORE_H",
          "#define SHUTTLE_APPROX_LOG_EXPLORE_H",
          "#include <stdint.h>",
          "#if defined(__GNUC__) || defined(__clang__)",
          "#define AL_INLINE static inline __attribute__((always_inline))",
          "#else",
          "#define AL_INLINE static inline",
          "#endif",
          "",
          "/* rounded high-half of a signed 128-bit product: ((a*x)+2^63)>>64 */",
          "AL_INLINE int64_t al_mulhi(__int128 a, uint64_t x){",
          "    return (int64_t)(((a*(__int128)(__uint128_t)x)+((__int128)1<<63))>>64);",
          "}",
          "/* constant-time equality mask: all-ones iff a==b */",
          "AL_INLINE uint64_t al_eqmask(uint32_t a, uint32_t b){",
          "    uint64_t z=(uint64_t)(a^b); uint64_t nz=(z|(~z+1))>>63; return nz-1;",
          "}",
          ""]
    # baseline (single segment, degree 21, Q62 multiplier u=b-1, >>62 rounding)
    db = DEPLOYED_BASELINE
    L += [f"/* baseline: deployed single-segment degree {len(db)-1}, Q62, u=b-1 as Q62 */",
          f"static const int64_t kLogBaseline[{len(db)}] = {{",
          "    " + ", ".join(f"INT64_C({c})" for c in db),
          "};",
          "AL_INLINE int64_t log2_frac_baseline(uint64_t u_q62){",
          f"    __int128 acc = kLogBaseline[{len(db)-1}];",
          f"    for (int k={len(db)-2}; k>=0; k--)",
          "        acc = (__int128)kLogBaseline[k] + (((acc*(__int128)(__uint128_t)u_q62)+((__int128)1<<61))>>62);",
          "    return (int64_t)acc;",
          "}",
          ""]
    schemes_meta = [("baseline", 0, len(db) - 1, 1)]
    for g, d in EXPLORE_SCHEMES:
        res = audit(g, d)
        if not (res.fits_i64 and res.no_i128_overflow):
            raise SystemExit(f"explore scheme g={g} d={d} unsafe")
        nseg = 1 << g
        rows = ["    {" + ", ".join(f"INT64_C({c})" for c in sf.coeffs_q62) + "}," for sf in res.segs]
        L += [f"/* g={g}: {nseg} segments, degree {d}, Q62, err 2^-{mp.nstr(res.bits,5)} */",
              f"static const int64_t kLogPoly_g{g}[{nseg}][{d+1}] = {{"]
        L += rows
        L += ["};",
              f"AL_INLINE int64_t log2_frac_g{g}(uint32_t sel, uint64_t x_q64){{",
              f"    int64_t c[{d+1}]; for (int k=0;k<={d};k++) c[k]=0;",
              f"    for (uint32_t j=0;j<{nseg};j++){{ uint64_t m=al_eqmask(j,sel);",
              f"        for (int k=0;k<={d};k++) c[k]|=(int64_t)(m&(uint64_t)kLogPoly_g{g}[j][k]); }}",
              f"    __int128 acc=c[{d}]; for (int k={d-1};k>=0;k--) acc=(__int128)c[k]+(__int128)al_mulhi(acc,x_q64);",
              "    return (int64_t)acc;",
              "}"]
        # N-way batched variants: one shared table load per entry, N interleaved
        # Horner chains -> amortizes the scan loads and hides the multiply latency.
        for N in (2, 3, 4, 8):
            L += emit_nway(g, d, nseg, N)
        L.append("")
        schemes_meta.append((f"g{g}", g, d, nseg))
    # a small descriptor table so the harness can loop over schemes
    L += ["/* scheme descriptors: {name, g, degree, segments, scan_ops} */",
          "typedef struct { const char *name; int g; int degree; int segments; int scan_ops; } al_scheme_t;",
          "static const al_scheme_t AL_SCHEMES[] = {"]
    for name, g, d, nseg in schemes_meta:
        scan = 0 if name == "baseline" else nseg * (d + 1)
        L.append(f'    {{"{name}", {g}, {d}, {nseg}, {scan}}},')
    L += ["};",
          f"#define AL_NUM_SCHEMES {len(schemes_meta)}",
          "#undef AL_INLINE",
          "#endif",
          ""]
    path.write_text("\n".join(L))
    print(f"wrote {path} ({len(schemes_meta)} schemes)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", type=Path, default=Path(__file__).resolve().parent)
    ap.add_argument("--g", type=int, default=SELECTED_G)
    ap.add_argument("--degree", type=int, default=SELECTED_DEGREE)
    ap.add_argument("--sweep", action="store_true", help="run full g=0..7 comparison sweep")
    ap.add_argument("--explore", type=Path, default=None,
                    help="emit a combined benchmarking header at this path and exit")
    args = ap.parse_args()
    if args.explore is not None:
        emit_explore(args.explore)
        return
    out_dir = args.out_dir
    log_dir = out_dir / "log"
    log_dir.mkdir(parents=True, exist_ok=True)

    selected = audit(args.g, args.degree)
    if selected.bits < TARGET_BITS:
        raise SystemExit(f"selected only reaches 2^-{mp.nstr(selected.bits,6)} (< 2^-57)")
    if not selected.fits_i64:
        raise SystemExit("selected coefficients overflow int64")
    if not selected.no_i128_overflow:
        raise SystemExit("selected scheme overflows the 128-bit product")
    if not selected.pins_zero:
        raise SystemExit("ApproxLog(0,1) != 0")

    sweep = sweep_table() if args.sweep else [selected]
    emit_header(out_dir / "approx_log_poly.h", selected)
    emit_log(log_dir / "approx_log_poly_generation.txt", selected, sweep)


if __name__ == "__main__":
    main()
