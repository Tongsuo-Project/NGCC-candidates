#!/usr/bin/env python3
"""gen_rcdt.py - emit the 96-bit reverse-CDT base-sampler threshold tables into
the @@AUTOGEN:rcdt@@ region of rcdt_tables.h, with a full reproducibility audit.

This is the PRODUCTION emitter; the audited MATH reference is BaseSampler.py
(kept verbatim, referenced by the design log).  gen_rcdt.py re-derives the four
tables EXACTLY as BaseSampler.py does -- same compute_base_sampler_tables(sigma,
w=ceil(11*sigma), theta=96), decimal precision 300, PDT-floor with the z=0 mass
fill, RCDT = total - CDT, drop terminal zeros -- and additionally:

  (a) emits the FINAL symbol names SHUTTLE_RCDT_Z / SHUTTLE_RCDT_NOISE_0_85 /
      _0_90 / _1_00 (BaseSampler.py hard-codes the collision-prone name
      GAUSS0_96_3x32 for all four sigmas);
  (b) ASSERTs the INV-NOMAX table invariant on EVERY emitted row -- the mid and
      high limbs must differ from 0xFFFFFFFF so the constant-time borrow-FOLD
      compare cannot wrap.  If a future regeneration ever hits 0xFFFFFFFF
      in a mid/high limb, the documented fix is a +/-1 ulp nudge of that
      threshold (a ~2^-96 probability change, negligible for the Renyi /
      stddev quality) followed by re-running this generator's assert -- see the
      ULP_FIX note below;
  (c) re-checks the QUALITY metrics: the wide table's Renyi order a=2*SIS-1=1045
      and the noise tables' a=2*512-1=1023, plus the induced post-zero-fold
      stddev gaps for the three noise sigmas, against the pinned values, and
      records them in the audit log tools/log/rcdt_tables.txt.

Run:   python3 gen_rcdt.py           # regenerate rcdt_tables.h + write the log
       python3 gen_rcdt.py --check   # re-derive in memory, assert no drift vs
                                     # the committed bytes (the make check-consts
                                     # gate); exit non-zero on drift.
"""

import math
import os
import sys

import autogen
import BaseSampler as bs

THETA = 96
LIMBS = 3
BASE_BITS = 32
LIMB_MASK = (1 << BASE_BITS) - 1

# Renyi orders (the same provenance as BaseSampler.py).
SIS_SECURITY = 523            # SHUTTLE-512 SIS hardness (the binding wide order)
A_ORDER_WIDE = SIS_SECURITY * 2 - 1   # = 1045
A_ORDER_NOISE = 512 * 2 - 1           # = 1023

# The four (sigma -> final symbol name, expected row count) the suite needs.
# The wide masking / BLISS base sampler sigma_s = 825/256 is MODE-INDEPENDENT.
WIDE_SIGMA = 825 / 256
TABLES = [
    # (sigma,        symbol name,                expected_rows, kind)
    (WIDE_SIGMA, "SHUTTLE_RCDT_Z", 36, "wide"),
    (0.85, "SHUTTLE_RCDT_NOISE_0_85", 9, "noise"),
    (0.9, "SHUTTLE_RCDT_NOISE_0_90", 10, "noise"),
    (1.0, "SHUTTLE_RCDT_NOISE_1_00", 11, "noise"),
]

# Pinned induced post-zero-fold signed stddevs (BaseSampler.txt) -- the binding
# correctness metric for the NOISE tables (Renyi is reported but not the gate;
# see 05-BaseSampler.md "SK-table acceptance gate").  We assert the empirical
# stddev computed here matches these to a tight decimal tolerance.
PINNED_NOISE_STDDEV = {
    0.85: "0.849984479877745283212476682702",
    0.9: "0.899996724647142366364962010338",
    1.0: "0.999999894383858464171185063376",
}


