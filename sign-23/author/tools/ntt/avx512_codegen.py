#!/usr/bin/env python3
"""Shared full-ZMM AVX-512 code generator for the NTT parameter directories.

The AVX2 generators in the subdirectories keep their original output.  This
module adds an opt-in AVX-512BW path that uses the 32 architectural ZMM
registers to keep a whole n=256 or n=512 polynomial register-resident.
"""

from dataclasses import dataclass
from pathlib import Path


R = 1 << 16
VL = 32


@dataclass(frozen=True)
class Avx512Config:
    q: int
    n: int
    root: int
    brv_width: int
    levels: tuple
    scale_mont: int
    incomplete: bool
    lazy: bool
    basemul_abi: str = ""
    prefix: str = "ntt512"
    signed: bool = False               # SIGNED Kyber-style path (q < 2^15)
    red_v: int = 0                     # centered Barrett red16 mulhw constant
    red_rnd: int = 0                   # centered Barrett red16 mulhrsw rounding constant
    # SHUTTLE namespacing: a per-config symbol prefix ("s256_"/"s512_"/"s1024_")
    # threaded through every PUBLIC AVX-512 symbol so the three vendored configs
    # co-link in one process.  Empty string => the original un-prefixed names
    # (upstream-identical output).  cfg.prefix (the consts token, e.g.
    # "s256_ntt512") MUST already carry sym_prefix when sym_prefix is non-empty.
    sym_prefix: str = ""


# The complete set of PUBLIC AVX-512 symbols the asm/consts emit and that the
# vendored .S references by name (RIP-relative).  _namespace_files() rewrites
# each whole-word occurrence to sym_prefix+name in BOTH the .S and the consts
# .c, in lock-step, so renaming a const also fixes the asm reference (a missed
# one would silently link the wrong config's table, not error).  The ntt512_*
# consts are ALREADY prefixed via cfg.prefix; this list covers the function
# labels (which double as the .global symbol) and the shuffle index tables.
_NS_FUNCS = ("ntt_avx512", "invntt_tomont_avx512", "pointwise_avx512",
             "basemul_avx512", "nttunpack_avx512")
_NS_SHUFS = tuple(f"shuf{k}_{w}" for k in (16, 8, 4, 2, 1) for w in ("c", "d"))


def _namespace_files(cfg, *paths):
    """Post-process the just-emitted AVX-512 .S/.c files: prefix every public
    function label and shuf table with cfg.sym_prefix (whole-word, so e.g.
    `shuf16_c` is renamed but `shuf16_512` -- a macro name -- is not).  Part of
    the generator, so it survives `make tables` deterministically."""
    if not cfg.sym_prefix:
        return
    import re
    syms = _NS_FUNCS + _NS_SHUFS
    pat = re.compile(r"\b(" + "|".join(re.escape(s) for s in syms) + r")\b")
    for p in paths:
        p = Path(p)
        if not p.exists():
            continue
        txt = p.read_text(encoding="ascii")
        txt = pat.sub(lambda m: cfg.sym_prefix + m.group(1), txt)
        p.write_text(txt, encoding="ascii")


def brv(x, w):
    r = 0
    for i in range(w):
        r |= ((x >> i) & 1) << (w - 1 - i)
    return r


def s16(v):
    v %= R
    return v - R if v >= (R >> 1) else v


def vec_literal(v):
    return "{" + ",".join(str(s16(x)) for x in v) + "}"


def fqmul_factory(q):
    rinv = pow(R, -1, q)

    def fqmul(a, b):
        return (a * b * rinv) % q

    return fqmul


def zeta_table(cfg):
    q = cfg.q
    mont = R % q
    return [pow(cfg.root, brv(k, cfg.brv_width), q) * mont % q
            for k in range(cfg.n)]


def fwd_k_map(cfg):
    out = {}
    k = 1
    for length in cfg.levels:
        s = 0
        while s < cfg.n:
            out[(length, s)] = k
            k += 1
            s += 2 * length
    return out


