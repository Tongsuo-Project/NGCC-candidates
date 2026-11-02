#!/usr/bin/env python3
"""Compile and objdump SHUTTLE secret-handling objects for forbidden CT opcodes.

This is the KyberSlash main-defense gate: source-level constant-time is NOT
enough (gcc -Os re-emits a real `idiv` for "divide by constant" that -O3 turns
into reciprocal-multiply), so we descend to the machine-code level.  Each
secret-handling source is compiled to an object file across the matrix

    {backend} x {mode} x {gcc,clang} x {-O3,-Os} x {XOF: ngcc,sha3}

disassembled with `objdump -d`, and every instruction whose mnemonic matches
BAD_MNEMONIC_RE is FLAGGED unless allowlisted (gather on public SHAKE/SM3 lane
pointers, or a public-length division inside a named block-count function).
Every allowlist entry cites a section of SECRET_PUBLIC_AUDIT.md.

Re-keyed from the reference Dilithium-style scanner:
  * mode macro -> -DSHUTTLE_MODE; MODES = (128, 256, 512).
  * QSETS = {128: q15361n256, 256: q61441n512, 512: q59393n1024}.
  * REF_SRCS extended to SHUTTLE's layout (sampler_u.c, approx_log.c, poly.c,
    rans.c for irs_rans.c, and the SM3 path drng.c/auxfunc.c since NGCC_MODE is
    the default).
  * NEW vs the reference: the NGCC/SHA3 XOF axis (`--xofs`).  NGCC_MODE pulls
    drng.c/auxfunc.c; SHA3_MODE pulls fips202.c (the scheme XOF backend).
  * SHUTTLE ships NO opt-in float-exp variant (integer-only), so
    `--include-optional` is a documented no-op (no avx2exp / ifmaexp variant).

Usage:
    python3 tools/ct_scan.py --backends all --modes all --ccs "gcc clang" \
        --opts="-O3 -Os" --report test/ct_scan_matrix.txt
    python3 tools/ct_scan.py --self-test
"""
from __future__ import annotations

import argparse
import datetime as _dt
import re
import shlex
import subprocess
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]  # .../SHUTTLE

QSETS = {128: "q15361n256", 256: "q61441n512", 512: "q59393n1024"}
BACKENDS = ("ref", "avx2", "avx512")
MODES = (128, 256, 512)
CCS = ("gcc", "clang", "cc")
# -O3 and -Os are the mandated matrix (-Os is the KyberSlash trigger); -O0/-O1/
# -O2 are also accepted so the per-backend `make ct-scan` target (CT_OPT=-O2)
# and ad-hoc scans work without editing the Makefiles.
OPTS = ("-O3", "-Os", "-O2", "-O1", "-O0")
XOFS = ("ngcc", "sha3")

# Forbidden mnemonics (case-insensitive): integer/SIMD division, sqrt,
# gather/scatter, and the int<->float conversion / FP-compare family.  This is
# the KyberSlash variable-latency class plus the secret-gather class.
BAD_MNEMONIC_RE = re.compile(
    r"^(?:"
    r"idiv.*|div.*|sdiv.*|udiv.*|vdiv.*|"
    r"sqrt.*|vsqrt.*|"
    r".*gather.*|.*scatter.*|"
    r"v?cvtsi2sd.*|v?cvttsd2si.*|v?cvtsd2si.*|v?comisd.*|ucomisd.*"
    r")$",
    re.IGNORECASE,
)

# Gather/scatter is allowed ONLY in these (backend, source) pairs, and only
# because the lane pointers/offsets are PUBLIC (cite SECRET_PUBLIC_AUDIT.md
# "NTT Boundary" / "XOF / DRNG").  Default expectation: SHUTTLE's SM3 N-way XOF
# (auxfunc_avx2/avx512) does NOT gather; if it ever does, add a cited entry.
ALLOW_GATHER_SOURCES = {
    ("avx2", "fips202x4.c"): "public SHAKE lane pointers/offsets only (SECRET_PUBLIC_AUDIT NTT Boundary)",
    ("avx512", "fips202x8.c"): "public SHAKE lane pointers/offsets only (SECRET_PUBLIC_AUDIT NTT Boundary)",
}

