/*
Copyright (c) 2026 Yu Zhang.
Organization: State Key Laboratory of Cyberspace Security Defense,Institute of Information Engineering, CAS
              School of Cyber Security, University of Chinese Academy of Sciences  
File Description: Declares the ZEN key-encapsulation mechanism layer for the optimized ZEN-128 instance.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "sample.h"
#include "ntt.h"
#include "poly.h"
#include "drng.h"

extern DRNG_ctx drng_algorithm;

static inline uint16_t ct_abs_i16(int16_t x)
{
    uint16_t ux = (uint16_t)x;
    uint16_t sign = (uint16_t)(ux >> 15);
    uint16_t mask = (uint16_t)(0u - sign);

    return (uint16_t)((ux ^ mask) + sign);
}

/* Returns 0xffffffff iff a <= b.  Here |a-b| < 2^31. */
static inline uint32_t ct_le_mask_i32(int32_t a, int32_t b)
{
    uint32_t negative = (uint32_t)(b - a) >> 31;

    return 0u - (negative ^ 1u);
}

static inline int32_t ct_select_i32(uint32_t mask, int32_t a, int32_t b)
{
    return (int32_t)(((uint32_t)a & mask) | ((uint32_t)b & ~mask));
}

/*
 * min(Q/2 - |x|, Q/2 - |y|) = Q/2 - max(|x|, |y|).
 * Compare these maxima directly; ties select the first half.
 * Always write BOTH public offsets, including when the correction bit is zero.
 * Never select an output address with the comparison result.
 */
static inline void simple_decoding(int16_t *correction, const int16_t *t0)
{
    unsigned int i;
    int32_t c0, c1, c2, c3;
    uint32_t mask;
    uint16_t bit;

    for(i = 0; i < ZEN_N4; i++)
    {
        bit = (uint16_t)((t0[i] ^ t0[i + ZEN_N4] ^
                          t0[i + 2*ZEN_N4] ^ t0[i + 3*ZEN_N4]) & 1);

        c0 = (int32_t)ct_abs_i16(t0[i]);
        c1 = (int32_t)ct_abs_i16(t0[i + ZEN_N4]);
        c2 = (int32_t)ct_abs_i16(t0[i + 2*ZEN_N4]);
        c3 = (int32_t)ct_abs_i16(t0[i + 3*ZEN_N4]);

        mask = ct_le_mask_i32(c2, c0);
        c0 = ct_select_i32(mask, c0, c2);
        mask = ct_le_mask_i32(c3, c1);
        c1 = ct_select_i32(mask, c1, c3);
        mask = ct_le_mask_i32(c1, c0);

        correction[i] = (int16_t)(bit & (uint16_t)mask);
        correction[i + ZEN_N4] = (int16_t)(bit & (uint16_t)~mask);
    }
}

int pke_keygen_derand(unsigned char *pk, unsigned char *sk, const unsigned char *seed)
{
    unsigned i;
    /* Wider than uint8_t so exhaustion is detected before nonce reuse. */
    unsigned int nonce;
    int16_t f[ZEN_N], g[ZEN_N], f2[ZEN_N4];
    int16_t t0[ZEN_N], t1[ZEN_N];

    if (pk == NULL || sk == NULL || seed == NULL)
        return -1;

    nonce = 0;
    //generate f
    if (poly_generate_f(f, seed, nonce++) != 0)
        return -1;
    //chechk f
    for(;;)
    {
        for(i = 0; i < ZEN_N4; i++)
        {
            t0[i] = (f[i] & 1) ^ (f[i + ZEN_N4] & 1) ^ (f[i + 2*ZEN_N4] & 1) ^ (f[i + 3*ZEN_N4] & 1); //t0 = f mod (x^(n/4)+1)
        }
        if(check_poly_inv_Z2(t0))
        {
            if (nonce == 256u)
                return -1;
            if (poly_generate_f(f, seed, nonce++) != 0)
                return -1;
            continue;
        }
        poly_ntt_mq(f);
        if(check_poly_inv_Zq(f))
        {
            if (nonce == 256u)
                return -1;
            if (poly_generate_f(f, seed, nonce++) != 0)
                return -1;
            continue;
        }

        poly_baseinv_ntt(t1, f); //t1 = f^(-1) mod (x^n+1, q)
        FastInversion(f2, t0); //t0 = f2
        
        break;
    }

    //generate g
    if (nonce == 256u)
        return -1;
    if (poly_generate_g(g, seed, nonce++) != 0)
        return -1;
    //check g
    for(;;)
    {
        poly_ntt_mq(g);
        if(check_poly_inv_Zq(g))
        {
            if (nonce == 256u)
                return -1;
            if (poly_generate_g(g, seed, nonce++) != 0)
                return -1;
            continue;
        }
        break;
    }

    poly_basemul_ntt_mq(t0, g, t1);
    poly_publickey_pack(pk, t0);
    poly_secretkey_pack(sk, f);
    poly_bit2byte_pack(sk+ZEN_F_NTT_PACK, f2, ZEN_N4);

    return 0;
}

