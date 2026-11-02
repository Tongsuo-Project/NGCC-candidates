/*
 * Copyright (c) 2026 Hang Zhang.
 * State Key Laboratory of Cyberspace Security Defense,
 * Institute of Information Engineering, CAS
 * School of Cyber Security, University of Chinese Academy of Sciences
 */
#include "../../common/ct_audit.h"
#include "packing.h"
#include "polyvec.h"
#include "endian.h"
#include <immintrin.h>
#include <stdint.h>
#include <string.h>

#define BIT_IMPL_AVX 1
#include "../../common/packing_public.h"

static void pack_polyveck_public(unsigned char *out, const polyveck *v)
{
    for (int i = 0; i < BIT_K; i++)
        pack_poly_public(out + i * BIT_POLY_PUBLIC_BYTES, &v->vec[i]);
}

static int unpack_polyveck_public(polyveck *v, const unsigned char *in)
{
    for (int i = 0; i < BIT_K; i++)
        if (unpack_poly_public(&v->vec[i], in + i * BIT_POLY_PUBLIC_BYTES) != 0)
            return -1;
    return 0;
}

static void pack_poly_small2(unsigned char out[BIT_N / 4], const poly *a)
{
    const __m256i mask = _mm256_set1_epi16(3);
    const __m128i pair_weights = _mm_set1_epi16(0x0401);
    const __m128i quad_weights = _mm_set1_epi32(0x00100001);
    const __m128i gather =
        _mm_setr_epi8(0, 4, 8, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);

    for (int i = 0; i < BIT_N; i += 16) {
        __m256i coeffs = _mm256_and_si256(_mm256_loadu_si256((const __m256i *)&a->coeffs[i]), mask);
        __m128i bytes =
            _mm_packus_epi16(_mm256_castsi256_si128(coeffs), _mm256_extracti128_si256(coeffs, 1));
        __m128i pairs = _mm_maddubs_epi16(bytes, pair_weights);
        __m128i quads = _mm_madd_epi16(pairs, quad_weights);
        __m128i packed = _mm_shuffle_epi8(quads, gather);

        store64_le_n(out + i / 4, (uint32_t)_mm_cvtsi128_si32(packed), 4);
    }
}

static inline void store_small2_quads(int16_t *out, __m128i c0, __m128i c1, __m128i c2, __m128i c3)
{
    __m128i c01_lo = _mm_unpacklo_epi16(c0, c1);
    __m128i c01_hi = _mm_unpackhi_epi16(c0, c1);
    __m128i c23_lo = _mm_unpacklo_epi16(c2, c3);
    __m128i c23_hi = _mm_unpackhi_epi16(c2, c3);
    __m128i v0 = _mm_unpacklo_epi32(c01_lo, c23_lo);
    __m128i v1 = _mm_unpackhi_epi32(c01_lo, c23_lo);
    __m128i v2 = _mm_unpacklo_epi32(c01_hi, c23_hi);
    __m128i v3 = _mm_unpackhi_epi32(c01_hi, c23_hi);

#define SIGN_EXTEND_SMALL2(v) _mm_srai_epi16(_mm_slli_epi16((v), 14), 14)
    _mm_storeu_si128((__m128i *)(out), SIGN_EXTEND_SMALL2(v0));
    _mm_storeu_si128((__m128i *)(out + 8), SIGN_EXTEND_SMALL2(v1));
    _mm_storeu_si128((__m128i *)(out + 16), SIGN_EXTEND_SMALL2(v2));
    _mm_storeu_si128((__m128i *)(out + 24), SIGN_EXTEND_SMALL2(v3));
#undef SIGN_EXTEND_SMALL2
}

static int unpack_poly_small2(poly *r, const unsigned char in[BIT_N / 4])
{
    const __m128i mask = _mm_set1_epi16(3);
    const __m128i reserved = _mm_set1_epi16(2);
    uint32_t bad = 0;

    for (int i = 0; i < BIT_N; i += 32) {
        __m128i packed = _mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i *)(in + i / 4)));
        __m128i c0 = _mm_and_si128(packed, mask);
        __m128i c1 = _mm_and_si128(_mm_srli_epi16(packed, 2), mask);
        __m128i c2 = _mm_and_si128(_mm_srli_epi16(packed, 4), mask);
        __m128i c3 = _mm_srli_epi16(packed, 6);

        bad |= (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi16(c0, reserved));
        bad |= (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi16(c1, reserved));
        bad |= (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi16(c2, reserved));
        bad |= (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi16(c3, reserved));
        store_small2_quads(r->coeffs + i, c0, c1, c2, c3);
    }
    /* Constant over valid generated signing keys; keep decoded coefficients secret. */
    BIT_CT_DECLASSIFY(&bad, sizeof bad);
    return bad != 0 ? -1 : 0;
}

void pack_pk(unsigned char *pk, const unsigned char *seed_A, const polyveck *public_vector)
{
    memcpy(pk, seed_A, BIT_SEEDBYTES);
    pk += BIT_SEEDBYTES;
    pack_polyveck_public(pk, public_vector);
}

void pack_sk(unsigned char *sk, const unsigned char *seed_A, const polyveck *public_vector,
             const unsigned char *secret_seed, const unsigned char *tr, const polyvecl *s_0,
             const polyveck *e)
{
    memcpy(sk, seed_A, BIT_SEEDBYTES);
    sk += BIT_SEEDBYTES;
    pack_polyveck_public(sk, public_vector);
    sk += BIT_POLYVECK_PUBLIC_BYTES;
    memcpy(sk, secret_seed, BIT_SEEDBYTES);
    sk += BIT_SEEDBYTES;
    memcpy(sk, tr, BIT_TRBYTES);
    sk += BIT_TRBYTES;
    for (int i = 0; i < BIT_L; ++i) {
        pack_poly_small2(sk, &s_0->vec[i]);
        sk += BIT_N / 4;
    }

    for (int i = 0; i < BIT_K; i++) {
        pack_poly_small2(sk, &e->vec[i]);
        sk += BIT_N / 4;
    }
}

int unpack_sk(unsigned char *seed_A, polyveck *public_vector, unsigned char *secret_seed,
              unsigned char *tr, polyvecl *s_0, polyveck *e, const unsigned char *sk)
{
    memcpy(seed_A, sk, BIT_SEEDBYTES);
    sk += BIT_SEEDBYTES;
    if (unpack_polyveck_public(public_vector, sk) != 0)
        return -1;
    sk += BIT_POLYVECK_PUBLIC_BYTES;
    memcpy(secret_seed, sk, BIT_SEEDBYTES);
    sk += BIT_SEEDBYTES;
    memcpy(tr, sk, BIT_TRBYTES);
    sk += BIT_TRBYTES;

    for (int i = 0; i < BIT_L; ++i) {
        if (unpack_poly_small2(&s_0->vec[i], sk) != 0)
            return -1;
        sk += BIT_N / 4;
    }

    for (int i = 0; i < BIT_K; i++) {
        if (unpack_poly_small2(&e->vec[i], sk) != 0)
            return -1;
        sk += BIT_N / 4;
    }

    return 0;
}

int unpack_pk(unsigned char *seed_A, polyveck *public_vector, const unsigned char *pk)
{
    memcpy(seed_A, pk, BIT_SEEDBYTES);
    pk += BIT_SEEDBYTES;

    return unpack_polyveck_public(public_vector, pk);
}

/* Full-support rANS response model with sigma=sqrt((24576^2-185^2)/3588). */
#include "../../common/signature_codec_256.inc"
