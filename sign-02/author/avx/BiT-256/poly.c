/*
 * Copyright (c) 2026 Hang Zhang.
 * State Key Laboratory of Cyberspace Security Defense,
 * Institute of Information Engineering, CAS
 * School of Cyber Security, University of Chinese Academy of Sciences
 */
#include "../../common/ct_audit.h"
#include "poly.h"
#include "../common/highbits.h"
#include "ntt.h"
#include "symmetric.h"
#include "fips202x4.h"
#include "endian.h"
#include "../../common/packing_w1.h"
#include <immintrin.h>
#include <stddef.h>
#include <string.h>

static inline __m256i reduce_to_unsigned_i16x16(__m256i x)
{
    const __m256i q = _mm256_set1_epi16(BIT_Q);
    for (int i = 0; i < 5; i++) {
        x = _mm256_add_epi16(x, _mm256_and_si256(_mm256_srai_epi16(x, 15), q));
        x = _mm256_sub_epi16(
            x, _mm256_and_si256(_mm256_cmpgt_epi16(x, _mm256_set1_epi16(BIT_Q - 1)), q));
    }
    return x;
}

static inline __m256i reduce_to_signed_i16x16(__m256i x)
{
    const __m256i half = _mm256_set1_epi16(BIT_Q_HALF);
    const __m256i q = _mm256_set1_epi16(BIT_Q);
    return _mm256_sub_epi16(x, _mm256_and_si256(_mm256_cmpgt_epi16(x, half), q));
}

static inline __m256i reduce_sum_signed_i16x16(__m256i x)
{
    const __m256i lower = _mm256_set1_epi16(-BIT_Q_HALF);
    const __m256i half = _mm256_set1_epi16(BIT_Q_HALF);
    const __m256i q = _mm256_set1_epi16(BIT_Q);

    for (int i = 0; i < 4; i++) {
        x = _mm256_add_epi16(x, _mm256_and_si256(_mm256_cmpgt_epi16(lower, x), q));
        x = _mm256_sub_epi16(x, _mm256_and_si256(_mm256_cmpgt_epi16(x, half), q));
    }
    return x;
}

/* ntt_dot3_asm addresses three adjacent polynomial bodies directly. */
typedef char poly_ntt_must_be_dense[sizeof(poly_ntt) == BIT_N * sizeof(int16_t) ? 1 : -1];

static void unpack_S1_4bit(int16_t *a, const uint8_t *buf)
{
    const __m256i gather =
        _mm256_broadcastsi128_si256(_mm_setr_epi8(0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3));
    const __m256i mults = _mm256_broadcastsi128_si256(_mm_setr_epi16(16, 1, 16, 1, 16, 1, 16, 1));
    const __m256i mask = _mm256_set1_epi16(15);
    const __m256i zero = _mm256_setzero_si256();
    const __m256i one = _mm256_set1_epi16(1);

    for (int i = 0; i < BIT_N / 64; i++) {
        __m256i input = _mm256_loadu_si256((const __m256i *)(buf + 32 * i));

        for (int group = 0; group < 4; group++) {
            __m256i idx = _mm256_add_epi8(gather, _mm256_set1_epi8((char)(4 * group)));
            __m256i v = _mm256_shuffle_epi8(input, idx);
            v = _mm256_and_si256(_mm256_srli_epi16(_mm256_mullo_epi16(v, mults), 4), mask);
            __m256i s = _mm256_sub_epi16(_mm256_srli_epi16(_mm256_cmpeq_epi16(v, one), 15),
                                         _mm256_srli_epi16(_mm256_cmpeq_epi16(v, zero), 15));
            _mm_storeu_si128((__m128i *)&a[64 * i + 8 * group], _mm256_castsi256_si128(s));
            _mm_storeu_si128((__m128i *)&a[64 * i + 32 + 8 * group],
                             _mm256_extracti128_si256(s, 1));
        }
    }
}

