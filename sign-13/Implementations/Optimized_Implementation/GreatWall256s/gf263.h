#ifndef GF263_H
#define GF263_H

#include <assert.h>
#include <immintrin.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define BLOCK263_BITS 263
#define BLOCK263_WORDS 5
#define BLOCK263_BYTES 33
#define BLOCK263_REDUCTION_TERM 93
typedef struct { uint64_t data[BLOCK263_WORDS]; } block263;
static inline void block263_canonicalize(block263* a) {
    a->data[4] &= 0x7FULL;
}

static inline bool block263_is_canonical(const block263* in)
{
    return (in->data[4] & ~0x7FULL) == 0;
}

static inline void block263_load_to_block(block263* out, const uint8_t* in)
{
    assert((in[32] & 0x80) == 0);

    memset(out, 0, sizeof(*out));
    memcpy(out, in, 33);

    out->data[4] &= 0x7FULL;
}

static inline void block263_load_words(block263* out, const uint64_t in[5])
{
    out->data[0] = in[0];
    out->data[1] = in[1];
    out->data[2] = in[2];
    out->data[3] = in[3];
    out->data[4] = in[4] & 0x7FULL;

    assert((in[4] & ~0x7FULL) == 0);
}

static inline void block263_store_from_block(uint8_t* out, const block263* in)
{
    assert(block263_is_canonical(in));

    memcpy(out, in, 33);

    out[32] &= 0x7F;
}

static inline block263 block263_xor(block263 x, block263 y)
{
    block263 out;

    out.data[0] = x.data[0] ^ y.data[0];
    out.data[1] = x.data[1] ^ y.data[1];
    out.data[2] = x.data[2] ^ y.data[2];
    out.data[3] = x.data[3] ^ y.data[3];
    out.data[4] = (x.data[4] ^ y.data[4]) & 0x7FULL;

    return out;
}

static inline bool block263_is_zero(const block263* value)
{
    uint64_t combined = 0;
    for (size_t i = 0; i < BLOCK263_WORDS; ++i) combined |= value->data[i];
    return combined == 0;
}
static inline bool block263_dot_is_one(const block263* a, const block263* b)
{
    unsigned parity = 0;
    for (size_t i = 0; i < BLOCK263_WORDS; ++i)
        parity ^= (unsigned)__builtin_parityll(a->data[i] & b->data[i]);
    return (parity & 1) != 0;
}
static inline void block263_mask(block263* x)
{
    x->data[4] &= 0x7FULL;
}

static inline block263 block263_set_one(void)
{
    block263 r = {{0, 0, 0, 0, 0}};
    r.data[0] = 1;
    return r;
}

static inline void clmul_schoolbook263(__m128i out[5],
                                       const __m128i a[3],
                                       const __m128i b[3])
{
    uint64_t aw[6], bw[6];
    uint64_t t[10] = {0};

    memcpy(aw, a, 48);
    memcpy(bw, b, 48);

    aw[4] &= 0x7FULL;
    bw[4] &= 0x7FULL;
    aw[5] = 0;
    bw[5] = 0;

    for (size_t i = 0; i < 5; i++) {
        for (size_t j = 0; j < 5; j++) {
            __m128i aa = _mm_set_epi64x(0, aw[i]);
            __m128i bb = _mm_set_epi64x(0, bw[j]);
            __m128i p = _mm_clmulepi64_si128(aa, bb, 0x00);

            uint64_t pp[2];
            _mm_storeu_si128((__m128i*)pp, p);

            t[i + j]     ^= pp[0];
            t[i + j + 1] ^= pp[1];
        }
    }

    out[0] = _mm_set_epi64x(t[1], t[0]);
    out[1] = _mm_set_epi64x(t[3], t[2]);
    out[2] = _mm_set_epi64x(t[5], t[4]);
    out[3] = _mm_set_epi64x(t[7], t[6]);
    out[4] = _mm_set_epi64x(t[9], t[8]);
}

