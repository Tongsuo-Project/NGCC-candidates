#ifndef GF163_H
#define GF163_H

/* Native Pylon GF(2^163), not the security-parameter authentication field.
 * Modulus: X^163 + X^7 + X^6 + X^3 + 1.
 * Little-endian encoding: 21 bytes; storage: 3 uint64_t words (24 bytes),
 * with 35 significant bits in word 2. All arithmetic inputs are canonical.
 * Public block163_* operations support exact-sized objects and in-place use.
 * No OWF linear matrices, round constants or GF(2^160) modulus are supplied. */
#include <assert.h>
#include <immintrin.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BLOCK163_BITS 163
#define BLOCK163_WORDS 3
#define BLOCK163_BYTES 21
#define BLOCK163_TOP_MASK UINT64_C(0x7ffffffff)
#define BLOCK163_REDUCTION_LOW UINT64_C(0xc9)

typedef struct { uint64_t data[BLOCK163_WORDS]; } block163;

static inline void block163_canonicalize(block163* a)
{
    a->data[2] &= BLOCK163_TOP_MASK;
}
static inline bool block163_is_canonical(const block163* a)
{
    return (a->data[2] & ~BLOCK163_TOP_MASK) == 0;
}
static inline void block163_load_to_block(block163* out, const uint8_t* in)
{
    assert((in[20] & 0xf8) == 0);
    block163 value = {{0}};
    memcpy(value.data, in, BLOCK163_BYTES);
    block163_canonicalize(&value);
    *out = value;
}
static inline void block163_load_words(block163* out, const uint64_t in[3])
{
    assert((in[2] & ~BLOCK163_TOP_MASK) == 0);
    memmove(out->data, in, sizeof(out->data));
    block163_canonicalize(out);
}
static inline void block163_store_from_block(uint8_t* out, const block163* in)
{
    assert(block163_is_canonical(in));
    memmove(out, in->data, BLOCK163_BYTES);
}
static inline block163 block163_xor(block163 a, block163 b)
{
    block163 out = {{a.data[0] ^ b.data[0], a.data[1] ^ b.data[1],
                    (a.data[2] ^ b.data[2]) & BLOCK163_TOP_MASK}};
    return out;
}
static inline bool block163_is_zero(const block163* a)
{
    return (a->data[0] | a->data[1] | a->data[2]) == 0;
}

static inline bool block163_dot_is_one(const block163* a, const block163* b)
{
    return __builtin_parityll((a->data[0] & b->data[0]) ^
                              (a->data[1] & b->data[1]) ^
                              (a->data[2] & b->data[2])) != 0;
}
static inline void block163_mask(block163* a) { block163_canonicalize(a); }
static inline block163 block163_set_one(void)
{
    const block163 one = {{1, 0, 0}};
    return one;
}

static inline void clmul163_product(__m128i out[3], const __m128i a[2],
                                    const __m128i b[2])
{
    const __m128i a0 = a[0], b0 = b[0];
    const __m128i a1 = _mm_srli_si128(a0, 8), b1 = _mm_srli_si128(b0, 8);
    const __m128i top_mask = _mm_set_epi64x(0, BLOCK163_TOP_MASK);
    const __m128i a2 = _mm_and_si128(a[1], top_mask);
    const __m128i b2 = _mm_and_si128(b[1], top_mask);
    const __m128i p0 = _mm_clmulepi64_si128(a0, b0, 0x00);
    const __m128i p1 = _mm_clmulepi64_si128(a0, b0, 0x11);
    const __m128i p2 = _mm_clmulepi64_si128(a2, b2, 0x00);
    const __m128i p01 = _mm_xor_si128(
        _mm_clmulepi64_si128(_mm_xor_si128(a0, a1), _mm_xor_si128(b0, b1), 0x00),
        _mm_xor_si128(p0, p1));
    const __m128i p02 = _mm_xor_si128(
        _mm_clmulepi64_si128(_mm_xor_si128(a0, a2), _mm_xor_si128(b0, b2), 0x00),
        _mm_xor_si128(p0, p2));
    const __m128i p12 = _mm_xor_si128(
        _mm_clmulepi64_si128(_mm_xor_si128(a1, a2), _mm_xor_si128(b1, b2), 0x00),
        _mm_xor_si128(p1, p2));
    out[0] = _mm_xor_si128(p0, _mm_slli_si128(p01, 8));
    out[1] = _mm_xor_si128(_mm_xor_si128(p1, p02),
        _mm_xor_si128(_mm_srli_si128(p01, 8), _mm_slli_si128(p12, 8)));
    out[2] = _mm_xor_si128(p2, _mm_srli_si128(p12, 8));
}
static inline void sqr163(__m128i out[3], const __m128i in[2])
{
    const __m128i low = in[0];
    const __m128i top = _mm_and_si128(in[1], _mm_set_epi64x(0, BLOCK163_TOP_MASK));
    out[0] = _mm_clmulepi64_si128(low, low, 0x00);
    out[1] = _mm_clmulepi64_si128(low, low, 0x11);
    out[2] = _mm_clmulepi64_si128(top, top, 0x00);
}

