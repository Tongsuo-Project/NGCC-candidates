#include "sampler.h"
#include "gauss_q80_constants.h"
#include "symmetric.h"

#ifdef DARTS_FIPS202_X4
#include "fips202x4.h"
#endif

#include <immintrin.h>
#include <stdint.h>
#include <string.h>

#define GAUSS_RAND_BYTES 31
#define GAUSS4_SIGN_BYTES (N / 8)
#define GAUSS4_INITIAL_NBLOCKS                                             \
    ((GAUSS_RAND_BYTES * N + STREAM256_BLOCKBYTES - 1) /                  \
     STREAM256_BLOCKBYTES)
#define GAUSS4_INITIAL_BYTES (GAUSS4_INITIAL_NBLOCKS * STREAM256_BLOCKBYTES)
#define GAUSS4_BUFFER_BYTES (GAUSS4_INITIAL_BYTES + STREAM256_BLOCKBYTES)

typedef struct {
#ifdef DARTS_FIPS202_X4
    keccakx4_state state;
#else
    stream256_state state[4];
#endif
    uint8_t active[4];
} gauss4_stream_state;

static uint64_t load64_le_4x(const uint8_t src[8]) {
    return (uint64_t)src[0] | ((uint64_t)src[1] << 8) |
           ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) |
           ((uint64_t)src[6] << 48) | ((uint64_t)src[7] << 56);
}

/* Four independent unsigned 112-bit inverse-CDF comparisons in parallel. */
static void sample_gauss112_4x(uint64_t x[4],
                               const uint8_t *const rand[4]) {
    const __m256i bias = _mm256_set1_epi64x((int64_t)INT64_MIN);
    const __m256i rand_lo = _mm256_set_epi64x(
        (int64_t)load64_le_4x(rand[3]), (int64_t)load64_le_4x(rand[2]),
        (int64_t)load64_le_4x(rand[1]), (int64_t)load64_le_4x(rand[0]));
    const __m256i rand_hi = _mm256_set_epi64x(
        (int64_t)(load64_le_4x(rand[3] + 8) & ((UINT64_C(1) << 48) - 1)),
        (int64_t)(load64_le_4x(rand[2] + 8) & ((UINT64_C(1) << 48) - 1)),
        (int64_t)(load64_le_4x(rand[1] + 8) & ((UINT64_C(1) << 48) - 1)),
        (int64_t)(load64_le_4x(rand[0] + 8) & ((UINT64_C(1) << 48) - 1)));
    const __m256i rand_lo_biased = _mm256_xor_si256(rand_lo, bias);
    __m256i count = _mm256_setzero_si256();
    unsigned int i;

    for (i = 0; i < CDTLEN; i++) {
        const __m256i threshold_lo =
            _mm256_set1_epi64x((int64_t)CDT112_LO[i]);
        const __m256i threshold_hi =
            _mm256_set1_epi64x((int64_t)CDT112_HI[i]);
        const __m256i lo_gt = _mm256_cmpgt_epi64(
            rand_lo_biased, _mm256_xor_si256(threshold_lo, bias));
        const __m256i hi_gt = _mm256_cmpgt_epi64(rand_hi, threshold_hi);
        const __m256i hi_eq = _mm256_cmpeq_epi64(rand_hi, threshold_hi);
        const __m256i greater =
            _mm256_or_si256(hi_gt, _mm256_and_si256(hi_eq, lo_gt));

        /* greater is all ones; subtracting it increments the lane count. */
        count = _mm256_sub_epi64(count, greater);
    }

    _mm256_storeu_si256((__m256i *)x, count);
}

