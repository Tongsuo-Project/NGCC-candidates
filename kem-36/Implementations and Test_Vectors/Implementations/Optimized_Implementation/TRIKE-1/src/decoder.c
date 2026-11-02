#ifdef OPTIMIZED_IMPLEMENTATION
#include <immintrin.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "decoder.h"
#include "gf2x.h"

// Returns the Hamming weight.
static inline uint32_t get_hamming_weight(const uint8_t *w)
{
#ifdef OPTIMIZED_IMPLEMENTATION
    __m512i sum = _mm512_setzero_si512();
    for (size_t i = 0; i < R_SIZE_BYTES; i += 64)
    {
        __m512i v = _mm512_load_si512((__m512i *)(w + i));
        sum = _mm512_add_epi64(sum, _mm512_popcnt_epi64(v));
    }
    __m512i hsum = _mm512_alignr_epi64(sum, sum, 4);
    sum = _mm512_add_epi64(sum, hsum);
    hsum = _mm512_alignr_epi64(sum, sum, 2);
    sum = _mm512_add_epi64(sum, hsum);
    hsum = _mm512_alignr_epi64(sum, sum, 1);
    sum = _mm512_add_epi64(sum, hsum);
    return _mm512_cvtsi512_si32(sum);
#else
    uint32_t weight = 0;
    for (size_t i = 0; i < R_SIZE_BYTES; i++)
    {
        uint32_t byte = w[i];
        byte -= (byte >> 1) & UINT32_C(0x55);
        byte = (byte & UINT32_C(0x33)) + ((byte >> 2) & UINT32_C(0x33));
        weight += (byte + (byte >> 4)) & UINT32_C(0x0f);
    }
    return weight;
#endif
}

// Returns the iteration threshold.
static inline uint32_t get_threshold(uint32_t sum_init, uint32_t unsat, size_t iter)
{
    const uint64_t scale = UINT64_C(1000000000000000);
    const uint64_t midpoint = (PARAM_D + 1) / 2;
    const uint64_t init = (COA / 1000) * (uint64_t)sum_init + COB;
    const uint64_t init_tail = (COA % 1000) * (uint64_t)sum_init;
    uint32_t t;

    if (iter == 0)
    {
        t = (uint32_t)((init + (init_tail + 999) / 1000 + scale - 1) / scale);
    }
    else if (iter == 1)
    {
        t = (uint32_t)((2 * init + midpoint * scale + (2 * init_tail + 999) / 1000
                        + 3 * scale - 1) / (3 * scale));
    }
    else if (iter == 2)
    {
        t = (uint32_t)((init + 2 * midpoint * scale + (init_tail + 999) / 1000
                        + 3 * scale - 1) / (3 * scale));
    }
    else
    {
        t = (uint32_t)midpoint;
    }
    t += PARAM_DELTA;

    const uint64_t current = (COA / 1000) * (uint64_t)unsat + COB;
    const uint64_t current_tail = (COA % 1000) * (uint64_t)unsat;
    uint32_t fs = (uint32_t)((current + (current_tail + 999) / 1000 + scale - 1) / scale);

    uint32_t mask = -(fs < t);

    return (t & mask) | (fs & ~mask);
}

#ifdef OPTIMIZED_IMPLEMENTATION
// Copies the array and clears padding.
static inline void trike_cpy(uint8_t *dst, const uint8_t *src)
{
    for (size_t i = 0; i <= R_SIZE_BYTES; i += 64)
    {
        __m512i v = _mm512_load_si512((__m512i *)(src + i));
        _mm512_store_si512((__m512i *)(dst + i), v);
    }
    dst[R_SIZE_BYTES - 1] &= R_8_LAST_MASK;
    memset(dst + R_SIZE_BYTES, 0, R_ZMM_SIZE_BYTES - R_SIZE_BYTES);
}

