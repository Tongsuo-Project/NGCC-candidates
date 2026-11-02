/*
 * Derived from HAETAE v2.0, Copyright (c) 2023 Team HAETAE.
 * Modifications Copyright (c) 2026 Hang Zhang.
 * SPDX-License-Identifier: MIT
 */
#include "../../common/ct_audit.h"
#include "align.h"
#include "fips202.h"
#include "fips202x4.h"
#include "parabola_internal.h"
#include "params.h"
#include "symmetric.h"
#include "../common/keccakx4_lanes.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <immintrin.h>

#define N BIT_N
#define CRHBYTES BIT_SEEDBYTES
#define STREAM256_BLOCKBYTES SHAKE256_RATE

#define GAUSS_RAND (72 + 16 + 48)
#define GAUSS_RAND_BYTES ((GAUSS_RAND + 7) / 8)

#ifndef BIT_GAUSS_PACKED_THRESHOLD
/* Select the threshold-table layout at compile time. */
#define BIT_GAUSS_PACKED_THRESHOLD 0
#endif

#define CDT4(a, b, c, d)                                                                           \
    ((uint64_t)((a) ^ 0x8000U) | ((uint64_t)((b) ^ 0x8000U) << 16) |                               \
     ((uint64_t)((c) ^ 0x8000U) << 32) | ((uint64_t)((d) ^ 0x8000U) << 48))
static const uint64_t cdt16x4[16] = {
    CDT4(3266, 6520, 9748, 12938),    CDT4(16079, 19159, 22168, 25096),
    CDT4(27934, 30674, 33309, 35833), CDT4(38241, 40531, 42698, 44742),
    CDT4(46663, 48460, 50135, 51690), CDT4(53128, 54454, 55670, 56781),
    CDT4(57794, 58712, 59541, 60287), CDT4(60956, 61554, 62085, 62556),
    CDT4(62972, 63337, 63657, 63936), CDT4(64178, 64388, 64569, 64724),
    CDT4(64857, 64970, 65066, 65148), CDT4(65216, 65273, 65321, 65361),
    CDT4(65394, 65422, 65444, 65463), CDT4(65478, 65490, 65500, 65508),
    CDT4(65514, 65519, 65523, 65527), CDT4(65529, 65531, 65533, 65534)};
#undef CDT4

static const union {
    __m256i vec[3];
    uint64_t arr[12];
} mask72_avx = {.arr = {(1ULL << 48) - 1, (1ULL << 48) - 1, (1ULL << 48) - 1, (1ULL << 48) - 1,
                        (1ULL << 24) - 1, (1ULL << 24) - 1, (1ULL << 24) - 1, (1ULL << 24) - 1,
                        (1ULL << 16) - 1, (1ULL << 16) - 1, (1ULL << 16) - 1, (1ULL << 16) - 1}};

static const union {
    __m256i vec[3];
    uint64_t arr[12];
} consts_avx = {
    .arr = {1 << 14, 1 << 14, 1 << 14, 1 << 14, 1 << 11, 1 << 11, 1 << 11, 1 << 11, 1, 1, 1, 1}};

static inline void smulh48_avx(__m256i *a, const __m256i *b, __m256i *ah, __m256i *bh, __m256i *al,
                               __m256i *bl, __m256i *tmp, __m256i *tmp2, const __m256i *mask24,
                               const __m256i *one23)
{
    /* a is signed Q48 while b is an unsigned 48-bit value.  Splitting both
     * operands at 24 bits keeps every partial product inside signed 32x32
     * multiplication.  AVX2 has no arithmetic 64-bit shift, so restore only
     * the high dword of each negative qword after the logical shift. */
    *tmp2 = _mm256_srai_epi32(*a, 31);
    *ah = _mm256_srli_epi64(*a, 24);
    *ah = _mm256_blend_epi32(*ah, *tmp2, 0xAA);
    *al = _mm256_and_si256(*a, *mask24);
    *bl = _mm256_and_si256(*b, *mask24);
    *bh = _mm256_srli_epi64(*b, 24);

    *a = _mm256_mul_epi32(*al, *bl);
    *tmp = _mm256_mul_epi32(*al, *bh);
    *tmp2 = _mm256_mul_epi32(*ah, *bl);
    *tmp = _mm256_add_epi64(*tmp, *tmp2);
    *a = _mm256_srli_epi64(*a, 24);
    *tmp = _mm256_add_epi64(*tmp, *a);
    *tmp = _mm256_add_epi64(*tmp, *one23);

    *tmp2 = _mm256_srai_epi32(*tmp, 31);
    *tmp = _mm256_srli_epi64(*tmp, 24);
    *tmp = _mm256_blend_epi32(*tmp, *tmp2, 0xAA);
    *tmp2 = _mm256_mul_epi32(*ah, *bh);
    *a = _mm256_add_epi64(*tmp, *tmp2);
}

