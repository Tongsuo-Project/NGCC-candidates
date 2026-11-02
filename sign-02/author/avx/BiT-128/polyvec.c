/*
 * Copyright (c) 2026 Hang Zhang.
 * State Key Laboratory of Cyberspace Security Defense,
 * Institute of Information Engineering, CAS
 * School of Cyber Security, University of Chinese Academy of Sciences
 */
#include "polyvec.h"
#include "../common/highbits.h"
#include "align.h"
#include "endian.h"
#include "ntt.h"
#include "symmetric.h"
#include "fips202x4.h"
#include "params.h"
#include <stddef.h>
#include <string.h>

#include "uniform_compact_table.inc"

static inline __m256i sign_extend_low16(__m256i x)
{
    return _mm256_srai_epi32(_mm256_slli_epi32(x, 16), 16);
}

static inline __m256i reduce_sum_signed_i16x16(__m256i x)
{
    const __m256i lower = _mm256_set1_epi16(-BIT_Q_HALF);
    const __m256i half = _mm256_set1_epi16(BIT_Q_HALF);
    const __m256i q = _mm256_set1_epi16(BIT_Q);

    x = _mm256_add_epi16(x, _mm256_and_si256(_mm256_cmpgt_epi16(lower, x), q));
    return _mm256_sub_epi16(x, _mm256_and_si256(_mm256_cmpgt_epi16(x, half), q));
}

static inline __m256i mul_qhat_centered_i16x16(__m256i x)
{
    const __m256i one = _mm256_set1_epi16(1);
    const __m256i qhat = _mm256_set1_epi16(BIT_Q_HAT);
    __m256i parity = _mm256_and_si256(x, one);
    __m256i result = _mm256_add_epi16(_mm256_srai_epi16(x, 1), _mm256_mullo_epi16(parity, qhat));

    /* qhat=(q+1)/2: x=2k+b gives qhat*x == k+b*qhat (mod q). */
    return reduce_sum_signed_i16x16(result);
}

static inline __m256i decompose_hint_i32x8(__m256i r)
{
    const __m256i q = _mm256_set1_epi32(BIT_Q);
    __m256i val = r;
    const __m256i qm1 = _mm256_set1_epi32(BIT_Q - 1);
    /* |r| <= q/2 + gamma1 < 3q. */
    for (int i = 0; i < 3; i++) {
        val = _mm256_add_epi32(val, _mm256_and_si256(_mm256_srai_epi32(val, 31), q));
        val = _mm256_sub_epi32(val, _mm256_and_si256(_mm256_cmpgt_epi32(val, qm1), q));
    }
    return bit_highbits_i32x8(val);
}

static inline void store_i32x8_as_i16(int16_t out[8], __m256i x)
{
    __m128i lo = _mm256_castsi256_si128(x);
    __m128i hi = _mm256_extracti128_si256(x, 1);
    _mm_storeu_si128((__m128i *)out, _mm_packs_epi32(lo, hi));
}

int polyvec_sample_S1_keypair(polyvecl *s, polyveck *e, const uint8_t *seed, uint16_t *nonce)
{
    poly *first[4] = {&s->vec[0], &s->vec[1], &s->vec[2], &e->vec[0]};
    poly *last[4] = {&e->vec[1], &e->vec[2], &e->vec[2], &e->vec[2]};

    if (poly_sample_S1_x4(first, 4, seed, nonce) != 0) {
        return -1;
    }
    if (poly_sample_S1_x4(last, 2, seed, nonce) != 0) {
        return -1;
    }
    return 0;
}

#define MATRIX_XOF_BUFFER_SIZE (3 * BIT_XOF128_RATE)

