/* Copyright (c) 2026 Hang Zhang. */

/*
 * Lane migration helpers for the AVX2 four-way Keccak representation.
 * SPDX-License-Identifier: MIT
 */
#include "keccakx4_lanes.h"

/* Mask selecting one 64-bit lane. */
static __m256i bit_keccakx4_lane_mask(unsigned int lane)
{
    const __m256i lane_numbers = _mm256_setr_epi64x(0, 1, 2, 3);
    return _mm256_cmpeq_epi64(lane_numbers, _mm256_set1_epi64x(lane));
}
/* Permutation indices broadcasting one 64-bit lane. */
static __m256i bit_keccakx4_lane_indices(unsigned int lane)
{
    int low = (int)(2U * lane);
    int high = low + 1;

    return _mm256_setr_epi32(low, high, low, high,
                             low, high, low, high);
}

void bit_keccakx4_copy_lane(keccakx4_state *dst, unsigned int dst_lane,
                            const keccakx4_state *src,
                            unsigned int src_lane)
{
    const __m256i mask = bit_keccakx4_lane_mask(dst_lane);
    const __m256i indices = bit_keccakx4_lane_indices(src_lane);

    for (size_t i = 0; i < BIT_KECCAK_STATE_WORDS; i++) {
        __m256i source = _mm256_permutevar8x32_epi32(src->s[i], indices);

        dst->s[i] = _mm256_blendv_epi8(dst->s[i], source, mask);
    }
}

void bit_keccakx4_copy_lanes2(keccakx4_state *dst,
                              unsigned int dst_lane0,
                              unsigned int dst_lane1,
                              const keccakx4_state *src,
                              unsigned int src_lane0,
                              unsigned int src_lane1)
{
    if (dst_lane0 == 2 && dst_lane1 == 3
            && src_lane0 == 0 && src_lane1 == 1) {
        for (size_t i = 0; i < BIT_KECCAK_STATE_WORDS; i++) {
            __m128i source = _mm256_castsi256_si128(src->s[i]);

            dst->s[i] = _mm256_inserti128_si256(dst->s[i], source, 1);
        }
        return;
    }
    if (dst_lane0 == 2 && dst_lane1 == 3
            && src_lane0 == 2 && src_lane1 == 3) {
        for (size_t i = 0; i < BIT_KECCAK_STATE_WORDS; i++)
            dst->s[i] = _mm256_blend_epi32(dst->s[i], src->s[i], 0xF0);
        return;
    }

    const __m256i mask0 = bit_keccakx4_lane_mask(dst_lane0);
    const __m256i mask1 = bit_keccakx4_lane_mask(dst_lane1);
    const __m256i indices0 = bit_keccakx4_lane_indices(src_lane0);
    const __m256i indices1 = bit_keccakx4_lane_indices(src_lane1);

    for (size_t i = 0; i < BIT_KECCAK_STATE_WORDS; i++) {
        __m256i source0 = _mm256_permutevar8x32_epi32(src->s[i], indices0);
        __m256i source1 = _mm256_permutevar8x32_epi32(src->s[i], indices1);
        __m256i result = _mm256_blendv_epi8(dst->s[i], source0, mask0);

        dst->s[i] = _mm256_blendv_epi8(result, source1, mask1);
    }
}

void bit_keccakx4_export_lane(uint64_t dst[BIT_KECCAK_STATE_WORDS],
                              const keccakx4_state *src,
                              unsigned int src_lane)
{
    const __m256i indices = bit_keccakx4_lane_indices(src_lane);

    for (size_t i = 0; i < BIT_KECCAK_STATE_WORDS; i++) {
        __m256i selected =
            _mm256_permutevar8x32_epi32(src->s[i], indices);

        dst[i] = (uint64_t)_mm_cvtsi128_si64(
            _mm256_castsi256_si128(selected));
    }
}

void bit_keccakx4_export_lanes2(uint64_t dst0[BIT_KECCAK_STATE_WORDS],
                                uint64_t dst1[BIT_KECCAK_STATE_WORDS],
                                const keccakx4_state *src,
                                unsigned int src_lane0,
                                unsigned int src_lane1)
{
    if (src_lane0 == 2 && src_lane1 == 3) {
        for (size_t i = 0; i < BIT_KECCAK_STATE_WORDS; i++) {
            __m128i high = _mm256_extracti128_si256(src->s[i], 1);

            dst0[i] = (uint64_t)_mm_cvtsi128_si64(high);
            dst1[i] = (uint64_t)_mm_extract_epi64(high, 1);
        }
        return;
    }

    bit_keccakx4_export_lane(dst0, src, src_lane0);
    bit_keccakx4_export_lane(dst1, src, src_lane1);
}

void bit_keccakx4_export_all(uint64_t dst0[BIT_KECCAK_STATE_WORDS],
                             uint64_t dst1[BIT_KECCAK_STATE_WORDS],
                             uint64_t dst2[BIT_KECCAK_STATE_WORDS],
                             uint64_t dst3[BIT_KECCAK_STATE_WORDS],
                             const keccakx4_state *src)
{
    for (size_t i = 0; i < BIT_KECCAK_STATE_WORDS; i++) {
        __m128i low = _mm256_castsi256_si128(src->s[i]);
        __m128i high = _mm256_extracti128_si256(src->s[i], 1);

        dst0[i] = (uint64_t)_mm_cvtsi128_si64(low);
        dst1[i] = (uint64_t)_mm_extract_epi64(low, 1);
        dst2[i] = (uint64_t)_mm_cvtsi128_si64(high);
        dst3[i] = (uint64_t)_mm_extract_epi64(high, 1);
    }
}

void bit_keccakx4_import_lane(keccakx4_state *dst, unsigned int dst_lane,
                              const uint64_t src[BIT_KECCAK_STATE_WORDS])
{
    const __m256i mask = bit_keccakx4_lane_mask(dst_lane);

    for (size_t i = 0; i < BIT_KECCAK_STATE_WORDS; i++) {
        __m256i source = _mm256_set1_epi64x((long long)src[i]);

        dst->s[i] = _mm256_blendv_epi8(dst->s[i], source, mask);
    }
}
