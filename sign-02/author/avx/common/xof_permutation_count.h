/* Copyright (c) 2026 Hang Zhang. */

#ifndef BIT_XOF_PERMUTATION_COUNT_H
#define BIT_XOF_PERMUTATION_COUNT_H

#include <stdint.h>

typedef struct {
    uint64_t scalar;
    uint64_t x4;
} bit_xof_permutation_counts;

void bit_xof_permutation_counts_reset(void);
bit_xof_permutation_counts bit_xof_permutation_counts_get(void);

#define bit_xof_count_scalar_permutation() ((void)0)
#define bit_xof_count_x4_permutation() ((void)0)

#endif