static void mul64_4x(__m256i *lo, __m256i *hi,
                     const __m256i a, const __m256i b) {
    const __m256i mask32 = _mm256_set1_epi64x(UINT64_C(0xffffffff));
    const __m256i ah = _mm256_srli_epi64(a, 32);
    const __m256i bh = _mm256_srli_epi64(b, 32);
    const __m256i p00 = _mm256_mul_epu32(a, b);
    const __m256i p01 = _mm256_mul_epu32(a, bh);
    const __m256i p10 = _mm256_mul_epu32(ah, b);
    const __m256i p11 = _mm256_mul_epu32(ah, bh);
    __m256i middle = _mm256_srli_epi64(p00, 32);

    middle = _mm256_add_epi64(middle, _mm256_and_si256(p01, mask32));
    middle = _mm256_add_epi64(middle, _mm256_and_si256(p10, mask32));
    *lo = _mm256_or_si256(_mm256_and_si256(p00, mask32),
                          _mm256_slli_epi64(middle, 32));
    *hi = _mm256_add_epi64(p11, _mm256_srli_epi64(p01, 32));
    *hi = _mm256_add_epi64(*hi, _mm256_srli_epi64(p10, 32));
    *hi = _mm256_add_epi64(*hi, _mm256_srli_epi64(middle, 32));
}

static __m256i smulh63_ceil_4x(const __m256i a, const __m256i b) {
    const __m256i zero = _mm256_setzero_si256();
    const __m256i one = _mm256_set1_epi64x(1);
    const __m256i sign = _mm256_cmpgt_epi64(zero, a);
    const __m256i magnitude =
        _mm256_sub_epi64(_mm256_xor_si256(a, sign), sign);
    __m256i lo, hi, quotient, remainder, increment, positive, negative;

    mul64_4x(&lo, &hi, magnitude, b);
    quotient = _mm256_or_si256(_mm256_slli_epi64(hi, 1),
                               _mm256_srli_epi64(lo, 63));
    remainder = _mm256_and_si256(lo, _mm256_set1_epi64x(INT64_MAX));
    increment = _mm256_andnot_si256(
        _mm256_cmpeq_epi64(remainder, zero), one);
    positive = _mm256_add_epi64(quotient, increment);
    negative = _mm256_sub_epi64(zero, quotient);
    return _mm256_or_si256(_mm256_andnot_si256(sign, positive),
                           _mm256_and_si256(sign, negative));
}

static __m256i approx_exp_4x(const __m256i x) {
    const __m256i bias = _mm256_set1_epi64x((int64_t)INT64_MIN);
    const __m256i limit = _mm256_set1_epi64x((int64_t)INT64_MIN);
    __m256i result = _mm256_set1_epi64x(EXP_C12);
    __m256i threshold, above_limit;

#define HORNER_STEP(C)                                                        \
    result = _mm256_add_epi64(smulh63_ceil_4x(result, x),                    \
                              _mm256_set1_epi64x((int64_t)(C)))
    HORNER_STEP(EXP_C11);
    HORNER_STEP(EXP_C10);
    HORNER_STEP(EXP_C9);
    HORNER_STEP(EXP_C8);
    HORNER_STEP(EXP_C7);
    HORNER_STEP(EXP_C6);
    HORNER_STEP(EXP_C5);
    HORNER_STEP(EXP_C4);
    HORNER_STEP(EXP_C3);
    HORNER_STEP(EXP_C2);
    HORNER_STEP(EXP_C1);
#undef HORNER_STEP
    result = smulh63_ceil_4x(result, x);
    threshold = _mm256_add_epi64(
        result, _mm256_set1_epi64x((int64_t)EXP_C0));
    above_limit = _mm256_cmpgt_epi64(_mm256_xor_si256(threshold, bias),
                                     _mm256_xor_si256(limit, bias));
    return _mm256_or_si256(_mm256_and_si256(above_limit, limit),
                           _mm256_andnot_si256(above_limit, threshold));
}

