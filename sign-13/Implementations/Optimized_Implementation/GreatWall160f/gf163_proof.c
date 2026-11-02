#include "gf163_proof.h"

void qs_gf163_load_witness(
    const quicksilver_state* state, qs_gf163_bits* out, size_t bit_offset)
{
    for (size_t bit = 0; bit < BLOCK163_BITS; ++bit) {
        out->bit[bit] = quicksilver_get_witness_vec(state, bit_offset + bit);
    }
}

void qs_gf163_load_constant(
    const quicksilver_state* state, qs_gf163_bits* out,
    const uint64_t words[BLOCK163_WORDS])
{
    for (size_t bit = 0; bit < BLOCK163_BITS; ++bit) {
        const uint8_t value = (words[bit >> 6] >> (bit & 63)) & 1;
        out->bit[bit] = value
            ? quicksilver_one_gf2(state)
            : quicksilver_zero_gf2();
    }
}

void qs_gf163_add(
    const quicksilver_state* state, qs_gf163_bits* out,
    const qs_gf163_bits* x, const qs_gf163_bits* y)
{
    for (size_t bit = 0; bit < BLOCK163_BITS; ++bit) {
        out->bit[bit] = quicksilver_add_gf2(state, x->bit[bit], y->bit[bit]);
    }
}

void qs_gf163_matmul(
    const quicksilver_state* state, qs_gf163_bits* out,
    const uint64_t matrix[BLOCK163_BITS][BLOCK163_WORDS],
    const qs_gf163_bits* in)
{
    for (size_t row = 0; row < BLOCK163_BITS; ++row) {

        const uint8_t* row_bytes = (const uint8_t*)matrix[row];
        quicksilver_vec_gf2 acc = quicksilver_const_gf2(state, poly1_load(0, 0));
        for (size_t col = 0; col < BLOCK163_BITS; ++col) {
            const poly1_vec coefficient = poly1_load(row_bytes[col / 8], col % 8);
            acc = quicksilver_add_gf2(
                state, acc, quicksilver_mul_const_gf2(state, in->bit[col], coefficient));
        }
        out->bit[row] = acc;
    }
}

static inline quicksilver_vec_gf2 qs_gf163_copy_bit(
    const quicksilver_state* state, const quicksilver_vec_gf2* in)
{
    quicksilver_vec_gf2 out;
    out.mac = in->mac;
    out.value = state->verifier ? 0 : in->value;
    return out;
}

static inline quicksilver_vec_gf2 qs_gf163_xor_bits(
    const quicksilver_state* state,
    const quicksilver_vec_gf2* a, const quicksilver_vec_gf2* b)
{
    quicksilver_vec_gf2 out;
    out.mac = poly_secpar_add(a->mac, b->mac);
    out.value = state->verifier ? 0 : a->value ^ b->value;
    return out;
}

