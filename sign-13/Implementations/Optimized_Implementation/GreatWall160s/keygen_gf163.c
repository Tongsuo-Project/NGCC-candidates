#include "keygen_gf163.h"
#include "gf163.h"
#include "greatwall.h"
#include "owf_alpha.h"
#include "timing.h"

GW_STATIC_ASSERT(SECURITY_PARAM == 160, "keygen_gf163 requires the GF160 parameter set");
GW_STATIC_ASSERT(OWF_BLOCK_SIZE == BLOCK163_BYTES, "GF163 encoding mismatch");

#define GREATWALL_KEYGEN_GF163_LOCAL_ARITH 1

static inline void keygen_gf163_square(block163* out, const block163* in)
{
#if GREATWALL_KEYGEN_GF163_LOCAL_ARITH
    assert(block163_is_canonical(in));
    const __m128i low = _mm_loadu_si128((const __m128i*)in->data);
    const __m128i top = _mm_set_epi64x(0, in->data[2]);
    const __m128i p0 = _mm_clmulepi64_si128(low, low, 0x00);
    const __m128i p1 = _mm_clmulepi64_si128(low, low, 0x11);
    const __m128i p2 = _mm_clmulepi64_si128(top, top, 0x00);
    const uint64_t w0 = (uint64_t)_mm_cvtsi128_si64(p0);
    const uint64_t w1 = (uint64_t)_mm_extract_epi64(p0, 1);
    const uint64_t w2 = (uint64_t)_mm_cvtsi128_si64(p1);
    const uint64_t w3 = (uint64_t)_mm_extract_epi64(p1, 1);
    const uint64_t w4 = (uint64_t)_mm_cvtsi128_si64(p2);
    const uint64_t w5 = (uint64_t)_mm_extract_epi64(p2, 1);
    const uint64_t h0 = (w2 >> 35) | (w3 << 29);
    const uint64_t h1 = (w3 >> 35) | (w4 << 29);
    const uint64_t h2 = (w4 >> 35) | (w5 << 29);
    const uint64_t t0 = w0 ^ h0 ^ (h0 << 3) ^ (h0 << 6) ^ (h0 << 7);
    const uint64_t t1 = w1 ^ h1 ^ (h1 << 3) ^ (h0 >> 61) ^
                        (h1 << 6) ^ (h0 >> 58) ^ (h1 << 7) ^ (h0 >> 57);
    const uint64_t t2 = (w2 & BLOCK163_TOP_MASK) ^ h2 ^
                        (h2 << 3) ^ (h1 >> 61) ^ (h2 << 6) ^ (h1 >> 58) ^
                        (h2 << 7) ^ (h1 >> 57);
    const uint64_t u = t2 >> 35;
    out->data[0] = t0 ^ u ^ (u << 3) ^ (u << 6) ^ (u << 7);
    out->data[1] = t1;
    out->data[2] = t2 & BLOCK163_TOP_MASK;
#else
    block163_sqr(out, in);
#endif
}
static inline void keygen_gf163_multiply(block163* out, const block163* a, const block163* b)
{
#if GREATWALL_KEYGEN_GF163_LOCAL_ARITH
    assert(block163_is_canonical(a) && block163_is_canonical(b));
    const __m128i a0 = _mm_loadu_si128((const __m128i*)a->data);
    const __m128i b0 = _mm_loadu_si128((const __m128i*)b->data);
    const __m128i a1 = _mm_srli_si128(a0, 8), b1 = _mm_srli_si128(b0, 8);
    const __m128i a2 = _mm_set_epi64x(0, a->data[2]);
    const __m128i b2 = _mm_set_epi64x(0, b->data[2]);
    const __m128i d0 = _mm_clmulepi64_si128(a0, b0, 0x00);
    const __m128i d1 = _mm_clmulepi64_si128(a0, b0, 0x11);
    const __m128i d2 = _mm_clmulepi64_si128(a2, b2, 0x00);
    const __m128i c01 = _mm_xor_si128(
        _mm_clmulepi64_si128(_mm_xor_si128(a0, a1), _mm_xor_si128(b0, b1), 0x00),
        _mm_xor_si128(d0, d1));
    const __m128i c02 = _mm_xor_si128(
        _mm_clmulepi64_si128(_mm_xor_si128(a0, a2), _mm_xor_si128(b0, b2), 0x00),
        _mm_xor_si128(d0, d2));
    const __m128i c12 = _mm_xor_si128(
        _mm_clmulepi64_si128(_mm_xor_si128(a1, a2), _mm_xor_si128(b1, b2), 0x00),
        _mm_xor_si128(d1, d2));
    const __m128i p0 = _mm_xor_si128(d0, _mm_slli_si128(c01, 8));
    const __m128i p1 = _mm_xor_si128(_mm_xor_si128(d1, c02),
        _mm_xor_si128(_mm_srli_si128(c01, 8), _mm_slli_si128(c12, 8)));
    const __m128i p2 = _mm_xor_si128(d2, _mm_srli_si128(c12, 8));
    const uint64_t w0 = (uint64_t)_mm_cvtsi128_si64(p0);
    const uint64_t w1 = (uint64_t)_mm_extract_epi64(p0, 1);
    const uint64_t w2 = (uint64_t)_mm_cvtsi128_si64(p1);
    const uint64_t w3 = (uint64_t)_mm_extract_epi64(p1, 1);
    const uint64_t w4 = (uint64_t)_mm_cvtsi128_si64(p2);
    const uint64_t w5 = (uint64_t)_mm_extract_epi64(p2, 1);
    const uint64_t h0 = (w2 >> 35) | (w3 << 29);
    const uint64_t h1 = (w3 >> 35) | (w4 << 29);
    const uint64_t h2 = (w4 >> 35) | (w5 << 29);
    const uint64_t t0 = w0 ^ h0 ^ (h0 << 3) ^ (h0 << 6) ^ (h0 << 7);
    const uint64_t t1 = w1 ^ h1 ^ (h1 << 3) ^ (h0 >> 61) ^
                        (h1 << 6) ^ (h0 >> 58) ^ (h1 << 7) ^ (h0 >> 57);
    const uint64_t t2 = (w2 & BLOCK163_TOP_MASK) ^ h2 ^
                        (h2 << 3) ^ (h1 >> 61) ^ (h2 << 6) ^ (h1 >> 58) ^
                        (h2 << 7) ^ (h1 >> 57);
    const uint64_t u = t2 >> 35;
    out->data[0] = t0 ^ u ^ (u << 3) ^ (u << 6) ^ (u << 7);
    out->data[1] = t1;
    out->data[2] = t2 & BLOCK163_TOP_MASK;
#else
    block163_mul(out, a, b);
#endif
}
static inline void keygen_gf163_square_n(block163* out, const block163* in, unsigned count)
{
    *out = *in;
    for (unsigned i = 0; i < count; ++i) keygen_gf163_square(out, out);
}

