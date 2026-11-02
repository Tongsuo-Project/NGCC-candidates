#include "config.h"
#include "faest_details.h"
#include "owf_proof.h"
#include "owf_alpha.h"
#include "gf163_proof.h"
#include "gf163_frobenius_tables.h"
#include "timing.h"
#include <stdlib.h>
#ifndef GREATWALL_GF163_POW_SQUARE
#define GREATWALL_GF163_POW_SQUARE 1
#endif
#define S_ENC OWF_ROUNDS
static ALWAYS_INLINE void enc_constraints(quicksilver_state* state, owf_block out, owf_block owf_iv) {

    if (!greatwall160_constants_available())
        abort();
    ENC_TIMING_DECLARE();
    ENC_TIMING_START();
    owf_block owf_alpha0 = {{0}};
    owf_block owf_alpha1 = {{0}};
    (void)derive_owf_alpha(&owf_alpha0, &owf_iv, 0);
    (void)derive_owf_alpha(&owf_alpha1, &owf_iv, 1);
    ENC_TIMING_PRINT_PROVER(state, "enc_constraints/derive_alpha");

        qs_gf163_bits inv_inputs[S_ENC];
        qs_gf163_bits inv_outputs[S_ENC];
        qs_gf163_bits pow_outputs[S_ENC];

        ENC_TIMING_START();
        qs_gf163_bits sk_bits;
        qs_gf163_load_witness(state, &sk_bits, 0);
        qs_gf163_load_witness(state, &inv_outputs[0], 168);

        qs_gf163_bits rc0_bits, rc1_bits, owf_iv_bits;

        qs_gf163_load_constant(state, &rc0_bits, greatwall_rc_163[0]);
        qs_gf163_load_constant(state, &rc1_bits, greatwall_rc_163[1]);
        qs_gf163_load_constant(state, &owf_iv_bits, owf_iv.data);
        ENC_TIMING_PRINT_PROVER(state, "enc_constraints/load_witness_and_constants");

        ENC_TIMING_START();
        qs_gf163_bits matrix0_output, matrix1_output, matrix2_output;
        qs_gf163_matmul(state, &matrix0_output, greatwall_mat_163_0, &sk_bits);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/linear_matrix0");
        ENC_TIMING_START();
        qs_gf163_add(state, &inv_inputs[0], &matrix0_output, &rc0_bits);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/build_inv_input0");

        ENC_TIMING_START();
#if GREATWALL_GF163_POW_SQUARE
        qs_gf163_frobenius(state, &pow_outputs[0], &inv_outputs[0], 79);
#else
        qs_gf163_matmul(state, &pow_outputs[0], greatwall_pow_mat_163_79, &inv_outputs[0]);
#endif
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/power_matrix0");

        ENC_TIMING_START();
        qs_gf163_bits tmp;
        qs_gf163_add(state, &tmp, &inv_outputs[0], &sk_bits);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/mix_inv_output0_with_key");
        ENC_TIMING_START();
        qs_gf163_matmul(state, &matrix1_output, greatwall_mat_163_1, &tmp);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/linear_matrix1");
        ENC_TIMING_START();
        qs_gf163_add(state, &inv_inputs[1], &matrix1_output, &owf_iv_bits);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/build_inv_input1");

        ENC_TIMING_START();
        qs_gf163_bits out_bits;
        qs_gf163_load_constant(state, &out_bits, out.data);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/load_public_output");
        ENC_TIMING_START();
        qs_gf163_add(state, &matrix2_output, &sk_bits, &out_bits);
        qs_gf163_add(state, &matrix2_output, &rc1_bits, &matrix2_output);
        qs_gf163_add(state, &matrix2_output, &inv_inputs[1], &matrix2_output);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/build_inverse_matrix2_input");
        ENC_TIMING_START();
        qs_gf163_matmul(state, &inv_outputs[1], greatwall_mat_163_2_inv, &matrix2_output);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/inverse_linear_matrix2");

        ENC_TIMING_START();
#if GREATWALL_GF163_POW_SQUARE
        qs_gf163_frobenius(state, &pow_outputs[1], &inv_outputs[1], 86);
#else
        qs_gf163_matmul(state, &pow_outputs[1], greatwall_pow_mat_163_86, &inv_outputs[1]);
#endif
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/power_matrix1");

        ENC_TIMING_START();
        qs_gf163_mul_constraints(state, inv_inputs, inv_outputs, pow_outputs, S_ENC);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/mul_constraints");

        ENC_TIMING_START();
        qs_gf163_dot_constraint(state, &inv_outputs[0], &owf_alpha0);
        qs_gf163_dot_constraint(state, &inv_outputs[1], &owf_alpha1);
        ENC_TIMING_PRINT_PROVER(state, "greatwall_constraints/dot_constraints");

}

static void owf_constraints(quicksilver_state* state, const public_key* pk)
{
    enc_constraints(state, pk->owf_output, pk->owf_iv);
}

void owf_constraints_prover(quicksilver_state* state, const public_key* pk)
{
	assert(!state->verifier);
	state->verifier = false;
	owf_constraints(state, pk);
}

void owf_constraints_verifier(quicksilver_state* state, const public_key* pk)
{
	assert(state->verifier);
	state->verifier = true;
	owf_constraints(state, pk);
}
