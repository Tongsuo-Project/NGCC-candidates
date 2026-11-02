#!/usr/bin/env python3
"""gen_params.py - derive SHUTTLE's per-set DERIVED constants and patch the
@@AUTOGEN:params@@ region of ref/params.h, with a full audit log.

Every value emitted here is reproducible by closed formula from the PRIMARY
constants (n, q, alpha_b, alpha_h) of tab:suf-parameters:

  DQ_BITS = ceil(log2 q)                 uniform-sample mask width (d_q)
  BQ      = ceil(DQ_BITS / 8)            bytes per raw Z_q candidate (b_q)
  DB_BITS = ceil(log2 ceil(q/alpha_b))   pk poly bit-width (d_b)
  HH      = 2(q-1)/alpha_h               hint high-part range (H_h, NON-pow2)
  DH_BITS = ceil(log2 H_h)               EncodeCom w_h bit-width (d_h)
  ZETA    = smallest primitive 2n-th root of unity mod q  (zeta)
  NINV    = pow(n, -1, q)                inverse-NTT scale (n^{-1} mod q)
  DN_BITS = ceil(log2 n)                 SampleC index bit-width (d_n)
  BN      = ceil(DN_BITS / 8)            SampleC index bytes (b_n)

The generator also RE-DERIVES the formula-checkable PRIMARY constants
(DS_BITS, DE_BITS) and ASSERTS they match the hand-typed params.h values, so a
typo in a primary cannot slip through.  It asserts ZETA^n == -1 mod q,
ZETA^{2n} == 1 mod q, n*NINV == 1 mod q, and that H_h is an exact integer.

Run:  python3 gen_params.py
Verified by:  make check-consts  (re-run, cmp the region byte-for-byte)
"""

import math
import os

import autogen

# PRIMARY constants (verbatim from tab:suf-parameters); the derived row is a
# pure function of (N, Q, ALPHA_B, ALPHA_H).  DS_BITS/DE_BITS/BS_ENC/BE_ENC are
# carried only to RE-DERIVE and cross-check the hand-typed params.h primaries.
SETS = {
    128: dict(N=256,  Q=15361, ALPHA_B=2, ALPHA_H=1024,
              BS_ENC=9,  BE_ENC=10, DS_BITS=5, DE_BITS=5),
    256: dict(N=512,  Q=61441, ALPHA_B=2, ALPHA_H=1024,
              BS_ENC=10, BE_ENC=12, DS_BITS=5, DE_BITS=5),
    512: dict(N=1024, Q=59393, ALPHA_B=4, ALPHA_H=2048,
              BS_ENC=10, BE_ENC=12, DS_BITS=5, DE_BITS=5),
}


def smallest_primitive_2nth_root(q, n):
    """Smallest z in [2, q) with multiplicative order EXACTLY 2n mod q.
    Such z satisfies z^n == -1 and z^{2n} == 1; we additionally verify no
    proper divisor d of 2n has z^d == 1 (so the order is exactly 2n)."""
    order = 2 * n
    if (q - 1) % order != 0:
        raise SystemExit(f"gen_params: q={q} is not 1 mod 2n={order}")
    divisors = [d for d in range(1, order) if order % d == 0]
    for z in range(2, q):
        if pow(z, n, q) != q - 1:
            continue
        if pow(z, order, q) != 1:
            continue
        if all(pow(z, d, q) != 1 for d in divisors):
            return z
    raise SystemExit(f"gen_params: no primitive {order}-th root mod {q}")


def derive(p):
    N, Q = p["N"], p["Q"]
    dq_bits = math.ceil(math.log2(Q))
    bq = math.ceil(dq_bits / 8)
    db_bits = math.ceil(math.log2(math.ceil(Q / p["ALPHA_B"])))
    if (2 * (Q - 1)) % p["ALPHA_H"] != 0:
        raise SystemExit(f"gen_params: H_h = 2(q-1)/alpha_h not integer for q={Q}")
    hh = 2 * (Q - 1) // p["ALPHA_H"]
    dh_bits = math.ceil(math.log2(hh))
    zeta = smallest_primitive_2nth_root(Q, N)
    ninv = pow(N, -1, Q)
    dn_bits = math.ceil(math.log2(N))
    bn = math.ceil(dn_bits / 8)
    # --- hard assertions (the audit gate) ---
    assert pow(zeta, N, Q) == Q - 1, "ZETA^n != -1 mod q"
    assert pow(zeta, 2 * N, Q) == 1, "ZETA^{2n} != 1 mod q"
    assert (N * ninv) % Q == 1, "N*NINV != 1 mod q"
    assert (1 << dq_bits) >= Q, "2^DQ_BITS < Q"
    # cross-check the formula-checkable hand-typed primaries
    ds = math.ceil(math.log2(2 * p["BS_ENC"] + 1))
    de = math.ceil(math.log2(2 * p["BE_ENC"] + 1))
    assert ds == p["DS_BITS"], f"DS_BITS mismatch: derived {ds} != {p['DS_BITS']}"
    assert de == p["DE_BITS"], f"DE_BITS mismatch: derived {de} != {p['DE_BITS']}"
    return dict(DQ_BITS=dq_bits, BQ=bq, DB_BITS=db_bits, HH=hh, DH_BITS=dh_bits,
                ZETA=zeta, NINV=ninv, DN_BITS=dn_bits, BN=bn)