static void qs_gf163_square_noalias(
    const quicksilver_state* state, qs_gf163_bits* restrict out,
    const qs_gf163_bits* restrict in)
{

    out->bit[0] = qs_gf163_xor_bits(state, &in->bit[0], &in->bit[160]);
    out->bit[1] = qs_gf163_xor_bits(state, &in->bit[82], &in->bit[160]);
    out->bit[1] = qs_gf163_xor_bits(state, &out->bit[1], &in->bit[162]);
    out->bit[2] = qs_gf163_xor_bits(state, &in->bit[1], &in->bit[161]);
    out->bit[3] = qs_gf163_xor_bits(state, &in->bit[83], &in->bit[160]);
    out->bit[3] = qs_gf163_xor_bits(state, &out->bit[3], &in->bit[161]);
    out->bit[4] = qs_gf163_xor_bits(state, &in->bit[2], &in->bit[82]);
    out->bit[4] = qs_gf163_xor_bits(state, &out->bit[4], &in->bit[160]);
    out->bit[5] = qs_gf163_xor_bits(state, &in->bit[84], &in->bit[161]);
    out->bit[5] = qs_gf163_xor_bits(state, &out->bit[5], &in->bit[162]);
    out->bit[6] = qs_gf163_xor_bits(state, &in->bit[3], &in->bit[83]);
    out->bit[6] = qs_gf163_xor_bits(state, &out->bit[6], &in->bit[160]);
    out->bit[6] = qs_gf163_xor_bits(state, &out->bit[6], &in->bit[161]);
    out->bit[7] = qs_gf163_xor_bits(state, &in->bit[82], &in->bit[85]);
    out->bit[8] = qs_gf163_xor_bits(state, &in->bit[4], &in->bit[82]);
    out->bit[8] = qs_gf163_xor_bits(state, &out->bit[8], &in->bit[84]);
    out->bit[8] = qs_gf163_xor_bits(state, &out->bit[8], &in->bit[160]);
    out->bit[8] = qs_gf163_xor_bits(state, &out->bit[8], &in->bit[161]);
    out->bit[9] = qs_gf163_xor_bits(state, &in->bit[83], &in->bit[86]);
    out->bit[10] = qs_gf163_xor_bits(state, &in->bit[5], &in->bit[83]);
    out->bit[10] = qs_gf163_xor_bits(state, &out->bit[10], &in->bit[85]);
    out->bit[10] = qs_gf163_xor_bits(state, &out->bit[10], &in->bit[161]);
    out->bit[10] = qs_gf163_xor_bits(state, &out->bit[10], &in->bit[162]);
    out->bit[11] = qs_gf163_xor_bits(state, &in->bit[84], &in->bit[87]);
    out->bit[12] = qs_gf163_xor_bits(state, &in->bit[6], &in->bit[84]);
    out->bit[12] = qs_gf163_xor_bits(state, &out->bit[12], &in->bit[86]);
    out->bit[12] = qs_gf163_xor_bits(state, &out->bit[12], &in->bit[162]);
    out->bit[13] = qs_gf163_xor_bits(state, &in->bit[85], &in->bit[88]);
    out->bit[14] = qs_gf163_xor_bits(state, &in->bit[7], &in->bit[85]);
    out->bit[14] = qs_gf163_xor_bits(state, &out->bit[14], &in->bit[87]);
    out->bit[15] = qs_gf163_xor_bits(state, &in->bit[86], &in->bit[89]);
    out->bit[16] = qs_gf163_xor_bits(state, &in->bit[8], &in->bit[86]);
    out->bit[16] = qs_gf163_xor_bits(state, &out->bit[16], &in->bit[88]);
    out->bit[17] = qs_gf163_xor_bits(state, &in->bit[87], &in->bit[90]);
    out->bit[18] = qs_gf163_xor_bits(state, &in->bit[9], &in->bit[87]);
    out->bit[18] = qs_gf163_xor_bits(state, &out->bit[18], &in->bit[89]);
    out->bit[19] = qs_gf163_xor_bits(state, &in->bit[88], &in->bit[91]);
    out->bit[20] = qs_gf163_xor_bits(state, &in->bit[10], &in->bit[88]);
    out->bit[20] = qs_gf163_xor_bits(state, &out->bit[20], &in->bit[90]);
    out->bit[21] = qs_gf163_xor_bits(state, &in->bit[89], &in->bit[92]);
    out->bit[22] = qs_gf163_xor_bits(state, &in->bit[11], &in->bit[89]);
    out->bit[22] = qs_gf163_xor_bits(state, &out->bit[22], &in->bit[91]);
    out->bit[23] = qs_gf163_xor_bits(state, &in->bit[90], &in->bit[93]);
    out->bit[24] = qs_gf163_xor_bits(state, &in->bit[12], &in->bit[90]);
    out->bit[24] = qs_gf163_xor_bits(state, &out->bit[24], &in->bit[92]);
    out->bit[25] = qs_gf163_xor_bits(state, &in->bit[91], &in->bit[94]);
    out->bit[26] = qs_gf163_xor_bits(state, &in->bit[13], &in->bit[91]);
    out->bit[26] = qs_gf163_xor_bits(state, &out->bit[26], &in->bit[93]);
    out->bit[27] = qs_gf163_xor_bits(state, &in->bit[92], &in->bit[95]);
    out->bit[28] = qs_gf163_xor_bits(state, &in->bit[14], &in->bit[92]);
    out->bit[28] = qs_gf163_xor_bits(state, &out->bit[28], &in->bit[94]);
    out->bit[29] = qs_gf163_xor_bits(state, &in->bit[93], &in->bit[96]);
    out->bit[30] = qs_gf163_xor_bits(state, &in->bit[15], &in->bit[93]);
    out->bit[30] = qs_gf163_xor_bits(state, &out->bit[30], &in->bit[95]);
    out->bit[31] = qs_gf163_xor_bits(state, &in->bit[94], &in->bit[97]);
    out->bit[32] = qs_gf163_xor_bits(state, &in->bit[16], &in->bit[94]);
    out->bit[32] = qs_gf163_xor_bits(state, &out->bit[32], &in->bit[96]);
    out->bit[33] = qs_gf163_xor_bits(state, &in->bit[95], &in->bit[98]);
    out->bit[34] = qs_gf163_xor_bits(state, &in->bit[17], &in->bit[95]);
    out->bit[34] = qs_gf163_xor_bits(state, &out->bit[34], &in->bit[97]);
    out->bit[35] = qs_gf163_xor_bits(state, &in->bit[96], &in->bit[99]);
    out->bit[36] = qs_gf163_xor_bits(state, &in->bit[18], &in->bit[96]);
    out->bit[36] = qs_gf163_xor_bits(state, &out->bit[36], &in->bit[98]);
    out->bit[37] = qs_gf163_xor_bits(state, &in->bit[97], &in->bit[100]);
    out->bit[38] = qs_gf163_xor_bits(state, &in->bit[19], &in->bit[97]);
    out->bit[38] = qs_gf163_xor_bits(state, &out->bit[38], &in->bit[99]);
    out->bit[39] = qs_gf163_xor_bits(state, &in->bit[98], &in->bit[101]);
    out->bit[40] = qs_gf163_xor_bits(state, &in->bit[20], &in->bit[98]);
    out->bit[40] = qs_gf163_xor_bits(state, &out->bit[40], &in->bit[100]);
    out->bit[41] = qs_gf163_xor_bits(state, &in->bit[99], &in->bit[102]);
    out->bit[42] = qs_gf163_xor_bits(state, &in->bit[21], &in->bit[99]);
    out->bit[42] = qs_gf163_xor_bits(state, &out->bit[42], &in->bit[101]);
    out->bit[43] = qs_gf163_xor_bits(state, &in->bit[100], &in->bit[103]);
    out->bit[44] = qs_gf163_xor_bits(state, &in->bit[22], &in->bit[100]);
    out->bit[44] = qs_gf163_xor_bits(state, &out->bit[44], &in->bit[102]);
    out->bit[45] = qs_gf163_xor_bits(state, &in->bit[101], &in->bit[104]);
    out->bit[46] = qs_gf163_xor_bits(state, &in->bit[23], &in->bit[101]);
    out->bit[46] = qs_gf163_xor_bits(state, &out->bit[46], &in->bit[103]);
    out->bit[47] = qs_gf163_xor_bits(state, &in->bit[102], &in->bit[105]);
    out->bit[48] = qs_gf163_xor_bits(state, &in->bit[24], &in->bit[102]);
    out->bit[48] = qs_gf163_xor_bits(state, &out->bit[48], &in->bit[104]);
    out->bit[49] = qs_gf163_xor_bits(state, &in->bit[103], &in->bit[106]);
    out->bit[50] = qs_gf163_xor_bits(state, &in->bit[25], &in->bit[103]);
    out->bit[50] = qs_gf163_xor_bits(state, &out->bit[50], &in->bit[105]);
    out->bit[51] = qs_gf163_xor_bits(state, &in->bit[104], &in->bit[107]);
    out->bit[52] = qs_gf163_xor_bits(state, &in->bit[26], &in->bit[104]);
    out->bit[52] = qs_gf163_xor_bits(state, &out->bit[52], &in->bit[106]);
    out->bit[53] = qs_gf163_xor_bits(state, &in->bit[105], &in->bit[108]);
    out->bit[54] = qs_gf163_xor_bits(state, &in->bit[27], &in->bit[105]);
    out->bit[54] = qs_gf163_xor_bits(state, &out->bit[54], &in->bit[107]);
    out->bit[55] = qs_gf163_xor_bits(state, &in->bit[106], &in->bit[109]);
    out->bit[56] = qs_gf163_xor_bits(state, &in->bit[28], &in->bit[106]);
    out->bit[56] = qs_gf163_xor_bits(state, &out->bit[56], &in->bit[108]);
    out->bit[57] = qs_gf163_xor_bits(state, &in->bit[107], &in->bit[110]);
    out->bit[58] = qs_gf163_xor_bits(state, &in->bit[29], &in->bit[107]);
    out->bit[58] = qs_gf163_xor_bits(state, &out->bit[58], &in->bit[109]);
    out->bit[59] = qs_gf163_xor_bits(state, &in->bit[108], &in->bit[111]);
    out->bit[60] = qs_gf163_xor_bits(state, &in->bit[30], &in->bit[108]);
    out->bit[60] = qs_gf163_xor_bits(state, &out->bit[60], &in->bit[110]);
    out->bit[61] = qs_gf163_xor_bits(state, &in->bit[109], &in->bit[112]);
    out->bit[62] = qs_gf163_xor_bits(state, &in->bit[31], &in->bit[109]);
    out->bit[62] = qs_gf163_xor_bits(state, &out->bit[62], &in->bit[111]);
    out->bit[63] = qs_gf163_xor_bits(state, &in->bit[110], &in->bit[113]);
    out->bit[64] = qs_gf163_xor_bits(state, &in->bit[32], &in->bit[110]);
    out->bit[64] = qs_gf163_xor_bits(state, &out->bit[64], &in->bit[112]);
    out->bit[65] = qs_gf163_xor_bits(state, &in->bit[111], &in->bit[114]);
    out->bit[66] = qs_gf163_xor_bits(state, &in->bit[33], &in->bit[111]);
    out->bit[66] = qs_gf163_xor_bits(state, &out->bit[66], &in->bit[113]);
    out->bit[67] = qs_gf163_xor_bits(state, &in->bit[112], &in->bit[115]);
    out->bit[68] = qs_gf163_xor_bits(state, &in->bit[34], &in->bit[112]);
    out->bit[68] = qs_gf163_xor_bits(state, &out->bit[68], &in->bit[114]);
    out->bit[69] = qs_gf163_xor_bits(state, &in->bit[113], &in->bit[116]);
    out->bit[70] = qs_gf163_xor_bits(state, &in->bit[35], &in->bit[113]);
    out->bit[70] = qs_gf163_xor_bits(state, &out->bit[70], &in->bit[115]);
    out->bit[71] = qs_gf163_xor_bits(state, &in->bit[114], &in->bit[117]);
    out->bit[72] = qs_gf163_xor_bits(state, &in->bit[36], &in->bit[114]);
    out->bit[72] = qs_gf163_xor_bits(state, &out->bit[72], &in->bit[116]);
    out->bit[73] = qs_gf163_xor_bits(state, &in->bit[115], &in->bit[118]);
    out->bit[74] = qs_gf163_xor_bits(state, &in->bit[37], &in->bit[115]);
    out->bit[74] = qs_gf163_xor_bits(state, &out->bit[74], &in->bit[117]);
    out->bit[75] = qs_gf163_xor_bits(state, &in->bit[116], &in->bit[119]);
    out->bit[76] = qs_gf163_xor_bits(state, &in->bit[38], &in->bit[116]);
    out->bit[76] = qs_gf163_xor_bits(state, &out->bit[76], &in->bit[118]);
    out->bit[77] = qs_gf163_xor_bits(state, &in->bit[117], &in->bit[120]);
    out->bit[78] = qs_gf163_xor_bits(state, &in->bit[39], &in->bit[117]);
    out->bit[78] = qs_gf163_xor_bits(state, &out->bit[78], &in->bit[119]);
    out->bit[79] = qs_gf163_xor_bits(state, &in->bit[118], &in->bit[121]);
    out->bit[80] = qs_gf163_xor_bits(state, &in->bit[40], &in->bit[118]);
    out->bit[80] = qs_gf163_xor_bits(state, &out->bit[80], &in->bit[120]);
    out->bit[81] = qs_gf163_xor_bits(state, &in->bit[119], &in->bit[122]);
    out->bit[82] = qs_gf163_xor_bits(state, &in->bit[41], &in->bit[119]);
    out->bit[82] = qs_gf163_xor_bits(state, &out->bit[82], &in->bit[121]);
    out->bit[83] = qs_gf163_xor_bits(state, &in->bit[120], &in->bit[123]);
    out->bit[84] = qs_gf163_xor_bits(state, &in->bit[42], &in->bit[120]);
    out->bit[84] = qs_gf163_xor_bits(state, &out->bit[84], &in->bit[122]);
    out->bit[85] = qs_gf163_xor_bits(state, &in->bit[121], &in->bit[124]);
    out->bit[86] = qs_gf163_xor_bits(state, &in->bit[43], &in->bit[121]);
    out->bit[86] = qs_gf163_xor_bits(state, &out->bit[86], &in->bit[123]);
    out->bit[87] = qs_gf163_xor_bits(state, &in->bit[122], &in->bit[125]);
    out->bit[88] = qs_gf163_xor_bits(state, &in->bit[44], &in->bit[122]);
    out->bit[88] = qs_gf163_xor_bits(state, &out->bit[88], &in->bit[124]);
    out->bit[89] = qs_gf163_xor_bits(state, &in->bit[123], &in->bit[126]);
    out->bit[90] = qs_gf163_xor_bits(state, &in->bit[45], &in->bit[123]);
    out->bit[90] = qs_gf163_xor_bits(state, &out->bit[90], &in->bit[125]);
    out->bit[91] = qs_gf163_xor_bits(state, &in->bit[124], &in->bit[127]);
    out->bit[92] = qs_gf163_xor_bits(state, &in->bit[46], &in->bit[124]);
    out->bit[92] = qs_gf163_xor_bits(state, &out->bit[92], &in->bit[126]);
    out->bit[93] = qs_gf163_xor_bits(state, &in->bit[125], &in->bit[128]);
    out->bit[94] = qs_gf163_xor_bits(state, &in->bit[47], &in->bit[125]);
    out->bit[94] = qs_gf163_xor_bits(state, &out->bit[94], &in->bit[127]);
    out->bit[95] = qs_gf163_xor_bits(state, &in->bit[126], &in->bit[129]);
    out->bit[96] = qs_gf163_xor_bits(state, &in->bit[48], &in->bit[126]);
    out->bit[96] = qs_gf163_xor_bits(state, &out->bit[96], &in->bit[128]);
    out->bit[97] = qs_gf163_xor_bits(state, &in->bit[127], &in->bit[130]);
    out->bit[98] = qs_gf163_xor_bits(state, &in->bit[49], &in->bit[127]);
    out->bit[98] = qs_gf163_xor_bits(state, &out->bit[98], &in->bit[129]);
    out->bit[99] = qs_gf163_xor_bits(state, &in->bit[128], &in->bit[131]);
    out->bit[100] = qs_gf163_xor_bits(state, &in->bit[50], &in->bit[128]);
    out->bit[100] = qs_gf163_xor_bits(state, &out->bit[100], &in->bit[130]);
    out->bit[101] = qs_gf163_xor_bits(state, &in->bit[129], &in->bit[132]);
    out->bit[102] = qs_gf163_xor_bits(state, &in->bit[51], &in->bit[129]);
    out->bit[102] = qs_gf163_xor_bits(state, &out->bit[102], &in->bit[131]);
    out->bit[103] = qs_gf163_xor_bits(state, &in->bit[130], &in->bit[133]);
    out->bit[104] = qs_gf163_xor_bits(state, &in->bit[52], &in->bit[130]);
    out->bit[104] = qs_gf163_xor_bits(state, &out->bit[104], &in->bit[132]);
    out->bit[105] = qs_gf163_xor_bits(state, &in->bit[131], &in->bit[134]);
    out->bit[106] = qs_gf163_xor_bits(state, &in->bit[53], &in->bit[131]);
    out->bit[106] = qs_gf163_xor_bits(state, &out->bit[106], &in->bit[133]);
    out->bit[107] = qs_gf163_xor_bits(state, &in->bit[132], &in->bit[135]);
    out->bit[108] = qs_gf163_xor_bits(state, &in->bit[54], &in->bit[132]);
    out->bit[108] = qs_gf163_xor_bits(state, &out->bit[108], &in->bit[134]);
    out->bit[109] = qs_gf163_xor_bits(state, &in->bit[133], &in->bit[136]);
    out->bit[110] = qs_gf163_xor_bits(state, &in->bit[55], &in->bit[133]);
    out->bit[110] = qs_gf163_xor_bits(state, &out->bit[110], &in->bit[135]);
    out->bit[111] = qs_gf163_xor_bits(state, &in->bit[134], &in->bit[137]);
    out->bit[112] = qs_gf163_xor_bits(state, &in->bit[56], &in->bit[134]);
    out->bit[112] = qs_gf163_xor_bits(state, &out->bit[112], &in->bit[136]);
    out->bit[113] = qs_gf163_xor_bits(state, &in->bit[135], &in->bit[138]);
    out->bit[114] = qs_gf163_xor_bits(state, &in->bit[57], &in->bit[135]);
    out->bit[114] = qs_gf163_xor_bits(state, &out->bit[114], &in->bit[137]);
    out->bit[115] = qs_gf163_xor_bits(state, &in->bit[136], &in->bit[139]);
    out->bit[116] = qs_gf163_xor_bits(state, &in->bit[58], &in->bit[136]);
    out->bit[116] = qs_gf163_xor_bits(state, &out->bit[116], &in->bit[138]);
    out->bit[117] = qs_gf163_xor_bits(state, &in->bit[137], &in->bit[140]);
    out->bit[118] = qs_gf163_xor_bits(state, &in->bit[59], &in->bit[137]);
    out->bit[118] = qs_gf163_xor_bits(state, &out->bit[118], &in->bit[139]);
    out->bit[119] = qs_gf163_xor_bits(state, &in->bit[138], &in->bit[141]);
    out->bit[120] = qs_gf163_xor_bits(state, &in->bit[60], &in->bit[138]);
    out->bit[120] = qs_gf163_xor_bits(state, &out->bit[120], &in->bit[140]);
    out->bit[121] = qs_gf163_xor_bits(state, &in->bit[139], &in->bit[142]);
    out->bit[122] = qs_gf163_xor_bits(state, &in->bit[61], &in->bit[139]);
    out->bit[122] = qs_gf163_xor_bits(state, &out->bit[122], &in->bit[141]);
    out->bit[123] = qs_gf163_xor_bits(state, &in->bit[140], &in->bit[143]);
    out->bit[124] = qs_gf163_xor_bits(state, &in->bit[62], &in->bit[140]);
    out->bit[124] = qs_gf163_xor_bits(state, &out->bit[124], &in->bit[142]);
    out->bit[125] = qs_gf163_xor_bits(state, &in->bit[141], &in->bit[144]);
    out->bit[126] = qs_gf163_xor_bits(state, &in->bit[63], &in->bit[141]);
    out->bit[126] = qs_gf163_xor_bits(state, &out->bit[126], &in->bit[143]);
    out->bit[127] = qs_gf163_xor_bits(state, &in->bit[142], &in->bit[145]);
    out->bit[128] = qs_gf163_xor_bits(state, &in->bit[64], &in->bit[142]);
    out->bit[128] = qs_gf163_xor_bits(state, &out->bit[128], &in->bit[144]);
    out->bit[129] = qs_gf163_xor_bits(state, &in->bit[143], &in->bit[146]);
    out->bit[130] = qs_gf163_xor_bits(state, &in->bit[65], &in->bit[143]);
    out->bit[130] = qs_gf163_xor_bits(state, &out->bit[130], &in->bit[145]);
    out->bit[131] = qs_gf163_xor_bits(state, &in->bit[144], &in->bit[147]);
    out->bit[132] = qs_gf163_xor_bits(state, &in->bit[66], &in->bit[144]);
    out->bit[132] = qs_gf163_xor_bits(state, &out->bit[132], &in->bit[146]);
    out->bit[133] = qs_gf163_xor_bits(state, &in->bit[145], &in->bit[148]);
    out->bit[134] = qs_gf163_xor_bits(state, &in->bit[67], &in->bit[145]);
    out->bit[134] = qs_gf163_xor_bits(state, &out->bit[134], &in->bit[147]);
    out->bit[135] = qs_gf163_xor_bits(state, &in->bit[146], &in->bit[149]);
    out->bit[136] = qs_gf163_xor_bits(state, &in->bit[68], &in->bit[146]);
    out->bit[136] = qs_gf163_xor_bits(state, &out->bit[136], &in->bit[148]);
    out->bit[137] = qs_gf163_xor_bits(state, &in->bit[147], &in->bit[150]);
    out->bit[138] = qs_gf163_xor_bits(state, &in->bit[69], &in->bit[147]);
    out->bit[138] = qs_gf163_xor_bits(state, &out->bit[138], &in->bit[149]);
    out->bit[139] = qs_gf163_xor_bits(state, &in->bit[148], &in->bit[151]);
    out->bit[140] = qs_gf163_xor_bits(state, &in->bit[70], &in->bit[148]);
    out->bit[140] = qs_gf163_xor_bits(state, &out->bit[140], &in->bit[150]);
    out->bit[141] = qs_gf163_xor_bits(state, &in->bit[149], &in->bit[152]);
    out->bit[142] = qs_gf163_xor_bits(state, &in->bit[71], &in->bit[149]);
    out->bit[142] = qs_gf163_xor_bits(state, &out->bit[142], &in->bit[151]);
    out->bit[143] = qs_gf163_xor_bits(state, &in->bit[150], &in->bit[153]);
    out->bit[144] = qs_gf163_xor_bits(state, &in->bit[72], &in->bit[150]);
    out->bit[144] = qs_gf163_xor_bits(state, &out->bit[144], &in->bit[152]);
    out->bit[145] = qs_gf163_xor_bits(state, &in->bit[151], &in->bit[154]);
    out->bit[146] = qs_gf163_xor_bits(state, &in->bit[73], &in->bit[151]);
    out->bit[146] = qs_gf163_xor_bits(state, &out->bit[146], &in->bit[153]);
    out->bit[147] = qs_gf163_xor_bits(state, &in->bit[152], &in->bit[155]);
    out->bit[148] = qs_gf163_xor_bits(state, &in->bit[74], &in->bit[152]);
    out->bit[148] = qs_gf163_xor_bits(state, &out->bit[148], &in->bit[154]);
    out->bit[149] = qs_gf163_xor_bits(state, &in->bit[153], &in->bit[156]);
    out->bit[150] = qs_gf163_xor_bits(state, &in->bit[75], &in->bit[153]);
    out->bit[150] = qs_gf163_xor_bits(state, &out->bit[150], &in->bit[155]);
    out->bit[151] = qs_gf163_xor_bits(state, &in->bit[154], &in->bit[157]);
    out->bit[152] = qs_gf163_xor_bits(state, &in->bit[76], &in->bit[154]);
    out->bit[152] = qs_gf163_xor_bits(state, &out->bit[152], &in->bit[156]);
    out->bit[153] = qs_gf163_xor_bits(state, &in->bit[155], &in->bit[158]);
    out->bit[154] = qs_gf163_xor_bits(state, &in->bit[77], &in->bit[155]);
    out->bit[154] = qs_gf163_xor_bits(state, &out->bit[154], &in->bit[157]);
    out->bit[155] = qs_gf163_xor_bits(state, &in->bit[156], &in->bit[159]);
    out->bit[156] = qs_gf163_xor_bits(state, &in->bit[78], &in->bit[156]);
    out->bit[156] = qs_gf163_xor_bits(state, &out->bit[156], &in->bit[158]);
    out->bit[157] = qs_gf163_xor_bits(state, &in->bit[157], &in->bit[160]);
    out->bit[158] = qs_gf163_xor_bits(state, &in->bit[79], &in->bit[157]);
    out->bit[158] = qs_gf163_xor_bits(state, &out->bit[158], &in->bit[159]);
    out->bit[159] = qs_gf163_xor_bits(state, &in->bit[158], &in->bit[161]);
    out->bit[160] = qs_gf163_xor_bits(state, &in->bit[80], &in->bit[158]);
    out->bit[160] = qs_gf163_xor_bits(state, &out->bit[160], &in->bit[160]);
    out->bit[161] = qs_gf163_xor_bits(state, &in->bit[159], &in->bit[162]);
    out->bit[162] = qs_gf163_xor_bits(state, &in->bit[81], &in->bit[159]);
    out->bit[162] = qs_gf163_xor_bits(state, &out->bit[162], &in->bit[161]);
}