static void square_y_4x(__m256i *square_lo, __m256i *square_hi,
                        const __m256i y_lo, const __m256i y_hi) {
    const __m256i mask48 =
        _mm256_set1_epi64x((int64_t)((UINT64_C(1) << 48) - 1));
    __m256i lo, hi, tmp_lo, tmp_hi, s0, s1;

    mul64_4x(&lo, &hi, y_lo, y_lo);
    s0 = _mm256_or_si256(_mm256_slli_epi64(hi, 16),
                         _mm256_srli_epi64(lo, 48));

    mul64_4x(&tmp_lo, &tmp_hi, y_lo, y_hi);
    s0 = _mm256_add_epi64(
        s0, _mm256_slli_epi64(_mm256_and_si256(tmp_lo, mask48), 1));
    s1 = _mm256_slli_epi64(
        _mm256_or_si256(_mm256_slli_epi64(tmp_hi, 16),
                        _mm256_srli_epi64(tmp_lo, 48)),
        1);

    s0 = _mm256_srli_epi64(s0, 28);
    s0 = _mm256_add_epi64(
        s0, _mm256_and_si256(_mm256_slli_epi64(s1, 20), mask48));
    s1 = _mm256_srli_epi64(s1, 28);

    mul64_4x(&tmp_lo, &tmp_hi, y_hi, y_hi);
    s0 = _mm256_add_epi64(
        s0, _mm256_and_si256(_mm256_slli_epi64(tmp_lo, 20), mask48));
    s1 = _mm256_add_epi64(s1, _mm256_srli_epi64(tmp_lo, 28));
    s1 = _mm256_add_epi64(s1, _mm256_slli_epi64(tmp_hi, 36));
    s1 = _mm256_add_epi64(s1, _mm256_srli_epi64(s0, 48));
    *square_lo = _mm256_and_si256(s0, mask48);
    *square_hi = s1;
}

static void sample_gauss_finish_4x(uint64_t sample[4], uint64_t square_lo[4],
                                   uint64_t square_hi[4], uint64_t accepted[4],
                                   const uint8_t *const rand[4],
                                   const uint64_t x_scalar[4]) {
    const __m256i zero = _mm256_setzero_si256();
    const __m256i mask48 =
        _mm256_set1_epi64x((int64_t)((UINT64_C(1) << 48) - 1));
    const __m256i x = _mm256_loadu_si256((const __m256i *)x_scalar);
    const __m256i y_lo = _mm256_set_epi64x(
        (int64_t)(load64_le_4x(rand[3] + 22) & ((UINT64_C(1) << 48) - 1)),
        (int64_t)(load64_le_4x(rand[2] + 22) & ((UINT64_C(1) << 48) - 1)),
        (int64_t)(load64_le_4x(rand[1] + 22) & ((UINT64_C(1) << 48) - 1)),
        (int64_t)(load64_le_4x(rand[0] + 22) & ((UINT64_C(1) << 48) - 1)));
    const __m256i tail_hi = _mm256_set_epi64x(
        (int64_t)(rand[3][28] | ((uint64_t)rand[3][29] << 8) |
                  ((uint64_t)rand[3][30] << 16)),
        (int64_t)(rand[2][28] | ((uint64_t)rand[2][29] << 8) |
                  ((uint64_t)rand[2][30] << 16)),
        (int64_t)(rand[1][28] | ((uint64_t)rand[1][29] << 8) |
                  ((uint64_t)rand[1][30] << 16)),
        (int64_t)(rand[0][28] | ((uint64_t)rand[0][29] << 8) |
                  ((uint64_t)rand[0][30] << 16)));
    const __m256i y_hi =
        _mm256_or_si256(tail_hi, _mm256_slli_epi64(x, 24));
    const __m256i rejection = _mm256_set_epi64x(
        (int64_t)load64_le_4x(rand[3] + 14),
        (int64_t)load64_le_4x(rand[2] + 14),
        (int64_t)load64_le_4x(rand[1] + 14),
        (int64_t)load64_le_4x(rand[0] + 14));
    __m256i sample_vec, sq_lo, sq_hi, x_sq, diff_hi, exp_in, threshold;
    __m256i rand_rej, zero_coin, nonzero, accept;

    sample_vec = _mm256_srli_epi64(
        _mm256_add_epi64(_mm256_srli_epi64(y_lo, 15),
                         _mm256_set1_epi64x(1)),
        1);
    sample_vec = _mm256_add_epi64(sample_vec,
                                  _mm256_slli_epi64(y_hi, 32));
    square_y_4x(&sq_lo, &sq_hi, y_lo, y_hi);

    x_sq = _mm256_mul_epu32(x, x);
    diff_hi = _mm256_sub_epi64(sq_hi, _mm256_slli_epi64(x_sq, 20));
    exp_in = _mm256_or_si256(_mm256_slli_epi64(diff_hi, 34),
                             _mm256_srli_epi64(sq_lo, 14));
    exp_in = _mm256_add_epi64(
        exp_in,
        _mm256_and_si256(_mm256_srli_epi64(sq_lo, 13),
                         _mm256_set1_epi64x(1)));
    threshold = approx_exp_4x(exp_in);

    rand_rej = _mm256_and_si256(rejection,
                                _mm256_set1_epi64x(INT64_MAX));
    zero_coin = _mm256_srli_epi64(rejection, 63);
    accept = _mm256_srli_epi64(_mm256_sub_epi64(rand_rej, threshold), 63);
    nonzero = _mm256_srli_epi64(
        _mm256_or_si256(sample_vec, _mm256_sub_epi64(zero, sample_vec)), 63);
    accept = _mm256_and_si256(accept,
                              _mm256_or_si256(nonzero, zero_coin));

    _mm256_storeu_si256((__m256i *)sample, sample_vec);
    _mm256_storeu_si256((__m256i *)square_lo,
                        _mm256_and_si256(sq_lo, mask48));
    _mm256_storeu_si256((__m256i *)square_hi, sq_hi);
    _mm256_storeu_si256((__m256i *)accepted, accept);
}

