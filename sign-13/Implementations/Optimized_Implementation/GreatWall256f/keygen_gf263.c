#include "keygen_gf263.h"
#include "gf263.h"
#include "greatwall.h"
#include "owf_alpha.h"
#include "timing.h"
#define GREATWALL_KEYGEN_GF263_LOCAL_ARITH 1
static inline void keygen_gf263_square(block263* out, const block263* in)
{
#if GREATWALL_KEYGEN_GF263_LOCAL_ARITH
    assert(block263_is_canonical(in));
    const uint64_t x0 = in->data[0];
    const uint64_t x1 = in->data[1];
    const uint64_t x2 = in->data[2];
    const uint64_t x3 = in->data[3];
    const uint64_t x4 = in->data[4];
    const __m128i table = _mm_set_epi64x(0x5554515045444140, 0x1514111005040100);
    const __m128i mask = _mm_set1_epi8(15);
    const __m128i a0 = _mm_set_epi64x(x1, x0);
    const __m128i lo0 = _mm_shuffle_epi8(table, _mm_and_si128(a0, mask));
    const __m128i hi0 = _mm_shuffle_epi8(table, _mm_and_si128(_mm_srli_epi64(a0, 4), mask));
    const __m128i p0 = _mm_unpacklo_epi8(lo0, hi0);
    const __m128i p1 = _mm_unpackhi_epi8(lo0, hi0);
    const __m128i a1 = _mm_set_epi64x(x3, x2);
    const __m128i lo1 = _mm_shuffle_epi8(table, _mm_and_si128(a1, mask));
    const __m128i hi1 = _mm_shuffle_epi8(table, _mm_and_si128(_mm_srli_epi64(a1, 4), mask));
    const __m128i p2 = _mm_unpacklo_epi8(lo1, hi1);
    const __m128i p3 = _mm_unpackhi_epi8(lo1, hi1);
    const __m128i a2 = _mm_set_epi64x(0, x4);
    const __m128i lo2 = _mm_shuffle_epi8(table, _mm_and_si128(a2, mask));
    const __m128i hi2 = _mm_shuffle_epi8(table, _mm_and_si128(_mm_srli_epi64(a2, 4), mask));
    const __m128i p4 = _mm_unpacklo_epi8(lo2, hi2);
    const uint64_t w0 = (uint64_t)_mm_cvtsi128_si64(p0);
    const uint64_t w1 = (uint64_t)_mm_extract_epi64(p0, 1);
    const uint64_t w2 = (uint64_t)_mm_cvtsi128_si64(p1);
    const uint64_t w3 = (uint64_t)_mm_extract_epi64(p1, 1);
    const uint64_t w4 = (uint64_t)_mm_cvtsi128_si64(p2);
    const uint64_t w5 = (uint64_t)_mm_extract_epi64(p2, 1);
    const uint64_t w6 = (uint64_t)_mm_cvtsi128_si64(p3);
    const uint64_t w7 = (uint64_t)_mm_extract_epi64(p3, 1);
    const uint64_t w8 = (uint64_t)_mm_cvtsi128_si64(p4);
    const uint64_t h0 = (w4 >> 7) | (w5 << 57);
    const uint64_t h1 = (w5 >> 7) | (w6 << 57);
    const uint64_t h2 = (w6 >> 7) | (w7 << 57);
    const uint64_t h3 = (w7 >> 7) | (w8 << 57);
    const uint64_t h4 = w8 >> 7;
    const uint64_t t0 = w0 ^ h0;
    const uint64_t t1 = w1 ^ h1 ^ (h0 << 29);
    const uint64_t t2 = w2 ^ h2 ^ (h1 << 29) ^ (h0 >> 35);
    const uint64_t t3 = w3 ^ h3 ^ (h2 << 29) ^ (h1 >> 35);
    const uint64_t t4 = (w4 & UINT64_C(0x7f)) ^ h4 ^ (h3 << 29) ^ (h2 >> 35);
    const uint64_t t5 = (h4 << 29) ^ (h3 >> 35);
    const uint64_t u0 = (t4 >> 7) | (t5 << 57);
    const uint64_t u1 = t5 >> 7;
    out->data[0] = t0 ^ u0;
    out->data[1] = t1 ^ u1 ^ (u0 << 29);
    out->data[2] = t2 ^ (u1 << 29) ^ (u0 >> 35);
    out->data[3] = t3;
    out->data[4] = t4 & UINT64_C(0x7f);
#else
    block263_sqr(out, in);
#endif
}
static inline void keygen_gf263_multiply(block263* out, const block263* a, const block263* b)
{
#if GREATWALL_KEYGEN_GF263_LOCAL_ARITH
    assert(block263_is_canonical(a) && block263_is_canonical(b));
    const __m128i a0 = _mm_cvtsi64_si128((long long)a->data[0]);
    const __m128i a1 = _mm_cvtsi64_si128((long long)a->data[1]);
    const __m128i a2 = _mm_cvtsi64_si128((long long)a->data[2]);
    const __m128i a3 = _mm_cvtsi64_si128((long long)a->data[3]);
    const __m128i a4 = _mm_cvtsi64_si128((long long)a->data[4]);
    const __m128i b0 = _mm_cvtsi64_si128((long long)b->data[0]);
    const __m128i b1 = _mm_cvtsi64_si128((long long)b->data[1]);
    const __m128i b2 = _mm_cvtsi64_si128((long long)b->data[2]);
    const __m128i b3 = _mm_cvtsi64_si128((long long)b->data[3]);
    const __m128i b4 = _mm_cvtsi64_si128((long long)b->data[4]);
    const __m128i p00 = _mm_clmulepi64_si128(a0, b0, 0x00);
    const __m128i p01 = _mm_clmulepi64_si128(a0, b1, 0x00);
    const __m128i p02 = _mm_clmulepi64_si128(a0, b2, 0x00);
    const __m128i p03 = _mm_clmulepi64_si128(a0, b3, 0x00);
    const __m128i p04 = _mm_clmulepi64_si128(a0, b4, 0x00);
    const __m128i p10 = _mm_clmulepi64_si128(a1, b0, 0x00);
    const __m128i p11 = _mm_clmulepi64_si128(a1, b1, 0x00);
    const __m128i p12 = _mm_clmulepi64_si128(a1, b2, 0x00);
    const __m128i p13 = _mm_clmulepi64_si128(a1, b3, 0x00);
    const __m128i p14 = _mm_clmulepi64_si128(a1, b4, 0x00);
    const __m128i p20 = _mm_clmulepi64_si128(a2, b0, 0x00);
    const __m128i p21 = _mm_clmulepi64_si128(a2, b1, 0x00);
    const __m128i p22 = _mm_clmulepi64_si128(a2, b2, 0x00);
    const __m128i p23 = _mm_clmulepi64_si128(a2, b3, 0x00);
    const __m128i p24 = _mm_clmulepi64_si128(a2, b4, 0x00);
    const __m128i p30 = _mm_clmulepi64_si128(a3, b0, 0x00);
    const __m128i p31 = _mm_clmulepi64_si128(a3, b1, 0x00);
    const __m128i p32 = _mm_clmulepi64_si128(a3, b2, 0x00);
    const __m128i p33 = _mm_clmulepi64_si128(a3, b3, 0x00);
    const __m128i p34 = _mm_clmulepi64_si128(a3, b4, 0x00);
    const __m128i p40 = _mm_clmulepi64_si128(a4, b0, 0x00);
    const __m128i p41 = _mm_clmulepi64_si128(a4, b1, 0x00);
    const __m128i p42 = _mm_clmulepi64_si128(a4, b2, 0x00);
    const __m128i p43 = _mm_clmulepi64_si128(a4, b3, 0x00);
    const __m128i p44 = _mm_clmulepi64_si128(a4, b4, 0x00);
    const uint64_t w0 = (uint64_t)_mm_cvtsi128_si64(p00);
    const uint64_t w1 = (uint64_t)_mm_extract_epi64(p00, 1) ^ (uint64_t)_mm_cvtsi128_si64(p01) ^ (uint64_t)_mm_cvtsi128_si64(p10);
    const uint64_t w2 = (uint64_t)_mm_extract_epi64(p01, 1) ^ (uint64_t)_mm_cvtsi128_si64(p02) ^ (uint64_t)_mm_extract_epi64(p10, 1) ^ (uint64_t)_mm_cvtsi128_si64(p11) ^ (uint64_t)_mm_cvtsi128_si64(p20);
    const uint64_t w3 = (uint64_t)_mm_extract_epi64(p02, 1) ^ (uint64_t)_mm_cvtsi128_si64(p03) ^ (uint64_t)_mm_extract_epi64(p11, 1) ^ (uint64_t)_mm_cvtsi128_si64(p12) ^ (uint64_t)_mm_extract_epi64(p20, 1) ^ (uint64_t)_mm_cvtsi128_si64(p21) ^ (uint64_t)_mm_cvtsi128_si64(p30);
    const uint64_t w4 = (uint64_t)_mm_extract_epi64(p03, 1) ^ (uint64_t)_mm_cvtsi128_si64(p04) ^ (uint64_t)_mm_extract_epi64(p12, 1) ^ (uint64_t)_mm_cvtsi128_si64(p13) ^ (uint64_t)_mm_extract_epi64(p21, 1) ^ (uint64_t)_mm_cvtsi128_si64(p22) ^ (uint64_t)_mm_extract_epi64(p30, 1) ^ (uint64_t)_mm_cvtsi128_si64(p31) ^ (uint64_t)_mm_cvtsi128_si64(p40);
    const uint64_t w5 = (uint64_t)_mm_extract_epi64(p04, 1) ^ (uint64_t)_mm_extract_epi64(p13, 1) ^ (uint64_t)_mm_cvtsi128_si64(p14) ^ (uint64_t)_mm_extract_epi64(p22, 1) ^ (uint64_t)_mm_cvtsi128_si64(p23) ^ (uint64_t)_mm_extract_epi64(p31, 1) ^ (uint64_t)_mm_cvtsi128_si64(p32) ^ (uint64_t)_mm_extract_epi64(p40, 1) ^ (uint64_t)_mm_cvtsi128_si64(p41);
    const uint64_t w6 = (uint64_t)_mm_extract_epi64(p14, 1) ^ (uint64_t)_mm_extract_epi64(p23, 1) ^ (uint64_t)_mm_cvtsi128_si64(p24) ^ (uint64_t)_mm_extract_epi64(p32, 1) ^ (uint64_t)_mm_cvtsi128_si64(p33) ^ (uint64_t)_mm_extract_epi64(p41, 1) ^ (uint64_t)_mm_cvtsi128_si64(p42);
    const uint64_t w7 = (uint64_t)_mm_extract_epi64(p24, 1) ^ (uint64_t)_mm_extract_epi64(p33, 1) ^ (uint64_t)_mm_cvtsi128_si64(p34) ^ (uint64_t)_mm_extract_epi64(p42, 1) ^ (uint64_t)_mm_cvtsi128_si64(p43);
    const uint64_t w8 = (uint64_t)_mm_extract_epi64(p34, 1) ^ (uint64_t)_mm_extract_epi64(p43, 1) ^ (uint64_t)_mm_cvtsi128_si64(p44);
    const uint64_t h0 = (w4 >> 7) | (w5 << 57);
    const uint64_t h1 = (w5 >> 7) | (w6 << 57);
    const uint64_t h2 = (w6 >> 7) | (w7 << 57);
    const uint64_t h3 = (w7 >> 7) | (w8 << 57);
    const uint64_t h4 = w8 >> 7;
    const uint64_t t0 = w0 ^ h0;
    const uint64_t t1 = w1 ^ h1 ^ (h0 << 29);
    const uint64_t t2 = w2 ^ h2 ^ (h1 << 29) ^ (h0 >> 35);
    const uint64_t t3 = w3 ^ h3 ^ (h2 << 29) ^ (h1 >> 35);
    const uint64_t t4 = (w4 & UINT64_C(0x7f)) ^ h4 ^ (h3 << 29) ^ (h2 >> 35);
    const uint64_t t5 = (h4 << 29) ^ (h3 >> 35);
    const uint64_t u0 = (t4 >> 7) | (t5 << 57);
    const uint64_t u1 = t5 >> 7;
    out->data[0] = t0 ^ u0;
    out->data[1] = t1 ^ u1 ^ (u0 << 29);
    out->data[2] = t2 ^ (u1 << 29) ^ (u0 >> 35);
    out->data[3] = t3;
    out->data[4] = t4 & UINT64_C(0x7f);
#else
    block263_mul(out, a, b);
#endif
}