def make_zeta_of(cfg):
    zetas = zeta_table(cfg)
    fmap = fwd_k_map(cfg)

    def zeta_of(length, coeff_low):
        start = (coeff_low // (2 * length)) * (2 * length)
        return zetas[fmap[(length, start)]]

    return zeta_of


def shuffle_pair(a, b, k):
    c = []
    d = []
    for base in range(0, VL, 2 * k):
        c.extend(a[base:base + k])
        c.extend(b[base:base + k])
        d.extend(a[base + k:base + 2 * k])
        d.extend(b[base + k:base + 2 * k])
    assert len(c) == VL and len(d) == VL
    return c, d


def shuffle_indices(k, which):
    a = list(range(VL))
    b = list(range(VL, 2 * VL))
    c, d = shuffle_pair(a, b, k)
    return c if which == "c" else d


def build_prog(cfg):
    regs = cfg.n // VL
    idx = [[VL * r + l for l in range(VL)] for r in range(regs)]
    zeta_of = make_zeta_of(cfg)
    prog = []
    for length in cfg.levels:
        if length >= VL:
            step = length // VL
            for r in range(regs):
                if (r // step) % 2 == 0:
                    p = r + step
                    z = zeta_of(length, idx[r][0])
                    prog.append(("xb", length, r, p, [z] * VL))
        else:
            for m in range(regs // 2):
                r0, r1 = 2 * m, 2 * m + 1
                c, d = shuffle_pair(idx[r0], idx[r1], length)
                z = [zeta_of(length, c[l]) for l in range(VL)]
                prog.append(("sh", length, r0, r1))
                prog.append(("ib", length, r0, r1, z))
                idx[r0], idx[r1] = c, d
    return prog, idx


def zinv_mont(cfg, z):
    q = cfg.q
    rinv = pow(R, -1, q)
    plain = z * rinv % q
    return pow(plain, -1, q) * (R % q) % q


def make_pair_table(cfg, zvecs):
    qinv = pow(cfg.q, -1, R)
    out = []
    for zvec in zvecs:
        out.append([(z * qinv) % R for z in zvec])
        out.append(list(zvec))
    return out


def collect_zvecs_forward(prog):
    return [op[-1] for op in prog if op[0] in ("xb", "ib")]


def collect_zvecs_inverse(cfg, prog):
    out = []
    for op in reversed(prog):
        if op[0] in ("xb", "ib"):
            out.append([zinv_mont(cfg, z) for z in op[-1]])
    return out


def model_run(cfg, regs, prog, inverse=False, sched=None):
    q = cfg.q
    fqmul = fqmul_factory(q)
    seq = list(reversed(prog)) if inverse else list(prog)
    si = 0
    for op in seq:
        if op[0] == "sh":
            _, k, r0, r1 = op
            regs[r0], regs[r1] = shuffle_pair(regs[r0], regs[r1], k)
            continue
        _, length, r0, r1, z = op
        if inverse:
            z = [zinv_mont(cfg, x) for x in z]
        use_csub = False
        if cfg.lazy:
            use_csub = sched[si][1]
            si += 1
        a = regs[r0]
        b = regs[r1]
        for l in range(VL):
            if not inverse:
                t = fqmul(b[l] % q, z[l])
                if cfg.lazy and not use_csub:
                    a[l], b[l] = a[l] + t, a[l] + (q - t)
                else:
                    ar = a[l] % q
                    a[l], b[l] = (ar + t) % q, (ar - t) % q
            else:
                if cfg.lazy and not use_csub:
                    s = a[l] + b[l]
                    d = (a[l] % q - b[l] % q) % q
                    a[l], b[l] = s, fqmul(d, z[l])
                else:
                    ar = a[l] % q
                    br = b[l] % q
                    a[l], b[l] = (ar + br) % q, fqmul((ar - br) % q, z[l])
    return regs


def model_ntt(cfg, poly, raw=False, fwd_sched=None):
    prog, _ = build_prog(cfg)
    regs = [poly[VL * r:VL * (r + 1)] for r in range(cfg.n // VL)]
    model_run(cfg, regs, prog, inverse=False, sched=fwd_sched)
    out = [x for reg in regs for x in reg]
    return out if raw else [x % cfg.q for x in out]


def model_intt(cfg, poly, inv_sched=None):
    q = cfg.q
    fqmul = fqmul_factory(q)
    prog, _ = build_prog(cfg)
    regs = [poly[VL * r:VL * (r + 1)] for r in range(cfg.n // VL)]
    model_run(cfg, regs, prog, inverse=True, sched=inv_sched)
    out = [x for reg in regs for x in reg]
    return [fqmul(x % q, cfg.scale_mont) for x in out]


def schoolbook(cfg, a, b):
    q = cfg.q
    n = cfg.n
    c = [0] * n
    for i in range(n):
        ai = a[i]
        for j in range(n):
            k = i + j
            p = ai * b[j] % q
            if k < n:
                c[k] = (c[k] + p) % q
            else:
                c[k - n] = (c[k - n] - p) % q
    return c


def gamma_of(cfg, i):
    assert cfg.incomplete
    q = cfg.q
    return pow(cfg.root, 2 * brv(i, cfg.brv_width) + 1, q)


def basemul_model(cfg, na, nb):
    q = cfg.q
    fqmul = fqmul_factory(q)
    _, idx_after = build_prog(cfg)
    idx_flat = [x for reg in idx_after for x in reg]
    out = [0] * cfg.n
    for i in range(cfg.n // 2):
        a0, a1 = na[2 * i] % q, na[2 * i + 1] % q
        b0, b1 = nb[2 * i] % q, nb[2 * i + 1] % q
        p0 = fqmul(a0, b0)
        p1 = fqmul(a1, b1)
        x0 = fqmul(a0, b1)
        x1 = fqmul(a1, b0)
        assert idx_flat[2 * i] // 2 == idx_flat[2 * i + 1] // 2
        g = gamma_of(cfg, idx_flat[2 * i] // 2)
        out[2 * i] = (p0 + fqmul(p1, g * (R % q) % q)) % q
        out[2 * i + 1] = (x0 + x1) % q
    return out


def pointwise_model(cfg, na, nb):
    q = cfg.q
    fqmul = fqmul_factory(q)
    return [fqmul(na[i] % q, nb[i] % q) for i in range(cfg.n)]


def derive_lazy_schedules(cfg):
    if not cfg.lazy:
        return None, None

    q = cfg.q
    prog, _ = build_prog(cfg)
    regs = [[(0, q - 1) for _ in range(VL)] for _ in range(cfg.n // VL)]
    fwd = []
    for op in prog:
        if op[0] == "sh":
            _, k, r0, r1 = op
            regs[r0], regs[r1] = shuffle_pair(regs[r0], regs[r1], k)
            continue
        _, length, r0, r1, _ = op
        maxa = max(x[1] for x in regs[r0])
        maxb = max(x[1] for x in regs[r1])
        # Lazy CT is legal if a+t fits uint16 and b is a valid montmul input.
        lazy_ok = maxa + q <= R and maxb < R and maxb * (q - 1) < R * q
        csub = not lazy_ok
        fwd.append((length, csub))
        for l in range(VL):
            ba = regs[r0][l][1]
            if csub:
                regs[r0][l] = (0, q - 1)
                regs[r1][l] = (0, q - 1)
            else:
                regs[r0][l] = (0, ba + q)
                regs[r1][l] = (0, ba + q)

    out_bound = max(x[1] for reg in regs for x in reg)
    regs = [[(0, out_bound) for _ in range(VL)] for _ in range(cfg.n // VL)]
    inv = []
    for op in reversed(prog):
        if op[0] == "sh":
            _, k, r0, r1 = op
            regs[r0], regs[r1] = shuffle_pair(regs[r0], regs[r1], k)
            continue
        _, length, r0, r1, _ = op
        maxu = max(x[1] for x in regs[r0])
        maxv = max(x[1] for x in regs[r1])
        d_bound = maxu + q
        lazy_ok = maxu + maxv <= R and d_bound <= R and d_bound * (q - 1) < R * q
        csub = not lazy_ok
        inv.append((length, csub))
        for l in range(VL):
            if csub:
                regs[r0][l] = (0, q - 1)
                regs[r1][l] = (0, q - 1)
            else:
                regs[r0][l] = (0, maxu + maxv)
                regs[r1][l] = (0, q - 1)
    return fwd, inv


ASM_PREAMBLE = r"""/* Auto-generated AVX-512BW full-ZMM NTT.  Edit gen_ntt.py. */
.text
.global ntt_avx512
.global invntt_tomont_avx512
"""


def asm_macros(lazy=False, include_dmul=False):
    csub = r"""
.macro csub512 a
  vpcmpuw $1,%zmm16,\a,%k1
  vpsubw  %zmm16,\a,\a
  vpaddw  %zmm16,\a,\a{%k1}
.endm
"""
    common = r"""
.macro load_consts512 qptr
  vmovdqa64 0(\qptr),%zmm16
  vmovdqa64 64(\qptr),%zmm17
.endm
.macro load_tw512 base,disp
  vmovdqa64 \disp(\base),%zmm18
  vmovdqa64 \disp+64(\base),%zmm19
.endm
.macro montmul512 out,in
  vpmullw  %zmm18,\in,%zmm20
  vpmulhuw %zmm19,\in,\out
  vpmulhuw %zmm16,%zmm20,%zmm20
  vpcmpuw  $1,%zmm20,\out,%k1
  vpsubw   %zmm20,\out,\out
  vpsubw   %zmm17,\out,\out{%k1}
.endm
.macro montmul512_pair out,in,zl,zh
  vpmullw  \zl,\in,%zmm20
  vpmulhuw \zh,\in,\out
  vpmulhuw %zmm16,%zmm20,%zmm20
  vpcmpuw  $1,%zmm20,\out,%k1
  vpsubw   %zmm20,\out,\out
  vpsubw   %zmm17,\out,\out{%k1}
.endm
.macro addmod512 dst,a,t
  vpsubw  \t,%zmm16,%zmm21
  vpcmpuw $1,%zmm21,\a,%k1
  vpsubw  %zmm21,\a,\dst
  vpsubw  %zmm17,\dst,\dst{%k1}
.endm
.macro submod512 dst,a,t
  vpcmpuw $1,\t,\a,%k1
  vpsubw  \t,\a,\dst
  vpsubw  %zmm17,\dst,\dst{%k1}
.endm
.macro ctbf512 a,b
  montmul512 %zmm22,\b
  submod512  \b,\a,%zmm22
  addmod512  \a,\a,%zmm22
.endm
.macro gsbf512 a,b
  submod512  %zmm22,\a,\b
  addmod512  \a,\a,\b
  montmul512 \b,%zmm22
.endm
.macro ctbf512_lazy a,b
  montmul512 %zmm22,\b
  vpsubw  %zmm22,%zmm16,%zmm21
  vpaddw  %zmm21,\a,\b
  vpaddw  %zmm22,\a,\a
.endm
.macro ctbf512_csub a,b
  montmul512 %zmm22,\b
  csub512 \a
  vpsubw  %zmm22,%zmm16,%zmm21
  vpaddw  %zmm21,\a,\b
  csub512 \b
  vpaddw  %zmm22,\a,\a
  csub512 \a
.endm
.macro gsbf512_lazy a,b
  vpsubw  \b,%zmm16,%zmm22
  vpaddw  \a,%zmm22,%zmm22
  vpaddw  \b,\a,\a
  montmul512 \b,%zmm22
.endm
.macro gsbf512_csub a,b
  csub512 \a
  csub512 \b
  vpsubw  \b,%zmm16,%zmm22
  vpaddw  \a,%zmm22,%zmm22
  vpaddw  \b,\a,\a
  csub512 \a
  montmul512 \b,%zmm22
.endm
"""
    dmul = r"""
.macro dmul512 out,x,y
  vpmullw  \x,\y,%zmm20
  vpmullw  %zmm28,%zmm20,%zmm20
  vpmulhuw \x,\y,\out
  vpmulhuw %zmm16,%zmm20,%zmm20
  vpcmpuw  $1,%zmm20,\out,%k1
  vpsubw   %zmm20,\out,\out
  vpsubw   %zmm17,\out,\out{%k1}
.endm
"""
    shuf = ""
    for k in (16, 8, 4, 2, 1):
        shuf += f""".macro shuf{k}_512 r0,r1
  vmovdqa64 \\r0,%zmm31
  vmovdqa64 shuf{k}_c(%rip),%zmm30
  vpermt2w \\r1,%zmm30,%zmm31
  vmovdqa64 shuf{k}_d(%rip),%zmm30
  vpermt2w \\r1,%zmm30,\\r0
  vmovdqa64 \\r0,\\r1
  vmovdqa64 %zmm31,\\r0
.endm
"""
    return common + (csub if lazy else "") + (dmul if include_dmul else "") + shuf


def emit_consts(cfg, prog, idx_after, path):
    q = cfg.q
    qinv = pow(q, -1, R)
    rmq = R - q
    fwd = make_pair_table(cfg, collect_zvecs_forward(prog))
    inv = make_pair_table(cfg, collect_zvecs_inverse(cfg, prog))
    qdata = [[q] * VL, [rmq] * VL]
    scale = make_pair_table(cfg, [[cfg.scale_mont] * VL])
    lines = ["#include <stdint.h>",
             f"/* Auto-generated AVX-512 constants. q={q} n={cfg.n}. */", ""]
    lines.append(f"const int16_t {cfg.prefix}_qdata[64] __attribute__((aligned(64))) = {{")
    lines.append("  " + ",".join(str(s16(x)) for x in qdata[0]) + ",")
    lines.append("  " + ",".join(str(s16(x)) for x in qdata[1]))
    lines.append("};\n")
    lines.append(f"const int16_t {cfg.prefix}_scale[64] __attribute__((aligned(64))) = {{")
    lines.append("  " + ",".join(str(s16(x)) for x in scale[0]) + ",")
    lines.append("  " + ",".join(str(s16(x)) for x in scale[1]))
    lines.append("};\n")
    for k in (16, 8, 4, 2, 1):
        for which in ("c", "d"):
            lines.append(f"const uint16_t shuf{k}_{which}[32] __attribute__((aligned(64))) = "
                         + vec_literal(shuffle_indices(k, which)) + ";\n")
    lines.append(f"__attribute__((aligned(64)))\nconst int16_t {cfg.prefix}_zetas_fwd[{len(fwd)}][32] = {{")
    lines.extend("  " + vec_literal(v) + "," for v in fwd)
    lines.append("};\n")
    lines.append(f"__attribute__((aligned(64)))\nconst int16_t {cfg.prefix}_zetas_inv[{len(inv)}][32] = {{")
    lines.extend("  " + vec_literal(v) + "," for v in inv)
    lines.append("};\n")

    if cfg.incomplete:
        r2 = (R % q) * (R % q) % q
        r2pair = make_pair_table(cfg, [[r2] * VL])
        lines.append(f"const int16_t {cfg.prefix}_qinv[32] __attribute__((aligned(64))) = "
                     + vec_literal([qinv] * VL) + ";\n")
        lines.append(f"const int16_t {cfg.prefix}_r2[64] __attribute__((aligned(64))) = {{")
        lines.append("  " + ",".join(str(s16(x)) for x in r2pair[0]) + ",")
        lines.append("  " + ",".join(str(s16(x)) for x in r2pair[1]))
        lines.append("};\n")
        gammas = []
        for r, reg in enumerate(idx_after):
            gvec = []
            for l, coef in enumerate(reg):
                if l % 2 == 0:
                    assert coef // 2 == reg[l + 1] // 2
                gvec.append(gamma_of(cfg, coef // 2) * (R % q) % q)
            gammas.extend(make_pair_table(cfg, [gvec]))
        lines.append(f"__attribute__((aligned(64)))\nconst int16_t {cfg.prefix}_basemul_gamma[{len(gammas)}][32] = {{")
        lines.extend("  " + vec_literal(v) + "," for v in gammas)
        lines.append("};\n")
    else:
        r2 = (R % q) * (R % q) % q
        r2pair = make_pair_table(cfg, [[r2] * VL])
        lines.append(f"const int16_t {cfg.prefix}_qinv[32] __attribute__((aligned(64))) = "
                     + vec_literal([qinv] * VL) + ";\n")
        lines.append(f"const int16_t {cfg.prefix}_r2[64] __attribute__((aligned(64))) = {{")
        lines.append("  " + ",".join(str(s16(x)) for x in r2pair[0]) + ",")
        lines.append("  " + ",".join(str(s16(x)) for x in r2pair[1]))
        lines.append("};\n")

    Path(path).write_text("\n".join(lines), encoding="ascii")
    return len(fwd), len(inv)


def zmm(r):
    return f"%zmm{r}"


def emit_asm(cfg, prog, path, fwd_sched=None, inv_sched=None):
    regs = cfg.n // VL
    lines = [ASM_PREAMBLE.rstrip()]
    if cfg.incomplete:
        lines.append(".global basemul_avx512")
    else:
        lines.append(".global pointwise_avx512")
    lines.append(asm_macros(cfg.lazy, include_dmul=True))

    def load_all(base="%rdi"):
        return [f"  vmovdqu16 {64*r}({base}),{zmm(r)}" for r in range(regs)]

    def store_all(base="%rdi"):
        return [f"  vmovdqu16 {zmm(r)},{64*r}({base})" for r in range(regs)]

    def load_tw(base, idx):
        disp = idx * 128
        return [f"  vmovdqa64 {disp}({base}),%zmm18",
                f"  vmovdqa64 {disp + 64}({base}),%zmm19"]

    # Forward.
    lines += ["", "ntt_avx512:", "  load_consts512 %rsi"]
    lines += load_all("%rdi")
    ki = 0
    si = 0
    for op in prog:
        if op[0] == "sh":
            _, k, r0, r1 = op
            lines.append(f"  shuf{k}_512 {zmm(r0)},{zmm(r1)}")
        else:
            _, _length, r0, r1, _z = op
            lines += load_tw("%rdx", ki)
            if cfg.lazy:
                csub = fwd_sched[si][1]
                si += 1
                macro = "ctbf512_csub" if csub else "ctbf512_lazy"
            else:
                macro = "ctbf512"
            lines.append(f"  {macro} {zmm(r0)},{zmm(r1)}")
            ki += 1
    lines += store_all("%rdi")
    lines += ["  vzeroupper", "  ret", ""]

    # Inverse.
    lines += ["invntt_tomont_avx512:", "  load_consts512 %rsi"]
    lines += load_all("%rdi")
    ki = 0
    si = 0
    for op in reversed(prog):
        if op[0] == "sh":
            _, k, r0, r1 = op
            lines.append(f"  shuf{k}_512 {zmm(r0)},{zmm(r1)}")
        else:
            _, _length, r0, r1, _z = op
            lines += load_tw("%rdx", ki)
            if cfg.lazy:
                csub = inv_sched[si][1]
                si += 1
                macro = "gsbf512_csub" if csub else "gsbf512_lazy"
            else:
                macro = "gsbf512"
            lines.append(f"  {macro} {zmm(r0)},{zmm(r1)}")
            ki += 1
    lines += ["  vmovdqa64 0(%rcx),%zmm18", "  vmovdqa64 64(%rcx),%zmm19"]
    for r in range(regs):
        if cfg.lazy:
            lines.append(f"  csub512 {zmm(r)}")
        lines.append(f"  montmul512 {zmm(r)},{zmm(r)}")
    lines += store_all("%rdi")
    lines += ["  vzeroupper", "  ret", ""]

    if cfg.incomplete:
        emit_basemul_asm(cfg, lines)
    else:
        emit_pointwise_asm(cfg, lines)

    emit_nttunpack_asm(cfg, prog, lines)

    lines.append('.section .note.GNU-stack,"",@progbits')
    Path(path).write_text("\n".join(lines) + "\n", encoding="ascii")
    return len(lines)


def emit_basemul_asm(cfg, lines):
    regs = cfg.n // VL
    if cfg.basemul_abi == "qdata_second":
        c_reg, q_reg, a_reg, b_reg, g_reg = "%rdi", "%rsi", "%rdx", "%rcx", "%r8"
    else:
        c_reg, a_reg, b_reg, q_reg, g_reg = "%rdi", "%rsi", "%rdx", "%rcx", "%r8"
    lines += ["basemul_avx512:", f"  load_consts512 {q_reg}",
              f"  vmovdqa64 {cfg.prefix}_qinv(%rip),%zmm28",
              "  mov $0xaaaaaaaa,%eax", "  kmovd %eax,%k3"]
    for r in range(regs):
        lines.append(f"  vmovdqu16 {64*r}({a_reg}),{zmm(r)}")
        lines.append(f"  vmovdqu16 {64*r}({b_reg}),{zmm(8+r)}")
    if cfg.lazy:
        for r in range(regs):
            lines.append(f"  csub512 {zmm(r)}")
            lines.append(f"  csub512 {zmm(8+r)}")
    for r in range(regs):
        lines += ["  vprold $16," + zmm(8 + r) + ",%zmm21",
                  f"  dmul512 %zmm22,{zmm(r)},{zmm(8+r)}",
                  f"  dmul512 %zmm23,{zmm(r)},%zmm21",
                  f"  vmovdqa64 {128*r}({g_reg}),%zmm18",
                  f"  vmovdqa64 {128*r+64}({g_reg}),%zmm19",
                  "  montmul512 %zmm24,%zmm22",
                  "  vprold $16,%zmm24,%zmm25",
                  "  addmod512 %zmm24,%zmm22,%zmm25",
                  "  vprold $16,%zmm23,%zmm25",
                  "  addmod512 %zmm23,%zmm23,%zmm25",
                  "  vmovdqa64 %zmm24,%zmm26",
                  "  vmovdqu16 %zmm23,%zmm26{%k3}",
                  f"  vmovdqu16 %zmm26,{64*r}({c_reg})"]
    lines += ["  vzeroupper", "  ret", ""]


def emit_nttunpack_asm(cfg, prog, lines):
    # nttunpack: standard-order int32 samples -> avx512 NTT layout.
    # Kyber/Dilithium-style (poly_nttunpack): replay the forward shuffle network
    # with NO butterflies, so out[s] = src[FINAL_IDX[s]].  Narrows int32 [0,q) ->
    # int16 on load (vpmovdw).  rdi = dst (int16[n]), rsi = src (int32[n]).
    regs = cfg.n // VL
    lines += ["",
              "/* nttunpack: standard-order int32 samples -> avx512 NTT layout.",
              " * Kyber-style forward shuffle ladder, no butterflies; narrows int32 [0,q) -> int16.",
              " *   rdi = dst (int16[n]), rsi = src (int32[n]). */",
              ".global nttunpack_avx512", "nttunpack_avx512:"]
    for r in range(regs):
        so = 128 * r  # 32 int32 per reg (VL coeffs) = 128 bytes; two 16-lane halves
        lines += [f"  vmovdqu32 {so}(%rsi),%zmm20",
                  f"  vpmovdw %zmm20,%ymm{r}",
                  f"  vmovdqu32 {so + 64}(%rsi),%zmm20",
                  f"  vpmovdw %zmm20,%ymm21",
                  f"  vinserti64x4 $1,%ymm21,%zmm{r},%zmm{r}"]
    for op in prog:
        if op[0] == "sh":
            _, k, r0, r1 = op
            lines.append(f"  shuf{k}_512 {zmm(r0)},{zmm(r1)}")
    for r in range(regs):
        lines.append(f"  vmovdqu16 {zmm(r)},{64 * r}(%rdi)")
    lines += ["  vzeroupper", "  ret", ""]


def emit_pointwise_asm(cfg, lines):
    regs = cfg.n // VL
    lines += ["pointwise_avx512:", "  load_consts512 %rcx",
              f"  vmovdqa64 {cfg.prefix}_qinv(%rip),%zmm28"]
    for r in range(regs):
        lines += [f"  vmovdqu16 {64*r}(%rsi),%zmm0",
                  f"  vmovdqu16 {64*r}(%rdx),%zmm1",
                  "  dmul512 %zmm21,%zmm0,%zmm1",
                  f"  vmovdqu16 %zmm21,{64*r}(%rdi)"]
    lines += ["  vzeroupper", "  ret", ""]


def verify(cfg, fwd_sched=None, inv_sched=None):
    import random

    random.seed(20260601 + cfg.q + cfg.n)
    ok = True
    for _ in range(80):
        a = [random.randrange(cfg.q) for _ in range(cfg.n)]
        mont = R % cfg.q
        want = [(x * mont) % cfg.q for x in a]
        if model_intt(cfg, model_ntt(cfg, a, raw=True, fwd_sched=fwd_sched), inv_sched=inv_sched) != want:
            ok = False
            break
    ok2 = True
    for _ in range(24 if cfg.n == 512 else 50):
        a = [random.randrange(cfg.q) for _ in range(cfg.n)]
        b = [random.randrange(cfg.q) for _ in range(cfg.n)]
        na = model_ntt(cfg, a, raw=True, fwd_sched=fwd_sched)
        nb = model_ntt(cfg, b, raw=True, fwd_sched=fwd_sched)
        nc = basemul_model(cfg, na, nb) if cfg.incomplete else pointwise_model(cfg, na, nb)
        if model_intt(cfg, nc, inv_sched=inv_sched) != schoolbook(cfg, a, b):
            ok2 = False
            break
    return ok and ok2, [f"avx512 roundtrip-to-mont: {'PASS' if ok else 'FAIL'}",
                       f"avx512 convolution: {'PASS' if ok2 else 'FAIL'}"]


# ==========================================================================
# BLOCKED full-ZMM path for n > 512.  A whole n-coeff polynomial needs n//32
# ZMM data registers; for n=1024 that is 32, leaving NO room for q/scratch.  So
# (exactly mirroring the AVX2 "blocks of 128" trick at ZMM granularity) we split
# into SUPERBLOCKS of 512 coeffs = 16 ZMM each.  Levels with len >= 512 are
# cross-superblock and go through memory; levels with len < 512 run inside one
# 16-ZMM superblock register-resident with the shuf16/8/4/2/1 network.  This is
# only used by the complete, non-lazy n=1024 config; the n<=512 path above is
# untouched.
# ==========================================================================
SBC = 512                          # coeffs per superblock (= 16 ZMM)


def sb_params(cfg):
    sb = min(cfg.n, SBC)
    regs_sb = sb // VL
    nsb = cfg.n // sb
    cross = [L for L in cfg.levels if L >= sb]            # cross-superblock (decreasing)
    in_levels = tuple(L for L in cfg.levels if L < sb)    # in-superblock
    return sb, regs_sb, nsb, cross, in_levels


def cross_sb_pairs(length, sb, nsb):
    s = length // sb
    return [(b, b + s) for b in range(nsb) if (b // s) % 2 == 0]


def build_prog_sb(cfg, base, regs_count, levels):
    """build_prog restricted to one superblock: regs_count ZMM holding the
    coeffs [base, base+regs_count*VL), only `levels`, zeta_of uses GLOBAL idx."""
    idx = [[base + VL * r + l for l in range(VL)] for r in range(regs_count)]
    zeta_of = make_zeta_of(cfg)
    prog = []
    for length in levels:
        if length >= VL:
            step = length // VL
            for r in range(regs_count):
                if (r // step) % 2 == 0:
                    p = r + step
                    z = zeta_of(length, idx[r][0])
                    prog.append(("xb", length, r, p, [z] * VL))
        else:
            for m in range(regs_count // 2):
                r0, r1 = 2 * m, 2 * m + 1
                c, d = shuffle_pair(idx[r0], idx[r1], length)
                z = [zeta_of(length, c[l]) for l in range(VL)]
                prog.append(("sh", length, r0, r1))
                prog.append(("ib", length, r0, r1, z))
                idx[r0], idx[r1] = c, d
    return prog, idx


def model_ntt_blocked(cfg, poly):
    q = cfg.q
    fqmul = fqmul_factory(q)
    sb, regs_sb, nsb, cross, in_levels = sb_params(cfg)
    zeta_of = make_zeta_of(cfg)
    a = list(poly)
    for L in cross:                                   # cross-superblock CT
        for (b0, b1) in cross_sb_pairs(L, sb, nsb):
            lo = sb * b0
            z = zeta_of(L, lo)
            for off in range(sb):
                j = lo + off; jp = sb * b1 + off
                t = fqmul(a[jp] % q, z)
                a[j], a[jp] = (a[j] + t) % q, (a[j] - t) % q
    for b in range(nsb):                              # in-superblock
        base = sb * b
        prog, _ = build_prog_sb(cfg, base, regs_sb, in_levels)
        regs = [a[base + VL * r: base + VL * (r + 1)] for r in range(regs_sb)]
        model_run(cfg, regs, prog, inverse=False)
        for r in range(regs_sb):
            a[base + VL * r: base + VL * (r + 1)] = regs[r]
    return [x % q for x in a]


def model_intt_blocked(cfg, poly):
    q = cfg.q
    fqmul = fqmul_factory(q)
    sb, regs_sb, nsb, cross, in_levels = sb_params(cfg)
    zeta_of = make_zeta_of(cfg)
    a = list(poly)
    for b in range(nsb):                              # in-superblock inverse
        base = sb * b
        prog, _ = build_prog_sb(cfg, base, regs_sb, in_levels)
        regs = [a[base + VL * r: base + VL * (r + 1)] for r in range(regs_sb)]
        model_run(cfg, regs, prog, inverse=True)
        for r in range(regs_sb):
            a[base + VL * r: base + VL * (r + 1)] = regs[r]
    for L in reversed(cross):                         # cross-superblock GS (reverse)
        for (b0, b1) in cross_sb_pairs(L, sb, nsb):
            lo = sb * b0
            zi = zinv_mont(cfg, zeta_of(L, lo))
            for off in range(sb):
                j = lo + off; jp = sb * b1 + off
                A, B = a[j] % q, a[jp] % q
                a[j], a[jp] = (A + B) % q, fqmul((A - B) % q, zi)
    return [fqmul(x % q, cfg.scale_mont) for x in a]


def verify_blocked(cfg):
    import random
    random.seed(20260601 + cfg.q + cfg.n)
    mont = R % cfg.q
    ok = True
    for _ in range(60):
        a = [random.randrange(cfg.q) for _ in range(cfg.n)]
        if model_intt_blocked(cfg, model_ntt_blocked(cfg, a)) != [(x * mont) % cfg.q for x in a]:
            ok = False; break
    ok2 = True
    for _ in range(16):
        a = [random.randrange(cfg.q) for _ in range(cfg.n)]
        b = [random.randrange(cfg.q) for _ in range(cfg.n)]
        na = model_ntt_blocked(cfg, a)
        nb = model_ntt_blocked(cfg, b)
        nc = pointwise_model(cfg, na, nb)
        if model_intt_blocked(cfg, nc) != schoolbook(cfg, a, b):
            ok2 = False; break
    return ok and ok2, [f"avx512 blocked roundtrip-to-mont: {'PASS' if ok else 'FAIL'}",
                        f"avx512 blocked convolution: {'PASS' if ok2 else 'FAIL'}"]


def emit_consts_blocked(cfg, path):
    q = cfg.q
    qinv = pow(q, -1, R)
    rmq = R - q
    sb, regs_sb, nsb, cross, in_levels = sb_params(cfg)
    zeta_of = make_zeta_of(cfg)
    fwd_z = []
    for L in cross:
        for (b0, b1) in cross_sb_pairs(L, sb, nsb):
            fwd_z.append([zeta_of(L, sb * b0)] * VL)
    for b in range(nsb):
        prog, _ = build_prog_sb(cfg, sb * b, regs_sb, in_levels)
        fwd_z += collect_zvecs_forward(prog)
    inv_z = []
    for b in range(nsb):
        prog, _ = build_prog_sb(cfg, sb * b, regs_sb, in_levels)
        inv_z += collect_zvecs_inverse(cfg, prog)
    for L in reversed(cross):
        for (b0, b1) in cross_sb_pairs(L, sb, nsb):
            inv_z.append([zinv_mont(cfg, zeta_of(L, sb * b0))] * VL)
    fwd = make_pair_table(cfg, fwd_z)
    inv = make_pair_table(cfg, inv_z)
    qdata = [[q] * VL, [rmq] * VL]
    scale = make_pair_table(cfg, [[cfg.scale_mont] * VL])
    r2 = (R % q) * (R % q) % q
    r2pair = make_pair_table(cfg, [[r2] * VL])
    lines = ["#include <stdint.h>",
             f"/* Auto-generated AVX-512 constants (BLOCKED, superblock=512). q={q} n={cfg.n}. */", ""]
    lines.append(f"const int16_t {cfg.prefix}_qdata[64] __attribute__((aligned(64))) = {{")
    lines.append("  " + ",".join(str(s16(x)) for x in qdata[0]) + ",")
    lines.append("  " + ",".join(str(s16(x)) for x in qdata[1]))
    lines.append("};\n")
    lines.append(f"const int16_t {cfg.prefix}_scale[64] __attribute__((aligned(64))) = {{")
    lines.append("  " + ",".join(str(s16(x)) for x in scale[0]) + ",")
    lines.append("  " + ",".join(str(s16(x)) for x in scale[1]))
    lines.append("};\n")
    for k in (16, 8, 4, 2, 1):
        for which in ("c", "d"):
            lines.append(f"const uint16_t shuf{k}_{which}[32] __attribute__((aligned(64))) = "
                         + vec_literal(shuffle_indices(k, which)) + ";\n")
    lines.append(f"__attribute__((aligned(64)))\nconst int16_t {cfg.prefix}_zetas_fwd[{len(fwd)}][32] = {{")
    lines.extend("  " + vec_literal(v) + "," for v in fwd)
    lines.append("};\n")
    lines.append(f"__attribute__((aligned(64)))\nconst int16_t {cfg.prefix}_zetas_inv[{len(inv)}][32] = {{")
    lines.extend("  " + vec_literal(v) + "," for v in inv)
    lines.append("};\n")
    lines.append(f"const int16_t {cfg.prefix}_qinv[32] __attribute__((aligned(64))) = "
                 + vec_literal([qinv] * VL) + ";\n")
    lines.append(f"const int16_t {cfg.prefix}_r2[64] __attribute__((aligned(64))) = {{")
    lines.append("  " + ",".join(str(s16(x)) for x in r2pair[0]) + ",")
    lines.append("  " + ",".join(str(s16(x)) for x in r2pair[1]))
    lines.append("};\n")
    Path(path).write_text("\n".join(lines), encoding="ascii")
    return len(fwd), len(inv)


def emit_asm_blocked(cfg, path):
    sb, regs_sb, nsb, cross, in_levels = sb_params(cfg)
    sb_bytes = sb * 2
    total_regs = cfg.n // VL
    lines = [ASM_PREAMBLE.rstrip(), ".global pointwise_avx512",
             asm_macros(False, include_dmul=True)]

    def load_tw(idx):
        disp = idx * 128
        return [f"  vmovdqa64 {disp}(%rdx),%zmm18",
                f"  vmovdqa64 {disp + 64}(%rdx),%zmm19"]

    # ---- forward ----
    lines += ["", "ntt_avx512:", "  load_consts512 %rsi"]
    ki = 0
    for L in cross:
        lines.append(f"  /* cross-superblock CT len={L} */")
        for (b0, b1) in cross_sb_pairs(L, sb, nsb):
            lines += load_tw(ki); ki += 1
            for r in range(regs_sb):
                o0 = sb_bytes * b0 + 64 * r; o1 = sb_bytes * b1 + 64 * r
                lines += [f"  vmovdqu16 {o0}(%rdi),%zmm0",
                          f"  vmovdqu16 {o1}(%rdi),%zmm1",
                          "  ctbf512 %zmm0,%zmm1",
                          f"  vmovdqu16 %zmm0,{o0}(%rdi)",
                          f"  vmovdqu16 %zmm1,{o1}(%rdi)"]
    for b in range(nsb):
        base = sb_bytes * b
        prog, _ = build_prog_sb(cfg, sb * b, regs_sb, in_levels)
        lines.append(f"  /* superblock {b} (in-register levels) */")
        lines += [f"  vmovdqu16 {base + 64 * r}(%rdi),{zmm(r)}" for r in range(regs_sb)]
        for op in prog:
            if op[0] == "sh":
                _, k, r0, r1 = op
                lines.append(f"  shuf{k}_512 {zmm(r0)},{zmm(r1)}")
            else:
                _, _len, r0, r1, _z = op
                lines += load_tw(ki); ki += 1
                lines.append(f"  ctbf512 {zmm(r0)},{zmm(r1)}")
        lines += [f"  vmovdqu16 {zmm(r)},{base + 64 * r}(%rdi)" for r in range(regs_sb)]
    lines += ["  vzeroupper", "  ret", ""]

    # ---- inverse ----
    lines += ["invntt_tomont_avx512:", "  load_consts512 %rsi"]
    ki = 0
    for b in range(nsb):
        base = sb_bytes * b
        prog, _ = build_prog_sb(cfg, sb * b, regs_sb, in_levels)
        lines.append(f"  /* superblock {b} inverse */")
        lines += [f"  vmovdqu16 {base + 64 * r}(%rdi),{zmm(r)}" for r in range(regs_sb)]
        for op in reversed(prog):
            if op[0] == "sh":
                _, k, r0, r1 = op
                lines.append(f"  shuf{k}_512 {zmm(r0)},{zmm(r1)}")
            else:
                _, _len, r0, r1, _z = op
                lines += load_tw(ki); ki += 1
                lines.append(f"  gsbf512 {zmm(r0)},{zmm(r1)}")
        lines += [f"  vmovdqu16 {zmm(r)},{base + 64 * r}(%rdi)" for r in range(regs_sb)]
    for L in reversed(cross):
        lines.append(f"  /* cross-superblock GS len={L} */")
        for (b0, b1) in cross_sb_pairs(L, sb, nsb):
            lines += load_tw(ki); ki += 1
            for r in range(regs_sb):
                o0 = sb_bytes * b0 + 64 * r; o1 = sb_bytes * b1 + 64 * r
                lines += [f"  vmovdqu16 {o0}(%rdi),%zmm0",
                          f"  vmovdqu16 {o1}(%rdi),%zmm1",
                          "  gsbf512 %zmm0,%zmm1",
                          f"  vmovdqu16 %zmm0,{o0}(%rdi)",
                          f"  vmovdqu16 %zmm1,{o1}(%rdi)"]
    lines += ["  /* final 1/n scale */",
              "  vmovdqa64 0(%rcx),%zmm18", "  vmovdqa64 64(%rcx),%zmm19"]
    for r in range(total_regs):
        lines += [f"  vmovdqu16 {64 * r}(%rdi),%zmm0",
                  "  montmul512 %zmm0,%zmm0",
                  f"  vmovdqu16 %zmm0,{64 * r}(%rdi)"]
    lines += ["  vzeroupper", "  ret", ""]

    # ---- pointwise ----
    lines += ["pointwise_avx512:", "  load_consts512 %rcx",
              f"  vmovdqa64 {cfg.prefix}_qinv(%rip),%zmm28"]
    for r in range(total_regs):
        lines += [f"  vmovdqu16 {64 * r}(%rsi),%zmm0",
                  f"  vmovdqu16 {64 * r}(%rdx),%zmm1",
                  "  dmul512 %zmm21,%zmm0,%zmm1",
                  f"  vmovdqu16 %zmm21,{64 * r}(%rdi)"]
    lines += ["  vzeroupper", "  ret", ""]

    # ---- nttunpack (per superblock: narrow int32->int16, replay in-sb shuffles) ----
    lines += ["",
              "/* nttunpack (blocked): standard-order int32 [0,q) -> avx512 NTT layout.",
              " * cross-superblock levels are butterfly-only (no lane motion), so only the",
              " * in-superblock shuffle ladder is replayed, per superblock.",
              " *   rdi = dst (int16[n]), rsi = src (int32[n]). */",
              ".global nttunpack_avx512", "nttunpack_avx512:"]
    for b in range(nsb):
        for r in range(regs_sb):
            so = (sb * b + VL * r) * 4
            lines += [f"  vmovdqu32 {so}(%rsi),%zmm20",
                      f"  vpmovdw %zmm20,%ymm{r}",
                      f"  vmovdqu32 {so + 64}(%rsi),%zmm20",
                      "  vpmovdw %zmm20,%ymm21",
                      f"  vinserti64x4 $1,%ymm21,%zmm{r},%zmm{r}"]
        prog, _ = build_prog_sb(cfg, sb * b, regs_sb, in_levels)
        for op in prog:
            if op[0] == "sh":
                _, k, r0, r1 = op
                lines.append(f"  shuf{k}_512 {zmm(r0)},{zmm(r1)}")
        for r in range(regs_sb):
            lines.append(f"  vmovdqu16 {zmm(r)},{(sb * b + VL * r) * 2}(%rdi)")
    lines += ["  vzeroupper", "  ret", ""]
    lines.append('.section .note.GNU-stack,"",@progbits')
    Path(path).write_text("\n".join(lines) + "\n", encoding="ascii")
    return len(lines)


def emit_avx512_blocked(cfg, out_dir="."):
    assert not cfg.incomplete and not cfg.lazy, "blocked path supports complete non-lazy only"
    ok, lines = verify_blocked(cfg)
    nf, ni = emit_consts_blocked(cfg, Path(out_dir) / "ntt_consts_avx512.c")
    nl = emit_asm_blocked(cfg, Path(out_dir) / "ntt_avx512.S")
    _namespace_files(cfg, Path(out_dir) / "ntt_avx512.S",
                     Path(out_dir) / "ntt_consts_avx512.c")
    return ok, lines + [
        f"emitted ntt_consts_avx512.c (blocked) fwd_vecs={nf} inv_vecs={ni}",
        f"emitted ntt_avx512.S (blocked, {nl} lines)",
    ]


# ==========================================================================
# SIGNED 16-bit path (q < 2^15): Kyber-style vpmulhw Montgomery + CENTERED
# Barrett red16 + lazy reduction (reduce only every other level).  Only used by
# the complete n=256 q=15361 config (cfg.signed); the unsigned paths above are
# untouched.  Single block (n<=512), so build_prog covers the whole transform.
# ==========================================================================
def _cent(q, x):
    c = x % q
    return c - q if c > q // 2 else c

def _s_mullw(a, b):   return s16((a * b) & 0xFFFF)
def _s_mulhw(a, b):   return (a * b) >> 16
def _s_mulhrsw(a, b): return (a * b + (1 << 14)) >> 15

def _s_montmul(cfg, qinv, inp, zeta):
    zl = s16((zeta * qinv) & 0xFFFF)
    m = _s_mullw(inp, zl)
    return _s_mulhw(zeta, inp) - _s_mulhw(cfg.q, m)

def _s_red16(cfg, r):
    hi = _s_mulhw(cfg.red_v, r)
    q1 = _s_mulhrsw(cfg.red_rnd, hi)
    return s16((r - s16((cfg.q * q1) & 0xFFFF)) & 0xFFFF)

def _signed_consts(cfg):
    HALF = 1 << 15
    MONT_T = 3 * cfg.q // 4
    RB = max(abs(_s_red16(cfg, r)) for r in range(-HALF, HALF))
    return pow(cfg.q, -1, R), HALF, MONT_T, RB

def derive_signed_schedules(cfg):
    q = cfg.q
    qinv, HALF, MONT_T, RB = _signed_consts(cfg)
    prog, _ = build_prog(cfg)
    nreg = cfg.n // VL
    regs = [[(0, q - 1) for _ in range(VL)] for _ in range(nreg)]
    fwd = []
    for op in prog:
        if op[0] == "sh":
            _, k, r0, r1 = op
            regs[r0], regs[r1] = shuffle_pair(regs[r0], regs[r1], k)
            continue
        _, length, r0, r1, _ = op
        maxa = max(x[1] for x in regs[r0])
        red = not (maxa + MONT_T <= HALF - 1)
        fwd.append((length, red))
        for l in range(VL):
            nb = (RB + MONT_T) if red else (regs[r0][l][1] + MONT_T)
            regs[r0][l] = (0, nb); regs[r1][l] = (0, nb)
    out_bound = max(x[1] for reg in regs for x in reg)
    regs = [[(0, out_bound) for _ in range(VL)] for _ in range(nreg)]
    inv = []
    for op in reversed(prog):
        if op[0] == "sh":
            _, k, r0, r1 = op
            regs[r0], regs[r1] = shuffle_pair(regs[r0], regs[r1], k)
            continue
        _, length, r0, r1, _ = op
        maxu = max(x[1] for x in regs[r0]); maxv = max(x[1] for x in regs[r1])
        red = not (maxu + maxv <= HALF - 1)
        inv.append((length, red))
        for l in range(VL):
            sbound = (2 * RB) if red else (regs[r0][l][1] + regs[r1][l][1])
            regs[r0][l] = (0, sbound); regs[r1][l] = (0, MONT_T)
    return fwd, inv

def _signed_zvecs_fwd(cfg, prog):
    return [[_cent(cfg.q, z) for z in op[-1]] for op in prog if op[0] in ("xb", "ib")]

def _signed_zvecs_inv(cfg, prog):
    out = []
    for op in reversed(prog):
        if op[0] in ("xb", "ib"):
            out.append([_cent(cfg.q, zinv_mont(cfg, z)) for z in op[-1]])
    return out

def model_run_signed(cfg, regs, prog, inverse, sched):
    q = cfg.q; qinv = pow(q, -1, R)
    seq = list(reversed(prog)) if inverse else list(prog)
    si = 0
    for op in seq:
        if op[0] == "sh":
            _, k, r0, r1 = op
            regs[r0], regs[r1] = shuffle_pair(regs[r0], regs[r1], k)
            continue
        _, _length, r0, r1, z = op
        zc = ([_cent(q, zinv_mont(cfg, x)) for x in z] if inverse
              else [_cent(q, x) for x in z])
        red = sched[si][1]; si += 1
        a = regs[r0]; b = regs[r1]
        for l in range(VL):
            if not inverse:
                if red: a[l] = _s_red16(cfg, a[l])
                t = _s_montmul(cfg, qinv, b[l], zc[l])
                a[l], b[l] = a[l] + t, a[l] - t
            else:
                if red:
                    a[l] = _s_red16(cfg, a[l]); b[l] = _s_red16(cfg, b[l])
                d = a[l] - b[l]
                a[l], b[l] = a[l] + b[l], _s_montmul(cfg, qinv, d, zc[l])
    return regs

def model_ntt_signed(cfg, poly, fwd_sched):
    prog, _ = build_prog(cfg)
    regs = [list(poly[VL*r:VL*(r+1)]) for r in range(cfg.n // VL)]
    model_run_signed(cfg, regs, prog, False, fwd_sched)
    return [x for reg in regs for x in reg]

def model_intt_signed(cfg, poly, inv_sched):
    q = cfg.q; qinv = pow(q, -1, R)
    prog, _ = build_prog(cfg)
    regs = [list(poly[VL*r:VL*(r+1)]) for r in range(cfg.n // VL)]
    model_run_signed(cfg, regs, prog, True, inv_sched)
    out = [x for reg in regs for x in reg]
    scale = _cent(q, cfg.scale_mont)
    return [_s_montmul(cfg, qinv, _s_red16(cfg, x), scale) % q for x in out]

def _pointwise_signed(cfg, na, nb):
    q = cfg.q; qinv = pow(q, -1, R)
    out = []
    for x, y in zip(na, nb):
        xr = _s_red16(cfg, x); yr = _s_red16(cfg, y)
        lo = _s_mullw(xr, yr); m = _s_mullw(lo, s16(qinv & 0xFFFF))
        out.append(_s_mulhw(xr, yr) - _s_mulhw(q, m))
    return out

def verify_signed(cfg, fwd_sched, inv_sched):
    import random
    random.seed(20260601 + cfg.q + cfg.n)
    mont = R % cfg.q
    ok = True
    for _ in range(80):
        a = [random.randrange(cfg.q) for _ in range(cfg.n)]
        if model_intt_signed(cfg, model_ntt_signed(cfg, a, fwd_sched), inv_sched) != [(x*mont)%cfg.q for x in a]:
            ok = False; break
    ok2 = True
    for _ in range(40):
        a = [random.randrange(cfg.q) for _ in range(cfg.n)]
        b = [random.randrange(cfg.q) for _ in range(cfg.n)]
        na = model_ntt_signed(cfg, a, fwd_sched); nb = model_ntt_signed(cfg, b, fwd_sched)
        nc = _pointwise_signed(cfg, na, nb)
        if model_intt_signed(cfg, nc, inv_sched) != schoolbook(cfg, a, b):
            ok2 = False; break
    return ok and ok2, [f"avx512 signed roundtrip-to-mont: {'PASS' if ok else 'FAIL'}",
                        f"avx512 signed convolution: {'PASS' if ok2 else 'FAIL'}"]

def asm_macros_signed():
    common = r"""
.macro load_consts512 qptr
  vmovdqa64 0(\qptr),%zmm16
  vmovdqa64 64(\qptr),%zmm17
  vmovdqa64 128(\qptr),%zmm29
.endm
.macro montmul512 out,in
  vpmullw  %zmm18,\in,%zmm20
  vpmulhw  %zmm19,\in,\out
  vpmulhw  %zmm16,%zmm20,%zmm20
  vpsubw   %zmm20,\out,\out
.endm
.macro red16_512 a
  vpmulhw   %zmm17,\a,%zmm21
  vpmulhrsw %zmm29,%zmm21,%zmm21
  vpmullw   %zmm16,%zmm21,%zmm21
  vpsubw    %zmm21,\a,\a
.endm
.macro dmul512 out,x,y
  vpmullw  \x,\y,%zmm20
  vpmullw  %zmm28,%zmm20,%zmm20
  vpmulhw  \x,\y,\out
  vpmulhw  %zmm16,%zmm20,%zmm20
  vpsubw   %zmm20,\out,\out
.endm
.macro ctbf512_lazy a,b
  montmul512 %zmm22,\b
  vpsubw  %zmm22,\a,\b
  vpaddw  %zmm22,\a,\a
.endm
.macro ctbf512_red a,b
  red16_512 \a
  montmul512 %zmm22,\b
  vpsubw  %zmm22,\a,\b
  vpaddw  %zmm22,\a,\a
.endm
.macro gsbf512_lazy a,b
  vpsubw  \b,\a,%zmm22
  vpaddw  \b,\a,\a
  montmul512 \b,%zmm22
.endm
.macro gsbf512_red a,b
  red16_512 \a
  red16_512 \b
  vpsubw  \b,\a,%zmm22
  vpaddw  \b,\a,\a
  montmul512 \b,%zmm22
.endm
"""
    shuf = ""
    for k in (16, 8, 4, 2, 1):
        shuf += f""".macro shuf{k}_512 r0,r1
  vmovdqa64 \\r0,%zmm31
  vmovdqa64 shuf{k}_c(%rip),%zmm30
  vpermt2w \\r1,%zmm30,%zmm31
  vmovdqa64 shuf{k}_d(%rip),%zmm30
  vpermt2w \\r1,%zmm30,\\r0
  vmovdqa64 \\r0,\\r1
  vmovdqa64 %zmm31,\\r0
.endm
"""
    return common + shuf

def emit_consts_signed(cfg, prog, path):
    q = cfg.q
    qinv = pow(q, -1, R)
    fwd = make_pair_table(cfg, _signed_zvecs_fwd(cfg, prog))
    inv = make_pair_table(cfg, _signed_zvecs_inv(cfg, prog))
    qdata = [[q] * VL, [cfg.red_v] * VL, [cfg.red_rnd] * VL]
    scale = make_pair_table(cfg, [[_cent(q, cfg.scale_mont)] * VL])
    lines = ["#include <stdint.h>",
             f"/* Auto-generated AVX-512 constants (SIGNED). q={q} n={cfg.n}.\n"
             f"   qdata = {{q, V, RND}} (centered Barrett red16); zetas CENTERED. */", ""]
    lines.append(f"const int16_t {cfg.prefix}_qdata[96] __attribute__((aligned(64))) = {{")
    lines.append("  " + ",".join(str(s16(x)) for x in qdata[0]) + ",")
    lines.append("  " + ",".join(str(s16(x)) for x in qdata[1]) + ",")
    lines.append("  " + ",".join(str(s16(x)) for x in qdata[2]))
    lines.append("};\n")
    lines.append(f"const int16_t {cfg.prefix}_scale[64] __attribute__((aligned(64))) = {{")
    lines.append("  " + ",".join(str(s16(x)) for x in scale[0]) + ",")
    lines.append("  " + ",".join(str(s16(x)) for x in scale[1]))
    lines.append("};\n")
    for k in (16, 8, 4, 2, 1):
        for which in ("c", "d"):
            lines.append(f"const uint16_t shuf{k}_{which}[32] __attribute__((aligned(64))) = "
                         + vec_literal(shuffle_indices(k, which)) + ";\n")
    lines.append(f"__attribute__((aligned(64)))\nconst int16_t {cfg.prefix}_zetas_fwd[{len(fwd)}][32] = {{")
    lines.extend("  " + vec_literal(v) + "," for v in fwd)
    lines.append("};\n")
    lines.append(f"__attribute__((aligned(64)))\nconst int16_t {cfg.prefix}_zetas_inv[{len(inv)}][32] = {{")
    lines.extend("  " + vec_literal(v) + "," for v in inv)
    lines.append("};\n")
    lines.append(f"const int16_t {cfg.prefix}_qinv[32] __attribute__((aligned(64))) = "
                 + vec_literal([qinv] * VL) + ";\n")
    Path(path).write_text("\n".join(lines), encoding="ascii")
    return len(fwd), len(inv)

def emit_asm_signed(cfg, prog, path, fwd_sched, inv_sched):
    regs = cfg.n // VL
    lines = [ASM_PREAMBLE.rstrip(), ".global pointwise_avx512", asm_macros_signed()]

    def load_all(base="%rdi"):
        return [f"  vmovdqu16 {64*r}({base}),{zmm(r)}" for r in range(regs)]
    def store_all(base="%rdi"):
        return [f"  vmovdqu16 {zmm(r)},{64*r}({base})" for r in range(regs)]
    def load_tw(idx):
        disp = idx * 128
        return [f"  vmovdqa64 {disp}(%rdx),%zmm18",
                f"  vmovdqa64 {disp + 64}(%rdx),%zmm19"]

    # forward
    lines += ["", "ntt_avx512:", "  load_consts512 %rsi"]
    lines += load_all("%rdi")
    ki = 0; si = 0
    for op in prog:
        if op[0] == "sh":
            _, k, r0, r1 = op
            lines.append(f"  shuf{k}_512 {zmm(r0)},{zmm(r1)}")
        else:
            _, _length, r0, r1, _z = op
            lines += load_tw(ki); ki += 1
            macro = "ctbf512_red" if fwd_sched[si][1] else "ctbf512_lazy"; si += 1
            lines.append(f"  {macro} {zmm(r0)},{zmm(r1)}")
    lines += store_all("%rdi")
    lines += ["  vzeroupper", "  ret", ""]

    # inverse
    lines += ["invntt_tomont_avx512:", "  load_consts512 %rsi"]
    lines += load_all("%rdi")
    ki = 0; si = 0
    for op in reversed(prog):
        if op[0] == "sh":
            _, k, r0, r1 = op
            lines.append(f"  shuf{k}_512 {zmm(r0)},{zmm(r1)}")
        else:
            _, _length, r0, r1, _z = op
            lines += load_tw(ki); ki += 1
            macro = "gsbf512_red" if inv_sched[si][1] else "gsbf512_lazy"; si += 1
            lines.append(f"  {macro} {zmm(r0)},{zmm(r1)}")
    lines += ["  /* final 1/n scale: red16 then montmul(NINV_TOMONT) */",
              "  vmovdqa64 0(%rcx),%zmm18", "  vmovdqa64 64(%rcx),%zmm19"]
    for r in range(regs):
        lines.append(f"  red16_512 {zmm(r)}")
        lines.append(f"  montmul512 {zmm(r)},{zmm(r)}")
    lines += store_all("%rdi")
    lines += ["  vzeroupper", "  ret", ""]

    # pointwise: red16 inputs then signed data*data Montgomery mul.
    lines += ["pointwise_avx512:", "  load_consts512 %rcx",
              f"  vmovdqa64 {cfg.prefix}_qinv(%rip),%zmm28"]
    for r in range(regs):
        lines += [f"  vmovdqu16 {64*r}(%rsi),%zmm0",
                  f"  vmovdqu16 {64*r}(%rdx),%zmm1",
                  "  red16_512 %zmm0",
                  "  red16_512 %zmm1",
                  "  dmul512 %zmm21,%zmm0,%zmm1",
                  f"  vmovdqu16 %zmm21,{64*r}(%rdi)"]
    lines += ["  vzeroupper", "  ret", ""]

    emit_nttunpack_asm(cfg, prog, lines)
    lines.append('.section .note.GNU-stack,"",@progbits')
    Path(path).write_text("\n".join(lines) + "\n", encoding="ascii")
    return len(lines)

def emit_avx512_signed(cfg, out_dir="."):
    assert not cfg.incomplete, "signed path is complete-NTT only here"
    prog, _ = build_prog(cfg)
    fwd_sched, inv_sched = derive_signed_schedules(cfg)
    ok, lines = verify_signed(cfg, fwd_sched, inv_sched)
    nf, ni = emit_consts_signed(cfg, prog, Path(out_dir) / "ntt_consts_avx512.c")
    nl = emit_asm_signed(cfg, prog, Path(out_dir) / "ntt_avx512.S", fwd_sched, inv_sched)
    _namespace_files(cfg, Path(out_dir) / "ntt_avx512.S",
                     Path(out_dir) / "ntt_consts_avx512.c")
    fred = sorted(set(L for L, r in fwd_sched if r))
    ired = sorted(set(L for L, r in inv_sched if r))
    return ok, lines + [
        f"avx512 signed fwd RED lengths: {fred}",
        f"avx512 signed inv RED lengths: {ired}",
        f"emitted ntt_consts_avx512.c (signed) fwd_vecs={nf} inv_vecs={ni}",
        f"emitted ntt_avx512.S (signed, {nl} lines)",
    ]


def emit_avx512(cfg, out_dir="."):
    if cfg.signed:
        return emit_avx512_signed(cfg, out_dir)
    if cfg.n > SBC:
        return emit_avx512_blocked(cfg, out_dir)
    prog, idx_after = build_prog(cfg)
    fwd_sched, inv_sched = derive_lazy_schedules(cfg)
    ok, lines = verify(cfg, fwd_sched=fwd_sched, inv_sched=inv_sched)
    nf, ni = emit_consts(cfg, prog, idx_after, Path(out_dir) / "ntt_consts_avx512.c")
    nl = emit_asm(cfg, prog, Path(out_dir) / "ntt_avx512.S", fwd_sched, inv_sched)
    _namespace_files(cfg, Path(out_dir) / "ntt_avx512.S",
                     Path(out_dir) / "ntt_consts_avx512.c")
    sched_lines = []
    if cfg.lazy:
        f = sorted(set(length for length, csub in fwd_sched if csub))
        i = sorted(set(length for length, csub in inv_sched if csub))
        sched_lines.append(f"avx512 lazy fwd CSUB lengths: {f}")
        sched_lines.append(f"avx512 lazy inv CSUB lengths: {i}")
    return ok, lines + sched_lines + [
        f"emitted ntt_consts_avx512.c fwd_vecs={nf} inv_vecs={ni}",
        f"emitted ntt_avx512.S ({nl} lines)",
    ]
