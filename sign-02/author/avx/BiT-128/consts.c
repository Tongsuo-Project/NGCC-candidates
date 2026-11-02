/*
 * Copyright (c) 2026 Hang Zhang.
 * State Key Laboratory of Cyberspace Security Defense,
 * Institute of Information Engineering, CAS
 * School of Cyber Security, University of Chinese Academy of Sciences
 */

#include <stdint.h>

#include "align.h"
#include "params.h"

#if BIT_Q != 7681 || BIT_N != 256
#error "consts.c is generated for BIT_Q=7681 and BIT_N=256"
#endif

#if BIT_L != 3
#error "the AVX2 dot3 kernel is specialized for BIT_L=3"
#endif

#if 4 * BIT_Q > 32767
#error "the 16-bit lazy NTT requires 4q <= INT16_MAX"
#endif

enum {
    QDATA_16XQ = 0,
    QDATA_16XQINV = 16,
    QDATA_16XINV_SCALE_L = 32,
    QDATA_16XINV_SCALE_H = 48,
    QDATA_16XR2_L = 64,
    QDATA_16XR2_H = 80,
    QDATA_WORDS = 96
};

typedef struct {
    int16_t coeffs[QDATA_WORDS];
} qdata_t;

#define DUP16(a) a,a,a,a,a,a,a,a,a,a,a,a,a,a,a,a

BIT_ALIGN const qdata_t qdata = {{
    /* q, q^-1, inverse scale low/high, and R^2 low/high. */
    DUP16(BIT_Q),
    DUP16(-7679),
    DUP16(256),
    DUP16(256),
    DUP16(30657),
    DUP16(5569)
}};

typedef char qdata_size_must_match[
    sizeof(qdata) == QDATA_WORDS * sizeof(int16_t) ? 1 : -1];

#undef DUP16
