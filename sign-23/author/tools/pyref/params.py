#!/usr/bin/env python3
"""params.py -- per-set / per-MODE constant table for the SHUTTLE Python
reference (P12).  A pure mirror of ref/params.h + ref/reduce.h; every value
here carries the source-file provenance.  Integer-only.

SHUTTLE_SETS = {128, 256, 512}.  Pull a set's dict via params(set_id).
"""

# ref/params.h primary + derived constants, per set.  reduce.h NTT-Montgomery
# constants (QINV/MONT/NINV_TOMONT) folded in.  ntt_ref.h NTT_LEVELS folded in.
_P = {
    128: dict(
        LAMBDA=128, N=256, Q=15361, ELL=3, EM=2 + 1 - 1, TAU=42,  # EM set below
        ALPHA_H=1024, ALPHA_B=2, ALPHA_1=90, ALPHA_S=10, ALPHA_E=5,
        BS_ENC=9, BE_ENC=10, DS_BITS=5, DE_BITS=5,
        DQ_BITS=14, BQ=2, DB_BITS=13, HH=30, DH_BITS=5,
        ZETA=98, NINV=15301, DN_BITS=8, BN=1,
        BK_SQ=87060, BK_LOW_SQ=84100, BV_SQ=54120740,
        # reduce.h NTT-Montgomery (R=2^16)
        QINV=15359, MONT=4092, NINV_TOMONT=3004,
        NTT_LEVELS=8,
        # params.h size pins
        PK_SIZE=1264, SK_SIZE=2288, CRYPTO_BYTES=1216,
    ),
    256: dict(
        LAMBDA=256, N=512, Q=61441, ELL=3, EM=2, TAU=58,
        ALPHA_H=1024, ALPHA_B=2, ALPHA_1=135, ALPHA_S=5, ALPHA_E=5,
        BS_ENC=10, BE_ENC=12, DS_BITS=5, DE_BITS=5,
        DQ_BITS=16, BQ=2, DB_BITS=15, HH=120, DH_BITS=7,
        ZETA=21, NINV=61321, DN_BITS=9, BN=2,
        BK_SQ=87728, BK_LOW_SQ=84100, BV_SQ=107859233,
        QINV=61439, MONT=4095, NINV_TOMONT=32632,
        NTT_LEVELS=9,
        PK_SIZE=1952, SK_SIZE=3680, CRYPTO_BYTES=2432,
    ),
    512: dict(
        LAMBDA=512, N=1024, Q=59393, ELL=3, EM=2, TAU=115,
        ALPHA_H=2048, ALPHA_B=4, ALPHA_1=144, ALPHA_S=3, ALPHA_E=3,
        BS_ENC=10, BE_ENC=12, DS_BITS=5, DE_BITS=5,
        DQ_BITS=16, BQ=2, DB_BITS=14, HH=58, DH_BITS=6,
        ZETA=3, NINV=59335, DN_BITS=10, BN=2,
        BK_SQ=85708, BK_LOW_SQ=84100, BV_SQ=634782986,
        QINV=59391, MONT=6143, NINV_TOMONT=36794,
        NTT_LEVELS=10,
        PK_SIZE=3648, SK_SIZE=7104, CRYPTO_BYTES=5056,
    ),
}
# EM for 128 is 3 (the dict expr above was a placeholder)
_P[128]["EM"] = 3

SHUTTLE_SETS = (128, 256, 512)

# shared (no #if) sampler / seed constants -- ref/params.h
SHARED = dict(
    RY=825, IRS_N=29, IRS_BDRY=15, KAPPA_A=80, KAPPA_B=57,
    THETA=96, WIDE_SIGMA_NUM=825, WIDE_SIGMA_DEN=256, WIDE_K=256,
    WIDE_RCDT_LEN=36, TWO_RSQ=2 * 825 * 825,
    # domain-separation tags
    DS_EXPAND_SEEDS=0x00, DS_EXPAND_SIGNING=0x01, DS_EXPAND_A=0x02,
    DS_EXPAND_S=0x03, DS_HASH_CH=0x04, DS_HASH_PK=0x05, DS_HASH_MSG=0x06,
    DS_SAMPLE_C=0x07, DS_SAMPLE_Y=0x08, DS_IRS=0x09,
    SEEDLEN=55,  # drng.h SEEDLEN (V/C/reseed_counter byte length)
)


def params(set_id):
    if set_id not in _P:
        raise ValueError("unknown SHUTTLE set %r" % (set_id,))
    p = dict(_P[set_id])
    p.update(SHARED)
    n = p["N"]
    q = p["Q"]
    ell, em = p["ELL"], p["EM"]
    # derived seed/hash byte lengths (params.h)
    p["SEEDBYTES"] = p["LAMBDA"] // 8
    p["CHALLENGESEEDBYTES"] = p["LAMBDA"] // 4
    p["DQ"] = 2 * q
    p["KVEC"] = 1 + ell + em
    p["Z1LEN"] = 1 + ell
    p["CHALLENGE_PACKEDBYTES"] = (n + 7) // 8
    # packed-poly byte sizes
    p["POLYPK_PACKEDBYTES"] = (n * p["DB_BITS"] + 7) // 8
    p["POLYS_PACKEDBYTES"] = (n * p["DS_BITS"] + 7) // 8
    p["POLYE_PACKEDBYTES"] = (n * p["DE_BITS"] + 7) // 8
    p["POLYWH_PACKEDBYTES"] = (n * p["DH_BITS"] + 7) // 8
    p["POLYW0_PACKEDBYTES"] = (n + 7) // 8
    p["ENCODECOM_BYTES"] = p["POLYWH_PACKEDBYTES"] + p["POLYW0_PACKEDBYTES"]
    # key/sig sizes (mirror params.h)
    p["CRYPTO_PUBLICKEYBYTES"] = p["SEEDBYTES"] + em * p["POLYPK_PACKEDBYTES"]
    p["CRYPTO_SECRETKEYBYTES"] = (
        p["SEEDBYTES"] + 2 * p["CHALLENGESEEDBYTES"]
        + ell * p["POLYS_PACKEDBYTES"] + em * p["POLYE_PACKEDBYTES"]
        + em * p["POLYPK_PACKEDBYTES"])
    # CEIL_Q_ALPHA_B (packing.c)
    p["CEIL_Q_ALPHA_B"] = (q + p["ALPHA_B"] - 1) // p["ALPHA_B"]
    p["LOG2_ALPHA_B"] = 1 if p["ALPHA_B"] == 2 else 2
    return p


def _selftest():
    for s in SHUTTLE_SETS:
        p = params(s)
        assert p["CRYPTO_PUBLICKEYBYTES"] == p["PK_SIZE"], (s, "pk")
        assert p["CRYPTO_SECRETKEYBYTES"] == p["SK_SIZE"], (s, "sk")
        # ZETA^n == -1, ZETA^2n == 1, n*NINV == 1 mod q
        q, n, z = p["Q"], p["N"], p["ZETA"]
        assert pow(z, n, q) == q - 1, (s, "zeta^n")
        assert pow(z, 2 * n, q) == 1, (s, "zeta^2n")
        assert (n * p["NINV"]) % q == 1, (s, "ninv")
        # 2^QBITS < 2q (unpack_pk_bn single cond-subtract validity)
        # H_h = 2(q-1)/alpha_h integer
        assert (2 * (q - 1)) % p["ALPHA_H"] == 0
        assert 2 * (q - 1) // p["ALPHA_H"] == p["HH"], (s, "HH")
    print("params.py self-test: pk/sk sizes + ZETA/NINV/HH consistent (3 sets)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_selftest())