static void gauss4_stream_init(gauss4_stream_state *stream,
                               const uint8_t seed[CRHBYTES],
                               const uint16_t nonce[4],
                               const size_t len[4]) {
    unsigned int lane;

    for (lane = 0; lane < 4; lane++)
        stream->active[lane] = (uint8_t)(len[lane] != 0);

#ifdef DARTS_FIPS202_X4
    /* fips202x4 gathers a full final uint64_t; keep six padding bytes valid. */
    uint8_t input[4][CRHBYTES + 8] = {{0}};

    for (lane = 0; lane < 4; lane++) {
        memcpy(input[lane], seed, CRHBYTES);
        input[lane][CRHBYTES] = (uint8_t)nonce[lane];
        input[lane][CRHBYTES + 1] = (uint8_t)(nonce[lane] >> 8);
    }
    shake256x4_absorb_once(&stream->state, input[0], input[1], input[2],
                           input[3], CRHBYTES + 2);
#else
    for (lane = 0; lane < 4; lane++) {
        if (stream->active[lane])
            stream256_init(&stream->state[lane], seed, nonce[lane]);
    }
#endif
}

static void gauss4_stream_squeezeblocks(uint8_t *out[4], size_t nblocks,
                                        gauss4_stream_state *stream) {
#ifdef DARTS_FIPS202_X4
    shake256x4_squeezeblocks(out[0], out[1], out[2], out[3], nblocks,
                             &stream->state);
#else
    unsigned int lane;
    const size_t outlen = nblocks * STREAM256_BLOCKBYTES;

    for (lane = 0; lane < 4; lane++) {
        if (stream->active[lane])
            stream256_squeezeblocks(out[lane], nblocks,
                                    &stream->state[lane]);
        else
            memset(out[lane], 0, outlen);
    }
#endif
}

static int gauss4_supported_length(const size_t len) {
    return len == 0 || len == N || len == N + 1;
}

