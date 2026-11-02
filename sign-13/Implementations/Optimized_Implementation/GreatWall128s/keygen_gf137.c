#include "keygen_gf137.h"
#include "gf137.h"
#include "greatwall.h"
#include "owf_alpha.h"
#include "timing.h"

GW_STATIC_ASSERT(SECURITY_PARAM == 128, "keygen_gf137 is only for GreatWall128");
GW_STATIC_ASSERT(OWF_BLOCK_SIZE == BLOCK137_BYTES, "GF137 encoding mismatch");

#define GREATWALL_KEYGEN_GF137_LOCAL_ARITH 1

static inline void keygen_gf137_square(block137* out, const block137* in)
{
#if GREATWALL_KEYGEN_GF137_LOCAL_ARITH
    assert(block137_is_canonical(in));

    const uint64_t x0 = in->data[0], x1 = in->data[1], x2 = in->data[2];
    const __m128i input_low = _mm_set_epi64x((long long)x1, (long long)x0);
    const __m128i input_high = _mm_cvtsi64_si128((long long)x2);
    const __m128i sqr_table = _mm_set_epi64x(0x5554515045444140, 0x1514111005040100);
    const __m128i nibble_mask = _mm_set_epi64x(0x0F0F0F0F0F0F0F0F, 0x0F0F0F0F0F0F0F0F);

    const __m128i low_even = _mm_shuffle_epi8(sqr_table, _mm_and_si128(input_low, nibble_mask));
    const __m128i low_odd = _mm_shuffle_epi8(sqr_table,
        _mm_and_si128(_mm_srli_epi64(input_low, 4), nibble_mask));
    const __m128i product_low = _mm_unpacklo_epi8(low_even, low_odd);
    const __m128i product_mid = _mm_unpackhi_epi8(low_even, low_odd);
    const __m128i high_even = _mm_shuffle_epi8(sqr_table, _mm_and_si128(input_high, nibble_mask));
    const __m128i high_odd = _mm_shuffle_epi8(sqr_table,
        _mm_and_si128(_mm_srli_epi64(input_high, 4), nibble_mask));
    const __m128i product_high = _mm_unpacklo_epi8(high_even, high_odd);

    const uint64_t w0 = (uint64_t)_mm_cvtsi128_si64(product_low);
    const uint64_t w1 = (uint64_t)_mm_extract_epi64(product_low, 1);
    const uint64_t w2 = (uint64_t)_mm_cvtsi128_si64(product_mid);
    const uint64_t w3 = (uint64_t)_mm_extract_epi64(product_mid, 1);
    const uint64_t w4 = (uint64_t)_mm_cvtsi128_si64(product_high);
    assert((w4 >> 17) == 0);

    const uint64_t h0 = (w2 >> 9) | (w3 << 55);
    const uint64_t h1 = (w3 >> 9) | (w4 << 55);
    const uint64_t h2 = w4 >> 9;
    const uint64_t t0 = w0 ^ h0 ^ (h0 << 21);
    const uint64_t t1 = w1 ^ h1 ^ (h1 << 21) ^ (h0 >> 43);
    const uint64_t t2 = (w2 & UINT64_C(0x1ff)) ^ h2 ^ (h2 << 21) ^ (h1 >> 43);
    const uint64_t u = t2 >> 9;
    out->data[0] = t0 ^ u ^ (u << 21);
    out->data[1] = t1;
    out->data[2] = t2 & UINT64_C(0x1ff);
#else
    block137_sqr(out, in);
#endif
}

