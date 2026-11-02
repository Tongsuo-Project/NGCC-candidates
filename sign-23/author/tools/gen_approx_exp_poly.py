#!/usr/bin/env python3
"""Generate and audit the branchless accept-branch polynomial for SHUTTLE.

We need exp(-N/(2*825^2)) for N=y*(y+512*x), x in [0,36], y in
[0,255].  The runtime implementation has no secret-dependent branch, no
runtime division, and no lookup table.  It evaluates a small Taylor polynomial
for exp(-a*s) and then squares a fixed number of times:

    s = N / 2^23
    exp(-N/(2*r^2)) = exp(-a_t*s)^(2^t)
    a_t = 2^23 / ((2^t)*2*r^2)

The script searches the useful t,d grid under the exact same fixed-point
semantics used by the generated C header, emits the selected scheme, and logs a
comparison table for auditing.
"""
import argparse
from dataclasses import dataclass
from pathlib import Path

import mpmath as mp

mp.mp.dps = 140
R = 825
K = 256
X_MAX = 36
Y_MAX = 255
SCALE = 1 << 64
S_BITS = 23
TARGET_BITS = mp.mpf(53)
SELECTED_SQUARINGS = 7
SELECTED_DEGREE = 8

POINTS = [(x, y, y * (y + 2 * K * x)) for x in range(X_MAX + 1) for y in range(Y_MAX + 1)]


@dataclass
class AuditResult:
    squarings: int
    degree: int
    precision_bits: mp.mpf
    max_rel: mp.mpf
    worst_x: int
    worst_y: int
    max_coeff_bits: int
    coeffs_fit_i64: bool

    @property
    def split(self) -> int:
        return 1 << self.squarings

    @property
    def multiplies(self) -> int:
        return self.degree + self.squarings


def exponent_from_n(n: int) -> mp.mpf:
    return -mp.mpf(n) / (2 * R * R)


def a_for_t(t: int) -> mp.mpf:
    return mp.mpf(2) ** S_BITS / ((mp.mpf(2) ** t) * 2 * R * R)


def coeffs(t: int, degree: int):
    a = a_for_t(t)
    out = []
    for k in range(degree + 1):
        c = ((-a) ** k) / mp.factorial(k)
        out.append(int(mp.nint(c * SCALE)))
    out[0] = SCALE - 1
    return out


def high64_signed(a: int, b: int) -> int:
    return (a * b) >> 64


def mul_q64_q63_to_q64(a: int, b: int) -> int:
    return high64_signed(a, b) * 2


def mul_q64(a: int, b: int) -> int:
    return (a * b) >> 64


def eval_poly_q64(n: int, cs, squarings: int) -> int:
    s_q63 = n << (63 - S_BITS)
    acc = cs[-1]
    for c in reversed(cs[:-1]):
        acc = c + mul_q64_q63_to_q64(acc, s_q63)
    v = int(acc)
    for _ in range(squarings):
        v = mul_q64(v, v)
    return v


def audit_scheme(t: int, degree: int) -> AuditResult:
    cs = coeffs(t, degree)
    max_rel = mp.mpf(0)
    worst_x = 0
    worst_y = 0
    for x, y, n in POINTS:
        got = mp.mpf(eval_poly_q64(n, cs, t)) / SCALE
        ref = mp.exp(exponent_from_n(n))
        rel = abs(got - ref) / ref
        if rel > max_rel:
            max_rel = rel
            worst_x = x
            worst_y = y
    max_coeff_bits = max(abs(c).bit_length() for c in cs[1:]) if degree else 0
    coeffs_fit_i64 = all(-(1 << 63) <= c <= (1 << 63) - 1 for c in cs[1:])
    return AuditResult(t, degree, -mp.log(max_rel, 2), max_rel, worst_x, worst_y, max_coeff_bits, coeffs_fit_i64)


def comparison_rows():
    rows = []
    for t in range(0, 10):
        best = None
        for degree in range(1, 40):
            result = audit_scheme(t, degree)
            if result.precision_bits >= TARGET_BITS:
                best = result
                break
        if best is None:
            candidates = [audit_scheme(t, degree) for degree in range(1, 16)]
            best = max(candidates, key=lambda r: r.precision_bits)
        rows.append(best)
    return rows