void sample_gauss_N_4x(uint64_t *r0, uint64_t *r1,
                       uint64_t *r2, uint64_t *r3,
                       uint8_t *signs0, uint8_t *signs1,
                       uint8_t *signs2, uint8_t *signs3,
                       fp96_76 *sqsum, const uint8_t seed[CRHBYTES],
                       const uint16_t nonce0, const uint16_t nonce1,
                       const uint16_t nonce2, const uint16_t nonce3,
                       const size_t len0, const size_t len1,
                       const size_t len2, const size_t len3) {
    uint64_t *const result[4] = {r0, r1, r2, r3};
    uint8_t *const signs[4] = {signs0, signs1, signs2, signs3};
    const uint16_t nonce[4] = {nonce0, nonce1, nonce2, nonce3};
    const size_t len[4] = {len0, len1, len2, len3};
    size_t count[4] = {0, 0, 0, 0};
    uint8_t buffer[4][GAUSS4_BUFFER_BYTES] __attribute__((aligned(32)));
    uint8_t *squeeze_out[4] = {buffer[0], buffer[1], buffer[2], buffer[3]};
    gauss4_stream_state stream;
    size_t pos = GAUSS4_SIGN_BYTES;
    size_t end = GAUSS4_INITIAL_BYTES;
    unsigned int lane;

    if (!gauss4_supported_length(len0) || !gauss4_supported_length(len1) ||
        !gauss4_supported_length(len2) || !gauss4_supported_length(len3)) {
        for (lane = 0; lane < 4; lane++) {
            if (len[lane] != 0)
                sample_gauss_N(result[lane], signs[lane], sqsum, seed,
                               nonce[lane], len[lane]);
        }
        return;
    }

    gauss4_stream_init(&stream, seed, nonce, len);
    gauss4_stream_squeezeblocks(squeeze_out, GAUSS4_INITIAL_NBLOCKS,
                                &stream);

    for (lane = 0; lane < 4; lane++) {
        if (len[lane] != 0)
            memcpy(signs[lane], buffer[lane], GAUSS4_SIGN_BYTES);
    }

    for (;;) {
        const uint8_t *candidate[4];
        uint64_t x[4];
        uint64_t sample[4], square_lo[4], square_hi[4], accepted[4];
        int finished = 1;

        for (lane = 0; lane < 4; lane++)
            finished &= count[lane] >= len[lane];
        if (finished)
            break;

        if (pos + GAUSS_RAND_BYTES > end) {
            const size_t remaining = end - pos;
            uint8_t refill[4][STREAM256_BLOCKBYTES]
                __attribute__((aligned(32)));
            uint8_t *refill_out[4] = {
                refill[0], refill[1], refill[2], refill[3]
            };

            for (lane = 0; lane < 4; lane++) {
                memmove(buffer[lane], buffer[lane] + pos, remaining);
            }
            gauss4_stream_squeezeblocks(refill_out, 1, &stream);
            for (lane = 0; lane < 4; lane++)
                memcpy(buffer[lane] + remaining, refill[lane],
                       STREAM256_BLOCKBYTES);
            pos = 0;
            end = remaining + STREAM256_BLOCKBYTES;
        }

        for (lane = 0; lane < 4; lane++)
            candidate[lane] = buffer[lane] + pos;
        sample_gauss112_4x(x, candidate);
        sample_gauss_finish_4x(sample, square_lo, square_hi, accepted,
                               candidate, x);

        for (lane = 0; lane < 4; lane++) {
            if (count[lane] >= len[lane])
                continue;
            if (accepted[lane] == 0)
                continue;

            if (count[lane] < N)
                result[lane][count[lane]] = sample[lane];
            count[lane]++;
            sqsum->limb48[0] += square_lo[lane];
            sqsum->limb48[1] += square_hi[lane];
        }

        pos += GAUSS_RAND_BYTES;
    }

    renormalize(sqsum);
}
