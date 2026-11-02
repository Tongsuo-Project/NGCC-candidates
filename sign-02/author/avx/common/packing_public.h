/* Copyright (c) 2026 Hang Zhang. */
#ifndef BIT_AVX_PACKING_PUBLIC_H
#define BIT_AVX_PACKING_PUBLIC_H

#include <immintrin.h>

/* Two overlapping eight-byte loads stay within each encoded group. */
static int unpack_poly_public(poly *r, const unsigned char *in)
{
#if BIT_PUBLIC_COEFF_BITS == 13
    const __m256i shuffle = _mm256_setr_epi8(
        0, 1, 2, -1, 1, 2, 3, -1, 3, 4, 5, -1, 4, 5, 6, -1,
        1, 2, 3, -1, 3, 4, 5, -1, 4, 5, 6, -1, 6, 7, -1, -1);
    const __m256i shifts = _mm256_setr_epi32(0, 5, 2, 7, 4, 1, 6, 3);
#elif BIT_PUBLIC_COEFF_BITS == 15
    const __m256i shuffle = _mm256_setr_epi8(
        0, 1, 2, -1, 1, 2, 3, -1, 3, 4, 5, -1, 5, 6, 7, -1,
        0, 1, 2, -1, 2, 3, 4, -1, 4, 5, 6, -1, 6, 7, -1, -1);
    const __m256i shifts = _mm256_setr_epi32(0, 7, 6, 5, 4, 3, 2, 1);
#else
#error "Unsupported public coefficient width"
#endif
    const __m256i mask = _mm256_set1_epi32((1U << BIT_PUBLIC_COEFF_BITS) - 1U);
    const __m256i max = _mm256_set1_epi32(BIT_Q - 1);
    __m256i invalid = _mm256_setzero_si256();

    for (int i = 0; i < BIT_N; i += 8, in += BIT_PUBLIC_COEFF_BITS) {
        __m128i lo = _mm_loadl_epi64((const __m128i *)in);
        __m128i hi = _mm_loadl_epi64((const __m128i *)(in + BIT_PUBLIC_COEFF_BITS - 8));
        __m256i bytes = _mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1);
        __m256i values = _mm256_and_si256(
            _mm256_srlv_epi32(_mm256_shuffle_epi8(bytes, shuffle), shifts), mask);
        invalid = _mm256_or_si256(invalid, _mm256_cmpgt_epi32(values, max));
#if BIT_PUBLIC_COEFF_BITS == 13
        __m128i packed = _mm_packs_epi32(_mm256_castsi256_si128(values),
                                        _mm256_extracti128_si256(values, 1));
        _mm_storeu_si128((__m128i *)&r->coeffs[i], packed);
#else
        _mm256_storeu_si256((__m256i *)&r->coeffs[i], values);
#endif
    }
    uint32_t bad = (uint32_t)_mm256_movemask_epi8(invalid);
    BIT_CT_DECLASSIFY(&bad, sizeof bad);
    return bad != 0;
}
#endif
