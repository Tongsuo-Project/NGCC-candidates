#include <string.h>
#include <stdlib.h>

#include "gf2x.h"

#ifdef OPTIMIZED_IMPLEMENTATION
#define K_SQR_THRESHOLD 64
#else
#define K_SQR_THRESHOLD 0
#endif

#define INV_MAX_ITER 14

const size_t k0[] = {0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
const size_t t0[] = {0, 4719, 7078, 6488, 5124, 1642, 6619, 4607, 636, 8142, 6676, 7462, 3144, 4197};
const size_t k1[] = {0, 1, 0, 3, 11, 0, 27, 91, 0, 0, 219, 0, 0, 1243};
const size_t t1[] = {0, 4719, 0, 3539, 5359, 0, 4194, 4219, 0, 0, 3176, 0, 0, 3005};


#ifdef OPTIMIZED_IMPLEMENTATION

// Multiplies two 512-bit polynomials.
static inline void vpclmul_mul(uint64_t *c, const uint64_t *a, const uint64_t *b)
{
    const __m512i Zero = _mm512_setzero_si512();

    const __m512i Mask0 = _mm512_set_epi64(1, 0, 1, 0, 1, 0, 1, 0);
    const __m512i Mask1 = _mm512_set_epi64(3, 2, 3, 2, 3, 2, 3, 2);
    const __m512i Mask2 = _mm512_set_epi64(5, 4, 5, 4, 5, 4, 5, 4);
    const __m512i Mask3 = _mm512_set_epi64(7, 6, 7, 6, 7, 6, 7, 6);

    const __m512i Perm0 = _mm512_set_epi64(5, 4, 3, 2, 1, 0, 7, 6);
    const __m512i Perm1 = _mm512_set_epi64(3, 2, 1, 0, 7, 6, 5, 4);
    const __m512i Perm2 = _mm512_set_epi64(1, 0, 7, 6, 5, 4, 3, 2);

    __m512i A0 = _mm512_load_si512((__m512i*)a);
    __m512i B = _mm512_load_si512((__m512i*)b);
    __m512i A1 = _mm512_permutexvar_epi64(Mask1, A0);
    __m512i A2 = _mm512_permutexvar_epi64(Mask2, A0);
    __m512i A3 = _mm512_permutexvar_epi64(Mask3, A0);
    A0 = _mm512_permutexvar_epi64(Mask0, A0);

    __m512i Even0 = _mm512_clmulepi64_epi128(A0, B, 0x00);
    __m512i Even1 = _mm512_clmulepi64_epi128(A0, B, 0x11);
    __m512i Even2 = _mm512_clmulepi64_epi128(A1, B, 0x00);
    __m512i Even3 = _mm512_clmulepi64_epi128(A1, B, 0x11);
    __m512i Even4 = _mm512_clmulepi64_epi128(A2, B, 0x00);
    __m512i Even5 = _mm512_clmulepi64_epi128(A2, B, 0x11);
    __m512i Even6 = _mm512_clmulepi64_epi128(A3, B, 0x00);
    __m512i Even7 = _mm512_clmulepi64_epi128(A3, B, 0x11);
    __m512i Odd0 = _mm512_clmulepi64_epi128(A0, B, 0x10);
    __m512i Odd1 = _mm512_clmulepi64_epi128(A0, B, 0x01);
    __m512i Odd2 = _mm512_clmulepi64_epi128(A1, B, 0x10);
    __m512i Odd3 = _mm512_clmulepi64_epi128(A1, B, 0x01);
    __m512i Odd4 = _mm512_clmulepi64_epi128(A2, B, 0x10);
    __m512i Odd5 = _mm512_clmulepi64_epi128(A2, B, 0x01);
    __m512i Odd6 = _mm512_clmulepi64_epi128(A3, B, 0x10);
    __m512i Odd7 = _mm512_clmulepi64_epi128(A3, B, 0x01);

    Even1 = _mm512_permutexvar_epi64(Perm0, Even1);
    Even2 = _mm512_permutexvar_epi64(Perm0, Even2);
    Even3 = _mm512_permutexvar_epi64(Perm1, Even3);
    Even4 = _mm512_permutexvar_epi64(Perm1, Even4);
    Even5 = _mm512_permutexvar_epi64(Perm2, Even5);
    Even6 = _mm512_permutexvar_epi64(Perm2, Even6);
    Odd2 = _mm512_permutexvar_epi64(Perm0, Odd2);
    Odd3 = _mm512_permutexvar_epi64(Perm0, Odd3);
    Odd4 = _mm512_permutexvar_epi64(Perm1, Odd4);
    Odd5 = _mm512_permutexvar_epi64(Perm1, Odd5);
    Odd6 = _mm512_permutexvar_epi64(Perm2, Odd6);
    Odd7 = _mm512_permutexvar_epi64(Perm2, Odd7);

    Even0 = _mm512_mask_xor_epi64(Even0, 0xFC, Even0, Even1);
    Even0 = _mm512_mask_xor_epi64(Even0, 0xFC, Even0, Even2);
    Even0 = _mm512_mask_xor_epi64(Even0, 0xF0, Even0, Even3);
    Even0 = _mm512_mask_xor_epi64(Even0, 0xF0, Even0, Even4);
    Even0 = _mm512_mask_xor_epi64(Even0, 0xC0, Even0, Even5);
    Even0 = _mm512_mask_xor_epi64(Even0, 0xC0, Even0, Even6);
    Even7 = _mm512_mask_xor_epi64(Even7, 0x03, Even7, Even1);
    Even7 = _mm512_mask_xor_epi64(Even7, 0x03, Even7, Even2);
    Even7 = _mm512_mask_xor_epi64(Even7, 0x0F, Even7, Even3);
    Even7 = _mm512_mask_xor_epi64(Even7, 0x0F, Even7, Even4);
    Even7 = _mm512_mask_xor_epi64(Even7, 0x3F, Even7, Even5);
    Even7 = _mm512_mask_xor_epi64(Even7, 0x3F, Even7, Even6);

    Odd0 = _mm512_xor_si512(Odd0, Odd1);
    Odd0 = _mm512_mask_xor_epi64(Odd0, 0xFC, Odd0, Odd2);
    Odd0 = _mm512_mask_xor_epi64(Odd0, 0xFC, Odd0, Odd3);
    Odd0 = _mm512_mask_xor_epi64(Odd0, 0xF0, Odd0, Odd4);
    Odd0 = _mm512_mask_xor_epi64(Odd0, 0xF0, Odd0, Odd5);
    Odd0 = _mm512_mask_xor_epi64(Odd0, 0xC0, Odd0, Odd6);
    Odd0 = _mm512_mask_xor_epi64(Odd0, 0xC0, Odd0, Odd7);
    Odd7 = _mm512_mask_xor_epi64(Zero, 0x3F, Odd7, Odd6);
    Odd7 = _mm512_mask_xor_epi64(Odd7, 0x0F, Odd7, Odd5);
    Odd7 = _mm512_mask_xor_epi64(Odd7, 0x0F, Odd7, Odd4);
    Odd7 = _mm512_mask_xor_epi64(Odd7, 0x03, Odd7, Odd3);
    Odd7 = _mm512_mask_xor_epi64(Odd7, 0x03, Odd7, Odd2);

    Odd7 = _mm512_alignr_epi64(Odd7, Odd0, 7);
    Odd0 = _mm512_alignr_epi64(Odd0, Zero, 7);

    Even0 = _mm512_xor_si512(Even0, Odd0);
    Even7 = _mm512_xor_si512(Even7, Odd7);

    _mm512_store_si512((__m512i*)c, Even0);
    _mm512_store_si512((__m512i*)(c + 8), Even7);
}

// Squares a polynomial without reduction.
static inline void gf2x_sqr_vpclmul(uint64_t *c, const uint64_t *a)
{
    const __m512i Perm = _mm512_set_epi64(7, 3, 6, 2, 5, 1, 4, 0);
    for (size_t i = 0; i < R_SIZE_ZMMS; i++)
    {
        __m512i A = _mm512_load_si512((__m512i*)(a + i * 8));
        A = _mm512_permutexvar_epi64(Perm, A);
        __m512i Result1 = _mm512_clmulepi64_epi128(A, A, 0x00);
        __m512i Result2 = _mm512_clmulepi64_epi128(A, A, 0x11);
        _mm512_store_si512((__m512i*)(c + (i << 4)), Result1);
        _mm512_store_si512((__m512i*)(c + (i << 4 ^ 8)), Result2);
    }
}
#else

// Multiplies two polynomials.
static inline void schoolbook_mul(uint64_t *c, const uint64_t *a, const uint64_t *b, size_t n64)
{
    for (size_t i = 0; i < n64; i++)
    {
        uint64_t mask = - (a[i] & 1);
        for (size_t j = 0; j < n64; j++)
        {
            c[i + j] ^= b[j] & mask;
        }
        for (size_t bit = 1; bit < 64; bit++)
        {
            mask = - ((a[i] >> bit) & 1);
            size_t low_bits = 64 - bit;
            for (size_t j = 0; j < n64; j++)
            {
                c[i + j] ^= (b[j] << bit) & mask;
                c[i + j + 1] ^= (b[j] >> low_bits) & mask;
            }
        }
    }
}
#endif

// Adds two vectors.
static inline void gf2x_add_len(uint64_t *c, const uint64_t *a, const uint64_t *b, size_t n64)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    for (size_t i = 0; i < n64; i += 8)
    {
        __m512i A = _mm512_load_si512((__m512i*)(a + i));
        __m512i B = _mm512_load_si512((__m512i*)(b + i));
        __m512i C = _mm512_xor_si512(A, B);
        _mm512_store_si512((__m512i*)(c + i), C);
    }
#else
    for (size_t i = 0; i < n64; i++)
    {
        c[i] = a[i] ^ b[i];
    }
#endif
}