static inline void keygen_gf263_square_n(block263* out, const block263* in, unsigned count)
{
    *out = *in;
    for (unsigned i = 0; i < count; ++i)
        keygen_gf263_square(out, out);
}

static inline void keygen_gf263_inverse1(block263* block)
{
#if GREATWALL_GF263_INVERSE_CHAIN
    const block263 x = *block;
    block263 x4, x5, r2, r3, r6, r12, r13, r26, t2, t3, t24, t27, t29;
    keygen_gf263_square_n(&x4, &x, 2);
    keygen_gf263_multiply(&x5, &x4, &x);
    keygen_gf263_square_n(&r2, &x4, 3);
    keygen_gf263_multiply(&r2, &r2, &x);
    keygen_gf263_square_n(&r3, &r2, 5);
    keygen_gf263_multiply(&r3, &r3, &x);
    keygen_gf263_square_n(&r6, &r3, 15);
    keygen_gf263_multiply(&r6, &r6, &r3);
    keygen_gf263_square_n(&r12, &r6, 30);
    keygen_gf263_multiply(&r12, &r12, &r6);
    keygen_gf263_square_n(&r13, &r12, 5);
    keygen_gf263_multiply(&r13, &r13, &x);
    keygen_gf263_square_n(&r26, &r13, 65);
    keygen_gf263_multiply(&r26, &r26, &r13);
    keygen_gf263_square(&t2, &r26);
    keygen_gf263_multiply(&t3, &t2, &r26);
    keygen_gf263_square_n(&t24, &t3, 3);
    keygen_gf263_multiply(&t27, &t24, &t3);
    keygen_gf263_multiply(&t29, &t27, &t2);
    keygen_gf263_square_n(&t29, &t29, 130);
    keygen_gf263_multiply(&t29, &t29, &t27);
    keygen_gf263_square_n(&t29, &t29, 3);
    keygen_gf263_multiply(block, &t29, &x5);
#else
    block263_mersenne_inverse1_binary(block);
#endif
}