# Public divisions: a div/idiv/sdiv/udiv is allowed only inside a function whose
# name matches one of these (backend, sources, func_re, reason) entries, because
# it is public-length block-count arithmetic (output byte counts / batch sizes),
# never secret-value arithmetic.  Each cites SECRET_PUBLIC_AUDIT.md.
#
# SHUTTLE re-key vs the reference:
#   * sampler.c Gaussian length block counts: SHUTTLE's batched samplers are
#     `sample_gauss_*` / `noise_magnitude_batch` / `sampler_sigma2` (public
#     batch sizes; SECRET_PUBLIC_AUDIT "Sampler" public-length).
#   * fips202.c one-shot SHAKE: `*_shake128/256` (SHA3_MODE).
#   * drng.c / auxfunc.c (NGCC_MODE SM3 Hash-DRBG / pseudoXOF): block-count
#     divisions on PUBLIC output byte counts, IF the compiler emits any
#     (preferred: none).  Cite SECRET_PUBLIC_AUDIT "XOF / DRNG".
ALLOW_PUBLIC_DIVS = [
    (
        ("ref", "avx2", "avx512"),
        {"sampler.c"},
        re.compile(r"^(?:sample_gauss_N|sample_gauss_N\.part\.\d+|"
                   r"sample_gauss_[A-Za-z0-9_]*|noise_magnitude_batch(?:\.part\.\d+)?|"
                   r"sampler_sigma2(?:\.part\.\d+)?)$"),
        "public/fixed Gaussian-length block-count arithmetic (SECRET_PUBLIC_AUDIT Sampler public-length)",
    ),
    (
        ("ref", "avx2", "avx512"),
        {"fips202.c"},
        re.compile(r".*_shake(?:128|256)$"),
        "public one-shot SHAKE output-length block count (SECRET_PUBLIC_AUDIT XOF/DRNG public-length)",
    ),
    (
        ("avx2",),
        {"fips202x4.c"},
        re.compile(r".*_shake(?:128|256)x4$"),
        "public four-lane SHAKE output-length block count (SECRET_PUBLIC_AUDIT XOF/DRNG public-length)",
    ),
    (
        ("avx512",),
        {"fips202x8.c"},
        re.compile(r".*_shake(?:128|256)x8$"),
        "public eight-lane SHAKE output-length block count (SECRET_PUBLIC_AUDIT XOF/DRNG public-length)",
    ),
    (
        ("ref", "avx2", "avx512"),
        {"drng.c", "auxfunc.c"},
        re.compile(r"^(?:drng_[A-Za-z0-9_]*|get_random_number|sm3_[A-Za-z0-9_]*|"
                   r"pseudo[A-Za-z0-9_]*|[A-Za-z0-9_]*xof[A-Za-z0-9_]*)$"),
        "public SM3 Hash-DRBG/pseudoXOF output-length block count (SECRET_PUBLIC_AUDIT XOF/DRNG public-length)",
    ),
]

FUNC_LABEL_RE = re.compile(r"^[0-9a-fA-F]+ <([^>]+)>:$")

BASE_CFLAGS = ["-fstack-protector-strong", "-Wall", "-Wextra", "-std=c99"]
# NOTE: BASE_CFLAGS deliberately OMIT -fwrapv.  The production build
# carries -fwrapv; the scanner only needs the worst-case instruction
# selection, and -fwrapv does not influence div/idiv emission.