void qs_gf163_square(
    const quicksilver_state* state, qs_gf163_bits* out, const qs_gf163_bits* in)
{
    if (out == in) {
        qs_gf163_bits tmp;
        qs_gf163_square_noalias(state, &tmp, in);
        *out = tmp;
    } else {
        qs_gf163_square_noalias(state, out, in);
    }
}

void qs_gf163_frobenius(
    const quicksilver_state* state, qs_gf163_bits* out,
    const qs_gf163_bits* in, unsigned exponent)
{
    if (exponent == 0) {
        for (size_t i = 0; i < BLOCK163_BITS; ++i)
            out->bit[i] = qs_gf163_copy_bit(state, &in->bit[i]);
        return;
    }
    if (exponent == 1) {
        qs_gf163_square(state, out, in);
        return;
    }

    qs_gf163_bits scratch[2];
    const qs_gf163_bits* current = in;
    for (unsigned round = 0; round < exponent - 1; ++round) {
        qs_gf163_bits* next = &scratch[round & 1];
        qs_gf163_square_noalias(state, next, current);
        current = next;
    }
    qs_gf163_square_noalias(state, out, current);
}

void qs_gf163_shift_reduce(
    const quicksilver_state* state, qs_gf163_bits* out,
    const qs_gf163_bits* in, size_t shift)
{
    assert(shift < BLOCK163_BITS);
    quicksilver_vec_gf2 tmp[2 * BLOCK163_BITS - 1];

    for (size_t i = 0; i < 2 * BLOCK163_BITS - 1; ++i) {
        tmp[i] = quicksilver_zero_gf2();
    }
    for (size_t bit = 0; bit < BLOCK163_BITS; ++bit) {
        tmp[bit + shift] = quicksilver_add_gf2(state, tmp[bit + shift], in->bit[bit]);
    }
    for (int pos = 2 * BLOCK163_BITS - 2; pos >= BLOCK163_BITS; --pos) {
        const quicksilver_vec_gf2 term = tmp[pos];
        tmp[pos] = quicksilver_zero_gf2();
        static const unsigned taps[] = {0, 3, 6, 7};
        for (size_t t = 0; t < sizeof(taps) / sizeof(taps[0]); ++t) {
            const size_t dest = (size_t)pos - BLOCK163_BITS + taps[t];
            tmp[dest] = quicksilver_add_gf2(state, tmp[dest], term);
        }
    }
    for (size_t bit = 0; bit < BLOCK163_BITS; ++bit) {
        out->bit[bit] = tmp[bit];
    }
}