static inline void keygen_gf163_inverse1(block163* block)
{
#if GREATWALL_GF163_INVERSE_CHAIN
    const block163 x = *block;
    block163 x4, x5, r2, r4, r8, r16, t2, t3, t24, t27, t29;
    keygen_gf163_square_n(&x4, &x, 2);
    keygen_gf163_multiply(&x5, &x4, &x);
    keygen_gf163_square_n(&r2, &x4, 3);
    keygen_gf163_multiply(&r2, &r2, &x);
    keygen_gf163_square_n(&r4, &r2, 10);
    keygen_gf163_multiply(&r4, &r4, &r2);
    keygen_gf163_square_n(&r8, &r4, 20);
    keygen_gf163_multiply(&r8, &r8, &r4);
    keygen_gf163_square_n(&r16, &r8, 40);
    keygen_gf163_multiply(&r16, &r16, &r8);
    keygen_gf163_square(&t2, &r16);
    keygen_gf163_multiply(&t3, &t2, &r16);
    keygen_gf163_square_n(&t24, &t3, 3);
    keygen_gf163_multiply(&t27, &t24, &t3);
    keygen_gf163_multiply(&t29, &t27, &t2);
    keygen_gf163_square_n(&t29, &t29, 80);
    keygen_gf163_multiply(&t29, &t29, &t27);
    keygen_gf163_square_n(&t29, &t29, 3);
    keygen_gf163_multiply(block, &t29, &x5);
#else
    block163_mersenne_inverse1_binary(block);
#endif
}