static const union {
    __m256i vec[9];
    int64_t arr[9 * 4];
} exp_avx = {.arr = {-0x0000B6C6340925AELL,
                     -0x0000B6C6340925AELL,
                     -0x0000B6C6340925AELL,
                     -0x0000B6C6340925AELL,
                     0x0000B4BD4DF85227LL,
                     0x0000B4BD4DF85227LL,
                     0x0000B4BD4DF85227LL,
                     0x0000B4BD4DF85227LL,
                     -0x0000887F727491E2LL,
                     -0x0000887F727491E2LL,
                     -0x0000887F727491E2LL,
                     -0x0000887F727491E2LL,
                     0x0000AAAA643C7E8DLL,
                     0x0000AAAA643C7E8DLL,
                     0x0000AAAA643C7E8DLL,
                     0x0000AAAA643C7E8DLL,
                     -0x0000AAAAA98179E6LL,
                     -0x0000AAAAA98179E6LL,
                     -0x0000AAAAA98179E6LL,
                     -0x0000AAAAA98179E6LL,
                     0x0000FFFFFFFB2E7ALL,
                     0x0000FFFFFFFB2E7ALL,
                     0x0000FFFFFFFB2E7ALL,
                     0x0000FFFFFFFB2E7ALL,
                     -0x0000FFFFFFFFF85FLL,
                     -0x0000FFFFFFFFF85FLL,
                     -0x0000FFFFFFFFF85FLL,
                     -0x0000FFFFFFFFF85FLL,
                     0x0000FFFFFFFFFFFCLL,
                     0x0000FFFFFFFFFFFCLL,
                     0x0000FFFFFFFFFFFCLL,
                     0x0000FFFFFFFFFFFCLL,
                     1 << 23,
                     1 << 23,
                     1 << 23,
                     1 << 23}};

/* Seven Horner products share the split input in ymm1/ymm2.
 * Twelve YMM registers hold the input, coefficients and temporaries. */
#define APPROX_EXP_SMULH48_FIRST_ASM                                                               \
    "vpsrad $31, %0, %%ymm7\n\t"                                                                   \
    "vpsrlq $24, %0, %%ymm3\n\t"                                                                   \
    "vpblendd $0xaa, %%ymm7, %%ymm3, %%ymm3\n\t"                                                   \
    "vpand %[mask24], %0, %%ymm4\n\t"                                                              \
    "vpmuldq %%ymm1, %%ymm4, %%ymm5\n\t"                                                           \
    "vpmuldq %%ymm2, %%ymm4, %%ymm6\n\t"                                                           \
    "vpmuldq %%ymm1, %%ymm3, %%ymm7\n\t"                                                           \
    "vpmuldq %%ymm2, %%ymm3, %0\n\t"                                                               \
    "vpaddq %%ymm7, %%ymm6, %%ymm6\n\t"                                                            \
    "vpsrlq $24, %%ymm5, %%ymm5\n\t"                                                               \
    "vpaddq %%ymm5, %%ymm6, %%ymm6\n\t"                                                            \
    "vpaddq %[one23], %%ymm6, %%ymm6\n\t"                                                          \
    "vpsrad $31, %%ymm6, %%ymm7\n\t"                                                               \
    "vpsrlq $24, %%ymm6, %%ymm6\n\t"                                                               \
    "vpblendd $0xaa, %%ymm7, %%ymm6, %%ymm6\n\t"                                                   \
    "vpaddq %0, %%ymm6, %0\n\t"

/* After the first multiply-add, Horner accumulators are nonnegative
 * for x in [0,2^48), so their high limbs use logical shifts. */
#define APPROX_EXP_SMULH48_FIXED_X_ASM                                                             \
    "vpsrlq $24, %0, %%ymm3\n\t"                                                                   \
    "vpand %[mask24], %0, %%ymm4\n\t"                                                              \
    "vpmuldq %%ymm1, %%ymm4, %%ymm5\n\t"                                                           \
    "vpmuldq %%ymm2, %%ymm4, %%ymm6\n\t"                                                           \
    "vpmuldq %%ymm1, %%ymm3, %%ymm7\n\t"                                                           \
    "vpmuldq %%ymm2, %%ymm3, %0\n\t"                                                               \
    "vpaddq %%ymm7, %%ymm6, %%ymm6\n\t"                                                            \
    "vpsrlq $24, %%ymm5, %%ymm5\n\t"                                                               \
    "vpaddq %%ymm5, %%ymm6, %%ymm6\n\t"                                                            \
    "vpaddq %[one23], %%ymm6, %%ymm6\n\t"                                                          \
    "vpsrad $31, %%ymm6, %%ymm7\n\t"                                                               \
    "vpsrlq $24, %%ymm6, %%ymm6\n\t"                                                               \
    "vpblendd $0xaa, %%ymm7, %%ymm6, %%ymm6\n\t"                                                   \
    "vpaddq %0, %%ymm6, %0\n\t"

#undef APPROX_EXP_SMULH48_FIXED_X_ASM
#undef APPROX_EXP_SMULH48_FIRST_ASM