static inline void reduce_clmul263(__m128i out[3], const __m128i in[5])
{
    uint64_t t[10] = {0};

    memcpy(&t[0], &in[0], 16);
    memcpy(&t[2], &in[1], 16);
    memcpy(&t[4], &in[2], 16);
    memcpy(&t[6], &in[3], 16);
    memcpy(&t[8], &in[4], 16);

    for (int bit = 639; bit >= 263; --bit) {
        int word = bit >> 6;
        int off  = bit & 63;
        uint64_t mask = 1ULL << off;

        if (t[word] & mask) {
            t[word] ^= mask;

            {
                int b = bit - 263;
                t[b >> 6] ^= (1ULL << (b & 63));
            }

            {
                int b = bit - 170;
                t[b >> 6] ^= (1ULL << (b & 63));
            }
        }
    }

    t[4] &= 0x7FULL;
    t[5] = 0;
    t[6] = 0;
    t[7] = 0;
    t[8] = 0;
    t[9] = 0;

    out[0] = _mm_set_epi64x(t[1], t[0]);
    out[1] = _mm_set_epi64x(t[3], t[2]);
    out[2] = _mm_set_epi64x(0, t[4] & 0x7FULL);
}

static inline void block263_fold_words(block263* out,
    uint64_t w0, uint64_t w1, uint64_t w2, uint64_t w3, uint64_t w4,
    uint64_t w5, uint64_t w6, uint64_t w7, uint64_t w8)
{
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
}

#ifndef GREATWALL_GF263_FAST_REDUCTION
#define GREATWALL_GF263_FAST_REDUCTION 1
#endif

static inline void reduce_clmul263_fold_product(__m128i out[3], const __m128i in[5])
{
    uint64_t words[10];
    memcpy(words, in, sizeof(words));
    assert((words[8] >> 13) == 0 && words[9] == 0);
    block263 result;
    block263_fold_words(&result, words[0], words[1], words[2], words[3], words[4],
                        words[5], words[6], words[7], words[8]);
    out[0] = _mm_set_epi64x(result.data[1], result.data[0]);
    out[1] = _mm_set_epi64x(result.data[3], result.data[2]);
    out[2] = _mm_set_epi64x(0, result.data[4]);
}

static inline void reduce_clmul263_product(__m128i out[3], const __m128i in[5])
{
#if GREATWALL_GF263_FAST_REDUCTION
    reduce_clmul263_fold_product(out, in);
#else
    reduce_clmul263(out, in);
#endif
}

static inline void sqr263(__m128i out[5], const __m128i a[3])
{
    __m128i tmp[2];

    __m128i sqrT = _mm_set_epi64x(
        0x5554515045444140,
        0x1514111005040100
    );

    __m128i mask = _mm_set_epi64x(
        0x0F0F0F0F0F0F0F0F,
        0x0F0F0F0F0F0F0F0F
    );

    tmp[0] = _mm_and_si128(a[0], mask);
    tmp[1] = _mm_srli_epi64(a[0], 4);
    tmp[1] = _mm_and_si128(tmp[1], mask);

    tmp[0] = _mm_shuffle_epi8(sqrT, tmp[0]);
    tmp[1] = _mm_shuffle_epi8(sqrT, tmp[1]);

    out[0] = _mm_unpacklo_epi8(tmp[0], tmp[1]);
    out[1] = _mm_unpackhi_epi8(tmp[0], tmp[1]);

    tmp[0] = _mm_and_si128(a[1], mask);
    tmp[1] = _mm_srli_epi64(a[1], 4);
    tmp[1] = _mm_and_si128(tmp[1], mask);

    tmp[0] = _mm_shuffle_epi8(sqrT, tmp[0]);
    tmp[1] = _mm_shuffle_epi8(sqrT, tmp[1]);

    out[2] = _mm_unpacklo_epi8(tmp[0], tmp[1]);
    out[3] = _mm_unpackhi_epi8(tmp[0], tmp[1]);

    tmp[0] = _mm_and_si128(a[2], mask);
    tmp[1] = _mm_srli_epi64(a[2], 4);
    tmp[1] = _mm_and_si128(tmp[1], mask);

    tmp[0] = _mm_shuffle_epi8(sqrT, tmp[0]);
    tmp[1] = _mm_shuffle_epi8(sqrT, tmp[1]);

    out[4] = _mm_unpacklo_epi8(tmp[0], tmp[1]);
}

