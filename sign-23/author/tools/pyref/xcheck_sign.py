#!/usr/bin/env python3
"""xcheck_sign.py -- end-to-end byte-for-byte cross-check of the SHUTTLE
Python KeyGen / Sign / Verify against the C oracle.

Builds + runs tools/pyref/sign_dump.c per mode (SHA3_MODE by default; also
NGCC_MODE if --ngcc), runs the C ref keygen+sign+verify on a FIXED (xi, msg,
rnd), and asserts Python pk / sk / sig == the C bytes (and that the C verify
ACCEPTS, and that the Python verify accepts its own + the C signature).

Usage:  python3 xcheck_sign.py [--mode 128|256|512|all] [--ngcc] [--raw]
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.normpath(os.path.join(HERE, "..", "..", "ref"))
TOOLS = os.path.normpath(os.path.join(HERE, "..", "..", "tools"))
QSET = {128: "q15361n256", 256: "q61441n512", 512: "q59393n1024"}

from sign_ref import Shuttle

# fixed deterministic inputs (must match sign_dump.c).
SEEDBYTES = {128: 16, 256: 32, 512: 64}


def _inputs(mode):
    sb = SEEDBYTES[mode]
    xi = bytes((0x10 + i) & 0xFF for i in range(sb))
    rnd = bytes((0xA0 + i) & 0xFF for i in range(sb))
    msg = bytes((0x30 + i) & 0xFF for i in range(33))
    return xi, rnd, msg


def build_and_run(mode, ngcc, raw):
    qs = QSET[mode]
    suffix = "%d%s%s" % (mode, "_ngcc" if ngcc else "_sha3",
                         "_raw" if raw else "")
    binp = os.path.join(HERE, "sign_dump_" + suffix)
    srcs = ["sign.c", "polyvec.c", "sampler.c", "sampler_u.c", "irs.c",
            "rounding.c", "packing.c", "poly.c", "poly_ntt.c", "reduce.c",
            "rans.c", "approx_exp.c", "approx_log.c", "symmetric.c",
            "ntt/%s/ntt_ref.c" % qs]
    if ngcc:
        srcs += ["drng.c", "auxfunc.c"]
    else:
        srcs += ["fips202.c"]
    cmd = ["gcc", "-O2", "-std=c99",
           "-I" + REF, "-I" + TOOLS, "-I" + os.path.join(REF, "ntt", qs),
           "-DDISABLE_NAMESPACE=1", "-DSHUTTLE_MODE=%d" % mode]
    if not ngcc:
        cmd += ["-DSHA3_MODE"]
    if raw:
        cmd += ["-DSIG_RAW"]
    cmd += [os.path.join(HERE, "sign_dump.c")]
    cmd += [os.path.join(REF, s) for s in srcs]
    cmd += ["-o", binp]
    subprocess.run(cmd, check=True)
    out = subprocess.run([binp], check=True, capture_output=True, text=True)
    return out.stdout.splitlines()


def parse(lines):
    d = {}
    for line in lines:
        parts = line.split()
        if not parts:
            continue
        if parts[0] in ("PK", "SK", "SIG"):
            d[parts[0]] = bytes.fromhex(parts[1]) if len(parts) > 1 else b""
        elif parts[0] in ("SIGLEN", "VERIFY", "MODE"):
            d[parts[0]] = int(parts[1])
    return d


def check_mode(mode, ngcc, raw):
    lines = build_and_run(mode, ngcc, raw)
    c = parse(lines)
    xi, rnd, msg = _inputs(mode)
    sh = Shuttle(mode, sha3=not ngcc, raw=raw)

    pk, sk = sh.keygen(xi)
    sig = sh.sign(sk, msg, rnd)

    fails = []
    if pk != c["PK"]:
        fails.append(("pk", _fmt(c["PK"]), _fmt(pk)))
    if sk != c["SK"]:
        fails.append(("sk", _fmt(c["SK"]), _fmt(sk)))
    if sig != c["SIG"]:
        fails.append(("sig", _fmt(c["SIG"]), _fmt(sig)))
    # cross-verification: Python verify on its own sig + the C sig must accept;
    # C verify must accept (VERIFY == 0).
    if not sh.verify(pk, msg, sig):
        fails.append(("py-verify(own)", "accept", "reject"))
    if not sh.verify(pk, msg, c["SIG"]):
        fails.append(("py-verify(C-sig)", "accept", "reject"))
    if c.get("VERIFY", -1) != 0:
        fails.append(("c-verify", "0", str(c.get("VERIFY"))))
    # tamper: flip a sig byte -> Python verify must reject.
    bad = bytearray(sig)
    bad[-1] ^= 0x01
    if sh.verify(pk, msg, bytes(bad)):
        fails.append(("py-verify(tamper)", "reject", "accept"))
    return fails


def _fmt(b):
    if not isinstance(b, (bytes, bytearray)):
        return repr(b)
    if len(b) <= 24:
        return b.hex()
    return "%s...%s (len %d)" % (b[:12].hex(), b[-12:].hex(), len(b))


def main():
    modes = [128, 256, 512]
    if "--mode" in sys.argv:
        v = sys.argv[sys.argv.index("--mode") + 1]
        if v != "all":
            modes = [int(v)]
    ngcc = "--ngcc" in sys.argv
    raw = "--raw" in sys.argv
    label = ("NGCC" if ngcc else "SHA3") + ("/RAW" if raw else "/rANS")
    rc = 0
    for m in modes:
        fails = check_mode(m, ngcc, raw)
        if fails:
            rc = 1
            print("=== SHUTTLE-%d sign xcheck [%s]: %d FAIL ===" %
                  (m, label, len(fails)))
            for tag, exp, got in fails:
                print("  BUG  %s  C/expect=%s  PY=%s" % (tag, exp, got))
        else:
            print("SHUTTLE-%d sign xcheck [%s]: pk/sk/sig BYTE-EXACT; "
                  "verify accepts (py own+C-sig, C ref); tamper rejected" %
                  (m, label))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
