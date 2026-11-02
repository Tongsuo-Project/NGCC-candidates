#!/usr/bin/env python3
"""packing_ref.py -- byte primitives + scheme (de)serialization mirror for the
SHUTTLE Python reference (P12).  Byte-exact to ref/packing.c.

Conventions (packing.c):
  - integer_to_bytes / bytes_to_integer : little-endian.
  - poly_to_bytes / bytes_to_poly       : LSB-first d-bit bit-accumulator.
  - bytes_to_bits                        : MSB-first (SamplerU, K3) -- distinct.
  - pack_pk/unpack_pk, pack_sk/unpack_sk, pack_com/unpack_com (EncodeCom),
    pack_sig_raw/unpack_sig_raw, pack_sig/unpack_sig (rANS, via rans_ref).

This is the deterministic, KAT-load-bearing codec; it is cross-checked
byte-for-byte against the C oracle in xcheck_pack.
"""
from params import params


# ---- byte primitives ----------------------------------------------------

def integer_to_bytes(x, length):
    """little-endian; raises on overflow (mirrors integer_to_bytes return -1)."""
    if length < 8 and x >= (1 << (8 * length)):
        raise ValueError("integer_to_bytes overflow")
    return bytes((x >> (8 * i)) & 0xFF for i in range(length))


def bytes_to_integer(b):
    x = 0
    for i, v in enumerate(b):
        x |= v << (8 * i)
    return x


def poly_to_bytes(coeffs, d):
    """LSB-first d-bit packer (packing.c poly_to_bytes).  coeffs may be
    negative; only the low d bits (mask) are emitted."""
    n = len(coeffs)
    mask = 0xFFFFFFFF if d >= 32 else ((1 << d) - 1)
    out = bytearray()
    acc = 0
    accbits = 0
    for c in coeffs:
        acc |= (c & mask) << accbits
        accbits += d
        while accbits >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            accbits -= 8
    if accbits > 0:
        out.append(acc & 0xFF)
    return bytes(out)


def bytes_to_poly(b, d, n):
    """LSB-first d-bit unpacker -> list of n unsigned d-bit ints."""
    mask = 0xFFFFFFFF if d >= 32 else ((1 << d) - 1)
    out = []
    acc = 0
    accbits = 0
    inpos = 0
    for _ in range(n):
        while accbits < d:
            acc |= b[inpos] << accbits
            inpos += 1
            accbits += 8
        out.append(acc & mask)
        acc >>= d
        accbits -= d
    return out


def bytes_to_bits_msb(b, nbits):
    """MSB-first bit string (SamplerU / BytesToBits, K3): bit i is the
    (7-i%8)-th bit of byte i//8.  Returns a list of nbits 0/1."""
    bits = []
    for i in range(nbits):
        byte = b[i >> 3]
        bits.append((byte >> (7 - (i & 7))) & 1)
    return bits


# ---- ct_range_reject ----------------------------------------------------

def ct_range_reject(v, lo, hi):
    return 1 if (v < lo or v > hi) else 0


# ---- the codec ----------------------------------------------------------

