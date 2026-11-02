#ifndef GF137_H
#define GF137_H

#include <assert.h>
#include <immintrin.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BLOCK137_BITS 137
#define BLOCK137_WORDS 3
#define BLOCK137_BYTES 18
#define BLOCK137_REDUCTION_TERM 21

typedef struct
{
	uint64_t data[BLOCK137_WORDS];
} block137;

static inline void block137_canonicalize(block137* a) {
    a->data[2] &= 0x1FFULL;
}

static inline void block137_load_to_block(block137* out, const uint8_t* in)
{
	assert((in[17] & 0xFE) == 0);
    memset(out, 0, sizeof(*out));
    memcpy(out, in, 18);
    out->data[2] &= 0x1FFULL;
}

static inline void block137_load_words(block137* out, const uint64_t in[3])
{
    out->data[0] = in[0];
    out->data[1] = in[1];
    out->data[2] = in[2] & 0x1FFULL;

    assert((in[2] & ~0x1FFULL) == 0);
}

static inline bool block137_is_canonical(const block137* in)
{
    return (in->data[2] & ~0x1FFULL) == 0;
}

static inline void block137_store_from_block(uint8_t* out, const block137* in)
{
	assert(block137_is_canonical(in));
	memcpy(out, in, 18);
}
static inline block137 block137_xor(block137 x, block137 y)
{
	block137 out;
	out.data[0] = x.data[0] ^ y.data[0];
	out.data[1] = x.data[1] ^ y.data[1];
	out.data[2] = x.data[2] ^ y.data[2];
	return out;
}

static inline bool block137_is_zero(const block137* value)
{
    uint64_t combined = 0;
    for (size_t word = 0; word < BLOCK137_WORDS; ++word)
        combined |= value->data[word];
    return combined == 0;
}

static inline bool block137_dot_is_one(const block137* a, const block137* b)
{
    unsigned int parity = 0;
    for (size_t word = 0; word < BLOCK137_WORDS; ++word)
        parity ^= (unsigned int)__builtin_parityll(a->data[word] & b->data[word]);
    return (parity & 1U) != 0;
}

static inline void clmul_schoolbook137(__m128i out[3], const __m128i a[2], const __m128i b[2]) {
  __m128i tmp[3];
  out[0] = _mm_clmulepi64_si128(a[0], b[0], 0x00);
  out[1] = _mm_clmulepi64_si128(a[0], b[0], 0x11);
  out[2] = _mm_clmulepi64_si128(a[1], b[1], 0x00);
  out[1] = _mm_xor_si128(out[1], _mm_clmulepi64_si128(a[0], b[1], 0x00));
  out[1] = _mm_xor_si128(out[1], _mm_clmulepi64_si128(a[1], b[0], 0x00));

  tmp[0] = _mm_clmulepi64_si128(a[0], b[0], 0x01);
  tmp[1] = _mm_clmulepi64_si128(a[0], b[0], 0x10);

  tmp[0] = _mm_xor_si128(tmp[0], tmp[1]);
  tmp[1] = _mm_slli_si128(tmp[0], 8);
  tmp[2] = _mm_srli_si128(tmp[0], 8);

  out[0] = _mm_xor_si128(out[0], tmp[1]);
  out[1] = _mm_xor_si128(out[1], tmp[2]);

  tmp[0] = _mm_clmulepi64_si128(a[1], b[0], 0x10);
  tmp[1] = _mm_clmulepi64_si128(a[0], b[1], 0x01);

  tmp[0] = _mm_xor_si128(tmp[0], tmp[1]);
  tmp[1] = _mm_slli_si128(tmp[0], 8);
  tmp[2] = _mm_srli_si128(tmp[0], 8);

  out[1] = _mm_xor_si128(out[1], tmp[1]);
  out[2] = _mm_xor_si128(out[2], tmp[2]);
}

static inline void reduce_clmul137(__m128i out[2], const __m128i in[3]) {
  uint64_t t[6] = {0};

  memcpy(&t[0], &in[0], 16);
  memcpy(&t[2], &in[1], 16);
  memcpy(&t[4], &in[2], 16);

  for (int bit = 383; bit >= 137; --bit) {
    int word = bit >> 6;
    int off  = bit & 63;
    uint64_t mask = 1ULL << off;
    if (t[word] & mask) {
      t[word] ^= mask;
      {
        int b = bit - 137;
        t[b >> 6] ^= (1ULL << (b & 63));
      }
      {
        int b = bit - 116;
        t[b >> 6] ^= (1ULL << (b & 63));
      }
    }
  }
  t[2] &= 0x1FFULL;
  t[3] = 0;
  t[4] = 0;
  t[5] = 0;
  memcpy(&out[0], &t[0], 16);
  out[1] = _mm_set_epi64x(0, t[2] & 0x1FFULL);
}

