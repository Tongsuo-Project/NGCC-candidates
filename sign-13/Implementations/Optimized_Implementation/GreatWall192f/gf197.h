#ifndef GREATWALL_GF197_H
#define GREATWALL_GF197_H

#include <assert.h>
#include <immintrin.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define BLOCK197_BITS 197
#define BLOCK197_WORDS 4
#define BLOCK197_BYTES 25
#define BLOCK197_REDUCTION_TERM 21
typedef struct { uint64_t data[4]; } block197;
static inline void block197_canonicalize(block197* a) {
    a->data[3] &= 0x1FULL;
}

static inline bool block197_is_canonical(const block197* in)
{
    return (in->data[3] & ~0x1FULL) == 0;
}

static inline void block197_load_to_block(block197* out, const uint8_t* in)
{
    assert((in[24] & 0xE0) == 0);
    memset(out, 0, sizeof(*out));
    memcpy(out, in, 25);
    out->data[3] &= 0x1FULL;
}

static inline void block197_load_words(block197* out, const uint64_t in[4])
{
    out->data[0] = in[0];
    out->data[1] = in[1];
    out->data[2] = in[2];
    out->data[3] = in[3] & 0x1FULL;

    assert((in[3] & ~0x1FULL) == 0);
}

static inline void block197_store_from_block(uint8_t* out, const block197* in)
{
    assert(block197_is_canonical(in));
    memcpy(out, in, 25);
}

static inline block197 block197_xor(block197 x, block197 y)
{
    block197 out;
    out.data[0] = x.data[0] ^ y.data[0];
    out.data[1] = x.data[1] ^ y.data[1];
    out.data[2] = x.data[2] ^ y.data[2];
    out.data[3] = (x.data[3] ^ y.data[3]) & 0x1FULL;
    return out;
}

static inline bool block197_is_zero(const block197* a) {
    return (a->data[0]|a->data[1]|a->data[2]|a->data[3])==0;
}
static inline bool block197_dot_is_one(const block197* a,const block197* b) {
    unsigned p=0;
    for(size_t i=0;i<4;++i) p^=(unsigned)__builtin_parityll(a->data[i]&b->data[i]);
    return (p&1)!=0;
}
static inline __m128i* block197_as_m128i(block197* block) {
  return (__m128i*)block;
}

static inline void block197_mask(block197* x)
{
    x->data[3] &= 0x1FULL;
}

static inline block197 block197_set_one(void)
{
    block197 r = {{0, 0, 0, 0}};
    r.data[0] = 1;
    return r;
}

static inline void clmul_schoolbook197(__m128i out[4],
                                       const __m128i a[2],
                                       const __m128i b[2])
{
    uint64_t aw[4], bw[4];
    uint64_t t[8] = {0};

    memcpy(aw, a, 32);
    memcpy(bw, b, 32);

    aw[3] &= 0x1FULL;
    bw[3] &= 0x1FULL;

    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 4; j++) {
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
}

static inline void reduce_clmul197(__m128i out[2], const __m128i in[4])
{
    uint64_t t[8] = {0};

    memcpy(&t[0], &in[0], 16);
    memcpy(&t[2], &in[1], 16);
    memcpy(&t[4], &in[2], 16);
    memcpy(&t[6], &in[3], 16);

    for (int bit = 511; bit >= 197; --bit) {
        int word = bit >> 6;
        int off  = bit & 63;
        uint64_t mask = 1ULL << off;

        if (t[word] & mask) {
            t[word] ^= mask;

            {
                int b = bit - 197;
                t[b >> 6] ^= (1ULL << (b & 63));
            }

            {
                int b = bit - 176;
                t[b >> 6] ^= (1ULL << (b & 63));
            }
            {
                int b = bit - 195;
                t[b >> 6] ^= (1ULL << (b & 63));
            }
            {
                int b = bit - 196;
                t[b >> 6] ^= (1ULL << (b & 63));
            }
        }
    }

    t[3] &= 0x1FULL;
    t[4] = 0;
    t[5] = 0;
    t[6] = 0;
    t[7] = 0;

    out[0] = _mm_set_epi64x(t[1], t[0]);
    out[1] = _mm_set_epi64x(t[3] & 0x1FULL, t[2]);
}

static inline void sqr197(__m128i out[4], const __m128i a[2])
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
}

