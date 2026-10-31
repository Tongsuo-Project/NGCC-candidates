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
#include <immintrin.h>
#include "params.h"
#include "sample.h"
#include "ntt.h"
#include "poly.h"
#include "drng.h"

extern DRNG_ctx drng_algorithm;

/*
 * min(Q/2 - |x|, Q/2 - |y|) = Q/2 - max(|x|, |y|).
 * Compare these maxima directly; ties select the first half.
 * Always write BOTH public offsets, including when the correction bit is zero.
 * Never select an output address with the comparison result.
 * Internal coefficient arrays are 32-byte aligned. Unsigned maxima also
 * handle the 0x8000 magnitude returned by vpabsw for INT16_MIN.
 */
static inline void simple_decoding(int16_t *correction, const int16_t *t0)
{
    unsigned int i;
    __m256i vdata0, vdata3, vdata4, vdata5, vdata6, vdata7;

    vdata0 = _mm256_set1_epi16(1);
    for(i = 0; i < ZEN_N4; i += 16)
    {
        vdata3 = _mm256_load_si256((const __m256i *)(t0 + i));
        vdata4 = vdata3;
        vdata3 = _mm256_abs_epi16(vdata3);

        vdata5 = _mm256_load_si256((const __m256i *)(t0 + i + ZEN_N4));
        vdata4 = _mm256_xor_si256(vdata4, vdata5);
        vdata5 = _mm256_abs_epi16(vdata5);

        vdata6 = _mm256_load_si256((const __m256i *)(t0 + i + 2*ZEN_N4));
        vdata4 = _mm256_xor_si256(vdata4, vdata6);
        vdata6 = _mm256_abs_epi16(vdata6);
        vdata3 = _mm256_max_epu16(vdata3, vdata6);

        vdata7 = _mm256_load_si256((const __m256i *)(t0 + i + 3*ZEN_N4));
        vdata4 = _mm256_xor_si256(vdata4, vdata7);
        vdata7 = _mm256_abs_epi16(vdata7);
        vdata5 = _mm256_max_epu16(vdata5, vdata7);

        vdata6 = _mm256_max_epu16(vdata3, vdata5);
        vdata6 = _mm256_cmpeq_epi16(vdata3, vdata6);
        vdata4 = _mm256_and_si256(vdata4, vdata0);
        _mm256_store_si256((__m256i *)(correction + i),
                           _mm256_and_si256(vdata4, vdata6));
        _mm256_store_si256((__m256i *)(correction + i + ZEN_N4),
                           _mm256_andnot_si256(vdata6, vdata4));
    }
}