static inline void keygen_gf137_multiply(block137* out, const block137* a, const block137* b)
{
#if GREATWALL_KEYGEN_GF137_LOCAL_ARITH
    assert(block137_is_canonical(a) && block137_is_canonical(b));

    const uint64_t a0 = a->data[0], a1 = a->data[1], a2 = a->data[2];
    const uint64_t b0 = b->data[0], b1 = b->data[1], b2 = b->data[2];
    const __m128i input_a_low = _mm_set_epi64x((long long)a1, (long long)a0);
    const __m128i input_a_high = _mm_cvtsi64_si128((long long)a2);
    const __m128i input_b_low = _mm_set_epi64x((long long)b1, (long long)b0);
    const __m128i input_b_high = _mm_cvtsi64_si128((long long)b2);

    __m128i product_low = _mm_clmulepi64_si128(input_a_low, input_b_low, 0x00);
    __m128i product_mid = _mm_clmulepi64_si128(input_a_low, input_b_low, 0x11);
    __m128i product_high = _mm_clmulepi64_si128(input_a_high, input_b_high, 0x00);
    product_mid = _mm_xor_si128(product_mid,
        _mm_clmulepi64_si128(input_a_low, input_b_high, 0x00));
    product_mid = _mm_xor_si128(product_mid,
        _mm_clmulepi64_si128(input_a_high, input_b_low, 0x00));

    const __m128i cross01 = _mm_xor_si128(
        _mm_clmulepi64_si128(input_a_low, input_b_low, 0x01),
        _mm_clmulepi64_si128(input_a_low, input_b_low, 0x10));
    product_low = _mm_xor_si128(product_low, _mm_slli_si128(cross01, 8));
    product_mid = _mm_xor_si128(product_mid, _mm_srli_si128(cross01, 8));

    const __m128i cross12 = _mm_xor_si128(
        _mm_clmulepi64_si128(input_a_high, input_b_low, 0x10),
        _mm_clmulepi64_si128(input_a_low, input_b_high, 0x01));
    product_mid = _mm_xor_si128(product_mid, _mm_slli_si128(cross12, 8));
    product_high = _mm_xor_si128(product_high, _mm_srli_si128(cross12, 8));

    const uint64_t w0 = (uint64_t)_mm_cvtsi128_si64(product_low);
    const uint64_t w1 = (uint64_t)_mm_extract_epi64(product_low, 1);
    const uint64_t w2 = (uint64_t)_mm_cvtsi128_si64(product_mid);
    const uint64_t w3 = (uint64_t)_mm_extract_epi64(product_mid, 1);
    const uint64_t w4 = (uint64_t)_mm_cvtsi128_si64(product_high);
    assert((w4 >> 17) == 0);

    const uint64_t h0 = (w2 >> 9) | (w3 << 55);
    const uint64_t h1 = (w3 >> 9) | (w4 << 55);
    const uint64_t h2 = w4 >> 9;
    const uint64_t t0 = w0 ^ h0 ^ (h0 << 21);
    const uint64_t t1 = w1 ^ h1 ^ (h1 << 21) ^ (h0 >> 43);
    const uint64_t t2 = (w2 & UINT64_C(0x1ff)) ^ h2 ^ (h2 << 21) ^ (h1 >> 43);
    const uint64_t u = t2 >> 9;
    out->data[0] = t0 ^ u ^ (u << 21);
    out->data[1] = t1;
    out->data[2] = t2 & UINT64_C(0x1ff);
#else
    block137_mul(out, a, b);
#endif
}

static inline void keygen_gf137_square_n(block137* out, const block137* in, unsigned count)
{
    *out = *in;
    for (unsigned i = 0; i < count; ++i)
        keygen_gf137_square(out, out);
}

static inline void keygen_gf137_inverse1(block137* block)
{
#if GREATWALL_GF137_INVERSE_CHAIN
    const block137 x = *block;
    block137 r2, r3, r5, r10, r20, t, u, v, w;
    keygen_gf137_square_n(&r2, &x, 3);
    keygen_gf137_multiply(&r2, &r2, &x);
    keygen_gf137_square_n(&r3, &r2, 3);
    keygen_gf137_multiply(&r3, &r3, &x);
    keygen_gf137_square_n(&r5, &r3, 6);
    keygen_gf137_multiply(&r5, &r5, &r2);
    keygen_gf137_square_n(&r10, &r5, 15);
    keygen_gf137_multiply(&r10, &r10, &r5);
    keygen_gf137_square_n(&r20, &r10, 30);
    keygen_gf137_multiply(&r20, &r20, &r10);
    keygen_gf137_square_n(&t, &r20, 9);
    keygen_gf137_multiply(&t, &t, &r3);
    keygen_gf137_square(&u, &t);
    keygen_gf137_multiply(&v, &u, &t);
    keygen_gf137_square(&u, &u);
    keygen_gf137_multiply(&w, &u, &t);
    keygen_gf137_square_n(&v, &v, 69);
    keygen_gf137_multiply(block, &v, &w);
#else
    block137_mersenne_inverse1_binary(block);
#endif
}

