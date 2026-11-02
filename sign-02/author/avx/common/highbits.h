/* Copyright (c) 2026 Hang Zhang. */
#ifndef BIT_AVX_HIGHBITS_H
#define BIT_AVX_HIGHBITS_H
#include <immintrin.h>
#include "params.h"

/* Canonical residues in [0,q). */
static inline __m256i bit_highbits_i32x8(__m256i x)
{
    const __m256i multiplier = _mm256_set1_epi32(BIT_GAMMA2_BARRETT_MULT);
    const __m256i modulus = _mm256_set1_epi32(BIT_HINT_MODULUS);
    __m256i n = _mm256_add_epi32(x, _mm256_set1_epi32(BIT_GAMMA2 / 2));
    __m256i even = _mm256_srli_epi64(_mm256_mul_epu32(n, multiplier), 32);
    __m256i odd = _mm256_srli_epi64(
        _mm256_mul_epu32(_mm256_srli_epi64(n, 32), multiplier), 32);
    __m256i high = _mm256_blend_epi32(even, _mm256_slli_epi64(odd, 32), 0xAA);

    return _mm256_sub_epi32(
        high, _mm256_and_si256(
            _mm256_cmpgt_epi32(high, _mm256_set1_epi32(BIT_HINT_MODULUS - 1)), modulus));
}

static inline __m256i bit_highbits_i16x16(__m256i x)
{
    __m256i lo = bit_highbits_i32x8(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(x)));
    __m256i hi = bit_highbits_i32x8(_mm256_cvtepu16_epi32(_mm256_extracti128_si256(x, 1)));
    return _mm256_permute4x64_epi64(_mm256_packs_epi32(lo, hi), 0xD8);
}
#endif