// Adds three vectors.
static inline void gf2x_add3_len(uint64_t *c, const uint64_t *a, const uint64_t *b, const uint64_t *d, size_t n64)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    for (size_t i = 0; i < n64; i += 8)
    {
        __m512i A = _mm512_load_si512((__m512i*)(a + i));
        __m512i B = _mm512_load_si512((__m512i*)(b + i));
        __m512i D = _mm512_load_si512((__m512i*)(d + i));
        __m512i C = _mm512_ternarylogic_epi64(A, B, D, 0x96);
        _mm512_store_si512((__m512i*)(c + i), C);
    }
#else
    for (size_t i = 0; i < n64; i++)
    {
        c[i] = a[i] ^ b[i] ^ d[i];
    }
#endif
}

// Multiplies two polynomials.
void karatsuba_mul(uint64_t *c, const uint64_t *a, const uint64_t *b, size_t n64, uint64_t *buf)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    if (n64 == 8)
    {
        vpclmul_mul(c, a, b);
        return;
    }
#else
    if (n64 <= 1)
    {
        memset(c, 0, n64 * 2 * 8);
        schoolbook_mul(c, a, b, n64);
        return;
    }
#endif

    size_t m64 = n64 / 2;
    const uint64_t *a_low = a, *a_high = a + m64;
    const uint64_t *b_low = b, *b_high = b + m64;
    uint64_t *a_sum = buf, *b_sum = buf + m64;
    uint64_t *z0 = buf + n64, *z1 = buf + n64 * 2, *z2 = buf + n64 * 3;
    uint64_t *buf_next = buf + n64 * 4;

    gf2x_add_len(a_sum, a_low, a_high, m64);
    gf2x_add_len(b_sum, b_low, b_high, m64);

    karatsuba_mul(z0, a_low, b_low, m64, buf_next);
    karatsuba_mul(z2, a_high, b_high, m64, buf_next);
    karatsuba_mul(z1, a_sum, b_sum, m64, buf_next);

    gf2x_add3_len(z1, z1, z0, z2, n64);

    fast_cpy(c, z0, n64);
    fast_cpy(c + n64, z2, n64);
    gf2x_add_len(c + m64, z1, c + m64, n64);
}