/* Evaluate the fixed-point exponential approximation in four lanes. */
static __attribute__((noinline)) void approx_exp_4x(__m256i *rej, size_t count)
{
    __m256i reg[16];
    size_t i;
    reg[1] = _mm256_load_si256(&consts_avx.vec[2]);
    reg[2] = _mm256_load_si256(&mask72_avx.vec[1]);
    reg[3] = _mm256_load_si256(&exp_avx.vec[8]);

    reg[11] = _mm256_load_si256(&exp_avx.vec[1]);

    for (i = 0; i < count; i++) {
        reg[0] = _mm256_load_si256(&exp_avx.vec[0]);
        reg[4] = _mm256_load_si256(&rej[i]);

        reg[12] = _mm256_load_si256(&exp_avx.vec[2]);
        reg[13] = _mm256_load_si256(&exp_avx.vec[3]);
        reg[14] = _mm256_load_si256(&exp_avx.vec[4]);

        reg[1] = _mm256_slli_epi64(reg[1], 2);

        smulh48_avx(&reg[0], &reg[4], &reg[5], &reg[6], &reg[7], &reg[8], &reg[9], &reg[10],
                    &reg[2], &reg[3]);
        reg[0] = _mm256_add_epi64(reg[0], reg[1]);
        reg[0] = _mm256_srli_epi64(reg[0], 3);
        reg[0] = _mm256_add_epi64(reg[0], reg[11]);

        smulh48_avx(&reg[0], &reg[4], &reg[5], &reg[6], &reg[7], &reg[8], &reg[9], &reg[10],
                    &reg[2], &reg[3]);
        reg[0] = _mm256_add_epi64(reg[0], reg[1]);
        reg[0] = _mm256_srli_epi64(reg[0], 3);
        reg[0] = _mm256_add_epi64(reg[0], reg[12]);

        reg[1] = _mm256_srli_epi64(reg[1], 1);

        smulh48_avx(&reg[0], &reg[4], &reg[5], &reg[6], &reg[7], &reg[8], &reg[9], &reg[10],
                    &reg[2], &reg[3]);
        reg[0] = _mm256_add_epi64(reg[0], reg[1]);
        reg[0] = _mm256_srli_epi64(reg[0], 2);
        reg[0] = _mm256_add_epi64(reg[0], reg[13]);

        smulh48_avx(&reg[0], &reg[4], &reg[5], &reg[6], &reg[7], &reg[8], &reg[9], &reg[10],
                    &reg[2], &reg[3]);
        reg[0] = _mm256_add_epi64(reg[0], reg[1]);
        reg[0] = _mm256_srli_epi64(reg[0], 2);
        reg[0] = _mm256_add_epi64(reg[0], reg[14]);

        reg[12] = _mm256_load_si256(&exp_avx.vec[5]);
        reg[13] = _mm256_load_si256(&exp_avx.vec[6]);
        reg[14] = _mm256_load_si256(&exp_avx.vec[7]);

        reg[1] = _mm256_srli_epi64(reg[1], 1);

        smulh48_avx(&reg[0], &reg[4], &reg[5], &reg[6], &reg[7], &reg[8], &reg[9], &reg[10],
                    &reg[2], &reg[3]);
        reg[0] = _mm256_add_epi64(reg[0], reg[1]);
        reg[0] = _mm256_srli_epi64(reg[0], 1);
        reg[0] = _mm256_add_epi64(reg[0], reg[12]);

        smulh48_avx(&reg[0], &reg[4], &reg[5], &reg[6], &reg[7], &reg[8], &reg[9], &reg[10],
                    &reg[2], &reg[3]);
        reg[0] = _mm256_add_epi64(reg[0], reg[1]);
        reg[0] = _mm256_srli_epi64(reg[0], 1);
        reg[0] = _mm256_add_epi64(reg[0], reg[13]);

        smulh48_avx(&reg[0], &reg[4], &reg[5], &reg[6], &reg[7], &reg[8], &reg[9], &reg[10],
                    &reg[2], &reg[3]);
        reg[0] = _mm256_add_epi64(reg[0], reg[14]);

        _mm256_store_si256(&rej[i], reg[0]);
    }
}

static const union {
    __m256i vec;
    uint64_t arr[4];
} nolsb_avx = {.arr = {~((uint64_t)1), ~((uint64_t)1), ~((uint64_t)1), ~((uint64_t)1)}};

static inline __attribute__((always_inline)) void
gauss_unpack_words_x4(__m256i *cdt, __m256i *random, __m256i *fill_lo, __m256i *fill_hi, __m256i a,
                      __m256i b, __m256i c, size_t record)
{
    const __m256i mask16 = _mm256_load_si256(&mask72_avx.vec[2]);
    const __m256i mask24 = _mm256_load_si256(&mask72_avx.vec[1]);
    const __m256i mask48 = _mm256_load_si256(&mask72_avx.vec[0]);
    __m256i head;
    __m256i fill;
    __m256i last;

#define GAUSS_ALIGN_RECORD(SHIFT)                                                                  \
    do {                                                                                           \
        head = _mm256_or_si256(_mm256_srli_epi64(a, (SHIFT)), _mm256_slli_epi64(b, 64 - (SHIFT))); \
        fill = _mm256_or_si256(_mm256_srli_epi64(b, (SHIFT)), _mm256_slli_epi64(c, 64 - (SHIFT))); \
        last = _mm256_srli_epi64(c, (SHIFT));                                                      \
    } while (0)

    switch (record & 7U) {
    case 0:
        head = a;
        fill = b;
        last = c;
        break;
    case 1:
        GAUSS_ALIGN_RECORD(8);
        break;
    case 2:
        GAUSS_ALIGN_RECORD(16);
        break;
    case 3:
        GAUSS_ALIGN_RECORD(24);
        break;
    case 4:
        GAUSS_ALIGN_RECORD(32);
        break;
    case 5:
        GAUSS_ALIGN_RECORD(40);
        break;
    case 6:
        GAUSS_ALIGN_RECORD(48);
        break;
    default:
        GAUSS_ALIGN_RECORD(56);
        break;
    }
#undef GAUSS_ALIGN_RECORD

    *cdt = _mm256_and_si256(head, mask16);
    *cdt = _mm256_or_si256(*cdt, _mm256_slli_epi64(*cdt, 32));
    *random = _mm256_and_si256(_mm256_srli_epi64(head, 16), mask48);
    *fill_lo = _mm256_and_si256(fill, mask48);
    *fill_hi = _mm256_or_si256(_mm256_srli_epi64(fill, 48), _mm256_slli_epi64(last, 16));
    *fill_hi = _mm256_and_si256(*fill_hi, mask24);
}

