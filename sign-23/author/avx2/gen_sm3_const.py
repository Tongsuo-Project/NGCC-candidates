#!/usr/bin/env python3
"""Reproduce and audit the SM3 constants embedded in sm3_const.h.

Every constant used by the vectorised SM3 implementations must be reproducible
from the SM3 specification (GB/T 32905-2016).  This script derives:

  * SM3_IV[8] : the fixed initial digest words.
  * SM3_T[64] : the per-round constants Tj = ROTL32(Tbase, i & 31), with
                Tbase = 0x79CC4519 for rounds 0..15 and 0x7A879D8A for 16..63.
                The reference auxfunc.c computes L_SHIFT(Tbase, i & 0x1F) inside
                the round loop; ROTL32(x, n & 31) is exactly that (and equals the
                identity for n == 0, matching x86's shift-count masking that the
                reference relies on for round 0).

Usage:
  python3 gen_sm3_const.py            # print the C arrays
  python3 gen_sm3_const.py --check    # verify sm3_const.h matches (exit!=0 on diff)
"""
import re
import sys
import os

MASK = 0xFFFFFFFF

IV = [0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
      0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E]


def rotl32(x, n):
    n &= 31
    if n == 0:
        return x & MASK
    return ((x << n) | (x >> (32 - n))) & MASK


def gen_T():
    T = []
    for i in range(64):
        base = 0x79CC4519 if i < 16 else 0x7A879D8A
        T.append(rotl32(base, i & 31))
    return T


def emit(name, vals, perline=4):
    out = [f"static const uint32_t {name} = {{"]
    for r in range(0, len(vals), perline):
        out.append("    " + ", ".join("0x%08X" % v for v in vals[r:r + perline]) + ",")
    out.append("};")
    return "\n".join(out)


def parse_header_array(text, name):
    m = re.search(r"%s\s*=\s*\{([^}]*)\}" % re.escape(name), text, re.S)
    if not m:
        return None
    return [int(tok, 16) for tok in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]


def main():
    T = gen_T()
    if "--check" in sys.argv:
        hdr = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sm3_const.h")
        with open(hdr) as f:
            text = f.read()
        iv = parse_header_array(text, "SM3_IV[8]")
        t = parse_header_array(text, "SM3_T[64]")
        ok = (iv == IV) and (t == T)
        print("SM3_IV match:", iv == IV)
        print("SM3_T  match:", t == T)
        print("OVERALL:", "OK" if ok else "MISMATCH")
        sys.exit(0 if ok else 1)
    print(emit("SM3_IV[8]", IV))
    print()
    print(emit("SM3_T[64]", T))


if __name__ == "__main__":
    main()