#ifdef OPTIMIZED_IMPLEMENTATION
// Multiplies operands while skipping the public zero allocation padding.
// n64 is a power of two and used64 is a positive multiple of eight. Inputs
// a[used64..n64) and b[used64..n64) are zero by the polynomial contract.
// Every 2*n64 output word is initialized, including the skipped zero tail.
static void karatsuba_mul_partial(uint64_t *c, const uint64_t *a,
                                 const uint64_t *b, size_t n64,
                                 size_t used64, uint64_t *buf)
{
    if (used64 == n64) {
        karatsuba_mul(c, a, b, n64, buf);
        return;
    }
    size_t m64 = n64 / 2;
    if (used64 <= m64) {
        karatsuba_mul_partial(c, a, b, m64, used64, buf);
        trike_setz((uint8_t *)(c + n64), n64 * sizeof(uint64_t));
        return;
    }

    uint64_t *a_sum = buf, *b_sum = buf + m64;
    uint64_t *z0 = buf + n64, *z1 = buf + n64 * 2, *z2 = buf + n64 * 3;
    uint64_t *buf_next = buf + n64 * 4;
    gf2x_add_len(a_sum, a, a + m64, m64);
    gf2x_add_len(b_sum, b, b + m64, m64);
    karatsuba_mul(z0, a, b, m64, buf_next);
    karatsuba_mul_partial(z2, a + m64, b + m64, m64,
                          used64 - m64, buf_next);
    karatsuba_mul(z1, a_sum, b_sum, m64, buf_next);
    gf2x_add3_len(z1, z1, z0, z2, n64);
    fast_cpy(c, z0, n64);
    fast_cpy(c + n64, z2, n64);
    gf2x_add_len(c + m64, z1, c + m64, n64);
}
#endif