static inline __attribute__((always_inline)) __m256i gauss_cdt16_one_x4(__m256i input)
{
    const __m256i bias = _mm256_set1_epi16((short)0x8000);
    const __m256i mask16 = _mm256_set1_epi64x(0xFFFF);
    __m256i sum = _mm256_setzero_si256();

    input = _mm256_or_si256(input, _mm256_slli_epi64(input, 16));
    input = _mm256_xor_si256(input, bias);
    for (size_t i = 0; i < 16; i++) {
        __m256i threshold = _mm256_set1_epi64x((long long)cdt16x4[i]);

        sum = _mm256_sub_epi16(sum, _mm256_cmpgt_epi16(input, threshold));
    }
    sum = _mm256_add_epi16(sum, _mm256_srli_epi64(sum, 32));
    sum = _mm256_add_epi16(sum, _mm256_srli_epi64(sum, 16));
    return _mm256_and_si256(sum, mask16);
}

static inline __attribute__((always_inline)) void
gauss_construct_one_x4(__m256i *candidate, __m256i *square_lo, __m256i *square_hi,
                       __m256i *exponent, __m256i coarse, __m256i fill_lo, __m256i fill_hi)
{
    const __m256i mask48 = _mm256_load_si256(&mask72_avx.vec[0]);
    const __m256i one = _mm256_load_si256(&consts_avx.vec[2]);
    __m256i high16;
    __m256i p00;
    __m256i p01;
    __m256i p02;
    __m256i p11;
    __m256i p12;
    __m256i p22;
    __m256i low;
    __m256i high;
    __m256i cross_term;

    fill_hi = _mm256_or_si256(fill_hi, _mm256_slli_epi64(coarse, 24));
    high16 = _mm256_srli_epi64(fill_lo, 32);
    p00 = _mm256_mul_epu32(fill_lo, fill_lo);
    p01 = _mm256_mul_epu32(fill_lo, high16);
    p02 = _mm256_mul_epu32(fill_lo, fill_hi);
    p11 = _mm256_mul_epu32(high16, high16);
    p12 = _mm256_mul_epu32(high16, fill_hi);
    p22 = _mm256_mul_epu32(fill_hi, fill_hi);

    low = _mm256_srli_epi64(p00, 33);
    low = _mm256_add_epi64(low, p01);
    low = _mm256_srli_epi64(low, 16);
    low = _mm256_add_epi64(low, p02);
    low = _mm256_srli_epi64(low, 15);
    low = _mm256_add_epi64(low, p11);
    low = _mm256_srli_epi64(low, 12);
    low = _mm256_add_epi64(low, _mm256_slli_epi64(p12, 5));
    cross_term = _mm256_and_si256(_mm256_slli_epi64(p22, 20), mask48);
    low = _mm256_add_epi64(low, cross_term);
    high = _mm256_srli_epi64(p22, 28);
    high = _mm256_add_epi64(high, _mm256_srli_epi64(low, 48));
    low = _mm256_and_si256(low, mask48);

    *candidate = _mm256_or_si256(_mm256_srli_epi64(fill_lo, 15), _mm256_slli_epi64(fill_hi, 33));
    *candidate = _mm256_srli_epi64(_mm256_add_epi64(*candidate, one), 1);

    coarse = _mm256_mullo_epi16(coarse, coarse);
    *exponent = _mm256_sub_epi64(high, _mm256_slli_epi64(coarse, 20));
    *exponent = _mm256_or_si256(_mm256_slli_epi64(*exponent, 20), _mm256_srli_epi64(low, 28));
    *exponent = _mm256_srli_epi64(_mm256_add_epi64(*exponent, one), 1);
    *square_lo = low;
    *square_hi = high;
}

#define GAUSS_X7_STREAMS 7U
#define GAUSS_X7_MAX_BLOCKS 11U
#define GAUSS_X7_MAX_CANDIDATES (8U * GAUSS_X7_MAX_BLOCKS)
#define GAUSS_X7_PACKED_VECTORS ((GAUSS_X7_MAX_CANDIDATES * GAUSS_RAND_BYTES + 7U) / 8U)
#define GAUSS_X7_BLOCK_CANDIDATES 8U
#define GAUSS_X7_BLOCK_RECORD_VECTORS (STREAM256_BLOCKBYTES / sizeof(uint64_t))
#ifndef GAUSS_X7_TILE_BLOCKS
#define GAUSS_X7_TILE_BLOCKS 8U
#endif
#define GAUSS_X7_TILE_CANDIDATES (GAUSS_X7_TILE_BLOCKS * GAUSS_X7_BLOCK_CANDIDATES)