GW_STATIC_ASSERT(SECURITY_PARAM == 160 && POLY_VEC_LEN == 1,
               "GF163 hash fusion requires scalar GF160 authentication");
GW_STATIC_ASSERT(BLOCK163_BITS == 163 && BLOCK163_REDUCTION_LOW == 0xc9,
               "GF163 projection recurrence assumes t^163 + t^7 + t^6 + t^3 + 1");
GW_STATIC_ASSERT(HASHER_GFSECPAR_KEY_POWS == 2,
               "GF163 block append assumes two-coefficient hash groups");

typedef struct {
    poly_secpar_vec weights[2 * BLOCK163_BITS - 1];
    poly_secpar_vec r, defect, r162, r163, r164;
} qs_gf163_projection_key;

static inline poly_secpar_vec qs_gf163_mac_mul(poly_secpar_vec a, poly_secpar_vec b)
{
    return poly_2secpar_reduce_secpar(poly_secpar_mul(a, b));
}

static void qs_gf163_projection_init(qs_gf163_projection_key* key, poly_secpar_vec r)
{

    key->r = r;
    key->weights[162] = poly_secpar_set_low32(1);
    for (int k = 161; k >= 0; --k)
        key->weights[k] = qs_gf163_mac_mul(r, key->weights[k + 1]);
    for (size_t k = 163; k <= 324; ++k)
        key->weights[k] = poly_secpar_add(
            poly_secpar_add(key->weights[k - 163], key->weights[k - 160]),
            poly_secpar_add(key->weights[k - 157], key->weights[k - 156]));
    key->r162 = key->weights[0];
    key->r163 = qs_gf163_mac_mul(r, key->r162);
    key->r164 = qs_gf163_mac_mul(r, key->r163);
    key->defect = poly_secpar_add(poly_secpar_set_low32(1),
        poly_secpar_add(key->r163, poly_secpar_add(key->weights[2],
            poly_secpar_add(key->weights[5], key->weights[6]))));
}