static inline void keygen_gf163_inverse2(block163* block)
{
#if GREATWALL_GF163_INVERSE_CHAIN
    const block163 x = *block;
    block163 x2, x3, x7, x29, x119, x476, p, d, r2, r4, r8, b, t;
    keygen_gf163_square(&x2, &x);
    keygen_gf163_multiply(&x3, &x2, &x);
    keygen_gf163_square(&x7, &x3);
    keygen_gf163_multiply(&x7, &x7, &x);
    keygen_gf163_square_n(&x29, &x7, 2);
    keygen_gf163_multiply(&x29, &x29, &x);
    keygen_gf163_square_n(&x119, &x29, 2);
    keygen_gf163_multiply(&x119, &x119, &x3);
    keygen_gf163_square_n(&x476, &x119, 2);
    keygen_gf163_multiply(&p, &x476, &x2);
    keygen_gf163_multiply(&d, &x476, &x);
    keygen_gf163_square_n(&r2, &p, 9);
    keygen_gf163_multiply(&r2, &r2, &p);
    keygen_gf163_square_n(&r4, &r2, 18);
    keygen_gf163_multiply(&r4, &r4, &r2);
    keygen_gf163_square_n(&r8, &r4, 36);
    keygen_gf163_multiply(&r8, &r8, &r4);
    keygen_gf163_square_n(&b, &r8, 5);
    keygen_gf163_multiply(&b, &b, &x29);
    keygen_gf163_square_n(&t, &b, 77);
    keygen_gf163_multiply(&t, &t, &b);
    keygen_gf163_square_n(&t, &t, 9);
    keygen_gf163_multiply(block, &t, &d);
#else
    block163_mersenne_inverse2_binary(block);
#endif
}

static inline void keygen_gf163_matrix(
    block163* value, const uint64_t matrix[BLOCK163_BITS][BLOCK163_WORDS])
{
    block163_multiply_with_GF2_matrix(value, matrix);
}

bool greatwall_keygen_pubkey_163(uint8_t* pk, const uint8_t* sk)
{

    if (!pk || !sk || !greatwall160_constants_available()) return false;
    if ((sk[BLOCK163_BYTES-1] & 0xf8) ||
        (sk[2*BLOCK163_BYTES-1] & 0xf8)) return false;
    block163 key, iv;
    block163_load_to_block(&key, sk);
    block163_load_to_block(&iv, sk + BLOCK163_BYTES);

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
    block163 state = key, rc0, rc1;
    block163_load_words(&rc0, greatwall_rc_163[0]);
    block163_load_words(&rc1, greatwall_rc_163[1]);
    FAEST_TIMING_PRINT("keygen_owf/load_constants");

    FAEST_TIMING_START();
    keygen_gf163_matrix(&state, greatwall_mat_163_0);
    state = block163_xor(state, rc0);
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix0_and_add_constant");
    FAEST_TIMING_START();
    keygen_gf163_inverse1(&state);
    FAEST_TIMING_PRINT("keygen_owf/mersenne_inverse1");
    FAEST_TIMING_START();
    if (!owf_alpha_dot_is_one(&alpha0, &state)) {
        FAEST_TIMING_PRINT("keygen_owf/check_alpha0");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/check_alpha0");

    FAEST_TIMING_START();
    state = block163_xor(state, key);
    keygen_gf163_matrix(&state, greatwall_mat_163_1);
    state = block163_xor(state, iv);
    const block163 tmp = state;
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix1_and_mix_input");
    FAEST_TIMING_START();
    keygen_gf163_inverse2(&state);
    FAEST_TIMING_PRINT("keygen_owf/mersenne_inverse2");
    FAEST_TIMING_START();
    if (!owf_alpha_dot_is_one(&alpha1, &state)) {
        FAEST_TIMING_PRINT("keygen_owf/check_alpha1");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/check_alpha1");

    FAEST_TIMING_START();
    keygen_gf163_matrix(&state, greatwall_mat_163_2);
    state = block163_xor(state, rc1);
    state = block163_xor(state, tmp);
    state = block163_xor(state, key);
    block163_store_from_block(pk, &state);
    block163_store_from_block(pk + BLOCK163_BYTES, &iv);
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix2_and_pack_output");
    return true;
}