typedef struct {
    uint8_t stream[4];
    uint8_t first_block[4];
    uint8_t blocks;
} gauss_x7_stage;

static const gauss_x7_stage gauss_x7_schedule[] = {
    {{0, 1, 3, 5}, {0, 0, 0, 0}, 8},     {{0, 2, 3, 5}, {8, 0, 8, 8}, 8},
    {{0, 2, 4, 5}, {16, 8, 0, 16}, 8},   {{0, 2, 4, 6}, {24, 16, 8, 0}, 11},
    {{1, 2, 4, 6}, {8, 27, 19, 11}, 8},  {{1, 3, 4, 6}, {16, 16, 27, 19}, 8},
    {{1, 3, 5, 6}, {24, 24, 24, 27}, 8}, {{1, 3, 5, UINT8_MAX}, {32, 32, 32, 0}, 3},
};

static void gauss_x7_store_new_signs(uint8_t signs[GAUSS_X7_STREAMS * (N / 8)],
                                     const gauss_x7_stage *plan, const keccakx4_state *state)
{
    __m256i pair01_lo = _mm256_unpacklo_epi64(state->s[0], state->s[1]);
    __m256i pair01_hi = _mm256_unpackhi_epi64(state->s[0], state->s[1]);
    __m256i pair23_lo = _mm256_unpacklo_epi64(state->s[2], state->s[3]);
    __m256i pair23_hi = _mm256_unpackhi_epi64(state->s[2], state->s[3]);
    __m256i lane_signs[4];

    lane_signs[0] = _mm256_permute2x128_si256(pair01_lo, pair23_lo, 0x20);
    lane_signs[1] = _mm256_permute2x128_si256(pair01_hi, pair23_hi, 0x20);
    lane_signs[2] = _mm256_permute2x128_si256(pair01_lo, pair23_lo, 0x31);
    lane_signs[3] = _mm256_permute2x128_si256(pair01_hi, pair23_hi, 0x31);
    for (size_t lane = 0; lane < 4; lane++) {
        unsigned int stream = plan->stream[lane];

        if (stream != UINT8_MAX && plan->first_block[lane] == 0)
            _mm256_storeu_si256((__m256i *)(signs + (size_t)stream * (N / 8)), lane_signs[lane]);
    }
}

static inline __attribute__((always_inline)) __m256i gauss_x7_record_word(
    const keccakx4_state *state, size_t word, __m256i carry_prefix, __m256i fresh_mask)
{
    const size_t sign_words = (N / 8) / sizeof(uint64_t);
    __m256i previous = word == 0 ? carry_prefix : _mm256_srli_epi64(state->s[word - 1], 48);
    __m256i continued = _mm256_or_si256(_mm256_slli_epi64(state->s[word], 16), previous);
    __m256i fresh = word + sign_words < GAUSS_X7_BLOCK_RECORD_VECTORS ? state->s[word + sign_words]
                                                                      : _mm256_setzero_si256();

    return _mm256_blendv_epi8(continued, fresh, fresh_mask);
}

static inline __attribute__((always_inline)) __m256i gauss_x7_lane_mask(uint8_t lanes)
{
    return _mm256_set_epi64x(-(long long)((lanes >> 3) & 1U), -(long long)((lanes >> 2) & 1U),
                             -(long long)((lanes >> 1) & 1U), -(long long)(lanes & 1U));
}

static inline __attribute__((always_inline)) void
gauss_produce_record_x4(__m256i *sample_candidates, __m256i *squares, __m256i *exponents
#if !BIT_GAUSS_PACKED_THRESHOLD
                        ,
                        __m256i *rejection
#endif
                        ,
                        __m256i a, __m256i b, __m256i c, size_t record, __m256i valid)
{
#if BIT_GAUSS_PACKED_THRESHOLD
    const __m256i invalid_random = _mm256_set1_epi64x((INT64_C(1) << 48) - 1);
#else
    const __m256i invalid_random = _mm256_set1_epi64x(INT64_MAX);
#endif
    __m256i coarse;
    __m256i random;
    __m256i fill_lo;
    __m256i fill_hi;
    __m256i candidate;
    __m256i square_lo;
    __m256i square_hi;
    __m256i exponent;

    gauss_unpack_words_x4(&coarse, &random, &fill_lo, &fill_hi, a, b, c, record);
    coarse = gauss_cdt16_one_x4(coarse);
    gauss_construct_one_x4(&candidate, &square_lo, &square_hi, &exponent, coarse, fill_lo, fill_hi);
#if BIT_GAUSS_PACKED_THRESHOLD
    random = _mm256_and_si256(_mm256_blendv_epi8(invalid_random, random, valid),
                              _mm256_load_si256(&nolsb_avx.vec));
    /* Store the 47-bit rejection threshold in unused bits of square_lo/hi.
     * Squared values occupy bits 0..47 and 0..32, respectively. */
    square_lo = _mm256_or_si256(square_lo, _mm256_slli_epi64(random, 47));
    square_hi = _mm256_or_si256(square_hi, _mm256_slli_epi64(_mm256_srli_epi64(random, 17), 33));
#else
    random = _mm256_blendv_epi8(invalid_random, random, valid);
#endif
    _mm256_store_si256(&sample_candidates[record], candidate);
    _mm256_store_si256(&squares[2 * record], square_lo);
    _mm256_store_si256(&squares[2 * record + 1], square_hi);
    _mm256_store_si256(&exponents[record], exponent);
#if !BIT_GAUSS_PACKED_THRESHOLD
    _mm256_store_si256(&rejection[record], random);
#endif
}

