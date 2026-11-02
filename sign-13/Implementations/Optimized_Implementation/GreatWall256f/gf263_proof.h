#ifndef GREATWALL_GF263_PROOF_H
#define GREATWALL_GF263_PROOF_H

#include "gf263.h"
#include "quicksilver.h"

typedef struct {
    quicksilver_vec_gf2 bit[BLOCK263_BITS];
} qs_gf263_bits;

void qs_gf263_load_witness(
    const quicksilver_state* state, qs_gf263_bits* out, size_t bit_offset);

void qs_gf263_load_constant(
    const quicksilver_state* state, qs_gf263_bits* out,
    const uint64_t words[BLOCK263_WORDS]);

void qs_gf263_add(
    const quicksilver_state* state, qs_gf263_bits* out,
    const qs_gf263_bits* x, const qs_gf263_bits* y);

void qs_gf263_matmul(
    const quicksilver_state* state, qs_gf263_bits* out,
    const uint64_t matrix[BLOCK263_BITS][BLOCK263_WORDS],
    const qs_gf263_bits* in);

void qs_gf263_square(
    const quicksilver_state* state, qs_gf263_bits* out, const qs_gf263_bits* in);

void qs_gf263_frobenius(
    const quicksilver_state* state, qs_gf263_bits* out,
    const qs_gf263_bits* in, unsigned exponent);

void qs_gf263_shift_reduce(
    const quicksilver_state* state, qs_gf263_bits* out,
    const qs_gf263_bits* in, size_t shift);

static inline quicksilver_vec_deg2 qs_gf263_mul_bits(
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

void qs_gf263_mul_constraint(
    quicksilver_state* state, const qs_gf263_bits* x,
    const qs_gf263_bits* y, const qs_gf263_bits* z);

void qs_gf263_mul_constraints(
    quicksilver_state* state, const qs_gf263_bits* x,
    const qs_gf263_bits* y, const qs_gf263_bits* z, size_t count);

void qs_gf263_dot_constraint(
    quicksilver_state* state, const qs_gf263_bits* y, const block263* alpha);

#endif
