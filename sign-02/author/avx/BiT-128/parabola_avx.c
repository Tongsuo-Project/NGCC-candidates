/*
 * Fixed-point projection derived from HAETAE v2.0,
 * Copyright (c) 2023 Team HAETAE (MIT license).
 * Modifications Copyright (c) 2026 Hang Zhang.
 * Adapted to BiT's Q76 scale and fused Q13 squared-norm accumulation.
 */
#include "parabola_internal.h"

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

static const union {
    __m256i vec[3];
    uint64_t u64[12];
} round_constants = {.u64 = {2, 2, 2, 2, 1ULL << 27, 1ULL << 27, 1ULL << 27, 1ULL << 27, 1ULL << 30,
                             1ULL << 30, 1ULL << 30, 1ULL << 30}};

static inline __m256i mul_round13_4(const uint64_t samples[4], const __m256i scale[3])
{
    __m256i sample_lo = _mm256_loadu_si256((const __m256i *)samples);
    __m256i sample_hi = _mm256_srli_epi64(sample_lo, 32);
    __m256i t0 = _mm256_mul_epu32(sample_lo, scale[0]);
    __m256i t1 = _mm256_mul_epu32(sample_hi, scale[0]);
    __m256i t2 = _mm256_mul_epu32(sample_lo, scale[1]);
    __m256i t3 = _mm256_mul_epu32(sample_hi, scale[1]);
    __m256i t4 = _mm256_mul_epu32(sample_lo, scale[2]);
    __m256i t5 = _mm256_mul_epu32(sample_hi, scale[2]);

    t0 = _mm256_srli_epi64(t0, 46);
    t0 = _mm256_add_epi64(t0, t2);
    t0 = _mm256_add_epi64(t0, round_constants.vec[0]);
    t0 = _mm256_srli_epi64(t0, 2);
    t0 = _mm256_add_epi64(t0, t1);
    t0 = _mm256_add_epi64(t0, round_constants.vec[1]);
    t0 = _mm256_srli_epi64(t0, 28);
    t0 = _mm256_add_epi64(t0, t4);
    t0 = _mm256_srli_epi64(t0, 2);
    t0 = _mm256_add_epi64(t0, t3);
    t0 = _mm256_srli_epi64(t0, 30);
    t0 = _mm256_add_epi64(t0, t5);
    t0 = _mm256_add_epi64(t0, round_constants.vec[2]);
    return _mm256_srli_epi64(t0, 31);
}

static inline __m256i pack_magnitudes(__m256i lo, __m256i hi)
{
    __m256i hi_dup = _mm256_permute4x64_epi64(hi, 0x44);
    __m256i lo_dup = _mm256_permute4x64_epi64(lo, 0xEE);
    __m256i hi_even = _mm256_shuffle_epi32(hi, 0x85);
    __m256i hi_odd = _mm256_shuffle_epi32(hi_dup, 0x58);
    __m256i lo_odd = _mm256_shuffle_epi32(lo_dup, 0x85);
    __m256i lo_even = _mm256_shuffle_epi32(lo, 0x58);
    __m256i low = _mm256_blend_epi32(lo_even, lo_odd, 0xCC);
    __m256i high = _mm256_blend_epi32(hi_odd, hi_even, 0xCC);

    return _mm256_blend_epi32(low, high, 0xF0);
}

static inline __m256i apply_signs(__m256i magnitudes, uint8_t signs)
{
    const __m256i shifts = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    __m256i bits = _mm256_srlv_epi32(_mm256_set1_epi32(signs), shifts);
    __m256i mask =
        _mm256_sub_epi32(_mm256_setzero_si256(), _mm256_and_si256(bits, _mm256_set1_epi32(1)));
    __m256i negated = _mm256_sub_epi32(_mm256_setzero_si256(), magnitudes);

    return _mm256_blendv_epi8(magnitudes, negated, mask);
}

static inline __m256i square8(__m256i values)
{
    __m256i odd = _mm256_srli_epi64(values, 32);
    __m256i even_sq = _mm256_mul_epi32(values, values);
    __m256i odd_sq = _mm256_mul_epi32(odd, odd);

    return _mm256_add_epi64(even_sq, odd_sq);
}

uint64_t bit_parabola_scale_round_avx(int32_t out[BIT_N], const uint64_t samples[BIT_N],
                                      const uint8_t signs[BIT_N / 8], const fp96_76 *scale)
{
    uint32_t word0 = (uint32_t)(scale->limb48[0] & ((1ULL << 30) - 1));
    uint32_t word1 =
        (uint32_t)(((scale->limb48[0] >> 30) | (scale->limb48[1] << 18)) & ((1ULL << 30) - 1));
    uint32_t word2 = (uint32_t)(scale->limb48[1] >> 12);
    __m256i scale_vec[3] = {_mm256_set1_epi64x(word0), _mm256_set1_epi64x(word1),
                            _mm256_set1_epi64x(word2)};
    __m256i sum = _mm256_setzero_si256();
    union {
        __m256i vec;
        uint64_t lane[4];
    } reduced;

    for (size_t i = 0; i < BIT_N / 8; i++) {
        __m256i lo = mul_round13_4(samples + 8 * i, scale_vec);
        __m256i hi = mul_round13_4(samples + 8 * i + 4, scale_vec);
        __m256i values = apply_signs(pack_magnitudes(lo, hi), signs[i]);

        _mm256_storeu_si256((__m256i *)(out + 8 * i), values);
        sum = _mm256_add_epi64(sum, square8(values));
    }
    reduced.vec = sum;
    return reduced.lane[0] + reduced.lane[1] + reduced.lane[2] + reduced.lane[3];
}

void bit_parabola_accumulate_response_stats_avx(parabola_fixed_stats *stats,
                                                const int32_t mask[BIT_N],
                                                const int16_t shift[BIT_N], int32_t shift_scale)
{
    const __m256i scale = _mm256_set1_epi32(shift_scale);
    __m256i z_sum = _mm256_setzero_si256();
    __m256i v_sum = _mm256_setzero_si256();
    __m256i dot_sum = _mm256_setzero_si256();
    union {
        __m256i vec;
        uint64_t u64[4];
        int64_t i64[4];
    } reduced;

    for (size_t i = 0; i < BIT_N; i += 8) {
        __m128i shift16 = _mm_loadu_si128((const __m128i *)(shift + i));
        __m256i v = _mm256_cvtepi16_epi32(shift16);
        __m256i z;
        __m256i z_odd;
        __m256i v_odd;
        __m256i products;

        v = _mm256_mullo_epi32(v, scale);
        z = _mm256_add_epi32(_mm256_loadu_si256((const __m256i *)(mask + i)), v);
        z_sum = _mm256_add_epi64(z_sum, square8(z));
        v_sum = _mm256_add_epi64(v_sum, square8(v));

        z_odd = _mm256_srli_epi64(z, 32);
        v_odd = _mm256_srli_epi64(v, 32);
        products = _mm256_add_epi64(_mm256_mul_epi32(z, v), _mm256_mul_epi32(z_odd, v_odd));
        dot_sum = _mm256_add_epi64(dot_sum, products);
    }

    reduced.vec = z_sum;
    stats->z_norm2 += reduced.u64[0] + reduced.u64[1] + reduced.u64[2] + reduced.u64[3];
    reduced.vec = v_sum;
    stats->v_norm2 += reduced.u64[0] + reduced.u64[1] + reduced.u64[2] + reduced.u64[3];
    reduced.vec = dot_sum;
    stats->dot += reduced.i64[0] + reduced.i64[1] + reduced.i64[2] + reduced.i64[3];
}
