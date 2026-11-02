#include "gf197_proof.h"

void qs_gf197_load_witness(
    const quicksilver_state* state, qs_gf197_bits* out, size_t bit_offset)
{
    for (size_t bit = 0; bit < BLOCK197_BITS; ++bit) {
        out->bit[bit] = quicksilver_get_witness_vec(state, bit_offset + bit);
    }
}

void qs_gf197_load_constant(
    const quicksilver_state* state, qs_gf197_bits* out,
    const uint64_t words[BLOCK197_WORDS])
{
    for (size_t bit = 0; bit < BLOCK197_BITS; ++bit) {
        const uint8_t value = (words[bit >> 6] >> (bit & 63)) & 1;
        out->bit[bit] = value
            ? quicksilver_one_gf2(state)
            : quicksilver_zero_gf2();
    }
}

void qs_gf197_add(
    const quicksilver_state* state, qs_gf197_bits* out,
    const qs_gf197_bits* x, const qs_gf197_bits* y)
{
    for (size_t bit = 0; bit < BLOCK197_BITS; ++bit) {
        out->bit[bit] = quicksilver_add_gf2(state, x->bit[bit], y->bit[bit]);
    }
}

void qs_gf197_matmul(
    const quicksilver_state* state, qs_gf197_bits* out,
    const uint64_t matrix[BLOCK197_BITS][BLOCK197_WORDS],
    const qs_gf197_bits* in)
{
    for (size_t row = 0; row < BLOCK197_BITS; ++row) {

        const uint8_t* row_bytes = (const uint8_t*)matrix[row];
        quicksilver_vec_gf2 acc = quicksilver_const_gf2(state, poly1_load(0, 0));
        for (size_t col = 0; col < BLOCK197_BITS; ++col) {
            const poly1_vec coefficient = poly1_load(row_bytes[col / 8], col % 8);
            acc = quicksilver_add_gf2(
                state, acc, quicksilver_mul_const_gf2(state, in->bit[col], coefficient));
        }
        out->bit[row] = acc;
    }
}

static inline quicksilver_vec_gf2 qs_gf197_copy_bit(
    const quicksilver_state* state, const quicksilver_vec_gf2* in)
{
    quicksilver_vec_gf2 out;
    out.mac = in->mac;
    out.value = state->verifier ? 0 : in->value;
    return out;
}

static inline quicksilver_vec_gf2 qs_gf197_xor_bits(
    const quicksilver_state* state,
    const quicksilver_vec_gf2* a, const quicksilver_vec_gf2* b)
{
    quicksilver_vec_gf2 out;
    out.mac = poly_secpar_add(a->mac, b->mac);
    out.value = state->verifier ? 0 : a->value ^ b->value;
    return out;
}

