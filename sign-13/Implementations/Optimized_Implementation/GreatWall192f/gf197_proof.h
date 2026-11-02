#ifndef GREATWALL_GF197_PROOF_H
#define GREATWALL_GF197_PROOF_H

#include "gf197.h"
#include "quicksilver.h"

typedef struct {
    quicksilver_vec_gf2 bit[BLOCK197_BITS];
} qs_gf197_bits;

void qs_gf197_load_witness(
    const quicksilver_state* state, qs_gf197_bits* out, size_t bit_offset);

void qs_gf197_load_constant(
    const quicksilver_state* state, qs_gf197_bits* out,
    const uint64_t words[BLOCK197_WORDS]);

void qs_gf197_add(
    const quicksilver_state* state, qs_gf197_bits* out,
    const qs_gf197_bits* x, const qs_gf197_bits* y);

void qs_gf197_matmul(
    const quicksilver_state* state, qs_gf197_bits* out,
    const uint64_t matrix[BLOCK197_BITS][BLOCK197_WORDS],
    const qs_gf197_bits* in);

void qs_gf197_square(
    const quicksilver_state* state, qs_gf197_bits* out, const qs_gf197_bits* in);

void qs_gf197_frobenius(
    const quicksilver_state* state, qs_gf197_bits* out,
    const qs_gf197_bits* in, unsigned exponent);

void qs_gf197_shift_reduce(
    const quicksilver_state* state, qs_gf197_bits* out,
    const qs_gf197_bits* in, size_t shift);

static inline quicksilver_vec_deg2 qs_gf197_mul_bits(
    const quicksilver_state* state, quicksilver_vec_gf2 x, quicksilver_vec_gf2 y)
{
    quicksilver_vec_deg2 out = quicksilver_zero_deg2();
    out.mac0 = poly_secpar_mul(x.mac, y.mac);
    if (!state->verifier) {
        poly_secpar_vec correction = poly_secpar_add(
            poly1xsecpar_mul(x.value, y.mac), poly1xsecpar_mul(y.value, x.mac));
        correction = poly_secpar_add(correction, poly_secpar_from_1(x.value & y.value));
        out.mac1 = poly_2secpar_add(out.mac0, poly_2secpar_from_secpar(correction));
    }
    return out;
}

void qs_gf197_mul_constraint(
    quicksilver_state* state, const qs_gf197_bits* x,
    const qs_gf197_bits* y, const qs_gf197_bits* z);

void qs_gf197_mul_constraints(
    quicksilver_state* state, const qs_gf197_bits* x,
    const qs_gf197_bits* y, const qs_gf197_bits* z, size_t count);

void qs_gf197_dot_constraint(
    quicksilver_state* state, const qs_gf197_bits* y, const block197* alpha);

#endif
