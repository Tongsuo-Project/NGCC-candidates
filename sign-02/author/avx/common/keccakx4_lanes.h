/* Copyright (c) 2026 Hang Zhang. */

/*
 * Lane migration helpers for the AVX2 four-way Keccak representation.
 * The caller owns the sponge phase and byte-consumer state.
 */
#ifndef BIT_KECCAKX4_LANES_H
#define BIT_KECCAKX4_LANES_H

#include "fips202x4.h"
#include <stddef.h>
#include <stdint.h>

#define BIT_KECCAK_STATE_WORDS 25U
#define BIT_KECCAKX4_LANES 4U

/* Lane indices must be in [0, 4).  Source and destination states may alias. */
void bit_keccakx4_copy_lane(keccakx4_state *dst, unsigned int dst_lane,
                            const keccakx4_state *src,
                            unsigned int src_lane);

/* Copy two distinct source lanes to two distinct destination lanes. */
void bit_keccakx4_copy_lanes2(keccakx4_state *dst,
                              unsigned int dst_lane0,
                              unsigned int dst_lane1,
                              const keccakx4_state *src,
                              unsigned int src_lane0,
                              unsigned int src_lane1);

/* Convert between one x4 lane and the scalar 25-word Keccak state layout. */
void bit_keccakx4_export_lane(uint64_t dst[BIT_KECCAK_STATE_WORDS],
                              const keccakx4_state *src,
                              unsigned int src_lane);
void bit_keccakx4_export_lanes2(uint64_t dst0[BIT_KECCAK_STATE_WORDS],
                                uint64_t dst1[BIT_KECCAK_STATE_WORDS],
                                const keccakx4_state *src,
                                unsigned int src_lane0,
                                unsigned int src_lane1);
void bit_keccakx4_export_all(uint64_t dst0[BIT_KECCAK_STATE_WORDS],
                             uint64_t dst1[BIT_KECCAK_STATE_WORDS],
                             uint64_t dst2[BIT_KECCAK_STATE_WORDS],
                             uint64_t dst3[BIT_KECCAK_STATE_WORDS],
                             const keccakx4_state *src);
void bit_keccakx4_import_lane(keccakx4_state *dst, unsigned int dst_lane,
                              const uint64_t src[BIT_KECCAK_STATE_WORDS]);

/* Extract bytes from squeezeblocks_vec(). */
static inline void bit_keccakx4_extract_bytes(uint8_t *dst,
                                               const __m256i *src,
                                               unsigned int src_lane,
                                               size_t offset, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)src;

    for (size_t i = 0; i < len; i++) {
        size_t position = offset + i;

        dst[i] = bytes[(position / sizeof(uint64_t)) * sizeof(__m256i)
                       + src_lane * sizeof(uint64_t)
                       + position % sizeof(uint64_t)];
    }
}

#endif