static void qs_gf197_square_noalias(const quicksilver_state* state,
    qs_gf197_bits* restrict out,const qs_gf197_bits* restrict in) {

    static const uint16_t offsets[198]={0,2,5,9,13,16,20,23,27,30,34,37,41,44,48,51,55,58,62,65,69,72,74,78,80,84,86,90,92,96,98,102,104,108,110,114,116,120,122,126,128,132,134,137,139,142,144,147,149,152,154,157,159,162,164,167,169,172,174,177,179,182,184,187,189,192,194,197,199,202,204,207,209,212,214,217,219,222,224,227,229,232,234,237,239,242,244,247,249,252,254,257,259,262,264,267,269,272,274,277,279,282,284,287,289,292,294,297,299,302,304,307,309,312,314,317,319,322,324,327,329,332,334,337,339,342,344,347,349,352,354,357,359,362,364,367,369,372,374,377,379,382,384,387,389,392,394,397,399,402,404,407,409,412,414,417,419,422,424,427,429,432,434,437,439,442,444,447,449,452,454,457,459,462,464,467,469,472,474,477,479,482,484,487,489,492,494,497,499,502,504,507,509,512,514,517,519,522};
    static const uint16_t columns[522]={0,196,99,187,196,1,99,187,196,99,100,187,188,2,100,188,100,101,188,189,3,101,189,101,102,189,190,4,102,190,102,103,190,191,5,103,191,103,104,191,192,6,104,192,104,105,192,193,7,105,193,105,106,193,194,8,106,194,106,107,194,195,9,107,195,107,108,195,196,10,108,196,108,109,11,99,109,187,109,110,12,100,110,188,110,111,13,101,111,189,111,112,14,102,112,190,112,113,15,103,113,191,113,114,16,104,114,192,114,115,17,105,115,193,115,116,18,106,116,194,116,117,19,107,117,195,117,118,20,108,118,196,118,119,21,109,119,119,120,22,110,120,120,121,23,111,121,121,122,24,112,122,122,123,25,113,123,123,124,26,114,124,124,125,27,115,125,125,126,28,116,126,126,127,29,117,127,127,128,30,118,128,128,129,31,119,129,129,130,32,120,130,130,131,33,121,131,131,132,34,122,132,132,133,35,123,133,133,134,36,124,134,134,135,37,125,135,135,136,38,126,136,136,137,39,127,137,137,138,40,128,138,138,139,41,129,139,139,140,42,130,140,140,141,43,131,141,141,142,44,132,142,142,143,45,133,143,143,144,46,134,144,144,145,47,135,145,145,146,48,136,146,146,147,49,137,147,147,148,50,138,148,148,149,51,139,149,149,150,52,140,150,150,151,53,141,151,151,152,54,142,152,152,153,55,143,153,153,154,56,144,154,154,155,57,145,155,155,156,58,146,156,156,157,59,147,157,157,158,60,148,158,158,159,61,149,159,159,160,62,150,160,160,161,63,151,161,161,162,64,152,162,162,163,65,153,163,163,164,66,154,164,164,165,67,155,165,165,166,68,156,166,166,167,69,157,167,167,168,70,158,168,168,169,71,159,169,169,170,72,160,170,170,171,73,161,171,171,172,74,162,172,172,173,75,163,173,173,174,76,164,174,174,175,77,165,175,175,176,78,166,176,176,177,79,167,177,177,178,80,168,178,178,179,81,169,179,179,180,82,170,180,180,181,83,171,181,181,182,84,172,182,182,183,85,173,183,183,184,86,174,184,184,185,87,175,185,185,186,88,176,186,186,187,89,177,187,187,188,90,178,188,188,189,91,179,189,189,190,92,180,190,190,191,93,181,191,191,192,94,182,192,192,193,95,183,193,193,194,96,184,194,194,195,97,185,195,195,196,98,186,196};
    for(size_t row=0;row<197;++row) {
        quicksilver_vec_gf2 a=quicksilver_zero_gf2();
        for(size_t k=offsets[row];k<offsets[row+1];++k)
            a=qs_gf197_xor_bits(state,&a,&in->bit[columns[k]]);
        out->bit[row]=a;
    }
}

void qs_gf197_square(
    const quicksilver_state* state, qs_gf197_bits* out, const qs_gf197_bits* in)
{
    if (out == in) {
        qs_gf197_bits tmp;
        qs_gf197_square_noalias(state, &tmp, in);
        *out = tmp;
    } else {
        qs_gf197_square_noalias(state, out, in);
    }
}

void qs_gf197_frobenius(
    const quicksilver_state* state, qs_gf197_bits* out,
    const qs_gf197_bits* in, unsigned exponent)
{
    if (exponent == 0) {
        for (size_t i = 0; i < BLOCK197_BITS; ++i)
            out->bit[i] = qs_gf197_copy_bit(state, &in->bit[i]);
        return;
    }
    if (exponent == 1) {
        qs_gf197_square(state, out, in);
        return;
    }

    qs_gf197_bits scratch[2];
    const qs_gf197_bits* current = in;
    for (unsigned round = 0; round < exponent - 1; ++round) {
        qs_gf197_bits* next = &scratch[round & 1];
        qs_gf197_square_noalias(state, next, current);
        current = next;
    }
    qs_gf197_square_noalias(state, out, current);
}