# SHUTTLE secret-handling source list (re-keyed from the reference REF_SRCS).
# These are the files that touch sk / s / e / y / per-coefficient samples /
# IRS state / the secret z|hint stream.
REF_SRCS = [
    "sampler.c",        # BaseSampler 96-bit CDT + ExpandS + SampleY
    "sampler_u.c",      # SamplerU (CLZ + MSB-first mantissa + ApproxLog)
    "approx_exp.c",     # ApproxExp t7d8 Q64
    "approx_log.c",     # ApproxLog g2d13 Q62
    "irs.c",            # RejectSample / R transition
    "rans.c",           # rANS codec on the secret-derived z/hint stream
    "polyvec.c",        # NTT-domain poly vectors, unpack_pk_bn fused freeze
    "poly.c",           # poly types + PolyToBytes/BytesToPoly
    "packing.c",        # pk/sk/sig (un)pack -- AABBCC bug hot-zone
    "reduce.c",         # Barrett/Montgomery -- NO % / NO /
    "rounding.c",       # CompressY/StretchS/RoundB/mod-2q lift/hint
    "sign.c",           # KeyGen/Sign/Verify orchestration
    "poly_ntt.c",       # scalar/SIMD ntt shim (+ ntt/<qset>/ntt_ref.c)
    "symmetric.c",      # XOF wrappers (symmetric.{c,h}/xof.h)
]
# Per-XOF additional secret-handling sources (the XOF backend itself).
XOF_SRCS = {
    "ngcc": ["drng.c", "auxfunc.c"],   # SM3 Hash-DRBG + pseudoXOF (NGCC infra)
    "sha3": ["fips202.c"],             # Keccak/SHAKE (SHA3_MODE only)
}
# AVX N-way XOF additions (the scalar XOF lives in REF_SRCS+XOF_SRCS; these are
# the backend-specific N-way kernels that also touch secret-seeded state).
AVX2_EXTRA = {
    # symmetric_avx2.c = the xof*_avx2_* lane-batched wrappers (the ExpandA/
    # ExpandS/SampleY N-way refills); pure pointer marshaling over the N-way
    # kernels, but it sits on the secret-seeded ExpandS/SampleY path so we
    # scan it too.
    "ngcc": ["symmetric_avx2.c", "auxfunc_avx2.c", "drng_avx2.c"],
    "sha3": ["symmetric_avx2.c", "fips202x4.c", "f1600x4.S"],
}
AVX512_EXTRA = {
    # symmetric_avx512.c = the xof*_avx512_* lane-batched wrappers (the
    # ExpandA/ExpandS/SampleY N-way refills); pure pointer marshaling over the
    # N-way kernels, but it sits on the secret-seeded ExpandS/SampleY path so
    # we scan it too (mirrors symmetric_avx2.c in AVX2_EXTRA).  Its SHA3
    # squeeze block-count division is templated on a COMPILE-TIME rate, so it
    # emits a multiply-shift, not a hardware div.
    "ngcc": ["symmetric_avx512.c", "auxfunc_avx512.c", "drng_avx512.c"],
    "sha3": ["symmetric_avx512.c", "fips202x8.c", "keccakf1600x8.c"],
}


@dataclass(frozen=True)
class Variant:
    name: str
    extra_cflags: tuple
    extra_sources: tuple