static inline void keygen_gf137_inverse2(block137* block)
{
#if GREATWALL_GF137_INVERSE_CHAIN
    const block137 x = *block;
    block137 x2, x3, x5, t, q, d, p, p2, p4, b, twice_b;
    keygen_gf137_square(&x2, &x);
    keygen_gf137_multiply(&x3, &x2, &x);
    keygen_gf137_square(&x5, &x2);
    keygen_gf137_multiply(&x5, &x5, &x);
    keygen_gf137_square_n(&t, &x3, 3);
    keygen_gf137_multiply(&t, &t, &x3);
    keygen_gf137_square_n(&t, &t, 2);
    keygen_gf137_multiply(&t, &t, &x);
    keygen_gf137_square_n(&q, &t, 3);
    keygen_gf137_multiply(&q, &q, &x5);
    keygen_gf137_square_n(&d, &q, 3);
    keygen_gf137_multiply(&d, &d, &x5);
    keygen_gf137_multiply(&p, &d, &x);
    keygen_gf137_square_n(&p2, &p, 13);
    keygen_gf137_multiply(&p2, &p2, &p);
    keygen_gf137_square_n(&p4, &p2, 26);
    keygen_gf137_multiply(&p4, &p4, &p2);
    keygen_gf137_square_n(&b, &p4, 10);
    keygen_gf137_multiply(&b, &b, &q);
    keygen_gf137_square_n(&twice_b, &b, 62);
    keygen_gf137_multiply(&twice_b, &twice_b, &b);
    keygen_gf137_square_n(&twice_b, &twice_b, 13);
    keygen_gf137_multiply(block, &twice_b, &d);
#else
    block137_mersenne_inverse2_binary(block);
#endif
}

static inline void keygen_gf137_matrix(block137* x, const uint64_t matrix[137][3])
{
    block137_multiply_with_GF2_matrix(x, matrix);
}

bool greatwall_keygen_pubkey_137(uint8_t* pk, const uint8_t* sk)
{
    block137 key, iv;
    block137_load_to_block(&key, sk);
    block137_load_to_block(&iv, sk + BLOCK137_BYTES);

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
    block137 state = key;
    block137 rc0, rc1;
    block137_load_words(&rc0, greatwall_rc_137[0]);
    block137_load_words(&rc1, greatwall_rc_137[1]);
    FAEST_TIMING_PRINT("keygen_owf/load_constants");

    FAEST_TIMING_START();
    keygen_gf137_matrix(&state, greatwall_mat_137_0);
    state = block137_xor(state, rc0);
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix0_and_add_constant");
    FAEST_TIMING_START();
    keygen_gf137_inverse1(&state);
    FAEST_TIMING_PRINT("keygen_owf/mersenne_inverse1");
    FAEST_TIMING_START();
    if (!owf_alpha_dot_is_one(&alpha0, &state)) {
        FAEST_TIMING_PRINT("keygen_owf/check_alpha0");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/check_alpha0");

    FAEST_TIMING_START();
    state = block137_xor(state, key);
    keygen_gf137_matrix(&state, greatwall_mat_137_1);
    state = block137_xor(state, iv);
    const block137 tmp = state;
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix1_and_mix_input");
    FAEST_TIMING_START();
    keygen_gf137_inverse2(&state);
    FAEST_TIMING_PRINT("keygen_owf/mersenne_inverse2");
    FAEST_TIMING_START();
    if (!owf_alpha_dot_is_one(&alpha1, &state)) {
        FAEST_TIMING_PRINT("keygen_owf/check_alpha1");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/check_alpha1");

    FAEST_TIMING_START();
    keygen_gf137_matrix(&state, greatwall_mat_137_2);
    state = block137_xor(state, rc1);
    state = block137_xor(state, tmp);
    state = block137_xor(state, key);

    block137_store_from_block(pk, &state);
    block137_store_from_block(pk + BLOCK137_BYTES, &iv);
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix2_and_pack_output");
    return true;
}