// Reduces a polynomial modulo x^r - 1.
static inline void gf2x_mod(uint64_t *c, const uint64_t *a)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    for (size_t i = 0; i < R_SIZE_64; i += 8)
    {
        __m512i High0 = _mm512_loadu_si512((__m512i*)(a + R_SIZE_64 + i - 1));
        __m512i High1 = _mm512_loadu_si512((__m512i*)(a + R_SIZE_64 + i));
        __m512i Low = _mm512_load_si512((__m512i*)(a + i));
        High0 = _mm512_srli_epi64(High0, R_64_LAST_BITS);
        High1 = _mm512_slli_epi64(High1, R_64_REST_BITS);
        Low = _mm512_ternarylogic_epi64(Low, High0, High1, 0x96);
        _mm512_store_si512((__m512i*)(c + i), Low);
    }
    c[R_SIZE_64 - 1] &= R_64_LAST_MASK;
    memset(c + R_SIZE_64, 0, (R_SIZE_ZMM_64 - R_SIZE_64) * 8);
#else
    memcpy(c, a, R_SIZE_64 << 3);
    for (size_t i = 0; i < R_SIZE_64; i++)
    {
        c[i] ^= (a[i + R_SIZE_64 - 1] >> R_64_LAST_BITS) | (a[i + R_SIZE_64] << R_64_REST_BITS);
    }
    c[R_SIZE_64 - 1] &= R_64_LAST_MASK;
#endif
}

// Adds two polynomials.
void gf2x_add(uint8_t *c, const uint8_t *a, const uint8_t *b)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    gf2x_add_len((uint64_t *)c, (const uint64_t *)a, (const uint64_t *)b, R_SIZE_64);
#else
    for (size_t i = 0; i < R_SIZE_BYTES; i++)
    {
        c[i] = a[i] ^ b[i];
    }
#endif
}