static inline void block163_fold_words(block163* out, const uint64_t w[6])
{
    const uint64_t h0 = (w[2] >> 35) | (w[3] << 29);
    const uint64_t h1 = (w[3] >> 35) | (w[4] << 29);
    const uint64_t h2 = (w[4] >> 35) | (w[5] << 29);
    const uint64_t t0 = w[0] ^ h0 ^ (h0 << 3) ^ (h0 << 6) ^ (h0 << 7);
    const uint64_t t1 = w[1] ^ h1 ^ (h1 << 3) ^ (h0 >> 61) ^
                        (h1 << 6) ^ (h0 >> 58) ^ (h1 << 7) ^ (h0 >> 57);
    const uint64_t t2 = (w[2] & BLOCK163_TOP_MASK) ^ h2 ^
                        (h2 << 3) ^ (h1 >> 61) ^ (h2 << 6) ^ (h1 >> 58) ^
                        (h2 << 7) ^ (h1 >> 57);
    const uint64_t u = t2 >> 35;
    out->data[0] = t0 ^ u ^ (u << 3) ^ (u << 6) ^ (u << 7);
    out->data[1] = t1;
    out->data[2] = t2 & BLOCK163_TOP_MASK;
}

static inline void reduce_clmul163(__m128i out[2], const __m128i in[3])
{
    uint64_t words[6];
    memcpy(words, in, sizeof(words));
    for (int bit = 383; bit >= BLOCK163_BITS; --bit) {
        const uint64_t selected = UINT64_C(0) - ((words[bit >> 6] >> (bit & 63)) & 1);
        words[bit >> 6] ^= (UINT64_C(1) << (bit & 63)) & selected;
        const int low = bit - BLOCK163_BITS;
        words[low >> 6] ^= (UINT64_C(1) << (low & 63)) & selected;
        words[(low + 3) >> 6] ^= (UINT64_C(1) << ((low + 3) & 63)) & selected;
        words[(low + 6) >> 6] ^= (UINT64_C(1) << ((low + 6) & 63)) & selected;
        words[(low + 7) >> 6] ^= (UINT64_C(1) << ((low + 7) & 63)) & selected;
    }
    out[0] = _mm_set_epi64x(words[1], words[0]);
    out[1] = _mm_set_epi64x(0, words[2] & BLOCK163_TOP_MASK);
}
#ifndef GREATWALL_GF163_FAST_REDUCTION
#define GREATWALL_GF163_FAST_REDUCTION 1
#endif
static inline void reduce_clmul163_fold_product(__m128i out[2], const __m128i in[3])
{
    uint64_t words[6];
    memcpy(words, in, sizeof(words));
    assert((words[5] >> 5) == 0);
    block163 value;
    block163_fold_words(&value, words);
    out[0] = _mm_set_epi64x(value.data[1], value.data[0]);
    out[1] = _mm_set_epi64x(0, value.data[2]);
}
static inline void reduce_clmul163_product(__m128i out[2], const __m128i in[3])
{
#if GREATWALL_GF163_FAST_REDUCTION
    reduce_clmul163_fold_product(out, in);
#else
    reduce_clmul163(out, in);
#endif
}
static inline void gf163_load_vectors(__m128i out[2], const block163* in)
{
    out[0] = _mm_loadu_si128((const __m128i*)in->data);
    out[1] = _mm_set_epi64x(0, in->data[2] & BLOCK163_TOP_MASK);
}
static inline void gf163_store_vectors(block163* out, const __m128i in[2])
{
    const __m128i low = in[0];
    const uint64_t top = (uint64_t)_mm_cvtsi128_si64(in[1]) & BLOCK163_TOP_MASK;
    _mm_storeu_si128((__m128i*)out->data, low);
    out->data[2] = top;
}
static inline void block163_sqr(block163* out, const block163* in)
{
    assert(block163_is_canonical(in));
    __m128i input[2], product[3], result[2];
    gf163_load_vectors(input, in);
    sqr163(product, input);
    reduce_clmul163_product(result, product);
    gf163_store_vectors(out, result);
}
static inline void block163_mul(block163* out, const block163* a, const block163* b)
{
    assert(block163_is_canonical(a) && block163_is_canonical(b));
    __m128i av[2], bv[2], product[3], result[2];
    gf163_load_vectors(av, a);
    gf163_load_vectors(bv, b);
    clmul163_product(product, av, bv);
    reduce_clmul163_product(result, product);
    gf163_store_vectors(out, result);
}

static inline void block163_square_n(block163* out, const block163* in, unsigned count)
{
    *out = *in;
    for (unsigned i = 0; i < count; ++i) block163_sqr(out, out);
}

