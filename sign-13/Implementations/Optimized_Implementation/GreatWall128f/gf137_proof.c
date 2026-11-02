#include "gf137_proof.h"

void qs_gf137_load_witness(
    const quicksilver_state* state, qs_gf137_bits* out, size_t bit_offset)
{
    for (size_t bit = 0; bit < BLOCK137_BITS; ++bit) {
        out->bit[bit] = quicksilver_get_witness_vec(state, bit_offset + bit);
    }
}

void qs_gf137_load_constant(
    const quicksilver_state* state, qs_gf137_bits* out,
    const uint64_t words[BLOCK137_WORDS])
{
    for (size_t bit = 0; bit < BLOCK137_BITS; ++bit) {
        const uint8_t value = (words[bit >> 6] >> (bit & 63)) & 1;
        out->bit[bit] = value
            ? quicksilver_one_gf2(state)
            : quicksilver_zero_gf2();
    }
}

void qs_gf137_add(
    const quicksilver_state* state, qs_gf137_bits* out,
    const qs_gf137_bits* x, const qs_gf137_bits* y)
{
    for (size_t bit = 0; bit < BLOCK137_BITS; ++bit) {
        out->bit[bit] = quicksilver_add_gf2(state, x->bit[bit], y->bit[bit]);
    }
}

void qs_gf137_matmul(
    const quicksilver_state* state, qs_gf137_bits* out,
    const uint64_t matrix[BLOCK137_BITS][BLOCK137_WORDS],
    const qs_gf137_bits* in)
{
    for (size_t row = 0; row < BLOCK137_BITS; ++row) {

        const uint8_t* row_bytes = (const uint8_t*)matrix[row];
        quicksilver_vec_gf2 acc = quicksilver_const_gf2(state, poly1_load(0, 0));
        for (size_t col = 0; col < BLOCK137_BITS; ++col) {
            const poly1_vec coefficient = poly1_load(row_bytes[col / 8], col % 8);
            acc = quicksilver_add_gf2(
                state, acc, quicksilver_mul_const_gf2(state, in->bit[col], coefficient));
        }
        out->bit[row] = acc;
    }
}

static inline quicksilver_vec_gf2 qs_gf137_copy_bit(
    const quicksilver_state* state, const quicksilver_vec_gf2* in)
{
    quicksilver_vec_gf2 out;
    out.mac = in->mac;
    out.value = state->verifier ? 0 : in->value;
    return out;
}

static inline quicksilver_vec_gf2 qs_gf137_xor_bits(
    const quicksilver_state* state,
    const quicksilver_vec_gf2* a, const quicksilver_vec_gf2* b)
{
    quicksilver_vec_gf2 out;
    out.mac = poly_secpar_add(a->mac, b->mac);
    out.value = state->verifier ? 0 : a->value ^ b->value;
    return out;
}

static void qs_gf137_square_noalias(
    const quicksilver_state* state, qs_gf137_bits* restrict out,
    const qs_gf137_bits* restrict in)
{

    for (size_t k = 0; k <= 10; ++k)
        out->bit[2 * k] = qs_gf137_copy_bit(state, &in->bit[k]);
    for (size_t k = 11; k <= 20; ++k) {
        const quicksilver_vec_gf2 pair = qs_gf137_xor_bits(
            state, &in->bit[k], &in->bit[k + 58]);
        out->bit[2 * k] = qs_gf137_xor_bits(state, &pair, &in->bit[k + 116]);
    }
    for (size_t k = 21; k <= 68; ++k)
        out->bit[2 * k] = qs_gf137_xor_bits(state, &in->bit[k], &in->bit[k + 58]);
    for (size_t k = 0; k <= 9; ++k)
        out->bit[2 * k + 1] = qs_gf137_xor_bits(
            state, &in->bit[k + 69], &in->bit[k + 127]);
    for (size_t k = 10; k <= 67; ++k)
        out->bit[2 * k + 1] = qs_gf137_copy_bit(state, &in->bit[k + 69]);
}

void qs_gf137_square(
    const quicksilver_state* state, qs_gf137_bits* out, const qs_gf137_bits* in)
{
    if (out == in) {
        qs_gf137_bits tmp;
        qs_gf137_square_noalias(state, &tmp, in);
        *out = tmp;
    } else {
        qs_gf137_square_noalias(state, out, in);
    }
}