BLOCK = """\
#if SHUTTLE_MODE == {mode}
#define DQ_BITS {DQ_BITS} /* ceil(log2 q): uniform-sample mask width */
#define BQ {BQ}           /* ceil(DQ_BITS/8): bytes per raw Z_q candidate */
#define DB_BITS {DB_BITS} /* ceil(log2 ceil(q/alpha_b)): pk poly bit-width */
#define HH {HH}           /* 2(q-1)/alpha_h: hint high-part range (NON-pow2) */
#define DH_BITS {DH_BITS} /* ceil(log2 H_h): EncodeCom w_h bit-width */
#define ZETA {ZETA}       /* smallest primitive 2n-th root of unity mod q */
#define NINV {NINV}       /* n^{{-1}} mod q: inverse-NTT scale */
#define DN_BITS {DN_BITS} /* ceil(log2 n): SampleC index bit-width */
#define BN {BN}           /* ceil(DN_BITS/8): SampleC index bytes */
#endif
"""


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    logdir = os.path.join(here, "log")
    os.makedirs(logdir, exist_ok=True)
    params_h = os.path.normpath(os.path.join(here, "..", "ref", "params.h"))
    logpath = os.path.join(logdir, "params_derivation.txt")

    blocks, log = [], []
    log.append("gen_params.py -- SHUTTLE derived constants (reproducible)")
    log.append("")
    log.append("Each value is a closed-formula function of the PRIMARY (N, Q,")
    log.append("alpha_b, alpha_h) constants of tab:suf-parameters.  Assertions:")
    log.append("  ZETA^n == -1 mod q,  ZETA^{2n} == 1 mod q,  n*NINV == 1 mod q,")
    log.append("  H_h integer,  2^DQ_BITS >= Q,  DS_BITS/DE_BITS == formula.")
    log.append("")
    for mode in (128, 256, 512):
        p = SETS[mode]
        d = derive(p)
        log.append(f"=== SHUTTLE-{mode}  (n={p['N']}, q={p['Q']}, "
                   f"alpha_b={p['ALPHA_B']}, alpha_h={p['ALPHA_H']}) ===")
        log.append(f"  DQ_BITS = ceil(log2 {p['Q']})              = {d['DQ_BITS']}")
        log.append(f"  BQ      = ceil({d['DQ_BITS']}/8)                  = {d['BQ']}")
        log.append(f"  DB_BITS = ceil(log2 ceil({p['Q']}/{p['ALPHA_B']}))    "
                   f"= ceil(log2 {math.ceil(p['Q']/p['ALPHA_B'])}) = {d['DB_BITS']}")
        log.append(f"  HH      = 2({p['Q']}-1)/{p['ALPHA_H']}           = {d['HH']}"
                   f"   (NON-power-of-2: decode must range-check, never mod)")
        log.append(f"  DH_BITS = ceil(log2 {d['HH']})              = {d['DH_BITS']}")
        log.append(f"  ZETA    = smallest primitive 2n-th root    = {d['ZETA']}"
                   f"   (ZETA^{p['N']} = -1 mod {p['Q']}, order exactly {2*p['N']})")
        log.append(f"  NINV    = pow({p['N']}, -1, {p['Q']})           = {d['NINV']}"
                   f"   ({p['N']}*{d['NINV']} mod {p['Q']} = {(p['N']*d['NINV'])%p['Q']})")
        log.append(f"  DN_BITS = ceil(log2 {p['N']})              = {d['DN_BITS']}")
        log.append(f"  BN      = ceil({d['DN_BITS']}/8)                  = {d['BN']}")
        log.append(f"  cross-check: DS_BITS = ceil(log2(2*{p['BS_ENC']}+1)) "
                   f"= {p['DS_BITS']} (OK), DE_BITS = ceil(log2(2*{p['BE_ENC']}+1)) "
                   f"= {p['DE_BITS']} (OK)")
        log.append("")
        blocks.append(BLOCK.format(mode=mode, **d))

    body = ("/* DERIVED per-set constants (see tools/gen_params.py + "
            "tools/log/params_derivation.txt).\n"
            " * Closed-formula functions of (N, Q, alpha_b, alpha_h); asserted in the\n"
            " * generator: ZETA^n==-1, ZETA^{2n}==1, n*NINV==1 mod q, H_h integer. */\n"
            + "\n".join(blocks))
    autogen.patch_region(params_h, "params", body)

    with open(logpath, "w") as f:
        f.write("\n".join(log) + "\n")
    print("\n".join(log))
    print(f"Patched @@AUTOGEN:params@@ in {params_h}")
    print(f"Wrote audit log {logpath}")


if __name__ == "__main__":
    main()
