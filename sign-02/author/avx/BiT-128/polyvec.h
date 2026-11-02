/*
 * Copyright (c) 2026 Hang Zhang.
 * State Key Laboratory of Cyberspace Security Defense,
 * Institute of Information Engineering, CAS
 * School of Cyber Security, University of Chinese Academy of Sciences
 */
#ifndef POLYVEC_H
#define POLYVEC_H

#include <stddef.h>
#include <stdint.h>
#include "params.h"
#include "poly.h"

typedef struct {
    poly vec[BIT_K];
} polyveck;

typedef struct {
    poly vec[BIT_L];
} polyvecl;

typedef struct {
    poly vec[BIT_L + 1];
} polyvecm1;

typedef struct {
    poly vec[BIT_L + BIT_K + 1];
} polyvecy;

typedef struct {
    poly_ntt vec[BIT_K];
} polyveck_ntt;

typedef struct {
    poly_ntt vec[BIT_L];
} polyvecl_ntt;

typedef struct {
    polyvecl_ntt vec[BIT_K];
} poly_matrix_ntt;

/* PSPM precomputation for the six small secret polynomials s_0 || e.
 * Each 64-bit word contains six independent base-2^7 digits. */
typedef struct {
    /* Negative words followed by positive words, in one array object. */
    uint64_t words[2 * BIT_N];
} BIT_ALIGN bit_pspm_se_precomp;

int poly_matrix_expand_ntt(poly_matrix_ntt *A_ntt, const unsigned char *seed);
int polyvec_sample_S1_keypair(polyvecl *s, polyveck *e, const uint8_t *seed, uint16_t *nonce);

void poly_matrix_mul_vector_ntt(polyveck *result, const poly_matrix_ntt *A_ntt,
                                const polyvecl_ntt *v_ntt);

void polyvecl_to_ntt(polyvecl_ntt *result, const polyvecl *v);
void polyvecy_y1_to_ntt(polyvecl_ntt *result, const polyvecy *y);
/* Destructive conversion: v_ntt is consumed. */
void polyveck_from_ntt(polyveck *result, polyveck_ntt *v_ntt);

void polyveck_add_small(polyveck *result, const polyveck *a, const polyveck *small);
void polyveck_canonicalize(polyveck *b);
void polyveck_public_to_ntt(polyveck_ntt *public_ntt, const polyveck *public_vector);

void polyvec_prepare_challenge_mul_se(bit_pspm_se_precomp *table, const polyvecl *s,
                                      const polyveck *e);
void polyvec_mul_challenge_no_reduce_se_pspm(polyvecl *c_s, polyveck *c_e,
                                             const bit_pspm_se_precomp *table,
                                             const sparse_challenge *c);

void polyveck_compute_w_ntt(polyveck *w, const poly_matrix_ntt *A_0_ntt, const polyveck_ntt *b_ntt,
                            const polyvecl_ntt *y1_ntt, const poly_ntt *y0_ntt, const polyvecy *y);
void polyveck_highbits(polyveck *w1, const polyveck *w);
void polyveck_pack_w1(unsigned char *packed, const polyveck *w1);
void polyveck_make_hint(polyveck *h, const polyveck *w1, const polyveck *w,
                          const polyveck *z2, const poly *c, uint8_t b);

void polyveck_compute_w_hat_prime_ntt(polyveck *w_hat_prime, const poly_matrix_ntt *A_0_ntt,
                                      const polyveck_ntt *public_ntt, const polyvecl_ntt *z1_tail_ntt,
                                      const poly_ntt *z0_ntt, const poly *z0, const poly *c_poly);
void polyveck_compute_w_prime(polyveck *w_prime, const polyveck *w_hat_prime, const polyveck *h);
#endif
