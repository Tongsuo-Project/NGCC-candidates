#ifndef GF2X_H
#define GF2X_H

#ifdef OPTIMIZED_IMPLEMENTATION
#include <immintrin.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "trike_types.h"

void gf2x_add(uint8_t *c, const uint8_t *a, const uint8_t *b);
int gf2x_mul(uint8_t *c, const uint8_t *a, const uint8_t *b);
int gf2x_inv(uint8_t *inv_a, const uint8_t *a);
int gf2x_shift(uint8_t *out, const uint8_t *in, uint32_t shift);
#ifdef OPTIMIZED_IMPLEMENTATION
int gf2x_shift_add(uint8_t *upc, const uint8_t *in, uint32_t shift, uint32_t logk);
int gf2x_shift_xor(uint8_t *out, const uint8_t *in, uint32_t shift);
#endif

// Allocates working storage, aligned for the optimized implementation.
static inline void *trike_alloc(size_t size)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    return aligned_alloc(64, size);
#else
    return malloc(size);
#endif
}

// Clears the array.
static inline void trike_setz(uint8_t *dst, size_t dst_size)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    __m512i zero = _mm512_setzero_si512();
    for (size_t i = 0; i < dst_size; i += 64)
    {
        _mm512_store_si512((__m512i *)(dst + i), zero);
    }
#else
    memset(dst, 0, dst_size);
#endif
}

// Copies the array.
static inline void fast_cpy(uint64_t *dst, const uint64_t *src, size_t size64)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    for (size_t i = 0; i < size64; i += 8)
    {
        __m512i Data = _mm512_load_si512((__m512i*)(src + i));
        _mm512_store_si512((__m512i*)(dst + i), Data);
    }
#else
    memcpy(dst, src, size64 * 8);
#endif
}

// Copies the array and clears padding.
static inline void trike_assign(const uint8_t *src, uint8_t *dst, size_t src_size, size_t dst_size)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    for (size_t i = 0; i < src_size; i += 64)
    {
        __m512i v = _mm512_load_si512((__m512i *)(src + i));
        _mm512_store_si512((__m512i *)(dst + i), v);
    }
    __m512i zero = _mm512_setzero_si512();
    for (size_t i = src_size; i < dst_size; i += 64)
    {
        _mm512_store_si512((__m512i *)(dst + i), zero);
    }
#else
    memcpy(dst, src, src_size);
    memset(dst + src_size, 0, dst_size - src_size);
#endif
}
#endif