static void poly_uniform_parse(poly_ntt *a, size_t *ctr, const unsigned char *buf, size_t buflen)
{
    size_t pos = 0;
    uint32_t reservoir = 0;
    unsigned int reservoir_bits = 0;
    const __m256i shifts = _mm256_setr_epi32(0, 5, 2, 7, 4, 1, 6, 3);
    const __m256i mask13 = _mm256_set1_epi32(0x1FFF);
    const __m256i q_minus_one = _mm256_set1_epi32(BIT_Q - 1);
    const __m256i gather = _mm256_setr_epi8(0, 1, 2, 3, 1, 2, 3, 4, 3, 4, 5, 6, 4, 5, 6, 7, 6, 7, 8,
                                            9, 8, 9, 10, 11, 9, 10, 11, 12, 11, 12, 13, 14);

    /* Decode eight 13-bit candidates per 13 bytes; handle the remainder below. */
    while (*ctr + 8 <= BIT_N && pos + 16 <= buflen) {
        __m128i input = _mm_loadu_si128((const __m128i *)(buf + pos));
        __m256i windows = _mm256_shuffle_epi8(_mm256_broadcastsi128_si256(input), gather);
        __m256i decoded = _mm256_and_si256(_mm256_srlv_epi32(windows, shifts), mask13);
        __m256i invalid = _mm256_cmpgt_epi32(decoded, q_minus_one);
        uint32_t valid_mask = (~(uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(invalid))) & 0xFFU;

        pos += 13;
        if (valid_mask == 0xFFU) {
            __m128i packed = _mm_packs_epi32(_mm256_castsi256_si128(decoded),
                                             _mm256_extracti128_si256(decoded, 1));
            _mm_storeu_si128((__m128i *)&a->coeffs[*ctr], packed);
            *ctr += 8;
        } else if (valid_mask != 0) {
            /* Matrix coefficients are public. Compact valid lanes in order. */
            __m128i indices8 =
                _mm_cvtsi64_si128((long long)bit_uniform_compact_indices[valid_mask]);
            __m256i indices = _mm256_cvtepu8_epi32(indices8);
            __m256i compacted = _mm256_permutevar8x32_epi32(decoded, indices);
            __m128i packed = _mm_packs_epi32(_mm256_castsi256_si128(compacted),
                                             _mm256_extracti128_si256(compacted, 1));

            _mm_storeu_si128((__m128i *)&a->coeffs[*ctr], packed);
            *ctr += (size_t)__builtin_popcount(valid_mask);
        }
    }

    while (*ctr < BIT_N) {
        uint16_t val;

        while (reservoir_bits < 13 && pos < buflen) {
            reservoir |= (uint32_t)buf[pos++] << reservoir_bits;
            reservoir_bits += 8;
        }
        if (reservoir_bits < 13)
            break;

        val = (uint16_t)(reservoir & 0x1FFFU);
        reservoir >>= 13;
        reservoir_bits -= 13;
        if (val < BIT_Q)
            a->coeffs[(*ctr)++] = (int16_t)val;
    }
}

static int poly_uniform_ntt(poly_ntt *a, const unsigned char *seed, size_t row, size_t col)
{
    BIT_ALIGN_64 unsigned char buf[MATRIX_XOF_BUFFER_SIZE];
    unsigned char msg[BIT_SEEDBYTES + 2];
    bit_xof128_state st;
    size_t ctr = 0;

    memcpy(msg, seed, BIT_SEEDBYTES);
    msg[BIT_SEEDBYTES] = (unsigned char)col;
    msg[BIT_SEEDBYTES + 1] = (unsigned char)row;

    bit_xof128_init(&st, msg, sizeof(msg));
    while (ctr < BIT_N) {
        if (bit_xof128_squeeze(&st, buf, sizeof(buf)) != 0) {
            bit_xof128_zeroize(&st);
            return -1;
        }
        poly_uniform_parse(a, &ctr, buf, sizeof(buf));
    }
    bit_xof128_zeroize(&st);
    ntt_pack_asm(a->coeffs);
    return 0;
}

