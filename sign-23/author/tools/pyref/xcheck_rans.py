#!/usr/bin/env python3
"""xcheck_rans.py -- byte-exact cross-check of the SHUTTLE Python rANS codec
(rans_ref) against the C shuttle_rans_encode on REAL response vectors (P12,
deliverable 2).

For each set:
  1. Read the committed ref/test/rans_vectors_<set>.txt (model-distributed
     Q0/Qs/h symbols + the recorded golden com produced by tools/rans.py).
  2. Assert rans_ref.encode(symbols) == the recorded golden com  (Python ==
     the P10 golden model).
  3. Build + run rans_oracle.c (the C shuttle_rans_encode) on the SAME
     vector and assert rans_ref.encode == the C output  (Python == C).
  4. Assert decode round-trip + re-encode equality (canonical-decode).

Usage:  python3 xcheck_rans.py
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.normpath(os.path.join(HERE, "..", "..", "ref"))
TOOLS = os.path.normpath(os.path.join(HERE, ".."))

import rans_ref


def read_vector(path):
    q0 = qs = h = None
    comlen = None
    com = None
    for line in open(path):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        key = parts[0]
        if key == "q0":
            q0 = [int(x) for x in parts[1:]]
        elif key == "qs":
            qs = [int(x) for x in parts[1:]]
        elif key == "h":
            h = [int(x) for x in parts[1:]]
        elif key == "comlen":
            comlen = int(parts[1])
        elif key == "com":
            com = bytes(int(x) for x in parts[1:])
    return q0, qs, h, comlen, com


def build_oracle(mode):
    binp = os.path.join(HERE, "rans_oracle_%d" % mode)
    cmd = ["gcc", "-O2", "-std=c99", "-I" + REF, "-I" + TOOLS,
           "-DDISABLE_NAMESPACE=1", "-DSHUTTLE_MODE=%d" % mode,
           os.path.join(HERE, "rans_oracle.c"), os.path.join(REF, "rans.c"),
           "-o", binp]
    subprocess.run(cmd, check=True)
    return binp


def main():
    rc = 0
    for mode in (128, 256, 512):
        vpath = os.path.join(REF, "test", "rans_vectors_%d.txt" % mode)
        q0, qs, h, comlen, golden = read_vector(vpath)
        # (2) Python == golden model
        py_com = rans_ref.encode(mode, q0, qs, h)
        ok_golden = (py_com == golden)
        # (3) Python == C oracle
        binp = build_oracle(mode)
        out = subprocess.run([binp, vpath], capture_output=True, text=True)
        c_com = None
        for line in out.stdout.splitlines():
            if line.startswith("RANSCOM"):
                c_com = bytes.fromhex(line.split()[1])
        ok_c = (py_com == c_com)
        # (4) decode round-trip + re-encode
        d0, ds, dh = rans_ref.decode(mode, py_com, len(q0), len(qs), len(h))
        ok_rt = (d0 == q0 and ds == qs and dh == h)
        ok_re = (rans_ref.encode(mode, d0, ds, dh) == py_com)

        if ok_golden and ok_c and ok_rt and ok_re:
            print("SHUTTLE-%d rans xcheck: rans_ref == golden == C "
                  "shuttle_rans_encode (%d com bytes); decode round-trip + "
                  "re-encode OK" % (mode, len(py_com)))
        else:
            rc = 1
            print("=== SHUTTLE-%d rans xcheck FAIL ===" % mode)
            if not ok_golden:
                print("  BUG: rans_ref != committed golden com")
            if not ok_c:
                print("  BUG: rans_ref != C shuttle_rans_encode")
                print("    C output:", out.stdout[:200])
            if not ok_rt:
                print("  BUG: decode round-trip mismatch")
            if not ok_re:
                print("  BUG: re-encode inequality")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