static poly_secpar_vec qs_gf163_project_product(
    const qs_gf163_projection_key* key,
    const poly_secpar_vec a[BLOCK163_BITS], const poly_secpar_vec b[BLOCK163_BITS])
{

    poly_2secpar_vec initial = poly_2secpar_from_secpar(b[0]);
    for (size_t j = 1; j < BLOCK163_BITS; ++j)
        initial = poly_2secpar_add(initial, poly_secpar_mul(key->weights[162 + j], b[j]));
    poly_secpar_vec v = poly_2secpar_reduce_secpar(initial);
    poly_2secpar_vec result = poly_secpar_mul(a[162], v);

    for (int i = 161; i >= 0; --i) {
        poly_secpar_vec correction = b[162 - i];
        if (i >= 156)
            correction = poly_secpar_add(correction, b[318 - i]);
        if (i >= 157)
            correction = poly_secpar_add(correction, b[319 - i]);
        if (i >= 160)
            correction = poly_secpar_add(correction, b[322 - i]);
        v = poly_2secpar_reduce_secpar(poly_2secpar_add(
            poly_secpar_mul(key->r, v), poly_secpar_mul(key->defect, correction)));
        result = poly_2secpar_add(result, poly_secpar_mul(a[i], v));
    }
    return poly_2secpar_reduce_secpar(result);
}