#if BIT_USE_SHAKE
static void poly_uniform_ntt_x4(poly_ntt *a0, poly_ntt *a1, poly_ntt *a2, poly_ntt *a3,
                                const unsigned char *seed, size_t index0)
{
    BIT_ALIGN_64 unsigned char buf[4][MATRIX_XOF_BUFFER_SIZE];
    unsigned char msg[4][BIT_SEEDBYTES + 2];
    poly_ntt *a[4] = {a0, a1, a2, a3};
    size_t ctr[4] = {0, 0, 0, 0};
    keccakx4_state st;

    for (size_t lane = 0; lane < 4; lane++) {
        size_t index = index0 + lane;

        memcpy(msg[lane], seed, BIT_SEEDBYTES);
        msg[lane][BIT_SEEDBYTES] = (unsigned char)(index % BIT_L);
        msg[lane][BIT_SEEDBYTES + 1] = (unsigned char)(index / BIT_L);
    }

    shake128x4_absorb_once(&st, msg[0], msg[1], msg[2], msg[3], sizeof(msg[0]));
    while (ctr[0] < BIT_N || ctr[1] < BIT_N || ctr[2] < BIT_N || ctr[3] < BIT_N) {
        shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3],
                                 MATRIX_XOF_BUFFER_SIZE / SHAKE128_RATE, &st);
        for (size_t lane = 0; lane < 4; lane++) {
            if (ctr[lane] < BIT_N)
                poly_uniform_parse(a[lane], &ctr[lane], buf[lane], sizeof(buf[lane]));
        }
    }

    bit_secure_zero(&st, sizeof(st));
    bit_secure_zero(buf, sizeof(buf));
    bit_secure_zero(msg, sizeof(msg));
    for (size_t lane = 0; lane < 4; lane++)
        ntt_pack_asm(a[lane]->coeffs);
}
#endif

int poly_matrix_expand_ntt(poly_matrix_ntt *A_ntt, const unsigned char *seed)
{
#if !BIT_USE_SHAKE
    for (size_t row = 0; row < BIT_K; row++)
        for (size_t col = 0; col < BIT_L; col++)
            if (poly_uniform_ntt(&A_ntt->vec[row].vec[col], seed, row, col) != 0) {
                return -1;
            }
#else
    poly_uniform_ntt_x4(&A_ntt->vec[0].vec[0], &A_ntt->vec[0].vec[1], &A_ntt->vec[0].vec[2],
                        &A_ntt->vec[1].vec[0], seed, 0);
    poly_uniform_ntt_x4(&A_ntt->vec[1].vec[1], &A_ntt->vec[1].vec[2], &A_ntt->vec[2].vec[0],
                        &A_ntt->vec[2].vec[1], seed, 4);
    if (poly_uniform_ntt(&A_ntt->vec[2].vec[2], seed, 2, 2) != 0) {
        return -1;
    }
#endif
    return 0;
}

void polyvecl_to_ntt(polyvecl_ntt *result, const polyvecl *v)
{
    for (int i = 0; i < BIT_L; i++) {
        poly_to_ntt(&result->vec[i], &v->vec[i]);
    }
}

void polyvecy_y1_to_ntt(polyvecl_ntt *result, const polyvecy *y)
{
    for (int i = 0; i < BIT_L; i++) {
        poly_to_ntt(&result->vec[i], &y->vec[1 + i]);
    }
}

void polyveck_from_ntt(polyveck *result, polyveck_ntt *v_ntt)
{
    for (int i = 0; i < BIT_K; i++) {
        poly_from_ntt_inplace(&result->vec[i], &v_ntt->vec[i]);
    }
}

void poly_matrix_mul_vector_ntt(polyveck *result, const poly_matrix_ntt *A_ntt,
                                const polyvecl_ntt *v_ntt)
{
    polyveck_ntt acc;

    for (int i = 0; i < BIT_K; i++) {
        poly_ntt_dot3_raw(&acc.vec[i], A_ntt->vec[i].vec, v_ntt->vec);
        poly_ntt_montgomery_lift(&acc.vec[i]);
    }

    polyveck_from_ntt(result, &acc);
}

void polyveck_add_small(polyveck *result, const polyveck *a, const polyveck *small)
{
    for (int i = 0; i < BIT_K; i++) {
        poly_add_small(&result->vec[i], &a->vec[i], &small->vec[i]);
    }
}