void decompose_hint(int16_t *highbits, int16_t r)
{
    uint32_t val = (uint32_t)reduce_to_unsigned(r);
    uint32_t numerator = val + (BIT_GAMMA2 >> 1);
    int32_t hb =
        (int32_t)(((uint64_t)numerator * BIT_GAMMA2_BARRETT_MULT) >> BIT_GAMMA2_BARRETT_SHIFT);

    int32_t edgecase = (BIT_HINT_MODULUS - (hb + 1)) >> 31;
    hb -= BIT_HINT_MODULUS & edgecase;
    *highbits = (int16_t)hb;
}

uint16_t reduce_barrett(int32_t a)
{
    int32_t t;
    t = (int32_t)(((int64_t)a * BIT_BARRETT_MULT) >> BIT_BARRETT_SHIFT);
    a -= t * BIT_Q;
    /* Barrett estimate may be off by one; fold back into [0, q) */
    a += (a >> 31) & BIT_Q;
    a -= BIT_Q;
    a += (a >> 31) & BIT_Q;
    return (uint16_t)a;
}

int poly_sample_S1(poly *a, const uint8_t *seed, uint16_t *nonce)
{
    uint8_t buf[BIT_SAMPLE_S1_BUFLEN];

    if (bit_xof256_nonce(buf, sizeof(buf), seed, BIT_SEEDBYTES, *nonce) != 0) {
        bit_secure_zero(buf, sizeof(buf));
        return -1;
    }
    *nonce = (uint16_t)(*nonce + 1U);
    unpack_S1_4bit(a->coeffs, buf);
    bit_secure_zero(buf, sizeof(buf));
    return 0;
}

int poly_sample_S1_x4(poly *out[4], size_t count, const uint8_t *seed, uint16_t *nonce)
{
    if (count == 0 || count > 4)
        return -1;
#if !BIT_USE_SHAKE
    for (size_t lane = 0; lane < count; lane++)
        if (poly_sample_S1(out[lane], seed, nonce) != 0) {
            return -1;
        }
#else
    BIT_ALIGN_64 uint8_t msg[4][BIT_SEEDBYTES + sizeof(uint16_t)];
    BIT_ALIGN_64 uint8_t buf[4][BIT_SAMPLE_S1_BUFLEN];

    for (size_t lane = 0; lane < 4; lane++) {
        uint16_t lane_nonce = (uint16_t)(*nonce + (lane < count ? lane : 0));

        memcpy(msg[lane], seed, BIT_SEEDBYTES);
        store16_le(msg[lane] + BIT_SEEDBYTES, lane_nonce);
    }
    shake256x4(buf[0], buf[1], buf[2], buf[3], BIT_SAMPLE_S1_BUFLEN, msg[0], msg[1], msg[2], msg[3],
               sizeof(msg[0]));
    for (size_t lane = 0; lane < count; lane++)
        unpack_S1_4bit(out[lane]->coeffs, buf[lane]);
    *nonce = (uint16_t)(*nonce + count);
    bit_secure_zero(msg, sizeof(msg));
    bit_secure_zero(buf, sizeof(buf));
#endif
    return 0;
}

static inline void poly_to_ntt_impl(poly_ntt *result, const poly *a, poly *centered)
{
    for (int i = 0; i < BIT_N; i += 16) {
        __m256i value = reduce_to_unsigned_i16x16(
            _mm256_loadu_si256((const __m256i *)&a->coeffs[i]));
        _mm256_storeu_si256((__m256i *)&result->coeffs[i], value);
        if (centered != NULL)
            _mm256_storeu_si256((__m256i *)&centered->coeffs[i],
                                reduce_to_signed_i16x16(value));
    }
    ntt_fwd_asm(result->coeffs);
}

void poly_to_ntt(poly_ntt *result, const poly *a)
{
    poly_to_ntt_impl(result, a, NULL);
}

void poly_to_ntt_centered(poly_ntt *result, poly *a)
{
    poly_to_ntt_impl(result, a, a);
}

void poly_from_ntt_inplace(poly *result, poly_ntt *a)
{
    ntt_inv_asm(a->coeffs);
    for (int i = 0; i < BIT_N; i += 16)
        _mm256_storeu_si256(
            (__m256i *)&result->coeffs[i],
            reduce_to_signed_i16x16(_mm256_loadu_si256((const __m256i *)&a->coeffs[i])));
}