void qs_gf197_shift_reduce(
    const quicksilver_state* state, qs_gf197_bits* out,
    const qs_gf197_bits* in, size_t shift)
{
    assert(shift < BLOCK197_BITS);
    quicksilver_vec_gf2 tmp[2 * BLOCK197_BITS - 1];

    for (size_t i = 0; i < 2 * BLOCK197_BITS - 1; ++i) {
        tmp[i] = quicksilver_zero_gf2();
    }
    for (size_t bit = 0; bit < BLOCK197_BITS; ++bit) {
        tmp[bit + shift] = quicksilver_add_gf2(state, tmp[bit + shift], in->bit[bit]);
    }
    for (int pos = 2 * BLOCK197_BITS - 2; pos >= BLOCK197_BITS; --pos) {
        const quicksilver_vec_gf2 term = tmp[pos];
        tmp[pos] = quicksilver_zero_gf2();
        tmp[pos - BLOCK197_BITS + BLOCK197_REDUCTION_TERM] = quicksilver_add_gf2(
            state, tmp[pos - BLOCK197_BITS + BLOCK197_REDUCTION_TERM], term);
        tmp[pos - BLOCK197_BITS + 2] = quicksilver_add_gf2(
            state, tmp[pos - BLOCK197_BITS + 2], term);
        tmp[pos - BLOCK197_BITS + 1] = quicksilver_add_gf2(
            state, tmp[pos - BLOCK197_BITS + 1], term);
        tmp[pos - BLOCK197_BITS] = quicksilver_add_gf2(
            state, tmp[pos - BLOCK197_BITS], term);
    }
    for (size_t bit = 0; bit < BLOCK197_BITS; ++bit) {
        out->bit[bit] = tmp[bit];
    }
}

GW_STATIC_ASSERT(SECURITY_PARAM==192 && POLY_VEC_LEN==1,"GF197 projection needs scalar GF192");
GW_STATIC_ASSERT(HASHER_GFSECPAR_KEY_POWS==2,"two-coefficient lazy hash required");
typedef struct {
    poly_secpar_vec weights[2 * BLOCK197_BITS - 1];
    poly_secpar_vec r, defect, r196, r197, r198;
} qs_gf197_projection_key;

static inline poly_secpar_vec qs_gf197_mac_mul(poly_secpar_vec a, poly_secpar_vec b)
{
    return poly_2secpar_reduce_secpar(poly_secpar_mul(a, b));
}

static void qs_gf197_projection_init(qs_gf197_projection_key* key,poly_secpar_vec r) {
    key->r=r; key->weights[196]=poly_secpar_set_low32(1);
    for(int k=195;k>=0;--k) key->weights[k]=qs_gf197_mac_mul(r,key->weights[k+1]);
    for(size_t k=197;k<=392;++k) {
        key->weights[k]=poly_secpar_add(poly_secpar_add(key->weights[k-197],key->weights[k-196]),
            poly_secpar_add(key->weights[k-195],key->weights[k-176]));
    }
    key->r196=key->weights[0]; key->r197=qs_gf197_mac_mul(r,key->r196);
    key->r198=qs_gf197_mac_mul(r,key->r197);
    key->defect=poly_secpar_add(poly_secpar_set_low32(1),poly_secpar_add(key->r197,
        poly_secpar_add(key->weights[0],poly_secpar_add(key->weights[1],key->weights[20]))));
}

static poly_secpar_vec qs_gf197_project_product(const qs_gf197_projection_key* key,
    const poly_secpar_vec a[BLOCK197_BITS],const poly_secpar_vec b[BLOCK197_BITS]) {
    poly_2secpar_vec initial=poly_2secpar_from_secpar(b[0]);
    for(size_t j=1;j<197;++j)
        initial=poly_2secpar_add(initial,poly_secpar_mul(key->weights[196+j],b[j]));
    poly_secpar_vec v=poly_2secpar_reduce_secpar(initial);
    poly_2secpar_vec result=poly_secpar_mul(a[196],v);

    for(int i=195;i>=0;--i) {
        poly_secpar_vec correction=b[196-i];
        if(i>=176) correction=poly_secpar_add(correction,b[372-i]);
        if(i>=195) correction=poly_secpar_add(correction,b[391-i]);
        v=poly_2secpar_reduce_secpar(poly_2secpar_add(poly_secpar_mul(key->r,v),
            poly_secpar_mul(key->defect,correction)));
        result=poly_2secpar_add(result,poly_secpar_mul(a[i],v));
    }
    return poly_2secpar_reduce_secpar(result);
}