void qs_gf137_frobenius(
    const quicksilver_state* state, qs_gf137_bits* out,
    const qs_gf137_bits* in, unsigned exponent)
{
    if (exponent == 0) {
        for (size_t i = 0; i < BLOCK137_BITS; ++i)
            out->bit[i] = qs_gf137_copy_bit(state, &in->bit[i]);
        return;
    }
    if (exponent == 1) {
        qs_gf137_square(state, out, in);
        return;
    }

    qs_gf137_bits scratch[2];
    const qs_gf137_bits* current = in;
    for (unsigned round = 0; round < exponent - 1; ++round) {
        qs_gf137_bits* next = &scratch[round & 1];
        qs_gf137_square_noalias(state, next, current);
        current = next;
    }
    qs_gf137_square_noalias(state, out, current);
}

void qs_gf137_shift_reduce(
    const quicksilver_state* state, qs_gf137_bits* out,
    const qs_gf137_bits* in, size_t shift)
{
    assert(shift < BLOCK137_BITS);
    quicksilver_vec_gf2 tmp[2 * BLOCK137_BITS - 1];

    for (size_t i = 0; i < 2 * BLOCK137_BITS - 1; ++i) {
        tmp[i] = quicksilver_zero_gf2();
    }
    for (size_t bit = 0; bit < BLOCK137_BITS; ++bit) {
        tmp[bit + shift] = quicksilver_add_gf2(state, tmp[bit + shift], in->bit[bit]);
    }
    for (int pos = 2 * BLOCK137_BITS - 2; pos >= BLOCK137_BITS; --pos) {
        const quicksilver_vec_gf2 term = tmp[pos];
        tmp[pos] = quicksilver_zero_gf2();
        tmp[pos - BLOCK137_BITS + BLOCK137_REDUCTION_TERM] = quicksilver_add_gf2(
            state, tmp[pos - BLOCK137_BITS + BLOCK137_REDUCTION_TERM], term);
        tmp[pos - BLOCK137_BITS] = quicksilver_add_gf2(
            state, tmp[pos - BLOCK137_BITS], term);
    }
    for (size_t bit = 0; bit < BLOCK137_BITS; ++bit) {
        out->bit[bit] = tmp[bit];
    }
}

GW_STATIC_ASSERT(SECURITY_PARAM == 128 && POLY_VEC_LEN == 1,
               "GF137 hash fusion requires scalar GF128 authentication");
GW_STATIC_ASSERT(BLOCK137_BITS == 137 && BLOCK137_REDUCTION_TERM == 21,
               "GF137 projection recurrence assumes t^137 + t^21 + 1");
GW_STATIC_ASSERT(HASHER_GFSECPAR_KEY_POWS == 2,
               "GF137 block append assumes two-coefficient hash groups");

typedef struct {
    poly_secpar_vec weights[2 * BLOCK137_BITS - 1];
    poly_secpar_vec r, defect, r136, r137, r138;
} qs_gf137_projection_key;

static inline poly_secpar_vec qs_gf137_mac_mul(poly_secpar_vec a, poly_secpar_vec b)
{
    return poly_2secpar_reduce_secpar(poly_secpar_mul(a, b));
}

static void qs_gf137_projection_init(qs_gf137_projection_key* key, poly_secpar_vec r)
{

    key->r = r;
    key->weights[136] = poly_secpar_set_low32(1);
    for (int k = 135; k >= 0; --k)
        key->weights[k] = qs_gf137_mac_mul(r, key->weights[k + 1]);
    for (size_t k = 137; k <= 272; ++k)
        key->weights[k] = poly_secpar_add(key->weights[k - 137], key->weights[k - 116]);
    key->r136 = key->weights[0];
    key->r137 = qs_gf137_mac_mul(r, key->r136);
    key->r138 = qs_gf137_mac_mul(r, key->r137);
    key->defect = poly_secpar_add(poly_secpar_set_low32(1),
        poly_secpar_add(key->r137, key->weights[20]));
}

static poly_secpar_vec qs_gf137_project_product(
    const qs_gf137_projection_key* key,
    const poly_secpar_vec a[BLOCK137_BITS], const poly_secpar_vec b[BLOCK137_BITS])
{

    poly_2secpar_vec initial = poly_2secpar_from_secpar(b[0]);
    for (size_t j = 1; j < BLOCK137_BITS; ++j)
        initial = poly_2secpar_add(initial, poly_secpar_mul(key->weights[136 + j], b[j]));
    poly_secpar_vec v = poly_2secpar_reduce_secpar(initial);
    poly_2secpar_vec result = poly_secpar_mul(a[136], v);

    for (int i = 135; i >= 0; --i) {
        poly_secpar_vec correction = b[136 - i];
        if (i >= 116)
            correction = poly_secpar_add(correction, b[252 - i]);
        v = poly_2secpar_reduce_secpar(poly_2secpar_add(
            poly_secpar_mul(key->r, v), poly_secpar_mul(key->defect, correction)));
        result = poly_2secpar_add(result, poly_secpar_mul(a[i], v));
    }
    return poly_2secpar_reduce_secpar(result);
}