static __attribute__((noinline)) size_t
gauss_x7_produce_fresh_block(__m256i *sample_candidates, __m256i *squares, __m256i *exponents
#if !BIT_GAUSS_PACKED_THRESHOLD
                             ,
                             __m256i *rejection
#endif
                             ,
                             const keccakx4_state *state, __m256i carry_prefix, __m256i fresh_mask,
                             size_t count, __m256i valid_first_six, __m256i valid_last_two)
{
    __m256i a = gauss_x7_record_word(state, 0, carry_prefix, fresh_mask);
    __m256i b = gauss_x7_record_word(state, 1, carry_prefix, fresh_mask);

    for (size_t i = 0; i < count; i++) {
        __m256i c = gauss_x7_record_word(state, 2 * i + 2, carry_prefix, fresh_mask);

        gauss_produce_record_x4(sample_candidates, squares, exponents
#if !BIT_GAUSS_PACKED_THRESHOLD
                                ,
                                rejection
#endif
                                ,
                                a, b, c, i, i < 6 ? valid_first_six : valid_last_two);
        if (i + 1 < count) {
            a = c;
            b = gauss_x7_record_word(state, 2 * i + 3, carry_prefix, fresh_mask);
        }
    }
    return count;
}

static inline __attribute__((always_inline)) size_t
gauss_x7_produce_continuation(__m256i *sample_candidates, __m256i *squares, __m256i *exponents
#if !BIT_GAUSS_PACKED_THRESHOLD
                              ,
                              __m256i *rejection
#endif
                              ,
                              const keccakx4_state *state, __m256i carry_prefix, __m256i valid)
{
    __m256i a = _mm256_or_si256(_mm256_slli_epi64(state->s[0], 16), carry_prefix);
    __m256i b =
        _mm256_or_si256(_mm256_slli_epi64(state->s[1], 16), _mm256_srli_epi64(state->s[0], 48));

    for (size_t i = 0; i < 8; i++) {
        __m256i c = _mm256_or_si256(_mm256_slli_epi64(state->s[2 * i + 2], 16),
                                    _mm256_srli_epi64(state->s[2 * i + 1], 48));

        gauss_produce_record_x4(sample_candidates, squares, exponents
#if !BIT_GAUSS_PACKED_THRESHOLD
                                ,
                                rejection
#endif
                                ,
                                a, b, c, i, valid);
        if (i != 7) {
            a = c;
            b = _mm256_or_si256(_mm256_slli_epi64(state->s[2 * i + 3], 16),
                                _mm256_srli_epi64(state->s[2 * i + 2], 48));
        }
    }
    return 8;
}