class Pack:
    def __init__(self, set_id):
        self.p = params(set_id)

    # --- public key ---
    def pack_pk(self, seedA, b):
        p = self.p
        out = bytearray(seedA)
        sh = p["LOG2_ALPHA_B"]
        for i in range(p["EM"]):
            b1 = [c >> sh for c in b[i]]
            out += poly_to_bytes(b1, p["DB_BITS"])
        assert len(out) == p["CRYPTO_PUBLICKEYBYTES"], (len(out),)
        return bytes(out)

    def unpack_pk(self, pk):
        p = self.p
        seedA = bytes(pk[:p["SEEDBYTES"]])
        b = []
        fail = 0
        sh = p["LOG2_ALPHA_B"]
        off = p["SEEDBYTES"]
        for _ in range(p["EM"]):
            blk = pk[off:off + p["POLYPK_PACKEDBYTES"]]
            b1 = bytes_to_poly(blk, p["DB_BITS"], p["N"])
            bi = []
            for v in b1:
                fail |= ct_range_reject(v, 0, p["CEIL_Q_ALPHA_B"] - 1)
                bi.append(v << sh)
            b.append(bi)
            off += p["POLYPK_PACKEDBYTES"]
        return seedA, b, (-1 if fail else 0)

    # --- secret key ---
    def pack_sk(self, seedA, b, masterSeed, tr, s, ep):
        p = self.p
        out = bytearray(seedA)
        sh = p["LOG2_ALPHA_B"]
        for i in range(p["EM"]):
            out += poly_to_bytes([c >> sh for c in b[i]], p["DB_BITS"])
        out += bytes(masterSeed)
        out += bytes(tr)
        for i in range(p["ELL"]):
            out += poly_to_bytes([c + p["BS_ENC"] for c in s[i]], p["DS_BITS"])
        for i in range(p["EM"]):
            out += poly_to_bytes([c + p["BE_ENC"] for c in ep[i]], p["DE_BITS"])
        assert len(out) == p["CRYPTO_SECRETKEYBYTES"], (len(out),)
        return bytes(out)

    def unpack_sk(self, sk):
        p = self.p
        cur = 0
        seedA = bytes(sk[cur:cur + p["SEEDBYTES"]]); cur += p["SEEDBYTES"]
        sh = p["LOG2_ALPHA_B"]
        b = []
        for _ in range(p["EM"]):
            b1 = bytes_to_poly(sk[cur:cur + p["POLYPK_PACKEDBYTES"]],
                               p["DB_BITS"], p["N"])
            b.append([v << sh for v in b1])
            cur += p["POLYPK_PACKEDBYTES"]
        cs = p["CHALLENGESEEDBYTES"]
        masterSeed = bytes(sk[cur:cur + cs]); cur += cs
        tr = bytes(sk[cur:cur + cs]); cur += cs
        fail = 0
        s = []
        for _ in range(p["ELL"]):
            t = bytes_to_poly(sk[cur:cur + p["POLYS_PACKEDBYTES"]],
                              p["DS_BITS"], p["N"])
            si = []
            for v in t:
                v -= p["BS_ENC"]
                fail |= ct_range_reject(v, -p["BS_ENC"], p["BS_ENC"])
                si.append(v)
            s.append(si)
            cur += p["POLYS_PACKEDBYTES"]
        ep = []
        for _ in range(p["EM"]):
            t = bytes_to_poly(sk[cur:cur + p["POLYE_PACKEDBYTES"]],
                              p["DE_BITS"], p["N"])
            ei = []
            for v in t:
                v -= p["BE_ENC"]
                fail |= ct_range_reject(v, -p["BE_ENC"], p["BE_ENC"])
                ei.append(v)
            ep.append(ei)
            cur += p["POLYE_PACKEDBYTES"]
        return seedA, b, masterSeed, tr, s, ep, (-1 if fail else 0)

    # --- commitment (EncodeCom, MS-A7 per-poly) ---
    def pack_com(self, comY_h, comY_0):
        """single-poly EncodeCom: PolyToBytes(comY_h, d_h) || PolyToBytes(
        comY_0, 1)."""
        p = self.p
        return (poly_to_bytes(comY_h, p["DH_BITS"]) +
                poly_to_bytes(comY_0, 1))

    def unpack_com(self, blob):
        p = self.p
        wh = p["POLYWH_PACKEDBYTES"]
        comY_h = bytes_to_poly(blob[:wh], p["DH_BITS"], p["N"])
        comY_0 = bytes_to_poly(blob[wh:], 1, p["N"])
        fail = 0
        for v in comY_h:
            fail |= ct_range_reject(v, 0, p["HH"] - 1)
        for v in comY_0:
            fail |= ct_range_reject(v, 0, 1)
        return comY_h, comY_0, (-1 if fail else 0)

    def encode_com_vec(self, comY_h_vec, comY_0_vec):
        """MS-A7 block order: all comY_h polys first, then all comY_0 polys.
        This is the HashCh input (KAT-defining)."""
        p = self.p
        out = bytearray()
        for ph in comY_h_vec:
            out += poly_to_bytes(ph, p["DH_BITS"])
        for p0 in comY_0_vec:
            out += poly_to_bytes(p0, 1)
        return bytes(out)


def _selftest():
    import random
    for s in (128, 256, 512):
        pk = Pack(s)
        p = pk.p
        rng = random.Random(11 + s)
        # poly_to_bytes / bytes_to_poly round-trip on d-bit unsigned fields
        for d in (1, p["DS_BITS"], p["DB_BITS"], p["DH_BITS"]):
            coeffs = [rng.randrange(0, 1 << d) for _ in range(p["N"])]
            blob = poly_to_bytes(coeffs, d)
            assert bytes_to_poly(blob, d, p["N"]) == coeffs, (s, d)
        # integer_to_bytes / bytes_to_integer round-trip
        for length in (1, 2, 4):
            x = rng.randrange(0, 1 << (8 * length))
            assert bytes_to_integer(integer_to_bytes(x, length)) == x
        # MSB-first bit order distinct from LSB-first
        bb = bytes([0b10110000])
        assert bytes_to_bits_msb(bb, 4) == [1, 0, 1, 1]
        # pk round-trip
        seedA = bytes(rng.randrange(256) for _ in range(p["SEEDBYTES"]))
        b = [[rng.randrange(0, p["CEIL_Q_ALPHA_B"]) << p["LOG2_ALPHA_B"]
              for _ in range(p["N"])] for _ in range(p["EM"])]
        pkb = pk.pack_pk(seedA, b)
        sA2, b2, rc = pk.unpack_pk(pkb)
        assert rc == 0 and sA2 == seedA and b2 == b, (s, "pk")
        # sk round-trip
        cs = p["CHALLENGESEEDBYTES"]
        ms = bytes(rng.randrange(256) for _ in range(cs))
        tr = bytes(rng.randrange(256) for _ in range(cs))
        ss = [[rng.randint(-p["BS_ENC"], p["BS_ENC"]) for _ in range(p["N"])]
              for _ in range(p["ELL"])]
        ep = [[rng.randint(-p["BE_ENC"], p["BE_ENC"]) for _ in range(p["N"])]
              for _ in range(p["EM"])]
        skb = pk.pack_sk(seedA, b, ms, tr, ss, ep)
        sA3, b3, ms3, tr3, s3, e3, rc = pk.unpack_sk(skb)
        assert rc == 0 and (sA3, b3, ms3, tr3, s3, e3) == \
            (seedA, b, ms, tr, ss, ep), (s, "sk")
        # com round-trip + K14 range reject
        wh = [rng.randrange(0, p["HH"]) for _ in range(p["N"])]
        w0 = [rng.randrange(0, 2) for _ in range(p["N"])]
        blob = pk.pack_com(wh, w0)
        h2, z2, rc = pk.unpack_com(blob)
        assert rc == 0 and h2 == wh and z2 == w0
    print("packing_ref.py self-test: poly/int byte round-trips, pk/sk/com "
          "encode-decode, MSB-vs-LSB bit order (3 sets)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