int pke_keygen_derand(unsigned char *pk, unsigned char *sk, const unsigned char *seed)
{
    unsigned i;
    /* Wider than uint8_t so exhaustion is detected before nonce reuse. */
    unsigned int nonce;
    ALIGN32 int16_t f[ZEN_N], f2[ZEN_N4];
    ALIGN32 int16_t t0[ZEN_N], t1[ZEN_N];
    __m256i vdata0, vdata1, vdata2;

    if (pk == NULL || sk == NULL || seed == NULL)
        return -1;

    nonce = 0;
    //generate f
    if (poly_generate_f(f, seed, nonce++) != 0)
        return -1;
    //chechk f
    for(;;)
    {
        vdata0 = _mm256_set1_epi16(1);
        for(i = 0; i < ZEN_N4; i += 16)
        {

            vdata1 = _mm256_load_si256((__m256i*)(f + i));
            vdata2 = _mm256_load_si256((__m256i*)(f + i + ZEN_N4));
            vdata1 = _mm256_xor_si256(vdata1, vdata2);
            vdata2 = _mm256_load_si256((__m256i*)(f + i + 2 * ZEN_N4));
            vdata1 = _mm256_xor_si256(vdata1, vdata2);
            vdata2 = _mm256_load_si256((__m256i*)(f + i + 3 * ZEN_N4));
            vdata1 = _mm256_xor_si256(vdata1, vdata2);
            vdata1 = _mm256_and_si256(vdata0, vdata1);
            _mm256_store_si256((__m256i *)(t0 + i), vdata1);
        }
        if(check_poly_inv_Z2(t0))
        {
            if (nonce == 256u)
                return -1;
            if (poly_generate_f(f, seed, nonce++) != 0)
                return -1;
            continue;
        }
        poly_ntt_mq(f, nttdata);
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
    if (poly_generate_g(t0, seed, nonce++) != 0)
        return -1;
    //check g
    for(;;)
    {
        poly_ntt_mq(t0, nttdata);
        if(check_poly_inv_Zq(t0))
        {
            if (nonce == 256u)
                return -1;
            if (poly_generate_g(t0, seed, nonce++) != 0)
                return -1;
            continue;
        }
        break;
    }

    poly_basemul_ntt_mq(t1, t0, t1, muldata);
    shuffled_order_2_bitreversed_order(t1);
    shuffled_order_2_bitreversed_order(f);
    poly_publickey_pack(pk, t1);
    poly_secretkey_pack(sk, f);
    poly_bit2byte_pack(sk+ZEN_F_NTT_PACK, f2, ZEN_N4);

    return 0;
}

int pke_keygen(unsigned char *pk, unsigned char *sk)
{
    ALIGN32 uint8_t seed[SEED_LEN_BYTES];

    if (get_random_number(&drng_algorithm, seed, SEED_LEN_BYTES*8) != 0)
        return -1;
    return pke_keygen_derand(pk, sk, seed);
}

int pke_enc(unsigned char *pk, unsigned char *m, unsigned char *seed, unsigned char *ct)
{
    unsigned i;
    uint8_t nonce;
    ALIGN32 int16_t h[ZEN_N], s[ZEN_N], e[ZEN_N];
    ALIGN32 int16_t t0[ZEN_N], t1[ZEN_N];
    __m256i vdata0, vdata1, vdata2, vdata3, vdata4;

    poly_publickey_unpack(h, pk);
    bitreversed_order_2_shuffled_order(h);

    nonce = 0;
    if (poly_generate_se(s, seed, nonce++) != 0)
        return -1;
    if (poly_generate_se(e, seed, nonce++) != 0)
        return -1;

    poly_byte2bit_unpack(t0, m, ZEN_N4);

    vdata0 = _mm256_set1_epi16(385);
    for(i = 0; i < ZEN_N4; i += 16)
    {
        vdata1 = _mm256_load_si256((__m256i *)(t0 + i));
        vdata1 = _mm256_mullo_epi16(vdata1, vdata0);
        _mm256_store_si256((__m256i *)(t1 + i), vdata1);
        _mm256_store_si256((__m256i *)(t1 + i + ZEN_N4), vdata1);
        _mm256_store_si256((__m256i *)(t1 + i + 2*ZEN_N4), vdata1);
        _mm256_store_si256((__m256i *)(t1 + i + 3*ZEN_N4), vdata1);
    }

    poly_ntt(s, nttdata);
    poly_basemul_ntt(t0, h, s, muldata);
    poly_intt(t0, inttdata);

    vdata0 = _mm256_set1_epi16(769);
    vdata1 = _mm256_set1_epi16(-767);
    vdata2 = _mm256_set1_epi16(171);
    for(i = 0; i < ZEN_N; i += 16)
    {
        vdata3 = _mm256_load_si256((__m256i *)(t0 + i));
        vdata4 = _mm256_load_si256((__m256i *)(e + i));
        vdata3 = _mm256_add_epi16(vdata3, vdata4);
        vdata4 = _mm256_load_si256((__m256i *)(t1 + i));
        vdata3 = _mm256_add_epi16(vdata3, vdata4);
        montmul(&vdata4, &vdata3, &vdata2, &vdata0, &vdata1);
        vdata4 = _mm256_add_epi16(vdata4, _mm256_and_si256(_mm256_srai_epi16(vdata4, 15), vdata0));
        _mm256_store_si256((__m256i *)(t1 + i), vdata4);
    }

    poly_compress(t1);
    poly_ciphertext_pack(ct, t1);

    return 0;
}

void pke_dec(unsigned char *sk, unsigned char *ct, unsigned char *m)
{
    unsigned int i;
    ALIGN32 int16_t f[ZEN_N], f2[ZEN_N2];
    ALIGN32 int16_t t0[ZEN_N], t1[ZEN_N];
    int16_t *mp0 = f;
    int16_t *correction = t1 + ZEN_N2;
    __m256i vdata0, vdata1, vdata2, vdata3, vdata4, vdata5, vdata6, vdata7;

    poly_secretkey_unpack(f, sk); 
    bitreversed_order_2_shuffled_order(f);
    poly_byte2bit_unpack(f2, sk+ZEN_F_NTT_PACK, ZEN_N4);
    memset(f2 + ZEN_N4, 0, ZEN_N4 * sizeof(int16_t));
    poly_ciphertext_unpack(t0, ct);
    poly_decompress(t0);

    // cf mod <q, x^n+1>
    poly_ntt(t0, nttdata);
    poly_basemul_ntt(t1, t0, f, muldata);
    poly_intt(t1, inttdata);

    // (x^(n/2)+1)cf mod <q, x^n+1>
    vdata0 = _mm256_set1_epi16(769);
    vdata1 = _mm256_set1_epi16(-767);
    vdata2 = _mm256_set1_epi16(171);
    for(i = 0; i < ZEN_N2; i += 16)
    {
        vdata3 = _mm256_load_si256((__m256i *)(t1 + i));
        vdata4 = _mm256_load_si256((__m256i *)(t1 + i + ZEN_N2));
        vdata5 = _mm256_sub_epi16(vdata4, vdata3);
        vdata6 = _mm256_add_epi16(vdata3, vdata4);
        montmul(&vdata7, &vdata5, &vdata2, &vdata0, &vdata1);
        _mm256_store_si256((__m256i *)(t0 + i), vdata7);
        montmul(&vdata7, &vdata6, &vdata2, &vdata0, &vdata1);
        _mm256_store_si256((__m256i *)(t0 + i + ZEN_N2), vdata7);
    }

    // a mod <2, x^(n/2)+1>
    vdata0 = _mm256_set1_epi16(1);
    for(i = 0; i < ZEN_N2; i += 16)
    {
        vdata1 = _mm256_load_si256((__m256i *)(t0 + i));
        vdata2 = _mm256_load_si256((__m256i *)(t0 + i + ZEN_N2));
        vdata1 = _mm256_xor_si256(vdata1, vdata2);
        vdata1 = _mm256_and_si256(vdata1, vdata0);
        _mm256_store_si256((__m256i *)(t1 + i), vdata1);
    }

    simple_decoding(correction, t0);

    mul_in_R2_512(t1, f2, mp0);
    mul_in_R2_512(correction, f2, t0);

    for(i = 0; i < ZEN_N4; i += 16)
    {
        vdata0 = _mm256_load_si256((__m256i *)(mp0 + i));
        vdata1 = _mm256_load_si256((__m256i *)(t0 + i));
        vdata1 = _mm256_xor_si256(vdata1, vdata0);
        _mm256_store_si256((__m256i *)(t0 + i), vdata1);
    }

    poly_bit2byte_pack(m, t0, ZEN_N4);
    
}
