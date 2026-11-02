/*
 * Copyright (c) 2026 Hang Zhang.
 * AVX2 union helpers derived from HAETAE v2.0,
 * Copyright (c) 2023 Team HAETAE (MIT license).
 */
#ifndef ALIGN_H
#define ALIGN_H

#include <stdint.h>
#include <immintrin.h>

#define BIT_ALIGN __attribute__((aligned(32)))
#define BIT_ALIGN_64 __attribute__((aligned(64)))

#define ALIGNED_UINT8(N)                                                                           \
    union {                                                                                        \
        uint8_t coeffs[N];                                                                         \
        __m256i vec[(N + 31) / 32];                                                                \
    }

#define ALIGNED_INT32(N)                                                                           \
    union {                                                                                        \
        int32_t coeffs[N];                                                                         \
        __m256i vec[(N + 7) / 8];                                                                  \
    }

#define ALIGNED_INT64(N)                                                                           \
    union {                                                                                        \
        int64_t coeffs[N];                                                                         \
        __m256i vec[(N + 3) / 4];                                                                  \
    }

#endif