static void qs_gf137_hash_append(
    hasher_gfsecpar_state* state, const qs_gf137_projection_key* key,
    poly_secpar_vec contribution)
{

    const poly_secpar_vec previous = poly_2secpar_reduce_secpar(state->state);
    poly_secpar_vec next;
    if (state->pow == 0) {
        next = poly_secpar_add(qs_gf137_mac_mul(key->r136, previous), contribution);
        state->pow = -1;
    } else {
        assert(state->pow == -1 || state->pow == 1);
        const poly_secpar_vec factor = state->pow == -1 ? key->r138 : key->r136;
        next = poly_2secpar_reduce_secpar(poly_2secpar_add(
            poly_secpar_mul(factor, previous), poly_secpar_mul(key->r, contribution)));
        state->pow = 0;
    }
    state->state = poly_2secpar_from_secpar(next);
}

static void qs_gf137_hash64_append(
    hasher_gfsecpar_64_state* state, const qs_gf137_projection_key* key,
    poly_secpar_vec contribution)
{

    state->state = poly_secpar_add(qs_gf137_mac_mul(key->r137, state->state), contribution);
}

void qs_gf137_mul_constraints(
    quicksilver_state* state, const qs_gf137_bits* x,
    const qs_gf137_bits* y, const qs_gf137_bits* z, size_t count)
{
    if (count == 0)
        return;
    qs_gf137_projection_key keys[2];
    qs_gf137_projection_init(&keys[0], state->key_secpar.key_pows[0]);
    qs_gf137_projection_init(&keys[1], poly_secpar_from_64(state->key_64.key));

    for (size_t relation = 0; relation < count; ++relation) {
        poly_secpar_vec a[BLOCK137_BITS], b[BLOCK137_BITS];
        poly_secpar_vec a_plus_value[BLOCK137_BITS], b_plus_value[BLOCK137_BITS];
        for (size_t i = 0; i < BLOCK137_BITS; ++i) {
            a[i] = x[relation].bit[i].mac;
            b[i] = y[relation].bit[i].mac;
            if (!state->verifier) {
                a_plus_value[i] = poly_secpar_add(a[i], poly_secpar_from_1(x[relation].bit[i].value));
                b_plus_value[i] = poly_secpar_add(b[i], poly_secpar_from_1(y[relation].bit[i].value));
            }
        }

        for (size_t h = 0; h < 2; ++h) {
            const qs_gf137_projection_key* key = &keys[h];
            poly_secpar_vec constant = qs_gf137_project_product(key, a, b);
            poly_2secpar_vec z_sum = poly_2secpar_set_zero();
            for (size_t i = 0; i < BLOCK137_BITS; ++i) {
                poly_secpar_vec zi = z[relation].bit[i].mac;
                if (!state->verifier)
                    zi = poly_secpar_add(zi, poly_secpar_from_1(z[relation].bit[i].value));
                z_sum = poly_2secpar_add(z_sum, poly_secpar_mul(key->weights[i], zi));
            }
            const poly_secpar_vec z_projection = poly_2secpar_reduce_secpar(z_sum);
            poly_secpar_vec linear = poly_secpar_set_zero();
            if (state->verifier) {

                constant = poly_secpar_add(constant, qs_gf137_mac_mul(state->delta, z_projection));
            } else {

                linear = poly_secpar_add(constant, poly_secpar_add(z_projection,
                    qs_gf137_project_product(key, a_plus_value, b_plus_value)));
            }
            if (h == 0) {
                qs_gf137_hash_append(&state->state_secpar_const, key, constant);
                if (!state->verifier)
                    qs_gf137_hash_append(&state->state_secpar_linear, key, linear);
            } else {
                qs_gf137_hash64_append(&state->state_64_const, key, constant);
                if (!state->verifier)
                    qs_gf137_hash64_append(&state->state_64_linear, key, linear);
            }
        }
    }
}

void qs_gf137_mul_constraint(
    quicksilver_state* state, const qs_gf137_bits* x,
    const qs_gf137_bits* y, const qs_gf137_bits* z)
{
    qs_gf137_mul_constraints(state, x, y, z, 1);
}

void qs_gf137_dot_constraint(
    quicksilver_state* state, const qs_gf137_bits* y, const block137* alpha)
{
    quicksilver_vec_gf2 dot = quicksilver_zero_gf2();
    for (size_t bit = 0; bit < BLOCK137_BITS; ++bit) {
        if ((alpha->data[bit >> 6] >> (bit & 63)) & 1ULL) {
            dot = quicksilver_add_gf2(state, dot, y->bit[bit]);
        }
    }
    quicksilver_inverse_constraint(
        state, quicksilver_combine_1_bit(state, dot), quicksilver_one_gfsecpar(state));
}
