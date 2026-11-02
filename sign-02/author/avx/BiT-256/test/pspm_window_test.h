/* Copyright (c) 2026 Hang Zhang. */
#ifndef BIT_TEST_PSPM_WINDOW_H
#define BIT_TEST_PSPM_WINDOW_H
#include <stdio.h>
#include "polyvec.h"

/* Independent scalar negacyclic convolution, including the global sign. */
static int pspm_test_product(const poly *out, const poly *in, const sparse_challenge *c)
{
    for (int i = 0; i < BIT_N; i++) {
        int32_t expected = 0;
        for (int t = 0; t < BIT_TAU; t++) {
            int j = i - c->pos[t];
            int sign = c->sign[t];
            if (j < 0) {
                j += BIT_N;
                sign = -sign;
            }
            expected += sign * in->coeffs[j];
        }
        if (c->bflip)
            expected = -expected;
        if (out->coeffs[i] != expected) {
            fprintf(stderr, "PSPM product mismatch at coefficient %d\n", i);
            return 1;
        }
    }
    return 0;
}

static int test_pspm_windows(void)
{
    polyvecl s, cs;
    polyveck e, ce;
    bit_pspm_se_precomp se_table;
    sparse_challenge c;
    const unsigned int rotations[] = {0, 1, BIT_N / 2, BIT_N - 1};
    for (int i = 0; i < BIT_L; i++)
        for (int j = 0; j < BIT_N; j++)
            s.vec[i].coeffs[j] = (17 * j + i) % 3 - 1;
    for (int i = 0; i < BIT_K; i++)
        for (int j = 0; j < BIT_N; j++) {
            e.vec[i].coeffs[j] = (13 * j + i + 1) % 3 - 1;
        }
    polyvec_prepare_challenge_mul_se(&se_table, &s, &e);
    for (unsigned int r = 0; r < 4; r++) {
        for (unsigned int pattern = 0; pattern < 3; pattern++) {
            for (int t = 0; t < BIT_TAU; t++) {
                c.pos[t] = (rotations[r] + 17 * t) % BIT_N;
                c.sign[t] = pattern == 0 ? 1 : pattern == 1 ? -1 : (t & 1) ? -1 : 1;
            }
            for (unsigned int flip = 0; flip < 2; flip++) {
                c.bflip = (uint8_t)flip;
                polyvec_mul_challenge_no_reduce_se_pspm(&cs, &ce, &se_table, &c);
                for (int i = 0; i < BIT_L; i++)
                    if (pspm_test_product(&cs.vec[i], &s.vec[i], &c))
                        return 1;
                for (int i = 0; i < BIT_K; i++)
                    if (pspm_test_product(&ce.vec[i], &e.vec[i], &c))
                        return 1;
            }
        }
    }
    return 0;
}
#endif