// Compares counts with the threshold.
static inline void bf_lt(const uint8_t *c, uint8_t *a, uint32_t threshold)
{
    __m512i one = _mm512_set1_epi32(-1);
    // Counters are at most PARAM_D. Do not truncate higher threshold bits
    // when comparing the LOG2D + 1 stored slices (e.g. a dense syndrome).
    __m512i valid = _mm512_set1_epi32(-(int32_t)(threshold <= PARAM_D));
    for (size_t i = 0; i < R_SIZE_BYTES; i += 64)
    {
        _mm512_store_si512((__m512i *)(a + i), one);
    }
    for (size_t i = 0, base = 0; i <= LOG2D; i++, base += R_ZMM_SIZE_BYTES)
    {
        __m512i vth = _mm512_set1_epi32(-(threshold >> i & 1));
        for (size_t j = 0; j < R_SIZE_BYTES; j += 64)
        {
            __m512i vc = _mm512_load_si512((__m512i *)(c + base + j));
            __m512i va = _mm512_load_si512((__m512i *)(a + j));
            __m512i vres = _mm512_ternarylogic_epi32(vc, va, vth, 0xD4);
            if (i == LOG2D) vres = _mm512_and_si512(vres, valid);
            _mm512_store_si512((__m512i *)(a + j), vres);
        }
    }
}

// Duplicates the syndrome.
static inline void duplicate_syndrome(uint8_t *s)
{
    uint64_t *s_64 = (uint64_t *)s;
    s_64[R_SIZE_64 - 1] = (s_64[R_SIZE_64 - 1] & R_64_LAST_MASK)
                       | (s_64[0] << R_64_LAST_BITS);
    size_t j = 0;
    for (size_t i = R_SIZE_64; i < R_SIZE_ZMM_64; i++, j++)
    {
        s_64[i] = (s_64[j] >> R_64_REST_BITS)
                | (s_64[j + 1] << R_64_LAST_BITS);
    }
    for (size_t i = R_SIZE_ZMM_64; i < DUP_R_SIZE_ZMM_64; i += 8, j += 8)
    {
        __m512i v0 = _mm512_loadu_si512((__m512i *)(s_64 + j));
        __m512i v1 = _mm512_loadu_si512((__m512i *)(s_64 + j + 1));
        v0 = _mm512_srli_epi64(v0, R_64_REST_BITS);
        v1 = _mm512_slli_epi64(v1, R_64_LAST_BITS);
        v0 = _mm512_or_si512(v0, v1);
        _mm512_store_si512((__m512i *)(s_64 + i), v0);
    }
}