static inline void keygen_gf263_inverse2(block263* block)
{
#if GREATWALL_GF263_INVERSE_CHAIN
    const block263 x = *block;
    block263 x3, x15, x255, x511, r2, r3, r6, r7, p, t;
    keygen_gf263_square(&x3, &x);
    keygen_gf263_multiply(&x3, &x3, &x);
    keygen_gf263_square_n(&x15, &x3, 2);
    keygen_gf263_multiply(&x15, &x15, &x3);
    keygen_gf263_square_n(&x255, &x15, 4);
    keygen_gf263_multiply(&x255, &x255, &x15);
    keygen_gf263_square(&x511, &x255);
    keygen_gf263_multiply(&x511, &x511, &x);
    keygen_gf263_square_n(&r2, &x255, 9);
    keygen_gf263_multiply(&r2, &r2, &x255);
    keygen_gf263_square_n(&r3, &r2, 9);
    keygen_gf263_multiply(&r3, &r3, &x255);
    keygen_gf263_square_n(&r6, &r3, 27);
    keygen_gf263_multiply(&r6, &r6, &r3);
    keygen_gf263_square_n(&r7, &r6, 9);
    keygen_gf263_multiply(&r7, &r7, &x255);
    keygen_gf263_square_n(&p, &r7, 63);
    keygen_gf263_multiply(&p, &p, &r7);
    keygen_gf263_square_n(&t, &p, 10);
    keygen_gf263_multiply(&t, &t, &x511);
    keygen_gf263_square_n(&t, &t, 126);
    keygen_gf263_multiply(&t, &t, &p);
    keygen_gf263_square_n(&t, &t, 2);
    keygen_gf263_multiply(block, &t, &x);
#else
    block263_mersenne_inverse2_binary(block);
#endif
}
static inline void keygen_gf263_matrix(block263* x, const uint64_t matrix[263][5])
{
    block263_multiply_with_GF2_matrix(x, matrix);
}