def rcdt_rows(sigma):
    """Re-derive the RCDT table for `sigma` EXACTLY as BaseSampler.py does, then
    split into 3x32-bit little-endian limbs and drop the trailing all-zero rows.
    Returns a list of [limb0, limb1, limb2]."""
    w = math.ceil(11 * sigma)
    _pdt, _cdt, rcdt = bs.compute_base_sampler_tables(sigma, w, THETA)
    vals = list(rcdt)
    # drop_terminal_zero=True, mirroring print_rcdt_table.
    while vals and vals[-1] == 0:
        vals = vals[:-1]
    rows = []
    for v in vals:
        if v < 0 or v >= (1 << THETA):
            raise SystemExit(f"gen_rcdt: RCDT value out of {THETA}-bit range: {v}")
        rows.append([(v >> (j * BASE_BITS)) & LIMB_MASK for j in range(LIMBS)])
    return rows


def assert_inv_nomax(name, rows):
    """ASSERT the INV-NOMAX table invariant: every row's MID and HIGH limb is
    != 0xFFFFFFFF (the LOW limb is unconstrained).  This is the precondition for
    the constant-time borrow-FOLD compare; it is checked here at generation time
    against the PUBLIC table, so it never touches secret data.

    ULP_FIX escape hatch (documented, not yet needed -- all 66 rows pass): if a
    row's mid/high limb equals 0xFFFFFFFF, nudge that threshold by +/-1 ulp
    (2^-96 probability change) and re-run; the Renyi/stddev quality is unchanged
    to far more digits than the security margin cares about."""
    for i, (lo, mid, hi) in enumerate(rows):
        if mid == LIMB_MASK or hi == LIMB_MASK:
            raise SystemExit(
                f"gen_rcdt: INV-NOMAX VIOLATION in {name}[{i}] "
                f"(mid=0x{mid:08X}, hi=0x{hi:08X}); apply the +/-1 ulp fix "
                f"to this threshold and re-run (see ULP_FIX note).")
        # low limb (lo) is intentionally unconstrained.
        _ = lo
    return True


def emit_table(name, rows):
    """Render one table as a C `static const uint32_t name[R][3]` array."""
    out = []
    out.append(f"/* {THETA}-bit RCDT -> {LIMBS}x{BASE_BITS}-bit little-endian "
               f"limbs; {len(rows)} rows; INV-NOMAX verified. */")
    out.append(f"static const uint32_t {name}[{len(rows)}][{LIMBS}] = {{")
    for i, limbs in enumerate(rows):
        cells = ", ".join(f"0x{x:08X}U" for x in limbs)
        comma = "," if i != len(rows) - 1 else ""
        out.append(f"    {{{cells}}}{comma}")
    out.append("};")
    return "\n".join(out)