static inline void block163_pow_words(block163* block, const uint64_t exponent[3])
{
    const block163 base = *block;
    block163 result = block163_set_one();
    for (int bit = BLOCK163_BITS - 1; bit >= 0; --bit) {
        block163_sqr(&result, &result);
        if ((exponent[bit >> 6] >> (bit & 63)) & 1)
            block163_mul(&result, &result, &base);
    }
    *block = result;
}
static inline void block163_mersenne_inverse1_binary(block163* block)
{

    static const uint64_t exponent[3] = {
        UINT64_C(0xef7bdef7bdef7bdd), UINT64_C(0xef7bdef7bdeef7bd),
        UINT64_C(0x77bdef7bd)};
    block163_pow_words(block, exponent);
}
static inline void block163_mersenne_inverse2_binary(block163* block)
{

    static const uint64_t exponent[3] = {
        UINT64_C(0xf77bbddeef77bbdd), UINT64_C(0x77bbddeef77bddee),
        UINT64_C(0x77bbddeef)};
    block163_pow_words(block, exponent);
}

static inline void block163_mersenne_inverse1_chain(block163* block)
{
    const block163 x = *block;
    block163 x4, x5, r2, r4, r8, r16, t2, t3, t24, t27, t29;
    block163_square_n(&x4, &x, 2);
    block163_mul(&x5, &x4, &x);
    block163_square_n(&r2, &x4, 3);
    block163_mul(&r2, &r2, &x);
    block163_square_n(&r4, &r2, 10);
    block163_mul(&r4, &r4, &r2);
    block163_square_n(&r8, &r4, 20);
    block163_mul(&r8, &r8, &r4);
    block163_square_n(&r16, &r8, 40);
    block163_mul(&r16, &r16, &r8);
    block163_sqr(&t2, &r16);
    block163_mul(&t3, &t2, &r16);
    block163_square_n(&t24, &t3, 3);
    block163_mul(&t27, &t24, &t3);
    block163_mul(&t29, &t27, &t2);
    block163_square_n(&t29, &t29, 80);
    block163_mul(&t29, &t29, &t27);
    block163_square_n(&t29, &t29, 3);
    block163_mul(block, &t29, &x5);
}

static inline void block163_mersenne_inverse2_chain(block163* block)
{
    const block163 x = *block;
    block163 x2, x3, x7, x29, x119, x476, p, d, r2, r4, r8, b, t;
    block163_sqr(&x2, &x);
    block163_mul(&x3, &x2, &x);
    block163_sqr(&x7, &x3);
    block163_mul(&x7, &x7, &x);
    block163_square_n(&x29, &x7, 2);
    block163_mul(&x29, &x29, &x);
    block163_square_n(&x119, &x29, 2);
    block163_mul(&x119, &x119, &x3);
    block163_square_n(&x476, &x119, 2);
    block163_mul(&p, &x476, &x2);
    block163_mul(&d, &x476, &x);
    block163_square_n(&r2, &p, 9);
    block163_mul(&r2, &r2, &p);
    block163_square_n(&r4, &r2, 18);
    block163_mul(&r4, &r4, &r2);
    block163_square_n(&r8, &r4, 36);
    block163_mul(&r8, &r8, &r4);
    block163_square_n(&b, &r8, 5);
    block163_mul(&b, &b, &x29);
    block163_square_n(&t, &b, 77);
    block163_mul(&t, &t, &b);
    block163_square_n(&t, &t, 9);
    block163_mul(block, &t, &d);
}
#ifndef GREATWALL_GF163_INVERSE_CHAIN
#define GREATWALL_GF163_INVERSE_CHAIN 1
#endif
static inline void block163_mersenne_inverse1(block163* block)
{
#if GREATWALL_GF163_INVERSE_CHAIN
    block163_mersenne_inverse1_chain(block);
#else
    block163_mersenne_inverse1_binary(block);
#endif
}
static inline void block163_mersenne_inverse2(block163* block)
{
#if GREATWALL_GF163_INVERSE_CHAIN
    block163_mersenne_inverse2_chain(block);
#else
    block163_mersenne_inverse2_binary(block);
#endif
}

static inline void block163_multiply_with_GF2_matrix(
    block163* block, const uint64_t matrix[BLOCK163_BITS][BLOCK163_WORDS])
{
    block163 out = {{0}};
    for (size_t row = 0; row < BLOCK163_BITS; ++row) {
        const uint64_t combined = (block->data[0] & matrix[row][0]) ^
            (block->data[1] & matrix[row][1]) ^
            (block->data[2] & matrix[row][2] & BLOCK163_TOP_MASK);
        out.data[row >> 6] |= (uint64_t)__builtin_parityll(combined) << (row & 63);
    }
    *block = out;
}
#endif