// Multiplies and reduces two polynomials.
int gf2x_mul(uint8_t *c, const uint8_t *a, const uint8_t *b)
{
    uint64_t *temp = (uint64_t *)trike_alloc(PADDED_R_SIZE_64 * 2 * sizeof(uint64_t));
    uint64_t *buf = (uint64_t *)trike_alloc(PADDED_R_SIZE_64 * 8 * sizeof(uint64_t));

    if (!temp || !buf) { free(temp); free(buf); return -1; }
    // Karatsuba initializes every output and scratch word before reading it.
    // In particular, its leaves overwrite 2*n64 words (or clear before XOR
    // in the portable implementation), so these buffers need no preclear.

#ifdef OPTIMIZED_IMPLEMENTATION
    karatsuba_mul_partial(temp, (const uint64_t *)a, (const uint64_t *)b,
                          PADDED_R_SIZE_64, R_SIZE_ZMM_64, buf);
    gf2x_mod((uint64_t *)c, temp);
#else
    uint64_t *words = (uint64_t *)calloc(PADDED_R_SIZE_64 * 2, sizeof(uint64_t));
    if (!words) { free(temp); free(buf); return -1; }
    uint64_t *a_words = words, *b_words = words + PADDED_R_SIZE_64;

    // Polynomial bytes are little-endian, independent of host byte order.
    for (size_t i = 0; i < R_SIZE_BYTES; i++)
    {
        a_words[i / 8] |= (uint64_t)a[i] << ((i % 8) * 8);
        b_words[i / 8] |= (uint64_t)b[i] << ((i % 8) * 8);
    }
    karatsuba_mul(temp, a_words, b_words, PADDED_R_SIZE_64, buf);
    gf2x_mod(a_words, temp);
    for (size_t i = 0; i < R_SIZE_BYTES; i++)
    {
        c[i] = (uint8_t)(a_words[i / 8] >> ((i % 8) * 8));
    }
    memset(c + R_SIZE_BYTES, 0, R_ZMM_SIZE_BYTES - R_SIZE_BYTES);
    free(words);
#endif

    free(buf);
    free(temp);
    return 0;
}

// Squares and reduces a polynomial.
static inline int gf2x_sqr(uint8_t *c, const uint8_t *a)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    uint64_t *temp = (uint64_t *)trike_alloc(PADDED_R_SIZE_64 * 2 * sizeof(uint64_t));
    if (!temp) return -1;
    trike_setz((uint8_t *)temp, PADDED_R_SIZE_64 * 2 * sizeof(uint64_t));
    gf2x_sqr_vpclmul(temp, (const uint64_t *)a);
    gf2x_mod((uint64_t *)c, temp);
    free(temp);
    return 0;
#else
    return gf2x_mul(c, a, a);
#endif
}

// Computes repeated squaring.
static inline int gf2x_k_sqr(uint8_t *c, const uint8_t *a, size_t k, size_t t)
{
    if (k <= K_SQR_THRESHOLD)
    {
#ifdef OPTIMIZED_IMPLEMENTATION
        fast_cpy((uint64_t *)c, (const uint64_t *)a, R_SIZE_ZMM_64);
#else
        memcpy(c, a, R_ZMM_SIZE_BYTES);
#endif
        for (size_t i = 0; i < k; i++)
        {
            if (gf2x_sqr(c, c)) return -1;
        }
    }
    else
    {
        uint64_t idx = 0;
        size_t pos = 0;
        for (size_t i = 0; i < R_SIZE_BYTES; i++)
        {
            uint8_t byte = 0;
            for (size_t bit = 0; bit < 8 && idx < PARAM_R; bit++, idx++)
            {
                byte |= (a[pos >> 3] >> (pos & 7) & 1) << bit;
                pos += t;
                pos -= -(pos >= PARAM_R) & PARAM_R;
            }
            c[i] = byte;
        }
    }
    return 0;
}