static inline void gf263sqr(__m128i* out, const __m128i* in)
{
    __m128i tmp[5];
    sqr263(tmp, in);
    reduce_clmul263(out, tmp);
}

static inline void gf263mul(__m128i* out, const __m128i* in1, const __m128i* in2)
{
    __m128i tmp[5];
    clmul_schoolbook263(tmp, in1, in2);
    reduce_clmul263(out, tmp);
}

static inline void block263_sqr(block263* out, const block263* in)
{
    __m128i in128[3] = {
        _mm_set_epi64x(in->data[1], in->data[0]),
        _mm_set_epi64x(in->data[3], in->data[2]),
        _mm_set_epi64x(0, in->data[4] & 0x7FULL)
    };

    __m128i out128[3];
    __m128i product[5];
    sqr263(product, in128);
    reduce_clmul263_product(out128, product);

    uint64_t words[6];
    _mm_storeu_si128((__m128i*)&words[0], out128[0]);
    _mm_storeu_si128((__m128i*)&words[2], out128[1]);
    _mm_storeu_si128((__m128i*)&words[4], out128[2]);

    out->data[0] = words[0];
    out->data[1] = words[1];
    out->data[2] = words[2];
    out->data[3] = words[3];
    out->data[4] = words[4] & 0x7FULL;
}

static inline void block263_mul(block263* out, const block263* a, const block263* b)
{
    __m128i a128[3] = {
        _mm_set_epi64x(a->data[1], a->data[0]),
        _mm_set_epi64x(a->data[3], a->data[2]),
        _mm_set_epi64x(0, a->data[4] & 0x7FULL)
    };

    __m128i b128[3] = {
        _mm_set_epi64x(b->data[1], b->data[0]),
        _mm_set_epi64x(b->data[3], b->data[2]),
        _mm_set_epi64x(0, b->data[4] & 0x7FULL)
    };

    __m128i out128[3];
    __m128i product[5];
    clmul_schoolbook263(product, a128, b128);
    reduce_clmul263_product(out128, product);

    uint64_t words[6];
    _mm_storeu_si128((__m128i*)&words[0], out128[0]);
    _mm_storeu_si128((__m128i*)&words[2], out128[1]);
    _mm_storeu_si128((__m128i*)&words[4], out128[2]);

    out->data[0] = words[0];
    out->data[1] = words[1];
    out->data[2] = words[2];
    out->data[3] = words[3];
    out->data[4] = words[4] & 0x7FULL;
}

static void block263_pow_bin(block263* block, const char* exp_bits)
{
    block263 base = *block;
    block263 result = block263_set_one();

    for (const char* p = exp_bits; *p; ++p) {
        if (*p != '0' && *p != '1')
            continue;

        block263 tmp;

        block263_sqr(&tmp, &result);
        result = tmp;

        if (*p == '1') {
            block263_mul(&result, &result, &base);
        }
    }

    block263_mask(&result);
    *block = result;
}

static inline void block263_mersenne_inverse1_binary(block263* block)
{
    static const char EXP_INV_1[] =
        "11101111011110111101111011110111101111011110111101111011110111101111011110111101111011110111101111011110111101111011110111101111011101111011110111101111011110111101111011110111101111011110111101111011110111101111011110111101111011110111101111011110111101111011101";

    block263_pow_bin(block, EXP_INV_1);
}

static inline void block263_mersenne_inverse2_binary(block263* block)
{
    static const char EXP_INV_2[] =
        "11111111011111111011111111011111111011111111011111111011111111011111111011111111011111111011111111011111111011111111011111111011111111101111111101111111101111111101111111101111111101111111101111111101111111101111111101111111101111111101111111101111111101111111101";

    block263_pow_bin(block, EXP_INV_2);
}

static inline void block263_square_n(block263* out, const block263* in, unsigned count)
{
    *out = *in;
    for (unsigned i = 0; i < count; ++i)
        block263_sqr(out, out);
}