// Optimized bit-flipping decoder.
int bit_flip_decode(
    uint8_t *e0, uint8_t *e1, uint8_t *e2,
    const uint8_t *s_init, const uint32_t *idx0, const uint32_t *idx1, const uint32_t *idx2)
{
    uint8_t *aligned_e0 = (uint8_t *)aligned_alloc(64, DUP_R_ZMM_SIZE_BYTES);
    uint8_t *aligned_e1 = (uint8_t *)aligned_alloc(64, DUP_R_ZMM_SIZE_BYTES);
    uint8_t *aligned_e2 = (uint8_t *)aligned_alloc(64, DUP_R_ZMM_SIZE_BYTES);
    uint8_t *aligned_s = (uint8_t *)aligned_alloc(64, DUP_R_ZMM_SIZE_BYTES);
    uint8_t *upc = (uint8_t *)aligned_alloc(64, UPC_SIZE_BYTES);
    uint8_t *aligned_v = (uint8_t *)aligned_alloc(64, R_ZMM_SIZE_BYTES);
    int rc = -1;
    if (!aligned_e0 || !aligned_e1 || !aligned_e2 || !aligned_s || !upc || !aligned_v) goto cleanup;
    trike_setz(aligned_e0, DUP_R_ZMM_SIZE_BYTES);
    trike_setz(aligned_e1, DUP_R_ZMM_SIZE_BYTES);
    trike_setz(aligned_e2, DUP_R_ZMM_SIZE_BYTES);
    trike_assign(s_init, aligned_s, R_ZMM_SIZE_BYTES, DUP_R_ZMM_SIZE_BYTES);

    uint32_t off0[PARAM_D], off1[PARAM_D], off2[PARAM_D];
    for (size_t i = 0; i < PARAM_D; i++)
    {
        uint32_t to = PARAM_R - idx0[i];
        uint32_t mask = -(to == PARAM_R);
        off0[i] = to - (PARAM_R & mask);
    }
    for (size_t i = 0; i < PARAM_D; i++)
    {
        uint32_t to = PARAM_R - idx1[i];
        uint32_t mask = -(to == PARAM_R);
        off1[i] = to - (PARAM_R & mask);
    }
    for (size_t i = 0; i < PARAM_D; i++)
    {
        uint32_t to = PARAM_R - idx2[i];
        uint32_t mask = -(to == PARAM_R);
        off2[i] = to - (PARAM_R & mask);
    }

    uint32_t sum_init = get_hamming_weight(aligned_s);
    uint32_t unsat = sum_init;

    for (size_t iter = 0; iter < BIT_FLIP_ITER; iter++)
    {
        uint32_t threshold = get_threshold(sum_init, unsat, iter);

        duplicate_syndrome(aligned_s);
        trike_setz(upc, UPC_SIZE_BYTES);
        for (size_t k = 0; k < PARAM_D; k++)
        {
            if (gf2x_shift_add(upc, aligned_s, idx0[k], LOGG(k + 1))) goto cleanup;
        }
        bf_lt(upc, aligned_v, threshold);
        gf2x_add(aligned_e0, aligned_e0, aligned_v);

        trike_setz(upc, UPC_SIZE_BYTES);
        for (size_t k = 0; k < PARAM_D; k++)
        {
            if (gf2x_shift_add(upc, aligned_s, idx1[k], LOGG(k + 1))) goto cleanup;
        }
        bf_lt(upc, aligned_v, threshold);
        gf2x_add(aligned_e1, aligned_e1, aligned_v);

        trike_setz(upc, UPC_SIZE_BYTES);
        for (size_t k = 0; k < PARAM_D; k++)
        {
            if (gf2x_shift_add(upc, aligned_s, idx2[k], LOGG(k + 1))) goto cleanup;
        }
        bf_lt(upc, aligned_v, threshold);
        gf2x_add(aligned_e2, aligned_e2, aligned_v);

        // The final syndrome is never consumed. This condition depends only
        // on the public iteration counter, not decoder convergence.
        if (iter + 1 == BIT_FLIP_ITER) continue;

        duplicate_syndrome(aligned_e0);
        duplicate_syndrome(aligned_e1);
        duplicate_syndrome(aligned_e2);

        trike_assign(s_init, aligned_s, R_ZMM_SIZE_BYTES, DUP_R_ZMM_SIZE_BYTES);
        for (size_t k = 0; k < PARAM_D; k++)
        {
            if (gf2x_shift_xor(aligned_s, aligned_e0, off0[k])) goto cleanup;
        }
        for (size_t k = 0; k < PARAM_D; k++)
        {
            if (gf2x_shift_xor(aligned_s, aligned_e1, off1[k])) goto cleanup;
        }
        for (size_t k = 0; k < PARAM_D; k++)
        {
            if (gf2x_shift_xor(aligned_s, aligned_e2, off2[k])) goto cleanup;
        }
        unsat = get_hamming_weight(aligned_s);
    }

    trike_cpy(e0, aligned_e0);
    trike_cpy(e1, aligned_e1);
    trike_cpy(e2, aligned_e2);

    rc = 0;
cleanup:
    free(aligned_e0);
    free(aligned_e1);
    free(aligned_e2);
    free(aligned_s);
    free(upc);
    free(aligned_v);
    return rc;
}
#else
// Counts unsatisfied parity checks.
void calc_upc_block(
    const uint8_t *s, const uint32_t *idx, uint32_t *unsat_cnt)
{
    for (size_t i = 0; i < PARAM_R; i++)
    {
        uint32_t count = 0;
        for (size_t k = 0; k < PARAM_D; k++)
        {
            uint32_t idx_to = (i + idx[k]) % PARAM_R;
            count += (s[idx_to >> 3] >> (idx_to & 7)) & 1;
        }
        unsat_cnt[i] = count;
    }
}