void polyveck_canonicalize(polyveck *b)
{
    for (int i = 0; i < BIT_K; i++)
        poly_canonicalize(&b->vec[i]);
}

void polyveck_public_to_ntt(polyveck_ntt *b_ntt, const polyveck *b)
{
    for (int i = 0; i < BIT_K; i++)
        poly_to_ntt(&b_ntt->vec[i], &b->vec[i]);
}

void polyveck_compute_w_ntt(polyveck *w, const poly_matrix_ntt *A_0_ntt, const polyveck_ntt *b_ntt,
                            const polyvecl_ntt *y1_ntt, const poly_ntt *y0_ntt, const polyvecy *y)
{
    polyveck_ntt w_ntt;
    poly_ntt tmp_ntt;

    for (int i = 0; i < BIT_K; i++) {
        poly_ntt_dot3_raw(&w_ntt.vec[i], A_0_ntt->vec[i].vec, y1_ntt->vec);
        poly_ntt_basemul_raw(&tmp_ntt, &b_ntt->vec[i], y0_ntt);
        for (int j = 0; j < BIT_N; j += 16) {
            /* Keep the NTT-domain subtraction lazy.  Three accumulated
             * products minus one product remain inside int16_t; the lift
             * below performs the next Montgomery reduction. */
            __m256i acc = _mm256_loadu_si256((const __m256i *)&w_ntt.vec[i].coeffs[j]);
            __m256i product = _mm256_loadu_si256((const __m256i *)&tmp_ntt.coeffs[j]);
            _mm256_storeu_si256((__m256i *)&w_ntt.vec[i].coeffs[j], _mm256_sub_epi16(acc, product));
        }
        poly_ntt_montgomery_lift(&w_ntt.vec[i]);
    }
    polyveck_from_ntt(w, &w_ntt);

    for (int j = 0; j < BIT_N; j += 16) {
        __m256i val = _mm256_loadu_si256((const __m256i *)&w->vec[0].coeffs[j]);
        __m256i y0 = _mm256_loadu_si256((const __m256i *)&y->vec[0].coeffs[j]);
        __m256i qhat_y0 = mul_qhat_centered_i16x16(y0);

        _mm256_storeu_si256((__m256i *)&w->vec[0].coeffs[j],
                            reduce_sum_signed_i16x16(_mm256_add_epi16(val, qhat_y0)));
    }

    for (int i = 0; i < BIT_K; i++) {
        poly_add(&w->vec[i], &w->vec[i], &y->vec[1 + BIT_L + i]);
    }
}

void polyveck_highbits(polyveck *w1, const polyveck *w)
{
    for (int i = 0; i < BIT_K; i++) {
        poly_highbits(&w1->vec[i], &w->vec[i]);
    }
}

void polyveck_pack_w1(unsigned char *r, const polyveck *w1)
{
    for (int i = 0; i < BIT_K; i++) {
        poly_pack_w1(r + i * BIT_POLY_W1_BYTES, &w1->vec[i]);
    }
}

/* Six base-2^7 digits hold the ternary s0/e products. With U=2 and
 * tau=30, accumulated digits lie in [30,90], without carries. */
#if BIT_L != 3 || BIT_K != 3 || BIT_TAU > 30
#error "the packed s/e challenge multiplier is specialized for 3+3 polynomials and tau<=30"
#endif

#define PSPM_SE_SHIFT 7
#define PSPM_SE_MASK ((UINT64_C(1) << PSPM_SE_SHIFT) - 1)
#define PSPM_SE_U 2
#define PSPM_SE_BIAS (BIT_TAU * PSPM_SE_U)

void polyvec_prepare_challenge_mul_se(bit_pspm_se_precomp *table, const polyvecl *s,
                                      const polyveck *e)
{
    const poly *src[BIT_L + BIT_K] = {&s->vec[0], &s->vec[1], &s->vec[2],
                                      &e->vec[0], &e->vec[1], &e->vec[2]};

    for (int i = 0; i < BIT_N; i++) {
        uint64_t plus = 0;
        uint64_t minus = 0;

        for (int j = 0; j < BIT_L + BIT_K; j++) {
            int32_t a = src[j]->coeffs[i];
            plus |= (uint64_t)(PSPM_SE_U + a) << (PSPM_SE_SHIFT * j);
            minus |= (uint64_t)(PSPM_SE_U - a) << (PSPM_SE_SHIFT * j);
        }
        table->words[BIT_N + i] = plus;
        table->words[i] = minus;
    }
}

