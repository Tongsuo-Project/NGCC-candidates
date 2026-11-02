#!/usr/bin/env python3
"""drng_ref.py -- SM3 Hash-DRBG mirror for the SHUTTLE Python reference (P12).
Byte-exact to ref/drng.c (init_random_number / get_random_number).

SEEDLEN = 55 (V/C/reseed_counter byte length).  OUTLEN = 32 (SM3 digest).

  init_random_number(seed_bytes):  SM3_DRNG_Instantiate
    df(seed_material, nonce_len) -> first SEEDLEN bytes -> V
    df(0x00 || V) -> C
    reseed_counter = 1 (big-number)
  get_random_number(nbits):  SM3_DRNG_Generate
    m = ceil(nbits / 256) SM3(V) blocks, V incremented per block (big-endian
        +1 over the 55-byte data buffer), concatenated, truncated to
        ceil(nbits/8) bytes; last byte MSB-masked to nbits.
    state update: H = SM3(0x03 || V) right-aligned in a 55-byte zero buffer;
        V = V + H + C + reseed_counter (4-operand big-number add mod 2^440);
        reseed_counter += 1.

The SM3 core is the bit-oriented sm3_bit from drng.c (standard SM3 with a
bit-length pad).  All draws here are whole bytes (msg_bitlen % 8 == 0).
"""

SEEDLEN = 55
OUTLEN = 32

_IV = [0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
       0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E]


def _rotl(a, n):
    a &= 0xFFFFFFFF
    return ((a << n) | (a >> (32 - n))) & 0xFFFFFFFF


def _P0(x):
    return x ^ _rotl(x, 9) ^ _rotl(x, 17)


def _P1(x):
    return x ^ _rotl(x, 15) ^ _rotl(x, 23)


def _ff(x, y, z, j):
    return (x ^ y ^ z) if j < 16 else ((x & y) | (x & z) | (y & z))


def _gg(x, y, z, j):
    return (x ^ y ^ z) if j < 16 else (((y ^ z) & x) ^ z)


def _compress(dgst, block):
    """one 64-byte block; dgst is a list of 8 u32 (mutated)."""
    W = [0] * 68
    for i in range(16):
        W[i] = int.from_bytes(block[4 * i:4 * i + 4], "big")
    for i in range(16, 68):
        W[i] = (_P1(W[i - 16] ^ W[i - 9] ^ _rotl(W[i - 3], 15))
                ^ _rotl(W[i - 13], 7) ^ W[i - 6])
    Wp = [(W[i] ^ W[i + 4]) for i in range(64)]
    A, B, C, D, E, F, G, H = dgst
    for j in range(64):
        Tj = 0x79CC4519 if j < 16 else 0x7A879D8A
        SS1 = _rotl((_rotl(A, 12) + E + _rotl(Tj, j & 0x1F)) & 0xFFFFFFFF, 7)
        SS2 = SS1 ^ _rotl(A, 12)
        TT1 = (_ff(A, B, C, j) + D + SS2 + Wp[j]) & 0xFFFFFFFF
        TT2 = (_gg(E, F, G, j) + H + SS1 + W[j]) & 0xFFFFFFFF
        D = C
        C = _rotl(B, 9)
        B = A
        A = TT1
        H = G
        G = _rotl(F, 19)
        F = E
        E = _P0(TT2)
    dgst[0] ^= A; dgst[1] ^= B; dgst[2] ^= C; dgst[3] ^= D
    dgst[4] ^= E; dgst[5] ^= F; dgst[6] ^= G; dgst[7] ^= H
    for i in range(8):
        dgst[i] &= 0xFFFFFFFF


def sm3(msg):
    """standard SM3 over a whole-byte message; returns 32 bytes.  Mirrors
    sm3_bit with msg_bitlen = 8*len(msg)."""
    msg_bitlen = len(msg) * 8
    block_num = msg_bitlen // 512
    remain = msg_bitlen & 0x1FF       # leftover bits (< 512)
    dg = list(_IV)
    for b in range(block_num):
        _compress(dg, msg[64 * b:64 * b + 64])
    block = bytearray(64)
    nbytes = (remain + 7) >> 3
    block[:nbytes] = msg[block_num * 64: block_num * 64 + nbytes]
    # block[remain>>3] &= ((0xFF00 >> (remain&7)) & 0xFF); then |= 0x80>>(remain&7)
    block[remain >> 3] &= ((0xFF00 >> (remain & 7)) & 0xFF)
    block[remain >> 3] |= (1 << (7 - (remain & 7)))
    if remain <= 512 - 65:
        # length goes in this block
        pass
    else:
        _compress(dg, bytes(block))
        block = bytearray(64)
    # PUT32(block+56, block_num>>32<<9); PUT32(block+60, (block_num<<9)+remain)
    hi = (block_num >> 32) << 9
    lo = ((block_num << 9) + remain) & 0xFFFFFFFF
    block[56:60] = hi.to_bytes(4, "big")
    block[60:64] = lo.to_bytes(4, "big")
    _compress(dg, bytes(block))
    return b"".join(d.to_bytes(4, "big") for d in dg)