void poly_ntt_basemul_raw(poly_ntt *result, const poly_ntt *a, const poly_ntt *b)
{
    ntt_basemul_asm(result->coeffs, a->coeffs, b->coeffs);
}

void poly_ntt_dot3_raw(poly_ntt *result, const poly_ntt a[BIT_L], const poly_ntt b[BIT_L])
{
    ntt_dot3_asm(result->coeffs, (const int16_t (*)[BIT_N])a, (const int16_t (*)[BIT_N])b);
}

void poly_ntt_montgomery_lift(poly_ntt *result)
{
    ntt_montgomery_lift_asm(result->coeffs);
}

void poly_add(poly *result, const poly *a, const poly *b)
{
    for (int i = 0; i < BIT_N; i += 16) {
        __m256i av = _mm256_loadu_si256((const __m256i *)&a->coeffs[i]);
        __m256i bv = _mm256_loadu_si256((const __m256i *)&b->coeffs[i]);
        _mm256_storeu_si256((__m256i *)&result->coeffs[i],
                            reduce_sum_signed_i16x16(_mm256_add_epi16(av, bv)));
    }
}

void poly_add_no_reduce(poly *result, const poly *a, const poly *b)
{
    for (int i = 0; i < BIT_N; i += 16) {
        __m256i av = _mm256_loadu_si256((const __m256i *)&a->coeffs[i]);
        __m256i bv = _mm256_loadu_si256((const __m256i *)&b->coeffs[i]);
        _mm256_storeu_si256((__m256i *)&result->coeffs[i], _mm256_add_epi16(av, bv));
    }
}

void poly_add_small(poly *result, const poly *a, const poly *small)
{
    for (int i = 0; i < BIT_N; i += 16) {
        __m256i av = _mm256_loadu_si256((const __m256i *)&a->coeffs[i]);
        __m256i bv = _mm256_loadu_si256((const __m256i *)&small->coeffs[i]);
        _mm256_storeu_si256((__m256i *)&result->coeffs[i],
                            reduce_sum_signed_i16x16(_mm256_add_epi16(av, bv)));
    }
}

void poly_highbits(poly *w1, const poly *w)
{
    for (int i = 0; i < BIT_N; i += 16) {
        __m256i value = reduce_to_unsigned_i16x16(
            _mm256_loadu_si256((const __m256i *)&w->coeffs[i]));
        _mm256_storeu_si256((__m256i *)&w1->coeffs[i], bit_highbits_i16x16(value));
    }
}

void poly_canonicalize(poly *b)
{
    for (int i = 0; i < BIT_N; i++)
        b->coeffs[i] = reduce_to_unsigned(b->coeffs[i]);
}

void poly_pack_w1(unsigned char *r, const poly *a)
{
    bit_pack_w1_5(r, a->coeffs);
}

int poly_challenge(poly *c, const unsigned char seed[BIT_CHALLENGEBYTES])
{
    unsigned int i, b, pos;
    uint8_t buf[BIT_XOF256_RATE];
    uint8_t sign_bits[(BIT_TAU + 7) / 8];
    bit_xof256_ctx ctx;

    bit_xof256_ctx_init(&ctx, seed, BIT_CHALLENGEBYTES);
    if (bit_xof256_ctx_squeezeblocks(&ctx, buf, 1) != 0) {
        bit_xof256_ctx_zeroize(&ctx);
        bit_secure_zero(buf, sizeof(buf));
        return -1;
    }
    for (size_t j = 0; j < sizeof(sign_bits); j++)
        sign_bits[j] = buf[j];
    BIT_CT_HAETAE_PUBLIC(sign_bits, sizeof sign_bits);
    pos = (unsigned int)sizeof(sign_bits);

    for (i = 0; i < BIT_N; ++i) {
        c->coeffs[i] = 0;
    }

    for (i = BIT_N - BIT_TAU; i < BIT_N; ++i) {
        do {
            if (pos + 1 >= BIT_XOF256_RATE) {
                if (bit_xof256_ctx_squeezeblocks(&ctx, buf, 1) != 0) {
                    bit_xof256_ctx_zeroize(&ctx);
                    bit_secure_zero(buf, sizeof(buf));
                    return -1;
                }
                pos = 0;
            }

            b = load16_le(buf + pos) & 0x01FFU;
            BIT_CT_HAETAE_PUBLIC(&b, sizeof b);
            pos += 2;
        } while (b > i);

        c->coeffs[i] = c->coeffs[b];
        {
            unsigned int t = i - (BIT_N - BIT_TAU);
            c->coeffs[b] = 1 - 2 * ((sign_bits[t >> 3] >> (t & 7)) & 1U);
        }
    }
    bit_xof256_ctx_zeroize(&ctx);
    return 0;
}