static __attribute__((noinline)) void gauss_x7_consume_microbatch(
    uint64_t samples[GAUSS_X7_STREAMS * N], fp96_76 *sqsum, size_t accepted[GAUSS_X7_STREAMS],
    const size_t needed[GAUSS_X7_STREAMS], const gauss_x7_stage *plan,
    const __m256i *sample_candidates, const __m256i *squares, const __m256i *exponents
#if !BIT_GAUSS_PACKED_THRESHOLD
    ,
    const __m256i *rejection
#endif
    ,
    size_t count)
{
#if BIT_GAUSS_PACKED_THRESHOLD
    const __m256i mask48 = _mm256_load_si256(&mask72_avx.vec[0]);
    const __m256i mask33 = _mm256_set1_epi64x((INT64_C(1) << 33) - 1);
#else
    const __m256i nolsb = _mm256_load_si256(&nolsb_avx.vec);
#endif
    const __m256i minus_one = _mm256_set1_epi64x(-1);
    __m256i sum_lo = _mm256_setzero_si256();
    __m256i sum_hi = _mm256_setzero_si256();
    uint64_t dummy[1];
    uint64_t *out[4];
    size_t store[4];
    size_t written[4] = {0};
    size_t remaining[4];
    union {
        __m256i vec;
        uint64_t lane[4];
    } candidate, accept, lo, hi;
    union {
        __m256i vec;
        int64_t lane[4];
    } left;

    for (size_t lane = 0; lane < 4; lane++) {
        unsigned int stream = plan->stream[lane];

        if (stream == UINT8_MAX) {
            out[lane] = dummy;
            store[lane] = 0;
            remaining[lane] = 0;
        } else {
            size_t stored = accepted[stream] < N ? accepted[stream] : N;

            out[lane] = samples + (size_t)stream * N + stored;
            store[lane] = N - stored;
            remaining[lane] = needed[stream] - accepted[stream];
        }
        left.lane[lane] = (int64_t)remaining[lane];
    }

    for (size_t i = 0; i < count; i++) {
        __m256i norm_accept;
#if BIT_GAUSS_PACKED_THRESHOLD
        __m256i random;
#endif

        candidate.vec = _mm256_load_si256(&sample_candidates[i]);
        lo.vec = _mm256_load_si256(&squares[2 * i]);
        hi.vec = _mm256_load_si256(&squares[2 * i + 1]);
#if BIT_GAUSS_PACKED_THRESHOLD
        random = _mm256_or_si256(_mm256_srli_epi64(lo.vec, 47),
                                 _mm256_slli_epi64(_mm256_srli_epi64(hi.vec, 33), 17));
        accept.vec = _mm256_cmpgt_epi64(_mm256_load_si256(&exponents[i]), random);
#else
        accept.vec = _mm256_cmpgt_epi64(_mm256_load_si256(&exponents[i]),
                                        _mm256_and_si256(_mm256_load_si256(&rejection[i]), nolsb));
#endif
        BIT_CT_DECLASSIFY(&accept, sizeof accept);
        left.vec = _mm256_add_epi64(left.vec, accept.vec);
        norm_accept = _mm256_and_si256(accept.vec, _mm256_cmpgt_epi64(left.vec, minus_one));
#if BIT_GAUSS_PACKED_THRESHOLD
        lo.vec = _mm256_and_si256(_mm256_and_si256(lo.vec, mask48), norm_accept);
        hi.vec = _mm256_and_si256(_mm256_and_si256(hi.vec, mask33), norm_accept);
#else
        lo.vec = _mm256_and_si256(lo.vec, norm_accept);
        hi.vec = _mm256_and_si256(hi.vec, norm_accept);
#endif
        sum_lo = _mm256_add_epi64(sum_lo, lo.vec);
        sum_hi = _mm256_add_epi64(sum_hi, hi.vec);

        for (size_t lane = 0; lane < 4; lane++) {
            if (written[lane] < store[lane])
                out[lane][written[lane]] = candidate.lane[lane];
            written[lane] += accept.lane[lane] & 1U;
        }
    }

    lo.vec = sum_lo;
    hi.vec = sum_hi;
    for (size_t lane = 0; lane < 4; lane++) {
        unsigned int stream = plan->stream[lane];

        sqsum->limb48[0] += lo.lane[lane];
        sqsum->limb48[1] += hi.lane[lane];
        if (stream != UINT8_MAX) {
            size_t taken =
                left.lane[lane] > 0 ? remaining[lane] - (size_t)left.lane[lane] : remaining[lane];
            accepted[stream] += taken;
        }
    }
    sqsum->limb48[1] += sqsum->limb48[0] >> 48;
    sqsum->limb48[0] &= (1ULL << 48) - 1ULL;
}