def _inc_bn(bn):
    """big-endian +1 over a bytearray (mirrors inc_Big_Number)."""
    i = len(bn)
    while i > 0:
        bn[i - 1] = (bn[i - 1] + 1) & 0xFF
        if bn[i - 1]:
            break
        i -= 1


def _plus_bn(BN1, BN2, BN3, BN4):
    """BN1 += BN2 + BN3 + BN4 (4-operand big-number add, big-endian, mod
    2^(8*len)).  Mirrors plus_Big_Number."""
    carry = 0
    for i in range(len(BN1) - 1, -1, -1):
        s = BN1[i] + BN2[i] + BN3[i] + BN4[i] + carry
        carry = s >> 8
        BN1[i] = s & 0xFF


def _df(input_string, input_len_bytes):
    """SM3_df: counter-mode hash df.  input_string is mutated to its first
    SEEDLEN derived bytes (mirrors SM3_df, which overwrites the buffer)."""
    length = (SEEDLEN + OUTLEN - 1) // OUTLEN      # ceil(SEEDLEN/OUTLEN) = 2
    temp = bytearray()
    counter = 1
    nbits_be = (8 * SEEDLEN).to_bytes(4, "big")
    for _ in range(length):
        data = bytes([counter]) + nbits_be + bytes(input_string[:input_len_bytes])
        temp += sm3(data)
        counter += 1
    return bytes(temp[:SEEDLEN])


class DRNG:
    def __init__(self):
        self.V = bytearray(SEEDLEN)
        self.C = bytearray(SEEDLEN)
        self.reseed_counter = bytearray(SEEDLEN)

    @classmethod
    def instantiate(cls, nonce):
        """SM3_DRNG_Instantiate -> init_random_number."""
        d = cls()
        nlen = len(nonce)
        seed_material = bytearray(max(nlen, SEEDLEN))
        seed_material[:nlen] = nonce
        seed = _df(seed_material, nlen)            # first SEEDLEN bytes
        d.V[:] = seed[:SEEDLEN]
        padded_V = bytearray([0x00]) + bytes(d.V)
        cbytes = _df(padded_V, len(padded_V))
        d.C[:] = cbytes[:SEEDLEN]
        _inc_bn(d.reseed_counter)                 # reseed_counter = 1
        return d

    def generate(self, nbits):
        """SM3_DRNG_Generate -> get_random_number.  Returns ceil(nbits/8)
        bytes, last byte MSB-masked to nbits."""
        m = (nbits + OUTLEN * 8 - 1) // (OUTLEN * 8)   # ceil(nbits/256)
        nbytes = (nbits + 7) // 8
        data = bytearray(self.V)                  # SEEDLEN bytes
        out = bytearray()
        for _ in range(m):
            w = sm3(bytes(data))                  # 32 bytes
            out += w
            _inc_bn(data)
        out = out[:nbytes]
        if nbytes >= 1:
            # HIGH_N_BIT_MASK(8 - (8*nbytes - nbits))
            extra = 8 * nbytes - nbits
            keep = 8 - extra
            mask = ((~0) << (8 - keep)) & 0xFF if keep else 0
            out[nbytes - 1] &= mask
        # state update: H = SM3(0x03 || V) right-aligned in 55-byte zero buffer
        H = bytearray(SEEDLEN)
        digest = sm3(bytes([0x03]) + bytes(self.V))   # 32 bytes
        H[SEEDLEN - OUTLEN:] = digest                  # right-aligned
        _plus_bn(self.V, H, self.C, self.reseed_counter)
        _inc_bn(self.reseed_counter)
        return bytes(out)


def init_random_number(nonce):
    return DRNG.instantiate(nonce)


def get_random_number(drng, nbits):
    return drng.generate(nbits)


def _selftest():
    # Known-answer: SM3("abc") = 66c7f0f4...  (standard test vector)
    h = sm3(b"abc").hex()
    expect = "66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0"
    assert h == expect, ("SM3(abc)", h)
    # DRNG determinism + state advance
    d = DRNG.instantiate(b"seed" * 16)
    a = get_random_number(d, 64 * 8)
    b = get_random_number(d, 64 * 8)
    assert len(a) == 64 and len(b) == 64 and a != b
    d2 = DRNG.instantiate(b"seed" * 16)
    assert get_random_number(d2, 64 * 8) == a, "DRNG not deterministic"
    print("drng_ref.py self-test: SM3(abc) KAT OK; DRNG deterministic + "
          "state advances")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