static inline void reduce_clmul137_fold_product(__m128i out[2], const __m128i in[3])
{
    uint64_t w[6];
    memcpy(w, in, sizeof(w));
    assert((w[4] >> 17) == 0 && w[5] == 0);

    const uint64_t h0 = (w[2] >> 9) | (w[3] << 55);
    const uint64_t h1 = (w[3] >> 9) | (w[4] << 55);
    const uint64_t h2 = w[4] >> 9;
    const uint64_t t0 = w[0] ^ h0 ^ (h0 << 21);
    const uint64_t t1 = w[1] ^ h1 ^ (h1 << 21) ^ (h0 >> 43);
    const uint64_t t2 = (w[2] & UINT64_C(0x1ff)) ^ h2 ^ (h2 << 21) ^ (h1 >> 43);
    const uint64_t u = t2 >> 9;
    const uint64_t result[4] = {
        t0 ^ u ^ (u << 21), t1, t2 & UINT64_C(0x1ff), 0
    };
    memcpy(out, result, sizeof(result));
}

#ifndef GREATWALL_GF137_FAST_REDUCTION
#define GREATWALL_GF137_FAST_REDUCTION 1
#endif
#if GREATWALL_GF137_FAST_REDUCTION != 0 && GREATWALL_GF137_FAST_REDUCTION != 1
#error "GREATWALL_GF137_FAST_REDUCTION must be 0 (bitwise) or 1 (fixed folds)"
#endif

static inline void reduce_clmul137_product(__m128i out[2], const __m128i in[3])
{
#if GREATWALL_GF137_FAST_REDUCTION
    reduce_clmul137_fold_product(out, in);
#else
    reduce_clmul137(out, in);
#endif
}

static inline void sqr137(__m128i out[3], const __m128i a[2]) {
  __m128i tmp[2];
  __m128i sqrT = _mm_set_epi64x(0x5554515045444140, 0x1514111005040100);
  __m128i mask = _mm_set_epi64x(0x0F0F0F0F0F0F0F0F, 0x0F0F0F0F0F0F0F0F);
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
}

static inline void gf137sqr(__m128i *out, const __m128i *in) {
  __m128i tmp[3];
  sqr137(tmp, in);
  reduce_clmul137(out, tmp);
}
static inline void gf137mul(__m128i *out, const __m128i *in1, const __m128i *in2) {
  __m128i tmp[3];
  clmul_schoolbook137(tmp, in1, in2);
  reduce_clmul137(out, tmp);
}

static inline void block137_sqr(block137* out, const block137* in)
{
    assert(block137_is_canonical(in));
    __m128i input[2] = {_mm_setzero_si128(), _mm_setzero_si128()};
    __m128i result[2], product[3];
    memcpy(input, in->data, sizeof(in->data));
    sqr137(product, input);
    reduce_clmul137_product(result, product);
    memcpy(out->data, result, sizeof(out->data));
    out->data[2] &= 0x1FFULL;
}

static inline void block137_mul(block137* out, const block137* a, const block137* b)
{
    assert(block137_is_canonical(a) && block137_is_canonical(b));
    __m128i input_a[2] = {_mm_setzero_si128(), _mm_setzero_si128()};
    __m128i input_b[2] = {_mm_setzero_si128(), _mm_setzero_si128()};
    __m128i result[2], product[3];
    memcpy(input_a, a->data, sizeof(a->data));
    memcpy(input_b, b->data, sizeof(b->data));
    clmul_schoolbook137(product, input_a, input_b);
    reduce_clmul137_product(result, product);
    memcpy(out->data, result, sizeof(out->data));
    out->data[2] &= 0x1FFULL;
}
static inline block137 block137_set_one(void)
{
    block137 r = {{0, 0, 0}};
    r.data[0] = 1;
    return r;
}

static inline void block137_mask(block137* x)
{
    x->data[2] &= 0x1FFULL;
}

static inline void block137_pow_bin(block137* block, const char* exp_bits)
{
    block137 base = *block;
    block137 result = block137_set_one();
    for (const char* p = exp_bits; *p; ++p) {
        if (*p != '0' && *p != '1')
            continue;

        block137 tmp;

        block137_sqr(&tmp, &result);
        result = tmp;

        if (*p == '1') {
            block137_mul(&result, &result, &base);
        }
    }

    block137_mask(&result);
    *block = result;
}

static inline void block137_mersenne_inverse1_binary(block137* block) {
    static const char EXP_INV_1[] =
        "11011011011011011011011011011011011011011011011011011011011011011011101101101101101101101101101101101101101101101101101101101101101101101";

    block137_pow_bin(block, EXP_INV_1);
}

