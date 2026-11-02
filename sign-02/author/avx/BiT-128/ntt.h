/*
 * Copyright (c) 2026 Hang Zhang.
 * State Key Laboratory of Cyberspace Security Defense,
 * Institute of Information Engineering, CAS
 * School of Cyber Security, University of Chinese Academy of Sciences
 */

#ifndef BIT_NTT_H
#define BIT_NTT_H

#include <stdint.h>
#include "params.h"

/* Forward output and inverse input use packed, centered NTT order. */
void ntt_fwd_asm(int16_t a[BIT_N]);
void ntt_inv_asm(int16_t a[BIT_N]);
void ntt_pack_asm(int16_t a[BIT_N]);
void ntt_basemul_asm(int16_t r[BIT_N], const int16_t a[BIT_N], const int16_t b[BIT_N]);
void ntt_dot3_asm(int16_t r[BIT_N], const int16_t a[BIT_L][BIT_N], const int16_t b[BIT_L][BIT_N]);
void ntt_montgomery_lift_asm(int16_t r[BIT_N]);

#endif