int pke_keygen(unsigned char *pk, unsigned char *sk)
{
    uint8_t seed[SEED_LEN_BYTES];

    if (get_random_number(&drng_algorithm, seed, SEED_LEN_BYTES*8) != 0)
        return -1;
    return pke_keygen_derand(pk, sk, seed);
}

int pke_enc(unsigned char *pk, unsigned char *m, unsigned char *seed, unsigned char *ct)
{
    unsigned i;
    uint8_t nonce;
    int16_t h[ZEN_N], s[ZEN_N], t[ZEN_N];

    poly_publickey_unpack(h, pk);

    nonce = 0;
    if (poly_generate_s(s, seed, nonce++) != 0)
        return -1;

    poly_ntt(s);
    poly_basemul_ntt(t, h, s);
    poly_intt(t);

    /* h and s are dead here; reuse them for e and the message bits. */
    if (poly_generate_e(h, seed, nonce) != 0)
        return -1;
    poly_byte2bit_unpack(s, m, ZEN_N4);

    for(i = 0; i < ZEN_N; i++)
    {
        int32_t encoded = s[i & (ZEN_N4 - 1)] * ((ZEN_Q + 1) / 2);
        t[i] = montgomery_reduce((t[i] + h[i] + encoded) * MONT);
        t[i] += (t[i] >> 15) & ZEN_Q;
    }

    poly_compress(t);
    poly_ciphertext_pack(ct, t);

    return 0;
}

void pke_dec(unsigned char *sk, unsigned char *ct, unsigned char *m)
{
    unsigned int i;
    int16_t f[ZEN_N], f2[ZEN_N2], mp0[ZEN_N2];
    int16_t t0[ZEN_N], t1[ZEN_N];
    int16_t *correction = t1 + ZEN_N2;

    poly_secretkey_unpack(f, sk); 
    poly_byte2bit_unpack(f2, sk+ZEN_F_NTT_PACK, ZEN_N4);
    memset(f2 + ZEN_N4, 0, ZEN_N4 * sizeof(int16_t));
    poly_ciphertext_unpack(t0, ct);
    poly_decompress(t0);

    // cf mod <q, x^n+1>
    poly_ntt(t0);
    poly_basemul_ntt(t1, t0, f);
    poly_intt(t1);

    // (x^(n/2)+1)cf mod <q, x^n+1>
    for(i = 0; i < ZEN_N2; i++)
    {
        int16_t lo = t1[i];
        int16_t hi = t1[i + ZEN_N2];
        t0[i] = montgomery_reduce((hi - lo) * MONT);
        t0[i + ZEN_N2] = montgomery_reduce((lo + hi) * MONT);
    }

    // a mod <2, x^(n/2)+1>
    for(i = 0; i < ZEN_N2; i++)
    {
        t1[i] = (t0[i] & 1) ^ (t0[i + ZEN_N2] & 1);
    }

    simple_decoding(correction, t0);
    // m_prim = af2 and the decoding correction, sharing one packed f2 scan.
    mul2_in_R2_256(t1, correction, f2, mp0, t0);

    for(i = 0; i < ZEN_N4; i++)
    {
        t0[i] ^= mp0[i];
    }

    poly_bit2byte_pack(m, t0, ZEN_N4);
    
}
