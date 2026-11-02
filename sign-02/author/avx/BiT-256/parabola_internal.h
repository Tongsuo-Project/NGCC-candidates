/* Copyright (c) 2026 Hang Zhang. */

#ifndef PARABOLA_INTERNAL_H
#define PARABOLA_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "params.h"
#include "symmetric.h"

typedef struct {
    uint64_t limb48[2];
} fp96_76;

typedef struct {
    uint64_t z_norm2;
    uint64_t v_norm2;
    int64_t dot;
} parabola_fixed_stats;

uint64_t bit_parabola_scale_round_avx(int32_t out[BIT_N], const uint64_t samples[BIT_N],
                                      const uint8_t signs[BIT_N / 8], const fp96_76 *scale);
void bit_parabola_accumulate_response_stats_avx(parabola_fixed_stats *stats,
                                                const int32_t mask[BIT_N],
                                                const int16_t shift[BIT_N], int32_t shift_scale);

/* Rare scalar refill after the scheduled AVX2 blocks are exhausted. */
size_t bit_gauss_refill_scalar(uint64_t *samples, fp96_76 *sqsum, const uint8_t *buf, size_t buflen,
                               size_t needed, int drop_last);

void bit_gauss_sample_x7_scheduled(uint64_t samples[BIT_PARABOLA_BLOCKS * BIT_N],
                                   uint8_t signs[BIT_PARABOLA_BLOCKS * (BIT_N / 8)], fp96_76 *sqsum,
                                   const uint8_t seed[BIT_SEEDBYTES], uint16_t nonce);
#endif