static void qs_gf163_hash_append(
    hasher_gfsecpar_state* state, const qs_gf163_projection_key* key,
    poly_secpar_vec contribution)
{

    const poly_secpar_vec previous = poly_2secpar_reduce_secpar(state->state);
    poly_secpar_vec next;
    if (state->pow == 0) {
        next = poly_secpar_add(qs_gf163_mac_mul(key->r162, previous), contribution);
        state->pow = -1;
    } else {
        assert(state->pow == -1 || state->pow == 1);
        const poly_secpar_vec factor = state->pow == -1 ? key->r164 : key->r162;
        next = poly_2secpar_reduce_secpar(poly_2secpar_add(
            poly_secpar_mul(factor, previous), poly_secpar_mul(key->r, contribution)));
        state->pow = 0;
    }
    state->state = poly_2secpar_from_secpar(next);
}

static void qs_gf163_hash64_append(
    hasher_gfsecpar_64_state* state, const qs_gf163_projection_key* key,
    poly_secpar_vec contribution)
{

    state->state = poly_secpar_add(qs_gf163_mac_mul(key->r163, state->state), contribution);
}

void qs_gf163_mul_constraints(
    quicksilver_state* state, const qs_gf163_bits* x,
    const qs_gf163_bits* y, const qs_gf163_bits* z, size_t count)
{
    if (count == 0)
        return;
    qs_gf163_projection_key keys[2];
    qs_gf163_projection_init(&keys[0], state->key_secpar.key_pows[0]);
    qs_gf163_projection_init(&keys[1], poly_secpar_from_64(state->key_64.key));

    for (size_t relation = 0; relation < count; ++relation) {
        poly_secpar_vec a[BLOCK163_BITS], b[BLOCK163_BITS];
        poly_secpar_vec a_plus_value[BLOCK163_BITS], b_plus_value[BLOCK163_BITS];
        for (size_t i = 0; i < BLOCK163_BITS; ++i) {
            a[i] = x[relation].bit[i].mac;
            b[i] = y[relation].bit[i].mac;
            if (!state->verifier) {
                a_plus_value[i] = poly_secpar_add(a[i], poly_secpar_from_1(x[relation].bit[i].value));
                b_plus_value[i] = poly_secpar_add(b[i], poly_secpar_from_1(y[relation].bit[i].value));
            }
        }

        for (size_t h = 0; h < 2; ++h) {
            const qs_gf163_projection_key* key = &keys[h];
            poly_secpar_vec constant = qs_gf163_project_product(key, a, b);
            poly_2secpar_vec z_sum = poly_2secpar_set_zero();
            for (size_t i = 0; i < BLOCK163_BITS; ++i) {
                poly_secpar_vec zi = z[relation].bit[i].mac;
                if (!state->verifier)
                    zi = poly_secpar_add(zi, poly_secpar_from_1(z[relation].bit[i].value));
                z_sum = poly_2secpar_add(z_sum, poly_secpar_mul(key->weights[i], zi));
            }
            const poly_secpar_vec z_projection = poly_2secpar_reduce_secpar(z_sum);
            poly_secpar_vec linear = poly_secpar_set_zero();
            if (state->verifier) {

                constant = poly_secpar_add(constant, qs_gf163_mac_mul(state->delta, z_projection));
            } else {

                linear = poly_secpar_add(constant, poly_secpar_add(z_projection,
                    qs_gf163_project_product(key, a_plus_value, b_plus_value)));
            }
            if (h == 0) {
                qs_gf163_hash_append(&state->state_secpar_const, key, constant);
                if (!state->verifier)
                    qs_gf163_hash_append(&state->state_secpar_linear, key, linear);
            } else {
                qs_gf163_hash64_append(&state->state_64_const, key, constant);
                if (!state->verifier)
                    qs_gf163_hash64_append(&state->state_64_linear, key, linear);
            }
        }
    }
}

void qs_gf163_mul_constraint(
    quicksilver_state* state, const qs_gf163_bits* x,
    const qs_gf163_bits* y, const qs_gf163_bits* z)
{
    qs_gf163_mul_constraints(state, x, y, z, 1);
}

void qs_gf163_dot_constraint(
    quicksilver_state* state, const qs_gf163_bits* y, const block163* alpha)
{
    quicksilver_vec_gf2 dot = quicksilver_zero_gf2();
    for (size_t bit = 0; bit < BLOCK163_BITS; ++bit) {
        if ((alpha->data[bit >> 6] >> (bit & 63)) & 1ULL) {
            dot = quicksilver_add_gf2(state, dot, y->bit[bit]);
        }
    }
    quicksilver_inverse_constraint(
        state, quicksilver_combine_1_bit(state, dot), quicksilver_one_gfsecpar(state));
}