static void qs_gf197_hash_append(
    hasher_gfsecpar_state* state, const qs_gf197_projection_key* key,
    poly_secpar_vec contribution)
{

    const poly_secpar_vec previous = poly_2secpar_reduce_secpar(state->state);
    poly_secpar_vec next;
    if (state->pow == 0) {
        next = poly_secpar_add(qs_gf197_mac_mul(key->r196, previous), contribution);
        state->pow = -1;
    } else {
        assert(state->pow == -1 || state->pow == 1);
        const poly_secpar_vec factor = state->pow == -1 ? key->r198 : key->r196;
        next = poly_2secpar_reduce_secpar(poly_2secpar_add(
            poly_secpar_mul(factor, previous), poly_secpar_mul(key->r, contribution)));
        state->pow = 0;
    }
    state->state = poly_2secpar_from_secpar(next);
}

static void qs_gf197_hash64_append(
    hasher_gfsecpar_64_state* state, const qs_gf197_projection_key* key,
    poly_secpar_vec contribution)
{

    state->state = poly_secpar_add(qs_gf197_mac_mul(key->r197, state->state), contribution);
}

void qs_gf197_mul_constraints(
    quicksilver_state* state, const qs_gf197_bits* x,
    const qs_gf197_bits* y, const qs_gf197_bits* z, size_t count)
{
    if (count == 0)
        return;
    qs_gf197_projection_key keys[2];
    qs_gf197_projection_init(&keys[0], state->key_secpar.key_pows[0]);
    qs_gf197_projection_init(&keys[1], poly_secpar_from_64(state->key_64.key));

    for (size_t relation = 0; relation < count; ++relation) {
        poly_secpar_vec a[BLOCK197_BITS], b[BLOCK197_BITS];
        poly_secpar_vec a_plus_value[BLOCK197_BITS], b_plus_value[BLOCK197_BITS];
        for (size_t i = 0; i < BLOCK197_BITS; ++i) {
            a[i] = x[relation].bit[i].mac;
            b[i] = y[relation].bit[i].mac;
            if (!state->verifier) {
                a_plus_value[i] = poly_secpar_add(a[i], poly_secpar_from_1(x[relation].bit[i].value));
                b_plus_value[i] = poly_secpar_add(b[i], poly_secpar_from_1(y[relation].bit[i].value));
            }
        }

        for (size_t h = 0; h < 2; ++h) {
            const qs_gf197_projection_key* key = &keys[h];
            poly_secpar_vec constant = qs_gf197_project_product(key, a, b);
            poly_2secpar_vec z_sum = poly_2secpar_set_zero();
            for (size_t i = 0; i < BLOCK197_BITS; ++i) {
                poly_secpar_vec zi = z[relation].bit[i].mac;
                if (!state->verifier)
                    zi = poly_secpar_add(zi, poly_secpar_from_1(z[relation].bit[i].value));
                z_sum = poly_2secpar_add(z_sum, poly_secpar_mul(key->weights[i], zi));
            }
            const poly_secpar_vec z_projection = poly_2secpar_reduce_secpar(z_sum);
            poly_secpar_vec linear = poly_secpar_set_zero();
            if (state->verifier) {

                constant = poly_secpar_add(constant, qs_gf197_mac_mul(state->delta, z_projection));
            } else {

                linear = poly_secpar_add(constant, poly_secpar_add(z_projection,
                    qs_gf197_project_product(key, a_plus_value, b_plus_value)));
            }
            if (h == 0) {
                qs_gf197_hash_append(&state->state_secpar_const, key, constant);
                if (!state->verifier)
                    qs_gf197_hash_append(&state->state_secpar_linear, key, linear);
            } else {
                qs_gf197_hash64_append(&state->state_64_const, key, constant);
                if (!state->verifier)
                    qs_gf197_hash64_append(&state->state_64_linear, key, linear);
            }
        }
    }
}

void qs_gf197_mul_constraint(
    quicksilver_state* state, const qs_gf197_bits* x,
    const qs_gf197_bits* y, const qs_gf197_bits* z)
{
    qs_gf197_mul_constraints(state, x, y, z, 1);
}

void qs_gf197_dot_constraint(
    quicksilver_state* state, const qs_gf197_bits* y, const block197* alpha)
{
    quicksilver_vec_gf2 dot = quicksilver_zero_gf2();
    for (size_t bit = 0; bit < BLOCK197_BITS; ++bit) {
        if ((alpha->data[bit >> 6] >> (bit & 63)) & 1ULL) {
            dot = quicksilver_add_gf2(state, dot, y->bit[bit]);
        }
    }
    quicksilver_inverse_constraint(
        state, quicksilver_combine_1_bit(state, dot), quicksilver_one_gfsecpar(state));
}