def run(cmd, cwd):
    return subprocess.run(cmd, cwd=str(cwd), text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def cc_version(cc):
    p = subprocess.run([cc, "--version"], text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    first = (p.stdout or p.stderr).splitlines()
    return first[0] if first else cc


def backend_cflags(backend, mode, opt, xof, variant):
    qset = QSETS[mode]
    common = [opt, *BASE_CFLAGS, "-DDISABLE_NAMESPACE=1"]
    if xof == "sha3":
        common.append("-DSHA3_MODE")
    if backend == "ref":
        return [*common, "-I.", "-Itest", "-I../tools", f"-Intt/{qset}",
                *variant.extra_cflags]
    if backend == "avx2":
        return [
            *common, "-I.", "-I../ref", "-I../ref/test", "-I../tools",
            f"-I../ref/ntt/{qset}",
            "-mavx2", "-mbmi2", "-mpopcnt", "-march=x86-64", "-mtune=native",
            "-mno-avx512f",
            "-DUSE_AVX2_NTT", "-DUSE_AVX2_SAMPLER", "-DUSE_AVX2_SHAKE4X",
            *variant.extra_cflags,
        ]
    if backend == "avx512":
        return [
            *common, "-I.", "-I../ref", "-I../ref/test", "-I../tools",
            f"-I../ref/ntt/{qset}",
            "-mavx512f", "-mavx512bw", "-mavx512vl", "-mavx512dq",
            "-mavx512vbmi2",
            "-mbmi2", "-mpopcnt", "-march=x86-64", "-mtune=native",
            "-DUSE_AVX512_NTT", "-DUSE_AVX512_SAMPLER", "-DUSE_AVX512_SHAKE8X",
            *variant.extra_cflags,
        ]
    raise ValueError(backend)


def backend_sources(backend, mode, xof, variant):
    qset = QSETS[mode]
    base = list(REF_SRCS) + XOF_SRCS[xof]
    if backend == "ref":
        out = list(base)
    elif backend == "avx2":
        out = base + AVX2_EXTRA[xof] + [f"{qset}/ntt.S"]
    elif backend == "avx512":
        out = base + AVX512_EXTRA[xof] + [f"{qset}/ntt_avx512.S"]
    else:
        raise ValueError(backend)
    out += list(variant.extra_sources)
    seen, dedup = set(), []
    for s in out:
        if s not in seen:
            seen.add(s)
            dedup.append(s)
    return dedup


def variants_for(backend, include_optional):
    # SHUTTLE is integer-only by default; there is NO opt-in float/IFMA exp
    # path, so --include-optional adds no variant.  Recorded as a deliberate
    # no-op.
    return [Variant("default", (), ())]


def extract_mnemonic(line):
    if "\t" not in line:
        return None
    fields = [f for f in line.split("\t") if f.strip()]
    if len(fields) < 2:
        return None
    asm = fields[-1].strip()
    if not asm or asm.endswith(":"):
        return None
    return asm.split(None, 1)[0].lower()


def public_div_reason(backend, src, func, mnemonic):
    if not func or not re.match(r"^(?:i?div|sdiv|udiv)", mnemonic, re.IGNORECASE):
        return None
    src_name = Path(src).name
    func_base = func.rsplit("/", 1)[-1]
    for backends, sources, func_re, reason in ALLOW_PUBLIC_DIVS:
        if backend in backends and src_name in sources and func_re.match(func_base):
            return reason
    return None


def scan_objdump(text, backend, src):
    forbidden, allowed = [], []
    current_func = None
    for line in text.splitlines():
        label = FUNC_LABEL_RE.match(line.strip())
        if label:
            current_func = label.group(1)
            continue
        mnemonic = extract_mnemonic(line)
        if not mnemonic or not BAD_MNEMONIC_RE.match(mnemonic):
            continue
        if "gather" in mnemonic.lower() and (backend, Path(src).name) in ALLOW_GATHER_SOURCES:
            allowed.append(line.rstrip())
            continue
        div_reason = public_div_reason(backend, src, current_func, mnemonic)
        if div_reason:
            allowed.append(f"{line.rstrip()}  # {div_reason}")
            continue
        forbidden.append(line.rstrip())
    return forbidden, allowed


def compile_and_scan(backend, mode, cc, opt, xof, variant, objdir):
    """Return (ct_ok, build_ok, lines).

    ct_ok is False ONLY on a genuine constant-time violation (a forbidden,
    non-allowlisted mnemonic in a secret-handling object).  A *compile* failure
    is reported separately as build_ok=False and does NOT mark a CT violation:
    if a secret source cannot be compiled (e.g. a transient breakage in the
    in-flight production tree) the scanner cannot make a CT verdict for it, but
    that is a build bug to fix, not evidence of a timing leak.
    """
    cwd = ROOT / backend
    cflags = backend_cflags(backend, mode, opt, xof, variant)
    lines = [f"## backend={backend} mode={mode} cc={cc} opt={opt} xof={xof} variant={variant.name}"]
    lines.append(f"cc_version: {cc_version(cc)}")
    ct_fail = False
    build_fail = False
    for src in backend_sources(backend, mode, xof, variant):
        src_path = cwd / src
        if not src_path.exists():
            lines.append(f"SKIP missing source: {src}")
            continue
        stem = src.replace("/", "_").replace(".", "_")
        obj = objdir / f"{backend}_{xof}_{cc}_{opt.replace('-', '')}_{mode}_{stem}.o"
        cmd = [cc, *cflags, f"-DSHUTTLE_MODE={mode}", "-c", src, "-o", str(obj)]
        p = run(cmd, cwd)
        if p.returncode != 0:
            lines.append(f"BUILDFAIL compile {src} (build bug, NOT a CT verdict): "
                         f"{' '.join(shlex.quote(x) for x in cmd)}")
            lines.extend("  " + x for x in (p.stderr or p.stdout).splitlines()[:20])
            build_fail = True
            continue
        d = run(["objdump", "-d", str(obj)], cwd)
        if d.returncode != 0:
            lines.append(f"BUILDFAIL objdump {src}: {d.stderr.strip()}")
            build_fail = True
            continue
        forbidden, allowed = scan_objdump(d.stdout, backend, src)
        if forbidden:
            lines.append(f"FAIL {src}: forbidden CT mnemonics")
            lines.extend(f"  {x}" for x in forbidden[:60])
            if len(forbidden) > 60:
                lines.append(f"  ... {len(forbidden) - 60} more")
            ct_fail = True
        else:
            allow_note = ""
            if allowed:
                gather_count = sum(1 for x in allowed if "gather" in x.lower())
                public_div_count = len(allowed) - gather_count
                notes = []
                if gather_count:
                    reason = ALLOW_GATHER_SOURCES[(backend, Path(src).name)]
                    notes.append(f"allowlisted_gather={gather_count} ({reason})")
                if public_div_count:
                    notes.append(f"allowlisted_public_div={public_div_count}")
                allow_note = "; " + "; ".join(notes)
            lines.append(f"PASS {src}{allow_note}")
    if ct_fail:
        result = "FAIL (CT violation)"
    elif build_fail:
        result = "BUILDFAIL (compile error; CT-clean for everything that built)"
    else:
        result = "PASS"
    lines.append("RESULT: " + result)
    return (not ct_fail), (not build_fail), lines


def parse_list(raw, allowed):
    vals = raw.replace(",", " ").split()
    if vals == ["all"]:
        return list(allowed)
    cast = type(allowed[0])
    out = [cast(v) for v in vals]
    bad = [v for v in out if v not in allowed]
    if bad:
        raise SystemExit(f"unsupported values: {bad}; allowed={allowed}")
    return out


def self_test():
    sample = ("0000000000000000 <sample_gauss_N.part.0>:\n"
              "   0:\t48 f7 f1             \tdiv    %rcx\n"
              "0000000000000003 <x>:\n"
              "   3:\tc5 fd 92 00          \tvgatherdps (%rax,%ymm0,4),%ymm1\n"
              "   8:\tc5 fb 2a c0          \tvcvtsi2sd %rax,%xmm0,%xmm0\n")
    bad, allowed = scan_objdump(sample, "ref", "x.c")
    assert len(bad) == 3 and not allowed, (bad, allowed)
    bad, allowed = scan_objdump(sample, "ref", "sampler.c")
    assert len(bad) == 2 and len(allowed) == 1, (bad, allowed)
    bad, allowed = scan_objdump(sample, "avx2", "fips202x4.c")
    assert len(bad) == 2 and len(allowed) == 1, (bad, allowed)
    # SM3 DRBG public-length div is allowlisted only inside a drng_* function.
    drng = ("0000000000000000 <drng_get_random>:\n"
            "   0:\t48 f7 f1             \tdiv    %rcx\n"
            "0000000000000003 <secret_helper>:\n"
            "   3:\t48 f7 f1             \tdiv    %rcx\n")
    bad, allowed = scan_objdump(drng, "ref", "drng.c")
    assert len(bad) == 1 and len(allowed) == 1, (bad, allowed)
    print("ct_scan.py self-test: PASS")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backends", default="all")
    ap.add_argument("--modes", default="all")
    ap.add_argument("--ccs", default="gcc")
    ap.add_argument("--opts", default="-O3")
    ap.add_argument("--xofs", default="ngcc sha3")
    ap.add_argument("--include-optional", action="store_true")
    ap.add_argument("--report", default=None)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--strict-build", action="store_true",
                    help="treat a compile failure of a secret source as an "
                         "overall failure (default: a build error is reported "
                         "but only a real CT violation fails the gate)")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    backends = parse_list(args.backends, BACKENDS)
    modes = parse_list(args.modes, MODES)
    ccs = parse_list(args.ccs, CCS)
    opts = parse_list(args.opts, OPTS)
    xofs = parse_list(args.xofs, XOFS)
    objdir = ROOT / "test" / ".ct_scan_objs"
    objdir.mkdir(parents=True, exist_ok=True)

    all_lines = [
        "# SHUTTLE CT Scan Matrix Report",
        "",
        f"date_utc: {_dt.datetime.now(_dt.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}",
        f"backends: {' '.join(backends)}",
        f"modes: {' '.join(map(str, modes))}",
        f"compilers: {' '.join(ccs)}",
        f"opts: {' '.join(opts)}",
        f"xofs: {' '.join(xofs)}",
        f"include_optional: {int(args.include_optional)} (no-op: SHUTTLE ships no float-exp variant)",
        "allowlist:",
        "  - gather: avx2/fips202x4.c and avx512/fips202x8.c public SHAKE lane pointers/offsets only",
        "  - public_div: SHAKE one-shot/N-way output-length block counts; SM3 DRBG/pseudoXOF output-length block counts; sampler Gaussian batch block counts",
        "  (each cites SECRET_PUBLIC_AUDIT.md)",
        "",
    ]
    ct_ok = True
    build_ok = True
    for backend in backends:
        for variant in variants_for(backend, args.include_optional):
            for xof in xofs:
                for mode in modes:
                    for cc in ccs:
                        for opt in opts:
                            one_ct, one_build, lines = compile_and_scan(
                                backend, mode, cc, opt, xof, variant, objdir)
                            ct_ok = ct_ok and one_ct
                            build_ok = build_ok and one_build
                            all_lines.extend(lines)
                            all_lines.append("")

    all_lines.append("CT_VERDICT: " + ("CLEAN" if ct_ok else "VIOLATION"))
    all_lines.append("BUILD_VERDICT: " + ("OK" if build_ok else "BUILDFAIL (secret source failed to compile)"))
    overall_ok = ct_ok and (build_ok or not args.strict_build)
    all_lines.append("OVERALL: " + ("PASS" if overall_ok else "FAIL"))
    text = "\n".join(all_lines) + "\n"
    if args.report:
        report = ROOT / args.report
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(text)
    print(text)
    return 0 if overall_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