// Inverts a polynomial.
int gf2x_inv(uint8_t *inv_a, const uint8_t *a)
{
    uint8_t *f = (uint8_t *)trike_alloc(PADDED_R_SIZE_BYTES);
    uint8_t *g = (uint8_t *)trike_alloc(PADDED_R_SIZE_BYTES);

    int rc = -1;
    if (!f || !g) goto cleanup;
    trike_assign(a, f, R_ZMM_SIZE_BYTES, PADDED_R_SIZE_BYTES);
    trike_assign(a, inv_a, R_ZMM_SIZE_BYTES, PADDED_R_SIZE_BYTES);
    trike_setz(g, PADDED_R_SIZE_BYTES);

    for (size_t i = 1; i < INV_MAX_ITER; i++)
    {
        if (gf2x_k_sqr(g, f, k0[i], t0[i])) goto cleanup;
        if (gf2x_mul(f, f, g)) goto cleanup;
        if(k1[i])
        {
            if (gf2x_k_sqr(g, f, k1[i], t1[i])) goto cleanup;
            if (gf2x_mul(inv_a, inv_a, g)) goto cleanup;
        }
    }
    rc = gf2x_sqr(inv_a, inv_a);
cleanup:

    free(f);
    free(g);
    return rc;
}

#ifdef OPTIMIZED_IMPLEMENTATION
enum {
    SHIFT_MAX_STEP = R_SIZE_ZMMS <= 32 ? 16 :
                     R_SIZE_ZMMS <= 64 ? 32 :
                     R_SIZE_ZMMS <= 128 ? 64 : 128,
    SHIFT_WORK_BLOCKS = R_SIZE_ZMMS + SHIFT_MAX_STEP
};

// Shifts whole 512-bit blocks using fixed-address stages.
static inline void gf2x_shift_big(uint64_t *work, const uint64_t *in, uint32_t blocks)
{
    // Fuse the first stage with the input copy. Only N + step blocks remain
    // live after a stage, so subsequent stages need no larger scratch array.
    // Split at the public input boundary rather than testing it per block.
    // Every output is stored unconditionally for either value of the mask.
    const __m512i zero = _mm512_setzero_si512();
    __m512i mask = _mm512_set1_epi64(-(int64_t)((blocks & SHIFT_MAX_STEP) != 0));
    for (size_t i = 0; i < 2 * R_SIZE_ZMMS - SHIFT_MAX_STEP; i++)
    {
        __m512i a = _mm512_load_si512((const __m512i *)(in + 8 * i));
        __m512i b = _mm512_load_si512((const __m512i *)(in + 8 * (i + SHIFT_MAX_STEP)));
        __m512i v = _mm512_ternarylogic_epi64(mask, b, a, 0xCA);
        _mm512_store_si512((__m512i *)(work + 8 * i), v);
    }
    for (size_t i = 2 * R_SIZE_ZMMS - SHIFT_MAX_STEP; i < SHIFT_WORK_BLOCKS; i++)
    {
        __m512i a = _mm512_load_si512((const __m512i *)(in + 8 * i));
        __m512i v = _mm512_ternarylogic_epi64(mask, zero, a, 0xCA);
        _mm512_store_si512((__m512i *)(work + 8 * i), v);
    }

    // Leave the last whole-block stage to the fine shift, where its selected
    // registers can be consumed directly instead of written back to scratch.
    for (size_t step = SHIFT_MAX_STEP >> 1; step > 1; step >>= 1)
    {
        __m512i mask = _mm512_set1_epi64(-(int64_t)((blocks & step) != 0));
        for (size_t i = 0; i < R_SIZE_ZMMS + step; i++)
        {
            __m512i a = _mm512_load_si512((const __m512i *)(work + 8 * i));
            __m512i b = _mm512_load_si512((const __m512i *)(work + 8 * (i + step)));
            // Select b where mask is set, a otherwise.
            __m512i v = _mm512_ternarylogic_epi64(mask, b, a, 0xCA);
            _mm512_store_si512((__m512i *)(work + 8 * i), v);
        }
    }
}