static void pspm_add_neg_words(uint64_t *dst, const uint64_t *src, uint64_t twice_bias)
{
    const __m256i bias = _mm256_set1_epi64x((int64_t)twice_bias);

    for (size_t i = 0; i < BIT_N; i += 4) {
        __m256i d = _mm256_loadu_si256((const __m256i *)(dst + i));
        __m256i x = _mm256_loadu_si256((const __m256i *)(src + i));
        _mm256_storeu_si256((__m256i *)(dst + i), _mm256_add_epi64(d, _mm256_sub_epi64(bias, x)));
    }
}

static void pspm_add_words(uint64_t *dst, const uint64_t *src, size_t count)
{
    size_t i = 0;

    for (; i + 4 <= count; i += 4) {
        __m256i d = _mm256_loadu_si256((const __m256i *)(dst + i));
        __m256i x = _mm256_loadu_si256((const __m256i *)(src + i));
        _mm256_storeu_si256((__m256i *)(dst + i), _mm256_add_epi64(d, x));
    }
    for (; i < count; i++)
        dst[i] += src[i];
}

void polyvec_mul_challenge_no_reduce_se_pspm(polyvecl *c_s, polyveck *c_e,
                                             const bit_pspm_se_precomp *table,
                                             const sparse_challenge *c)
{
    BIT_ALIGN uint64_t acc[BIT_N] = {0};
    poly *dst[BIT_L + BIT_K] = {&c_s->vec[0], &c_s->vec[1], &c_s->vec[2],
                                &c_e->vec[0], &c_e->vec[1], &c_e->vec[2]};
    const int32_t sign_mask = -(int32_t)c->bflip;
    uint64_t twice_bias = 0;

    for (int j = 0; j < BIT_L + BIT_K; j++)
        twice_bias |= (uint64_t)(2 * PSPM_SE_U) << (PSPM_SE_SHIFT * j);

    for (int t = 0; t < BIT_TAU; t++) {
        size_t rot = c->pos[t];

        /* A negacyclic rotation is an N-word window in the negative/positive table. */
        const uint64_t *window = table->words + BIT_N - rot;
        if (c->sign[t] > 0)
            pspm_add_words(acc, window, BIT_N);
        else
            pspm_add_neg_words(acc, window, twice_bias);
    }

    for (int j = 0; j < BIT_L + BIT_K; j++) {
        unsigned int shift = PSPM_SE_SHIFT * (unsigned int)j;

        for (int i = 0; i < BIT_N; i++) {
            int32_t value = (int32_t)((acc[i] >> shift) & PSPM_SE_MASK) - PSPM_SE_BIAS;
            dst[j]->coeffs[i] = (int16_t)((value ^ sign_mask) - sign_mask);
        }
    }
}

#undef PSPM_SE_SHIFT
#undef PSPM_SE_MASK
#undef PSPM_SE_U
#undef PSPM_SE_BIAS