void bit_gauss_sample_x7_scheduled(uint64_t samples[BIT_PARABOLA_BLOCKS * BIT_N],
                                   uint8_t signs[BIT_PARABOLA_BLOCKS * (BIT_N / 8)], fp96_76 *sqsum,
                                   const uint8_t seed[BIT_SEEDBYTES], uint16_t nonce)
{
    const size_t needed[GAUSS_X7_STREAMS] = {N + 1U, N + 1U, N + 1U, N + 1U, N, N, N};
    ALIGNED_UINT8(CRHBYTES + 2) extseed[8];
    ALIGNED_INT64(GAUSS_X7_TILE_CANDIDATES * 4) sample_candidates;
    ALIGNED_INT64(GAUSS_X7_TILE_CANDIDATES * 4) exponents;
    ALIGNED_INT64(GAUSS_X7_TILE_CANDIDATES * 8) squares;
#if !BIT_GAUSS_PACKED_THRESHOLD
    ALIGNED_INT64(GAUSS_X7_TILE_CANDIDATES * 4) rejection;
#endif
    shake256ctx scalar_state[GAUSS_X7_STREAMS];
    uint16_t carry[GAUSS_X7_STREAMS] = {0};
    uint8_t refill[GAUSS_X7_STREAMS][STREAM256_BLOCKBYTES + 2];
    size_t accepted[GAUSS_X7_STREAMS] = {0};
    keccakx4_state state, initial_b;

    for (size_t stream = 0; stream < 8; stream++) {
        size_t logical = stream < GAUSS_X7_STREAMS ? stream : 6;

        memset(&extseed[stream], 0, sizeof(extseed[stream]));
        memcpy(extseed[stream].coeffs, seed, BIT_SEEDBYTES);
        extseed[stream].coeffs[CRHBYTES] = (uint8_t)(nonce + logical);
        extseed[stream].coeffs[CRHBYTES + 1] = (uint8_t)((nonce + logical) >> 8);
    }
    shake256x4_absorb_once(&state, extseed[0].coeffs, extseed[1].coeffs, extseed[2].coeffs,
                           extseed[3].coeffs, CRHBYTES + 2);
    shake256x4_absorb_once(&initial_b, extseed[4].coeffs, extseed[5].coeffs, extseed[6].coeffs,
                           extseed[7].coeffs, CRHBYTES + 2);
    /* Stage zero runs streams {0,1,3,5}.  Save only the initially inactive
     * streams; active lanes remain in the x4 state across stage boundaries. */
    bit_keccakx4_export_lane(scalar_state[2].ctx, &state, 2);
    bit_keccakx4_export_lane(scalar_state[4].ctx, &initial_b, 0);
    bit_keccakx4_export_lane(scalar_state[6].ctx, &initial_b, 2);
    bit_keccakx4_copy_lane(&state, 2, &state, 3);
    bit_keccakx4_copy_lane(&state, 3, &initial_b, 1);

    for (size_t stage = 0; stage < sizeof(gauss_x7_schedule) / sizeof(gauss_x7_schedule[0]);
         stage++) {
        const gauss_x7_stage *plan = &gauss_x7_schedule[stage];
        union {
            __m256i vec;
            uint64_t lane[4];
        } lane_carry = {.lane = {0, 0, 0, 0}};
        uint8_t active_lanes = 0;
        uint8_t fresh_lanes = 0;

        for (size_t lane = 0; lane < 4; lane++) {
            unsigned int stream = plan->stream[lane];

            if (stream == UINT8_MAX)
                continue;
            active_lanes |= (uint8_t)(1U << lane);
            fresh_lanes |= (uint8_t)((plan->first_block[lane] == 0) << lane);
            lane_carry.lane[lane] = carry[stream];
        }
        for (size_t first = 0; first < plan->blocks; first += GAUSS_X7_TILE_BLOCKS) {
            size_t blocks = plan->blocks - first;
            size_t tile_count = 0;

            if (blocks > GAUSS_X7_TILE_BLOCKS)
                blocks = GAUSS_X7_TILE_BLOCKS;
            for (size_t local = 0; local < blocks; local++) {
                uint8_t block_fresh = first + local == 0 ? fresh_lanes : 0;
                __m256i fresh_mask = gauss_x7_lane_mask(block_fresh);
                size_t count;

                shake256x4_squeezeblock_state(&state);
                if (block_fresh != 0) {
                    gauss_x7_store_new_signs(signs, plan, &state);
                    count = block_fresh == active_lanes ? 6U : 8U;
                    count = gauss_x7_produce_fresh_block(
                        sample_candidates.vec + tile_count, squares.vec + 2 * tile_count,
                        exponents.vec + tile_count
#if !BIT_GAUSS_PACKED_THRESHOLD
                        ,
                        rejection.vec + tile_count
#endif
                        ,
                        &state, lane_carry.vec, fresh_mask, count, gauss_x7_lane_mask(active_lanes),
                        gauss_x7_lane_mask(active_lanes & (uint8_t)~block_fresh));
                } else {
                    count = gauss_x7_produce_continuation(
                        sample_candidates.vec + tile_count, squares.vec + 2 * tile_count,
                        exponents.vec + tile_count
#if !BIT_GAUSS_PACKED_THRESHOLD
                        ,
                        rejection.vec + tile_count
#endif
                        ,
                        &state, lane_carry.vec, gauss_x7_lane_mask(active_lanes));
                }
                lane_carry.vec = _mm256_srli_epi64(state.s[GAUSS_X7_BLOCK_RECORD_VECTORS - 1], 48);
                tile_count += count;
            }
            approx_exp_4x(exponents.vec, tile_count);
            gauss_x7_consume_microbatch(samples, sqsum, accepted, needed, plan,
                                        sample_candidates.vec, squares.vec, exponents.vec
#if !BIT_GAUSS_PACKED_THRESHOLD
                                        ,
                                        rejection.vec
#endif
                                        ,
                                        tile_count);
        }
        for (size_t lane = 0; lane < 4; lane++) {
            unsigned int stream = plan->stream[lane];

            if (stream != UINT8_MAX)
                carry[stream] = (uint16_t)lane_carry.lane[lane];
        }
        if (stage + 1 < sizeof(gauss_x7_schedule) / sizeof(gauss_x7_schedule[0])) {
            const gauss_x7_stage *next = &gauss_x7_schedule[stage + 1];

            /* Each stage replaces one lane; the other three sponge states stay resident. */
            for (size_t lane = 0; lane < 4; lane++) {
                unsigned int outgoing = plan->stream[lane];
                unsigned int incoming = next->stream[lane];

                if (outgoing == incoming)
                    continue;
                if (outgoing != UINT8_MAX)
                    bit_keccakx4_export_lane(scalar_state[outgoing].ctx, &state,
                                             (unsigned int)lane);
                if (incoming != UINT8_MAX)
                    bit_keccakx4_import_lane(&state, (unsigned int)lane,
                                             scalar_state[incoming].ctx);
            }
        } else {
            for (size_t lane = 0; lane < 4; lane++) {
                unsigned int stream = plan->stream[lane];

                if (stream != UINT8_MAX)
                    bit_keccakx4_export_lane(scalar_state[stream].ctx, &state, (unsigned int)lane);
            }
        }
    }

    for (size_t stream = 0; stream < GAUSS_X7_STREAMS; stream++) {
        refill[stream][0] = (uint8_t)carry[stream];
        refill[stream][1] = (uint8_t)(carry[stream] >> 8);
        while (accepted[stream] < needed[stream]) {
            size_t got;
            shake256_squeezeblocks(refill[stream] + 2, 1, &scalar_state[stream]);
            got = bit_gauss_refill_scalar(samples + stream * N + accepted[stream], sqsum,
                                          refill[stream], STREAM256_BLOCKBYTES + 2,
                                          needed[stream] - accepted[stream],
                                          (int)(needed[stream] % N));
            accepted[stream] += got;
            refill[stream][0] = refill[stream][STREAM256_BLOCKBYTES];
            refill[stream][1] = refill[stream][STREAM256_BLOCKBYTES + 1];
        }
    }
}