bool greatwall_keygen_pubkey_263(uint8_t* pk, const uint8_t* sk)
{
    block263 key, iv;
    block263_load_to_block(&key, sk);
    block263_load_to_block(&iv, sk + BLOCK263_BYTES);

    FAEST_TIMING_DECLARE();
    FAEST_TIMING_START();
    owf_block alpha0, alpha1;
    if (!derive_owf_alpha(&alpha0, &iv, 0) ||
        !derive_owf_alpha(&alpha1, &iv, 1) ||
        owf_alpha_is_zero(&alpha0) || owf_alpha_is_zero(&alpha1)) {
        FAEST_TIMING_PRINT("keygen_owf/derive_alpha_and_check");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/derive_alpha_and_check");

    FAEST_TIMING_START();
    block263 state = key;
    block263 rc0, rc1;
    block263_load_words(&rc0, greatwall_rc_263[0]);
    block263_load_words(&rc1, greatwall_rc_263[1]);
    FAEST_TIMING_PRINT("keygen_owf/load_constants");

    FAEST_TIMING_START();
    keygen_gf263_matrix(&state, greatwall_mat_263_0);
    state = block263_xor(state, rc0);
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix0_and_add_constant");
    FAEST_TIMING_START();
    keygen_gf263_inverse1(&state);
    FAEST_TIMING_PRINT("keygen_owf/mersenne_inverse1");
    FAEST_TIMING_START();
    if (!owf_alpha_dot_is_one(&alpha0, &state)) {
        FAEST_TIMING_PRINT("keygen_owf/check_alpha0");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/check_alpha0");

    FAEST_TIMING_START();
    state = block263_xor(state, key);
    keygen_gf263_matrix(&state, greatwall_mat_263_1);
    state = block263_xor(state, iv);
    const block263 tmp = state;
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix1_and_mix_input");
    FAEST_TIMING_START();
    keygen_gf263_inverse2(&state);
    FAEST_TIMING_PRINT("keygen_owf/mersenne_inverse2");
    FAEST_TIMING_START();
    if (!owf_alpha_dot_is_one(&alpha1, &state)) {
        FAEST_TIMING_PRINT("keygen_owf/check_alpha1");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/check_alpha1");

    FAEST_TIMING_START();
    keygen_gf263_matrix(&state, greatwall_mat_263_2);
    state = block263_xor(state, rc1);
    state = block263_xor(state, tmp);
    state = block263_xor(state, key);

    block263_store_from_block(pk, &state);
    block263_store_from_block(pk + BLOCK263_BYTES, &iv);
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix2_and_pack_output");
    return true;
}