static inline void block137_mersenne_inverse2_binary(block137* block) {
    static const char EXP_INV_1[] =
        "11011011011101101101101110110110110111011011011011101101101101110110110111011011011011101101101101110110110110111011011011011101101101101";

    block137_pow_bin(block, EXP_INV_1);
}

static inline void block137_square_n(block137* out, const block137* in, unsigned count)
{
    *out = *in;
    for (unsigned i = 0; i < count; ++i)
        block137_sqr(out, out);
}

static inline void block137_mersenne_inverse1_chain(block137* block)
{
    const block137 x = *block;
    block137 r2, r3, r5, r10, r20, t, u, v, w;
    block137_square_n(&r2, &x, 3);
    block137_mul(&r2, &r2, &x);
    block137_square_n(&r3, &r2, 3);
    block137_mul(&r3, &r3, &x);
    block137_square_n(&r5, &r3, 6);
    block137_mul(&r5, &r5, &r2);
    block137_square_n(&r10, &r5, 15);
    block137_mul(&r10, &r10, &r5);
    block137_square_n(&r20, &r10, 30);
    block137_mul(&r20, &r20, &r10);
    block137_square_n(&t, &r20, 9);
    block137_mul(&t, &t, &r3);
    block137_sqr(&u, &t);
    block137_mul(&v, &u, &t);
    block137_sqr(&u, &u);
    block137_mul(&w, &u, &t);
    block137_square_n(&v, &v, 69);
    block137_mul(block, &v, &w);
}

static inline void block137_mersenne_inverse2_chain(block137* block)
{
    const block137 x = *block;
    block137 x2, x3, x5, t, q, d, p, p2, p4, b, twice_b;
    block137_sqr(&x2, &x);
    block137_mul(&x3, &x2, &x);
    block137_sqr(&x5, &x2);
    block137_mul(&x5, &x5, &x);
    block137_square_n(&t, &x3, 3);
    block137_mul(&t, &t, &x3);
    block137_square_n(&t, &t, 2);
    block137_mul(&t, &t, &x);
    block137_square_n(&q, &t, 3);
    block137_mul(&q, &q, &x5);
    block137_square_n(&d, &q, 3);
    block137_mul(&d, &d, &x5);
    block137_mul(&p, &d, &x);
    block137_square_n(&p2, &p, 13);
    block137_mul(&p2, &p2, &p);
    block137_square_n(&p4, &p2, 26);
    block137_mul(&p4, &p4, &p2);
    block137_square_n(&b, &p4, 10);
    block137_mul(&b, &b, &q);
    block137_square_n(&twice_b, &b, 62);
    block137_mul(&twice_b, &twice_b, &b);
    block137_square_n(&twice_b, &twice_b, 13);
    block137_mul(block, &twice_b, &d);
}

#ifndef GREATWALL_GF137_INVERSE_CHAIN
#define GREATWALL_GF137_INVERSE_CHAIN 1
#endif
#if GREATWALL_GF137_INVERSE_CHAIN != 0 && GREATWALL_GF137_INVERSE_CHAIN != 1
#error "GREATWALL_GF137_INVERSE_CHAIN must be 0 (binary) or 1 (fixed chain)"
#endif

static inline void block137_mersenne_inverse1(block137* block)
{
#if GREATWALL_GF137_INVERSE_CHAIN
    block137_mersenne_inverse1_chain(block);
#else
    block137_mersenne_inverse1_binary(block);
#endif
}

static inline void block137_mersenne_inverse2(block137* block)
{
#if GREATWALL_GF137_INVERSE_CHAIN
    block137_mersenne_inverse2_chain(block);
#else
    block137_mersenne_inverse2_binary(block);
#endif
}

static inline void block137_multiply_with_GF2_matrix(block137* block, const uint64_t matrix[137][3]) {
    block137 tmp;
    tmp.data[0] = 0;
    tmp.data[1] = 0;
    tmp.data[2] = 0;

    const uint64_t x0 = block->data[0];
    const uint64_t x1 = block->data[1];
    const uint64_t x2 = block->data[2] & 0x1FFULL;

    for (size_t row = 0; row < 137; ++row) {
        const uint64_t* A = matrix[row];

        uint64_t bit =
            (_mm_popcnt_u64(x0 & A[0]) ^
             _mm_popcnt_u64(x1 & A[1]) ^
             _mm_popcnt_u64(x2 & A[2])) & 1ULL;

        tmp.data[row >> 6] ^= bit << (row & 63);
    }

    tmp.data[2] &= 0x1FFULL;
    *block = tmp;
}

#endif