void polyveck_make_hint(polyveck *h, const polyveck *w1, const polyveck *w,
                          const polyveck *z2, const poly *c, uint8_t b)
{
    const __m256i add_c_mask = _mm256_set1_epi32(-(int32_t)(1U - b));
    const __m256i modulus = _mm256_set1_epi32(BIT_HINT_MODULUS);
    const __m256i half = _mm256_set1_epi32(BIT_HINT_MODULUS / 2);

    for (int i = 0; i < BIT_K; i++) {
        for (int j = 0; j < BIT_N; j += 8) {
            __m256i wv =
                _mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i *)&w->vec[i].coeffs[j]));
            __m256i z2v =
                _mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i *)&z2->vec[i].coeffs[j]));
            __m256i w_hat = _mm256_sub_epi32(wv, z2v);
            __m256i w1v;
            __m256i h_val;
            __m256i over_half;

            if (i == 0) {
                __m256i cv = _mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i *)&c->coeffs[j]));
                w_hat = _mm256_add_epi32(w_hat, _mm256_and_si256(cv, add_c_mask));
            }
            w_hat = decompose_hint_i32x8(sign_extend_low16(w_hat));
            w1v = _mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i *)&w1->vec[i].coeffs[j]));
            h_val = _mm256_sub_epi32(w1v, w_hat);
            h_val =
                _mm256_add_epi32(h_val, _mm256_and_si256(_mm256_srai_epi32(h_val, 31), modulus));
            over_half = _mm256_cmpgt_epi32(h_val, half);
            h_val = _mm256_sub_epi32(h_val, _mm256_and_si256(over_half, modulus));
            store_i32x8_as_i16(&h->vec[i].coeffs[j], h_val);
        }
    }
}

void polyveck_compute_w_hat_prime_ntt(polyveck *w_hat_prime, const poly_matrix_ntt *A_0_ntt,
                                      const polyveck_ntt *public_ntt, const polyvecl_ntt *z1_tail_ntt,
                                      const poly_ntt *z0_ntt, const poly *z0, const poly *c_poly)
{
    polyveck_ntt acc;
    poly_ntt tmp_ntt;

    for (int i = 0; i < BIT_K; i++) {
        poly_ntt_dot3_raw(&acc.vec[i], A_0_ntt->vec[i].vec, z1_tail_ntt->vec);
        poly_ntt_basemul_raw(&tmp_ntt, &public_ntt->vec[i], z0_ntt);
        for (int j = 0; j < BIT_N; j += 16) {
            __m256i av = _mm256_loadu_si256((const __m256i *)&acc.vec[i].coeffs[j]);
            __m256i bv = _mm256_loadu_si256((const __m256i *)&tmp_ntt.coeffs[j]);
            _mm256_storeu_si256((__m256i *)&acc.vec[i].coeffs[j], _mm256_sub_epi16(av, bv));
        }
        poly_ntt_montgomery_lift(&acc.vec[i]);
    }

    polyveck_from_ntt(w_hat_prime, &acc);

    for (int j = 0; j < BIT_N; j += 16) {
        __m256i val = _mm256_loadu_si256((const __m256i *)&w_hat_prime->vec[0].coeffs[j]);
        __m256i z0v = _mm256_loadu_si256((const __m256i *)&z0->coeffs[j]);
        __m256i cv = _mm256_loadu_si256((const __m256i *)&c_poly->coeffs[j]);
        __m256i qhat_sum = mul_qhat_centered_i16x16(_mm256_add_epi16(z0v, cv));

        _mm256_storeu_si256((__m256i *)&w_hat_prime->vec[0].coeffs[j],
                            reduce_sum_signed_i16x16(_mm256_add_epi16(val, qhat_sum)));
    }
}

void polyveck_compute_w_prime(polyveck *w_prime, const polyveck *w_hat_prime, const polyveck *h)
{
    const __m256i modulus = _mm256_set1_epi32(BIT_HINT_MODULUS);

    for (int i = 0; i < BIT_K; i++) {
        for (int j = 0; j < BIT_N; j += 8) {
            __m256i r = _mm256_cvtepi16_epi32(
                _mm_loadu_si128((const __m128i *)&w_hat_prime->vec[i].coeffs[j]));
            __m256i hint =
                _mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i *)&h->vec[i].coeffs[j]));
            __m256i val = _mm256_add_epi32(decompose_hint_i32x8(r), hint);

            val = _mm256_add_epi32(val, _mm256_and_si256(_mm256_srai_epi32(val, 31), modulus));
            val = _mm256_sub_epi32(val, modulus);
            val = _mm256_add_epi32(val, _mm256_and_si256(_mm256_srai_epi32(val, 31), modulus));
            store_i32x8_as_i16(&w_prime->vec[i].coeffs[j], val);
        }
    }
}