static inline void gf197sqr(__m128i* out, const __m128i* in)
{
    __m128i tmp[4];
    sqr197(tmp, in);
    reduce_clmul197(out, tmp);
}

static inline void gf197mul(__m128i* out, const __m128i* in1, const __m128i* in2)
{
    __m128i tmp[4];
    clmul_schoolbook197(tmp, in1, in2);
    reduce_clmul197(out, tmp);
}

static inline void reduce_clmul197_fold_product(__m128i result[2],const __m128i in[4]) {
    uint64_t w[8]; memcpy(w,in,sizeof(w));
    assert((w[6]>>9)==0 && w[7]==0);
    const uint64_t w0=w[0],w1=w[1],w2=w[2],w3=w[3],w4=w[4],w5=w[5],w6=w[6];
    block197 value; block197* out=&value;
    const uint64_t h0=(w3>>5)|(w4<<59), h1=(w4>>5)|(w5<<59);
    const uint64_t h2=(w5>>5)|(w6<<59), h3=w6>>5;
    const uint64_t t0=w0^h0^(h0<<1)^(h0<<2)^(h0<<21);
    const uint64_t t1=w1^h1^(h1<<1)^(h1<<2)^(h1<<21)^(h0>>63)^(h0>>62)^(h0>>43);
    const uint64_t t2=w2^h2^(h2<<1)^(h2<<2)^(h2<<21)^(h1>>63)^(h1>>62)^(h1>>43);
    const uint64_t t3=(w3&UINT64_C(0x1f))^h3^(h3<<1)^(h3<<2)^(h3<<21)^(h2>>63)^(h2>>62)^(h2>>43);
    const uint64_t u=t3>>5;
    out->data[0]=t0^u^(u<<1)^(u<<2)^(u<<21);
    out->data[1]=t1; out->data[2]=t2; out->data[3]=t3&UINT64_C(0x1f);
    memcpy(result,&value,sizeof(value));
}
#ifndef GREATWALL_GF197_FAST_REDUCTION
#define GREATWALL_GF197_FAST_REDUCTION 1
#endif
static inline void reduce_clmul197_product(__m128i out[2],const __m128i in[4]) {
#if GREATWALL_GF197_FAST_REDUCTION
    reduce_clmul197_fold_product(out,in);
#else
    reduce_clmul197(out,in);
#endif
}
static inline void block197_sqr(block197* out,const block197* in) {
    assert(block197_is_canonical(in));
    __m128i a[2],p[4],r[2]; memcpy(a,in,sizeof(*in));
    sqr197(p,a); reduce_clmul197_product(r,p); memcpy(out,r,sizeof(*out));
}
static inline void block197_mul(block197* out,const block197* a,const block197* b) {
    assert(block197_is_canonical(a)&&block197_is_canonical(b));
    __m128i av[2],bv[2],p[4],r[2];
    memcpy(av,a,sizeof(*a)); memcpy(bv,b,sizeof(*b));
    clmul_schoolbook197(p,av,bv); reduce_clmul197_product(r,p); memcpy(out,r,sizeof(*out));
}
static inline void block197_pow_bin(block197* block, const char* exp_bits)
{
    block197 base = *block;
    block197 result = block197_set_one();

    for (const char* p = exp_bits; *p; ++p) {
        if (*p != '0' && *p != '1')
            continue;

        block197 tmp;

        block197_sqr(&tmp, &result);
        result = tmp;

        if (*p == '1') {
            block197_mul(&result, &result, &base);
        }
    }

    block197_mask(&result);
    *block = result;
}
static inline void block197_mersenne_inverse1_binary(block197* block)
{
    static const char EXP_INV_1[] =
        "11101110111101110111101110111101110111101110111101110111101110111101110111101110111101110111101110111011110111011110111011110111011110111011110111011110111011110111011110111011110111011110111011101";

    block197_pow_bin(block, EXP_INV_1);
}
static inline void block197_mersenne_inverse2_binary(block197* block)
{
    static const char EXP_INV_2[] =
        "11111111111101111111111110111111111111011111111111101111111111110111111111111011111111111101111111111111011111111111101111111111110111111111111011111111111101111111111110111111111111011111111111101";

    block197_pow_bin(block, EXP_INV_2);
}
static inline void block197_square_n(block197* out,const block197* in,unsigned count) {
    *out=*in; for(unsigned i=0;i<count;++i) block197_sqr(out,out);
}
static inline void block197_mersenne_inverse1_chain(block197* block)
{

    const block197 x = *block;
    block197 t, x3, x7, x14, p, r2, r4, r8, r10, b;
    block197_sqr(&t, &x);
    block197_mul(&x3, &t, &x);
    block197_sqr(&t, &x3);
    block197_mul(&x7, &t, &x);
    block197_sqr(&x14, &x7);
    block197_square_n(&t, &x7, 4);
    block197_mul(&t, &t, &x7);
    block197_square_n(&t, &t, 2);
    block197_mul(&p, &t, &x);
    block197_square_n(&r2, &p, 9);
    block197_mul(&r2, &r2, &p);
    block197_square_n(&r4, &r2, 18);
    block197_mul(&r4, &r4, &r2);
    block197_square_n(&r8, &r4, 36);
    block197_mul(&r8, &r8, &r4);
    block197_square_n(&r10, &r8, 18);
    block197_mul(&r10, &r10, &r2);
    block197_square_n(&b, &r10, 4);
    block197_mul(&b, &b, &x14);
    block197_square_n(&t, &b, 94);
    block197_mul(&t, &t, &b);
    block197_square_n(&t, &t, 9);
    block197_mul(block, &t, &p);
}
static inline void block197_mersenne_inverse2_chain(block197* block)
{

    const block197 x = *block;
    block197 t, a2, a3, a5, a10, a11, d, p, r2, r3, r6, r7, b;
    block197_sqr(&t, &x);
    block197_mul(&a2, &t, &x);
    block197_sqr(&t, &a2);
    block197_mul(&a3, &t, &x);
    block197_square_n(&t, &a3, 2);
    block197_mul(&a5, &t, &a2);
    block197_square_n(&t, &a5, 5);
    block197_mul(&a10, &t, &a5);
    block197_sqr(&t, &a10);
    block197_mul(&a11, &t, &x);
    block197_square_n(&t, &a11, 2);
    block197_mul(&d, &t, &x);
    block197_mul(&p, &d, &x);
    block197_square_n(&r2, &p, 13);
    block197_mul(&r2, &r2, &p);
    block197_square_n(&r3, &r2, 13);
    block197_mul(&r3, &r3, &p);
    block197_square_n(&r6, &r3, 39);
    block197_mul(&r6, &r6, &r3);
    block197_square_n(&r7, &r6, 13);
    block197_mul(&r7, &r7, &p);
    block197_sqr(&b, &r7);
    block197_mul(&b, &b, &x);
    block197_square_n(&t, &b, 92);
    block197_mul(&t, &t, &b);
    block197_square_n(&t, &t, 13);
    block197_mul(block, &t, &d);
}
#ifndef GREATWALL_GF197_INVERSE_CHAIN
#define GREATWALL_GF197_INVERSE_CHAIN 1
#endif
static inline void block197_mersenne_inverse1(block197* block) {
#if GREATWALL_GF197_INVERSE_CHAIN
    block197_mersenne_inverse1_chain(block);
#else
    block197_mersenne_inverse1_binary(block);
#endif
}
static inline void block197_mersenne_inverse2(block197* block) {
#if GREATWALL_GF197_INVERSE_CHAIN
    block197_mersenne_inverse2_chain(block);
#else
    block197_mersenne_inverse2_binary(block);
#endif
}
static inline void block197_multiply_with_GF2_matrix(
    block197* block,
    const uint64_t matrix[197][4]
) {
    block197 tmp;
    tmp.data[0] = 0;
    tmp.data[1] = 0;
    tmp.data[2] = 0;
    tmp.data[3] = 0;

    const uint64_t x0 = block->data[0];
    const uint64_t x1 = block->data[1];
    const uint64_t x2 = block->data[2];
    const uint64_t x3 = block->data[3] & 0x1FULL;

    for (size_t row = 0; row < 197; ++row) {
        const uint64_t* A = matrix[row];

        uint64_t bit =
            (_mm_popcnt_u64(x0 & A[0]) ^
             _mm_popcnt_u64(x1 & A[1]) ^
             _mm_popcnt_u64(x2 & A[2]) ^
             _mm_popcnt_u64(x3 & (A[3] & 0x1FULL))) & 1ULL;

        tmp.data[row >> 6] |= bit << (row & 63);
    }

    tmp.data[3] &= 0x1FULL;
    *block = tmp;
}
#endif