void poly_challenge_to_sparse(sparse_challenge *s, const poly *c)
{
    int ctr = 0;
    const __m256i zero = _mm256_setzero_si256();
    const __m256i ones = _mm256_set1_epi16(-1);

    s->bflip = 0;

    for (int i = 0; i < BIT_N && ctr < BIT_TAU; i += 16) {
        __m256i v = _mm256_loadu_si256((const __m256i *)&c->coeffs[i]);
        __m256i nonzero = _mm256_xor_si256(_mm256_cmpeq_epi16(v, zero), ones);
        __m256i bytes = _mm256_packs_epi16(nonzero, nonzero);
        uint32_t byte_mask = (uint32_t)_mm256_movemask_epi8(bytes);
        uint32_t lane_mask = (byte_mask & 0xFFU) | ((byte_mask >> 8) & 0xFF00U);

        while (lane_mask != 0 && ctr < BIT_TAU) {
            unsigned int lane = (unsigned int)__builtin_ctz(lane_mask);
            s->pos[ctr] = (uint16_t)(i + (int)lane);
            s->sign[ctr] = (int8_t)c->coeffs[s->pos[ctr]];
            ctr++;
            lane_mask &= lane_mask - 1U;
        }
    }
}

void poly_cneg(poly *a, uint8_t b)
{
    const __m256i mask = _mm256_set1_epi16((int16_t)-(int32_t)b);

    for (int i = 0; i < BIT_N; i += 16) {
        __m256i av = _mm256_loadu_si256((const __m256i *)&a->coeffs[i]);
        av = _mm256_sub_epi16(_mm256_xor_si256(av, mask), mask);
        _mm256_storeu_si256((__m256i *)&a->coeffs[i], av);
    }
}

void poly_mul_challenge_no_reduce(poly *result, const poly *a, const sparse_challenge *c)
{
    int16_t acc[BIT_N];
    const int16_t mask = (int16_t)(-(int32_t)c->bflip);

    for (int i = 0; i < BIT_N; i++) {
        acc[i] = 0;
    }

    /* Accumulate the two contiguous parts of each negacyclic rotation. */
    for (int t = 0; t < BIT_TAU; t++) {
        int i = c->pos[t];
        if (c->sign[t] > 0) {
            for (int j = 0; j < BIT_N - i; j++)
                acc[i + j] += a->coeffs[j];
            for (int j = BIT_N - i; j < BIT_N; j++)
                acc[i + j - BIT_N] -= a->coeffs[j];
        } else {
            for (int j = 0; j < BIT_N - i; j++)
                acc[i + j] -= a->coeffs[j];
            for (int j = BIT_N - i; j < BIT_N; j++)
                acc[i + j - BIT_N] += a->coeffs[j];
        }
    }

    for (int i = 0; i < BIT_N; i++) {
        result->coeffs[i] = (int16_t)((acc[i] ^ mask) - mask);
    }
}

int poly_reject_highbits_bimodal(const poly *w1, const poly *w0, const sparse_challenge *c)
{
    uint32_t reject_flag = 0;
    const int32_t signed_one = 1 - 2 * (int32_t)c->bflip;

    for (int t = 0; t < BIT_TAU; t++) {
        int i = c->pos[t];
        int16_t hb;
        int32_t w_prime_val = (int32_t)w0->coeffs[i] + (int32_t)c->sign[t] * signed_one;

        decompose_hint(&hb, w_prime_val);
        reject_flag |= (uint32_t)((uint16_t)w1->coeffs[i] ^ (uint16_t)hb);
    }

    return (reject_flag != 0) ? 1 : 0;
}