static inline void block263_mersenne_inverse1_chain(block263* block)
{
    const block263 x = *block;
    block263 x4, x5, r2, r3, r6, r12, r13, r26, t2, t3, t24, t27, t29;
    block263_square_n(&x4, &x, 2);
    block263_mul(&x5, &x4, &x);
    block263_square_n(&r2, &x4, 3);
    block263_mul(&r2, &r2, &x);
    block263_square_n(&r3, &r2, 5);
    block263_mul(&r3, &r3, &x);
    block263_square_n(&r6, &r3, 15);
    block263_mul(&r6, &r6, &r3);
    block263_square_n(&r12, &r6, 30);
    block263_mul(&r12, &r12, &r6);
    block263_square_n(&r13, &r12, 5);
    block263_mul(&r13, &r13, &x);
    block263_square_n(&r26, &r13, 65);
    block263_mul(&r26, &r26, &r13);
    block263_sqr(&t2, &r26);
    block263_mul(&t3, &t2, &r26);
    block263_square_n(&t24, &t3, 3);
    block263_mul(&t27, &t24, &t3);
    block263_mul(&t29, &t27, &t2);
    block263_square_n(&t29, &t29, 130);
    block263_mul(&t29, &t29, &t27);
    block263_square_n(&t29, &t29, 3);
    block263_mul(block, &t29, &x5);
}

static inline void block263_mersenne_inverse2_chain(block263* block)
{
    const block263 x = *block;
    block263 x3, x15, x255, x511, r2, r3, r6, r7, p, t;
    block263_sqr(&x3, &x);
    block263_mul(&x3, &x3, &x);
    block263_square_n(&x15, &x3, 2);
    block263_mul(&x15, &x15, &x3);
    block263_square_n(&x255, &x15, 4);
    block263_mul(&x255, &x255, &x15);
    block263_sqr(&x511, &x255);
    block263_mul(&x511, &x511, &x);
    block263_square_n(&r2, &x255, 9);
    block263_mul(&r2, &r2, &x255);
    block263_square_n(&r3, &r2, 9);
    block263_mul(&r3, &r3, &x255);
    block263_square_n(&r6, &r3, 27);
    block263_mul(&r6, &r6, &r3);
    block263_square_n(&r7, &r6, 9);
    block263_mul(&r7, &r7, &x255);
    block263_square_n(&p, &r7, 63);
    block263_mul(&p, &p, &r7);
    block263_square_n(&t, &p, 10);
    block263_mul(&t, &t, &x511);
    block263_square_n(&t, &t, 126);
    block263_mul(&t, &t, &p);
    block263_square_n(&t, &t, 2);
    block263_mul(block, &t, &x);
}
#ifndef GREATWALL_GF263_INVERSE_CHAIN
#define GREATWALL_GF263_INVERSE_CHAIN 1
#endif
static inline void block263_mersenne_inverse1(block263* block)
{
#if GREATWALL_GF263_INVERSE_CHAIN
    block263_mersenne_inverse1_chain(block);
#else
    block263_mersenne_inverse1_binary(block);
#endif
}
static inline void block263_mersenne_inverse2(block263* block)
{
#if GREATWALL_GF263_INVERSE_CHAIN
    block263_mersenne_inverse2_chain(block);
#else
    block263_mersenne_inverse2_binary(block);
#endif
}

static inline void block263_multiply_with_GF2_matrix(
    block263* block,
    const uint64_t matrix[263][5]
) {
    block263 tmp;
    tmp.data[0] = 0;
    tmp.data[1] = 0;
    tmp.data[2] = 0;
    tmp.data[3] = 0;
    tmp.data[4] = 0;

    const uint64_t x0 = block->data[0];
    const uint64_t x1 = block->data[1];
    const uint64_t x2 = block->data[2];
    const uint64_t x3 = block->data[3];
    const uint64_t x4 = block->data[4] & 0x7FULL;

    for (size_t row = 0; row < 263; ++row) {
        const uint64_t* A = matrix[row];

        uint64_t bit =
            (_mm_popcnt_u64(x0 & A[0]) ^
             _mm_popcnt_u64(x1 & A[1]) ^
             _mm_popcnt_u64(x2 & A[2]) ^
             _mm_popcnt_u64(x3 & A[3]) ^
             _mm_popcnt_u64(x4 & (A[4] & 0x7FULL))) & 1ULL;

        tmp.data[row >> 6] |= bit << (row & 63);
    }

    tmp.data[4] &= 0x7FULL;
    *block = tmp;
}

#endif