// Flips bits meeting the threshold.
void flip_bits(uint8_t *e, const uint32_t *unsat_cnt, uint32_t threshold)
{
    size_t idx = 0;
    for (size_t i = 0; i < R_SIZE_BYTES; i++)
    {
        uint8_t byte = 0;
        for (size_t j = 0; j < 8 && idx < PARAM_R; j++, idx++)
        {
            byte |= (uint8_t)(unsat_cnt[idx] >= threshold) << j;
        }
        e[i] ^= byte;
    }
}

// Recomputes the syndrome.
int calculate_syndrome(
    uint8_t *s, const uint8_t *s_init, const uint8_t *e0, const uint8_t *e1, const uint8_t *e2,
    const uint32_t *idx0, const uint32_t *idx1, const uint32_t *idx2)
{
    memcpy(s, s_init, R_SIZE_BYTES);
    uint8_t *temp = (uint8_t *)calloc(R_ZMM_SIZE_BYTES, sizeof(uint8_t));
    int rc = -1;
    if (temp == NULL) return -1;

    for (size_t k = 0; k < PARAM_D; k++)
    {
        if (gf2x_shift(temp, e0, idx0[k])) goto cleanup;
        gf2x_add(s, s, temp);
    }

    for (size_t k = 0; k < PARAM_D; k++)
    {
        if (gf2x_shift(temp, e1, idx1[k])) goto cleanup;
        gf2x_add(s, s, temp);
    }

    for (size_t k = 0; k < PARAM_D; k++)
    {
        if (gf2x_shift(temp, e2, idx2[k])) goto cleanup;
        gf2x_add(s, s, temp);
    }

    rc = 0;
cleanup:
    free(temp);
    return rc;
}

// Reference bit-flipping decoder.
int bit_flip_decode(
    uint8_t *e0, uint8_t *e1, uint8_t *e2,
    const uint8_t *s_init, const uint32_t *idx0, const uint32_t *idx1, const uint32_t *idx2)
{
    uint32_t *unsat_cnt = (uint32_t *)calloc(PARAM_N, sizeof(uint32_t));
    uint8_t *s = (uint8_t *)calloc(R_ZMM_SIZE_BYTES, sizeof(uint8_t));
    int rc = -1;
    if (unsat_cnt == NULL || s == NULL)
    {
        free(unsat_cnt);
        free(s);
        return -1;
    }

    memcpy(s, s_init, R_ZMM_SIZE_BYTES);
    uint32_t sum_init = get_hamming_weight(s);
    uint32_t unsat = sum_init;

    for (size_t iter = 0; iter < BIT_FLIP_ITER; iter++)
    {
        uint32_t threshold = get_threshold(sum_init, unsat, iter);

        calc_upc_block(s, idx0, unsat_cnt);
        calc_upc_block(s, idx1, unsat_cnt + PARAM_R);
        calc_upc_block(s, idx2, unsat_cnt + PARAM_R * 2);

        flip_bits(e0, unsat_cnt, threshold);
        flip_bits(e1, unsat_cnt + PARAM_R, threshold);
        flip_bits(e2, unsat_cnt + PARAM_R * 2, threshold);

        if (calculate_syndrome(s, s_init, e0, e1, e2, idx0, idx1, idx2)) goto cleanup;
        unsat = get_hamming_weight(s);
    }

    rc = 0;
cleanup:
    free(unsat_cnt);
    free(s);
    return rc;
}
#endif

// Decodes the error vector.
int decode(
    uint8_t *e0, uint8_t *e1, uint8_t *e2,
    const uint8_t *s, const uint32_t *idx0, const uint32_t *idx1, const uint32_t *idx2)
{
    return bit_flip_decode(e0, e1, e2, s, idx0, idx1, idx2);
}