def write_header(path: Path, cs, t: int, degree: int) -> None:
    lines = []
    lines.append("#ifndef SHUTTLE_APPROX_EXP_POLY_H")
    lines.append("#define SHUTTLE_APPROX_EXP_POLY_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("#if defined(__GNUC__) || defined(__clang__)")
    lines.append("#define SHUTTLE_ALWAYS_INLINE static inline __attribute__((always_inline))")
    lines.append("#else")
    lines.append("#define SHUTTLE_ALWAYS_INLINE static inline")
    lines.append("#endif")
    lines.append("")
    lines.append(f"#define SHUTTLE_EXP_POLY_DEGREE {degree}")
    lines.append(f"#define SHUTTLE_EXP_POLY_SQUARINGS {t}")
    lines.append(f"#define SHUTTLE_EXP_POLY_SPLIT {1 << t}")
    lines.append("#define SHUTTLE_EXP_POLY_X_MAX 36")
    lines.append("#define SHUTTLE_EXP_POLY_Y_MAX 255")
    lines.append("")
    lines.append(f"static const int64_t kShuttleExpPolyCoeff[{degree}] = {{")
    for c in cs[1:]:
        lines.append(f"    INT64_C({c}),")
    lines.append("};")
    lines.append("")
    lines.append("SHUTTLE_ALWAYS_INLINE int64_t shuttle_high64_s128(__int128 a, int64_t b)")
    lines.append("{")
    lines.append("    return (int64_t)((a * (__int128)b) >> 64);")
    lines.append("}")
    lines.append("")
    lines.append("SHUTTLE_ALWAYS_INLINE uint64_t shuttle_high64_u64(uint64_t a, uint64_t b)")
    lines.append("{")
    lines.append("    return (uint64_t)(((__uint128_t)a * (__uint128_t)b) >> 64);")
    lines.append("}")
    lines.append("")
    lines.append("SHUTTLE_ALWAYS_INLINE uint64_t shuttle_exp_accept_poly_q64(int x, int y)")
    lines.append("{")
    lines.append("    uint64_t n = (uint64_t)y * (uint64_t)(y + 512 * x);")
    lines.append("    int64_t s_q63 = (int64_t)(n << 40);")
    lines.append(f"    __int128 acc = kShuttleExpPolyCoeff[{degree - 1}];")
    for idx in range(degree - 2, -1, -1):
        lines.append(f"    acc = (__int128)kShuttleExpPolyCoeff[{idx}] +")
        lines.append("          ((__int128)shuttle_high64_s128(acc, s_q63) * 2);")
    lines.append("    acc = ((__int128)UINT64_MAX) +")
    lines.append("          ((__int128)shuttle_high64_s128(acc, s_q63) * 2);")
    lines.append("    uint64_t v = (uint64_t)acc;")
    for _ in range(t):
        lines.append("    v = shuttle_high64_u64(v, v);")
    lines.append("    return v;")
    lines.append("}")
    lines.append("")
    # canonical PERFORMANCE-OPTIMAL 4-way batched variant
    N = 4
    lines.append("/* PERFORMANCE-OPTIMAL variant (see ApproxExp.tex, Table tab:exp-batch):")
    lines.append("   4-way batched, ~42 cyc/output vs ~56 for the scalar above.  Four")
    lines.append("   INDEPENDENT exp evaluations share the coefficient table (one load per")
    lines.append("   Horner step) and interleave four Horner + squaring chains to hide the")
    lines.append("   multiply latency; only four accumulators are live.  Same constant-time")
    lines.append("   guarantees as the scalar.  Use when the caller can supply four")
    lines.append("   independent (x,y); otherwise use shuttle_exp_accept_poly_q64. */")
    lines.append("SHUTTLE_ALWAYS_INLINE void shuttle_exp_accept_poly_q64_x4(const int x[4], const int y[4], uint64_t out[4])")
    lines.append("{")
    for n in range(N):
        lines.append(f"    int64_t s{n} = (int64_t)(((uint64_t)y[{n}] * (uint64_t)(y[{n}] + 512 * x[{n}])) << 40);")
    lines.append("    " + " ".join(f"__int128 a{n} = kShuttleExpPolyCoeff[{degree - 1}];" for n in range(N)))
    lines.append(f"    for (int k = {degree - 2}; k >= 0; k--) {{")
    lines.append("        int64_t c = kShuttleExpPolyCoeff[k];")
    for n in range(N):
        lines.append(f"        a{n} = (__int128)c + ((__int128)shuttle_high64_s128(a{n}, s{n}) * 2);")
    lines.append("    }")
    for n in range(N):
        lines.append(f"    a{n} = ((__int128)UINT64_MAX) + ((__int128)shuttle_high64_s128(a{n}, s{n}) * 2);")
    for n in range(N):
        lines.append(f"    uint64_t v{n} = (uint64_t)a{n};")
    lines.append(f"    for (int q = 0; q < {t}; q++) {{")
    for n in range(N):
        lines.append(f"        v{n} = shuttle_high64_u64(v{n}, v{n});")
    lines.append("    }")
    for n in range(N):
        lines.append(f"    out[{n}] = v{n};")
    lines.append("}")
    lines.append("")
    lines.append("#undef SHUTTLE_ALWAYS_INLINE")
    lines.append("")
    lines.append("#endif")
    path.write_text("\n".join(lines) + "\n")


def write_log(path: Path, cs, selected: AuditResult, rows) -> None:
    min_p = mp.mpf(0)
    min_arg = (0, 0)
    for x, y, n in POINTS:
        p = exponent_from_n(n)
        if p < min_p:
            min_p = p
            min_arg = (x, y)
    lines = []
    lines.append("SHUTTLE accept-branch branchless polynomial")
    lines.append(f"r = {R}, k = {K}")
    lines.append("x in {0,...,36}, y in {0,...,255}")
    lines.append("N = y*(y+512*x), p = -N/(2*r^2)")
    lines.append(f"minimum p = {mp.nstr(min_p, 40)} at x={min_arg[0]} y={min_arg[1]}")
    lines.append(f"selected scheme = q(s)^(2^{selected.squarings}) = q(s)^{selected.split}")
    lines.append(f"s = N/2^{S_BITS}, q(s) degree-{selected.degree} Taylor for exp(-a*s)")
    lines.append(f"a = 2^{S_BITS}/({selected.split}*2*r^2) = {mp.nstr(a_for_t(selected.squarings), 40)}")
    lines.append("runtime constraints = no branch in generated approximation body, no division, no table access")
    lines.append("wide multiply use = high-half shifts; Q64*Q64 uses >>64, Q64*Q63 uses high64*2")
    lines.append(f"selected multiply count = degree+squarings = {selected.degree}+{selected.squarings} = {selected.multiplies}")
    lines.append(f"mpmath audit max relative error = {mp.nstr(selected.max_rel, 40)}")
    lines.append(f"mpmath audit precision = {mp.nstr(selected.precision_bits, 30)} bits at x={selected.worst_x} y={selected.worst_y}")
    lines.append(f"max signed coefficient bits = {selected.max_coeff_bits}; int64 fit = {selected.coeffs_fit_i64}")
    lines.append("")
    lines.append("comparison under identical fixed-point semantics:")
    lines.append("split squarings degree total_mul precision_bits worst_x worst_y coeff_bits int64_coeff note")
    for row in rows:
        note = "passes" if row.precision_bits >= TARGET_BITS else "fails"
        if row.squarings == 0:
            note += "; pure polynomial"
        if row.squarings == 4:
            note += "; previous scheme"
        if row.squarings == selected.squarings and row.degree == selected.degree:
            note += "; selected"
        lines.append(
            f"{row.split} {row.squarings} {row.degree} {row.multiplies} "
            f"{mp.nstr(row.precision_bits, 18)} {row.worst_x} {row.worst_y} "
            f"{row.max_coeff_bits} {row.coeffs_fit_i64} {note}"
        )
    lines.append("")
    lines.append("coefficients: c0 is UINT64_MAX, c1..cd are signed Q64")
    lines.append("c0 = UINT64_MAX")
    for i, c in enumerate(cs[1:], 1):
        lines.append(f"c{i} = {c}")
    text = "\n".join(lines) + "\n"
    path.write_text(text)
    print(text, end="")


def emit_exp_nway(cs_tab, t, N):
    """N-way batched accept-poly: shared Taylor coefficients, N interleaved
    (Horner + squaring) dependency chains.  Unlike ApproxLog there is no table
    scan and the coefficients are the SAME for every lane, so only N
    accumulators are live -- batching purely hides the Horner/squaring latency.
    cs_tab = [c1..cd] (c0 is the implicit UINT64_MAX step)."""
    d = len(cs_tab)
    nm = f"_t{t}d{d}"
    L = [f"AL_INLINE void shuttle_exp{nm}_x{N}(const int xx[{N}], const int yy[{N}], uint64_t out[{N}]){{"]
    L.append("    " + " ".join(
        f"int64_t s{n}=(int64_t)(((uint64_t)yy[{n}]*(uint64_t)(yy[{n}]+512*xx[{n}]))<<40);" for n in range(N)))
    L.append("    " + " ".join(f"__int128 a{n}=kExp{nm}[{d-1}];" for n in range(N)))
    L.append(f"    for(int k={d-2};k>=0;k--){{ int64_t c=kExp{nm}[k];")
    L.append("        " + " ".join(f"a{n}=(__int128)c+((__int128)al_exp_hs(a{n},s{n})*2);" for n in range(N)) + " }")
    L.append("    " + " ".join(f"a{n}=((__int128)UINT64_MAX)+((__int128)al_exp_hs(a{n},s{n})*2);" for n in range(N)))
    L.append("    " + " ".join(f"uint64_t v{n}=(uint64_t)a{n};" for n in range(N)))
    L.append(f"    for(int q=0;q<{t};q++){{")
    L.append("        " + " ".join(f"v{n}=al_exp_hu(v{n},v{n});" for n in range(N)))
    L.append("    }")
    L.append("    " + " ".join(f"out[{n}]=v{n};" for n in range(N)))
    L.append("}")
    return L


def find_min_degree(t, target=53.0):
    for d in range(1, 30):
        if audit_scheme(t, d).precision_bits >= target:
            return d
    return None


EXP_EXPLORE_T = [4, 5, 6, 7, 8]   # squaring counts to explore (each at min degree)


def emit_exp_explore(path: Path):
    L = ["#ifndef SHUTTLE_APPROX_EXP_EXPLORE_H", "#define SHUTTLE_APPROX_EXP_EXPLORE_H",
         "#include <stdint.h>",
         "#if defined(__GNUC__) || defined(__clang__)",
         "#define AL_INLINE static inline __attribute__((always_inline))",
         "#else", "#define AL_INLINE static inline", "#endif", "",
         "AL_INLINE int64_t al_exp_hs(__int128 a, int64_t b){ return (int64_t)((a*(__int128)b)>>64); }",
         "AL_INLINE uint64_t al_exp_hu(uint64_t a, uint64_t b){ return (uint64_t)(((__uint128_t)a*(__uint128_t)b)>>64); }",
         ""]
    meta = []
    for t in EXP_EXPLORE_T:
        d = find_min_degree(t)
        res = audit_scheme(t, d)
        if not res.coeffs_fit_i64:
            continue
        cs = coeffs(t, d)
        cs_tab = cs[1:]
        nm = f"_t{t}d{d}"
        L += [f"/* t={t} squarings, degree {d}, total mul {t+d}, precision 2^-{mp.nstr(res.precision_bits,5)} */",
              f"static const int64_t kExp{nm}[{d}] = {{ " + ", ".join(f"INT64_C({c})" for c in cs_tab) + " };"]
        for N in (1, 2, 3, 4, 8):
            L += emit_exp_nway(cs_tab, t, N)
        L.append("")
        meta.append((t, d, t + d))
    L += ["typedef struct { int t, degree, total_mul; } exp_scheme_t;",
          "static const exp_scheme_t EXP_SCHEMES[] = {"]
    for t, d, tm in meta:
        L.append(f"    {{{t}, {d}, {tm}}},")
    L += ["};", f"#define EXP_NUM_SCHEMES {len(meta)}", "#undef AL_INLINE", "#endif", ""]
    path.write_text("\n".join(L))
    print(f"wrote {path}: schemes " + ", ".join(f"t{t}d{d}" for t, d, _ in meta))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--squarings", type=int, default=SELECTED_SQUARINGS)
    parser.add_argument("--degree", type=int, default=SELECTED_DEGREE)
    parser.add_argument("--explore", type=Path, default=None,
                        help="emit a combined N-way benchmarking header and exit")
    args = parser.parse_args()
    if args.explore is not None:
        emit_exp_explore(args.explore)
        return
    out_dir = args.out_dir
    log_dir = out_dir / "log"
    log_dir.mkdir(parents=True, exist_ok=True)
    cs = coeffs(args.squarings, args.degree)
    selected = audit_scheme(args.squarings, args.degree)
    rows = comparison_rows()
    if selected.precision_bits < TARGET_BITS:
        raise SystemExit(f"selected scheme only has {selected.precision_bits} bits")
    if not selected.coeffs_fit_i64:
        raise SystemExit("selected coefficients do not fit int64")
    write_header(out_dir / "approx_exp_poly.h", cs, args.squarings, args.degree)
    write_log(log_dir / "approx_exp_poly_generation.txt", cs, selected, rows)


if __name__ == "__main__":
    main()