// Shifts the remaining 0..1023 bits. Fuse the last whole-block selection with
// the lane/bit shifts, reusing each selected register in the next iteration.
static inline void gf2x_shift_small(uint64_t *out, const uint64_t *in, uint32_t bits)
{
    __m512i lanes = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    __m512i idx = _mm512_add_epi64(lanes, _mm512_set1_epi64((bits >> 6) & 7));
    __m512i next_idx = _mm512_add_epi64(idx, _mm512_set1_epi64(1));
    __m512i right = _mm512_set1_epi64(bits & 63);
    __m512i left = _mm512_set1_epi64(64 - (bits & 63));
    __m512i mask = _mm512_set1_epi64(-(int64_t)((bits & 512) != 0));
    __m512i previous = _mm512_load_si512((const __m512i *)(in + 8));
    __m512i first = _mm512_load_si512((const __m512i *)in);
    __m512i a = _mm512_ternarylogic_epi64(mask, previous, first, 0xCA);

    for (size_t i = 0; i < R_SIZE_ZMMS; i++)
    {
        __m512i next = _mm512_load_si512((const __m512i *)(in + 8 * (i + 2)));
        __m512i b = _mm512_ternarylogic_epi64(mask, next, previous, 0xCA);
        __m512i lo = _mm512_permutex2var_epi64(a, idx, b);
        __m512i hi = _mm512_permutex2var_epi64(a, next_idx, b);
        lo = _mm512_srlv_epi64(lo, right);
        hi = _mm512_sllv_epi64(hi, left);
        _mm512_store_si512((__m512i *)(out + 8 * i), _mm512_or_si512(lo, hi));
        a = b;
        previous = next;
    }
}

// Consume the fine shift directly. The operation and all loop bounds are
// public; the shift still selects exclusively between fixed addresses.
static inline void gf2x_shift_small_consume(uint8_t *out, const uint64_t *in,
                                          uint32_t bits, uint32_t logk,
                                          int add_counters)
{
    // A duplicated input extends beyond r. Mask the final emitted block
    // before touching counters or the syndrome, just as gf2x_shift does.
#define SHIFT_TAIL_WORD(lane) \
    ((R_SIZE_ZMM_64 - 8 + (lane) < R_SIZE_64 - 1) ? UINT64_MAX : \
     (R_SIZE_ZMM_64 - 8 + (lane) == R_SIZE_64 - 1) ? R_64_LAST_MASK : 0)
    _Alignas(64) static const uint64_t tail_words[8] = {
        SHIFT_TAIL_WORD(0), SHIFT_TAIL_WORD(1), SHIFT_TAIL_WORD(2), SHIFT_TAIL_WORD(3),
        SHIFT_TAIL_WORD(4), SHIFT_TAIL_WORD(5), SHIFT_TAIL_WORD(6), SHIFT_TAIL_WORD(7)
    };
#undef SHIFT_TAIL_WORD
    const __m512i tail = _mm512_load_si512((const __m512i *)tail_words);
    const __m512i lanes = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0);
    const __m512i idx = _mm512_add_epi64(lanes, _mm512_set1_epi64((bits >> 6) & 7));
    const __m512i next_idx = _mm512_add_epi64(idx, _mm512_set1_epi64(1));
    const __m512i right = _mm512_set1_epi64(bits & 63);
    const __m512i left = _mm512_set1_epi64(64 - (bits & 63));
    const __m512i mask = _mm512_set1_epi64(-(int64_t)((bits & 512) != 0));
    __m512i previous = _mm512_load_si512((const __m512i *)(in + 8));
    const __m512i first = _mm512_load_si512((const __m512i *)in);
    __m512i a = _mm512_ternarylogic_epi64(mask, previous, first, 0xCA);

    for (size_t i = 0; i < R_SIZE_ZMMS; i++)
    {
        const __m512i next = _mm512_load_si512((const __m512i *)(in + 8 * (i + 2)));
        const __m512i b = _mm512_ternarylogic_epi64(mask, next, previous, 0xCA);
        __m512i lo = _mm512_permutex2var_epi64(a, idx, b);
        __m512i hi = _mm512_permutex2var_epi64(a, next_idx, b);
        lo = _mm512_srlv_epi64(lo, right);
        hi = _mm512_sllv_epi64(hi, left);
        __m512i value = _mm512_or_si512(lo, hi);
        if (i + 1 == R_SIZE_ZMMS) value = _mm512_and_si512(value, tail);
        if (add_counters)
        {
            for (size_t slice = 0; slice <= logk; slice++)
            {
                __m512i *dst = (__m512i *)(out + slice * R_ZMM_SIZE_BYTES + 64 * i);
                const __m512i count = _mm512_load_si512(dst);
                _mm512_store_si512(dst, _mm512_xor_si512(value, count));
                value = _mm512_and_si512(value, count);
            }
        }
        else
        {
            __m512i *dst = (__m512i *)(out + 64 * i);
            const __m512i value_before = _mm512_load_si512(dst);
            _mm512_store_si512(dst, _mm512_xor_si512(value_before, value));
        }
        a = b;
        previous = next;
    }
}