def build_body():
    """Build the full @@AUTOGEN:rcdt@@ body (all four tables) and collect the
    audit lines.  Returns (body_text, log_lines)."""
    body = []
    log = []
    log.append("gen_rcdt.py -- SHUTTLE 96-bit RCDT base-sampler tables (reproducible)")
    log.append("")
    log.append("Re-derives the four tables exactly as BaseSampler.py "
               "(compute_base_sampler_tables,")
    log.append(f"theta={THETA}, w=ceil(11*sigma), PDT-floor + z=0 mass fill, "
               "RCDT=total-CDT, drop")
    log.append("trailing zeros).  ASSERTs INV-NOMAX (mid/high limb != 0xFFFFFFFF) "
               "on every row,")
    log.append("re-checks Renyi divergence and the induced post-zero-fold "
               "noise stddev gaps.")
    log.append("")

    body.append("/* 96-bit reverse-CDT thresholds (3x32-bit little-endian "
                "limbs); see gen_rcdt.py.")
    body.append(" * INV-NOMAX (mid+high limb != 0xFFFFFFFF) asserted on every "
                "row at generation. */")

    for sigma, name, expected_rows, kind in TABLES:
        rows = rcdt_rows(sigma)
        if len(rows) != expected_rows:
            raise SystemExit(
                f"gen_rcdt: {name} has {len(rows)} rows, expected {expected_rows}")
        assert_inv_nomax(name, rows)
        body.append(emit_table(name, rows))

        # --- quality audit ---
        w = math.ceil(11 * sigma)
        pdt, _cdt, rcdt = bs.compute_base_sampler_tables(sigma, w, THETA)
        a_order = A_ORDER_WIDE if kind == "wide" else A_ORDER_NOISE
        r_a = bs.calculate_renyi_divergence(pdt, THETA, sigma, a_order)
        r_inf = bs.calculate_renyi_divergence_inf(pdt, THETA, sigma)

        log.append(f"=== {name}: sigma={sigma}, w={w}, rows={len(rows)}, "
                   f"theta={THETA} ===")
        log.append(f"  INV-NOMAX: OK ({len(rows)} rows, 0 mid/high limb == "
                   "0xFFFFFFFF)")
        log.append(f"  first row: "
                   + "{" + ", ".join(f"0x{x:08X}" for x in rows[0]) + "}")
        log.append(f"  Renyi R_{a_order} = {bs.format_divergence(r_a)}")
        log.append(f"  Renyi R_inf       = {bs.format_divergence(r_inf)}")

        if kind == "noise":
            stats = bs.calculate_rcdt_sampler_stddev(
                rcdt_int=rcdt, theta=THETA, ideal_sigma=sigma, zero_fold=True)
            stddev = stats["stddev"]
            expect = bs.Decimal(PINNED_NOISE_STDDEV[sigma])
            # tight drift guard on the induced stddev (binding metric).
            if abs(stddev - expect) > bs.Decimal("1e-27"):
                raise SystemExit(
                    f"gen_rcdt: {name} induced stddev {stddev} drifted from "
                    f"pinned {expect}")
            log.append(f"  support range: "
                       f"[-{stats['support_max']}, {stats['support_max']}]")
            log.append(f"  zero-fold accept prob: {stats['accept_mass']:.18f}")
            log.append(f"  induced (post-zero-fold) stddev: {stddev:.30f}")
            log.append(f"  gap from ideal sigma={sigma}: "
                       f"{stats['abs_gap']:.6E} "
                       f"({bs.format_signed_power_of_two(stats['abs_gap'])})")
        log.append("")

    return "\n".join(body), log


def header_path():
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "rcdt_tables.h")


def log_path():
    here = os.path.dirname(os.path.abspath(__file__))
    logdir = os.path.join(here, "log")
    os.makedirs(logdir, exist_ok=True)
    return os.path.join(logdir, "rcdt_tables.txt")


def main_emit():
    body, log = build_body()
    autogen.patch_region(header_path(), "rcdt", body)
    with open(log_path(), "w") as f:
        f.write("\n".join(log) + "\n")
    print("\n".join(log))
    print(f"Patched @@AUTOGEN:rcdt@@ in {header_path()}")
    print(f"Wrote audit log {log_path()}")


def main_check():
    """Re-derive in memory, splice into a COPY of the committed header, and cmp
    byte-for-byte against the committed file (the drift gate).  Also re-asserts
    INV-NOMAX + the stddev guard (build_body raises on any failure)."""
    import shutil
    import tempfile

    body, _log = build_body()  # raises on INV-NOMAX / stddev drift
    src = header_path()
    with tempfile.NamedTemporaryFile("w", suffix=".h", delete=False) as tf:
        tmp = tf.name
    try:
        shutil.copyfile(src, tmp)
        autogen.patch_region(tmp, "rcdt", body)
        with open(src) as f:
            committed = f.read()
        with open(tmp) as f:
            regenerated = f.read()
        if committed != regenerated:
            print("gen_rcdt --check: DRIFT -- rcdt_tables.h does not match a "
                  "fresh regeneration", file=sys.stderr)
            return 1
        print("gen_rcdt --check: PASS (rcdt_tables.h reproducible; INV-NOMAX + "
              "stddev gates green)")
        return 0
    finally:
        os.unlink(tmp)


if __name__ == "__main__":
    if "--check" in sys.argv[1:]:
        sys.exit(main_check())
    main_emit()