// Add a rotated syndrome to bit-sliced counters. logk is the public maximum
// counter slice to visit; counter bits above it and padding stay unchanged.
int gf2x_shift_add(uint8_t *upc, const uint8_t *in, uint32_t shift, uint32_t logk)
{
    _Alignas(64) uint64_t work[SHIFT_WORK_BLOCKS * 8];
    gf2x_shift_big(work, (const uint64_t *)in, shift >> 9);
    gf2x_shift_small_consume(upc, work, shift & 1023, logk, 1);
    return 0;
}

// XOR a rotated polynomial into out, preserving out's padding exactly like
// gf2x_add(out, out, gf2x_shift(...)).
int gf2x_shift_xor(uint8_t *out, const uint8_t *in, uint32_t shift)
{
    _Alignas(64) uint64_t work[SHIFT_WORK_BLOCKS * 8];
    gf2x_shift_big(work, (const uint64_t *)in, shift >> 9);
    gf2x_shift_small_consume(out, work, shift & 1023, 0, 0);
    return 0;
}
#endif

// Applies a cyclic shift.
int gf2x_shift(uint8_t *out, const uint8_t *in, uint32_t shift)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    _Alignas(64) uint64_t work[SHIFT_WORK_BLOCKS * 8];
    gf2x_shift_big(work, (const uint64_t *)in, shift >> 9);
    gf2x_shift_small((uint64_t *)out, work, shift & 1023);
    out[R_SIZE_BYTES - 1] &= R_8_LAST_MASK;
    memset(out + R_SIZE_BYTES, 0, R_ZMM_SIZE_BYTES - R_SIZE_BYTES);

#else

    uint8_t *temp = (uint8_t *)calloc(R_SIZE_BYTES * 2, sizeof(uint8_t));
    if (temp == NULL) return -1;

    memset(temp, 0, R_SIZE_BYTES * 2);

    uint32_t byte_shift = shift / 8, bit_shift = shift % 8, low_bits = 8 - bit_shift;
    temp[byte_shift] = in[0] << bit_shift;
    for (uint32_t i = 0; i < R_SIZE_BYTES - 1; i++)
    {
        temp[byte_shift + i + 1] = (in[i] >> low_bits) | (in[i + 1] << bit_shift);
    }
    temp[byte_shift + R_SIZE_BYTES] = (in[R_SIZE_BYTES - 1] >> low_bits);

    memcpy(out, temp, R_SIZE_BYTES);

    uint32_t high_bits = (PARAM_R - 1) % 8 + 1; low_bits = 8 - high_bits;

    for (uint32_t i = 0; i < R_SIZE_BYTES; i++)
    {
        out[i] ^= (temp[i + R_SIZE_BYTES - 1] >> high_bits) | (temp[i + R_SIZE_BYTES] << low_bits);
    }

    out[R_SIZE_BYTES - 1] &= (1U << high_bits) - 1;

    free(temp);

#endif
    return 0;
}
