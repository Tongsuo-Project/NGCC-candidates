#include "gf263_proof.h"

void qs_gf263_load_witness(
    const quicksilver_state* state, qs_gf263_bits* out, size_t bit_offset)
{
    for (size_t bit = 0; bit < BLOCK263_BITS; ++bit) {
        out->bit[bit] = quicksilver_get_witness_vec(state, bit_offset + bit);
    }
}

void qs_gf263_load_constant(
    const quicksilver_state* state, qs_gf263_bits* out,
    const uint64_t words[BLOCK263_WORDS])
{
    for (size_t bit = 0; bit < BLOCK263_BITS; ++bit) {
        const uint8_t value = (words[bit >> 6] >> (bit & 63)) & 1;
        out->bit[bit] = value
            ? quicksilver_one_gf2(state)
            : quicksilver_zero_gf2();
    }
}

void qs_gf263_add(
    const quicksilver_state* state, qs_gf263_bits* out,
    const qs_gf263_bits* x, const qs_gf263_bits* y)
{
    for (size_t bit = 0; bit < BLOCK263_BITS; ++bit) {
        out->bit[bit] = quicksilver_add_gf2(state, x->bit[bit], y->bit[bit]);
    }
}

void qs_gf263_matmul(
    const quicksilver_state* state, qs_gf263_bits* out,
    const uint64_t matrix[BLOCK263_BITS][BLOCK263_WORDS],
    const qs_gf263_bits* in)
{
    for (size_t row = 0; row < BLOCK263_BITS; ++row) {

        const uint8_t* row_bytes = (const uint8_t*)matrix[row];
        quicksilver_vec_gf2 acc = quicksilver_const_gf2(state, poly1_load(0, 0));
        for (size_t col = 0; col < BLOCK263_BITS; ++col) {
            const poly1_vec coefficient = poly1_load(row_bytes[col / 8], col % 8);
            acc = quicksilver_add_gf2(
                state, acc, quicksilver_mul_const_gf2(state, in->bit[col], coefficient));
        }
        out->bit[row] = acc;
    }
}

static inline quicksilver_vec_gf2 qs_gf263_copy_bit(
    const quicksilver_state* state, const quicksilver_vec_gf2* in)
{
    quicksilver_vec_gf2 out;
    out.mac = in->mac;
    out.value = state->verifier ? 0 : in->value;
    return out;
}

static inline quicksilver_vec_gf2 qs_gf263_xor_bits(
    const quicksilver_state* state,
    const quicksilver_vec_gf2* a, const quicksilver_vec_gf2* b)
{
    quicksilver_vec_gf2 out;
    out.mac = poly_secpar_add(a->mac, b->mac);
    out.value = state->verifier ? 0 : a->value ^ b->value;
    return out;
}

static void qs_gf263_square_noalias(
    const quicksilver_state* state, qs_gf263_bits* restrict out,
    const qs_gf263_bits* restrict in)
{

    out->bit[0] = qs_gf263_copy_bit(state, &in->bit[0]);
    out->bit[1] = qs_gf263_xor_bits(state, &in->bit[132], &in->bit[217]);
    out->bit[2] = qs_gf263_copy_bit(state, &in->bit[1]);
    out->bit[3] = qs_gf263_xor_bits(state, &in->bit[133], &in->bit[218]);
    out->bit[4] = qs_gf263_copy_bit(state, &in->bit[2]);
    out->bit[5] = qs_gf263_xor_bits(state, &in->bit[134], &in->bit[219]);
    out->bit[6] = qs_gf263_copy_bit(state, &in->bit[3]);
    out->bit[7] = qs_gf263_xor_bits(state, &in->bit[135], &in->bit[220]);
    out->bit[8] = qs_gf263_copy_bit(state, &in->bit[4]);
    out->bit[9] = qs_gf263_xor_bits(state, &in->bit[136], &in->bit[221]);
    out->bit[10] = qs_gf263_copy_bit(state, &in->bit[5]);
    out->bit[11] = qs_gf263_xor_bits(state, &in->bit[137], &in->bit[222]);
    out->bit[12] = qs_gf263_copy_bit(state, &in->bit[6]);
    out->bit[13] = qs_gf263_xor_bits(state, &in->bit[138], &in->bit[223]);
    out->bit[14] = qs_gf263_copy_bit(state, &in->bit[7]);
    out->bit[15] = qs_gf263_xor_bits(state, &in->bit[139], &in->bit[224]);
    out->bit[16] = qs_gf263_copy_bit(state, &in->bit[8]);
    out->bit[17] = qs_gf263_xor_bits(state, &in->bit[140], &in->bit[225]);
    out->bit[18] = qs_gf263_copy_bit(state, &in->bit[9]);
    out->bit[19] = qs_gf263_xor_bits(state, &in->bit[141], &in->bit[226]);
    out->bit[20] = qs_gf263_copy_bit(state, &in->bit[10]);
    out->bit[21] = qs_gf263_xor_bits(state, &in->bit[142], &in->bit[227]);
    out->bit[22] = qs_gf263_copy_bit(state, &in->bit[11]);
    out->bit[23] = qs_gf263_xor_bits(state, &in->bit[143], &in->bit[228]);
    out->bit[24] = qs_gf263_copy_bit(state, &in->bit[12]);
    out->bit[25] = qs_gf263_xor_bits(state, &in->bit[144], &in->bit[229]);
    out->bit[26] = qs_gf263_copy_bit(state, &in->bit[13]);
    out->bit[27] = qs_gf263_xor_bits(state, &in->bit[145], &in->bit[230]);
    out->bit[28] = qs_gf263_copy_bit(state, &in->bit[14]);
    out->bit[29] = qs_gf263_xor_bits(state, &in->bit[146], &in->bit[231]);
    out->bit[30] = qs_gf263_copy_bit(state, &in->bit[15]);
    out->bit[31] = qs_gf263_xor_bits(state, &in->bit[147], &in->bit[232]);
    out->bit[32] = qs_gf263_copy_bit(state, &in->bit[16]);
    out->bit[33] = qs_gf263_xor_bits(state, &in->bit[148], &in->bit[233]);
    out->bit[34] = qs_gf263_copy_bit(state, &in->bit[17]);
    out->bit[35] = qs_gf263_xor_bits(state, &in->bit[149], &in->bit[234]);
    out->bit[36] = qs_gf263_copy_bit(state, &in->bit[18]);
    out->bit[37] = qs_gf263_xor_bits(state, &in->bit[150], &in->bit[235]);
    out->bit[38] = qs_gf263_copy_bit(state, &in->bit[19]);
    out->bit[39] = qs_gf263_xor_bits(state, &in->bit[151], &in->bit[236]);
    out->bit[40] = qs_gf263_copy_bit(state, &in->bit[20]);
    out->bit[41] = qs_gf263_xor_bits(state, &in->bit[152], &in->bit[237]);
    out->bit[42] = qs_gf263_copy_bit(state, &in->bit[21]);
    out->bit[43] = qs_gf263_xor_bits(state, &in->bit[153], &in->bit[238]);
    out->bit[44] = qs_gf263_copy_bit(state, &in->bit[22]);
    out->bit[45] = qs_gf263_xor_bits(state, &in->bit[154], &in->bit[239]);
    out->bit[46] = qs_gf263_copy_bit(state, &in->bit[23]);
    out->bit[47] = qs_gf263_xor_bits(state, &in->bit[155], &in->bit[240]);
    out->bit[48] = qs_gf263_copy_bit(state, &in->bit[24]);
    out->bit[49] = qs_gf263_xor_bits(state, &in->bit[156], &in->bit[241]);
    out->bit[50] = qs_gf263_copy_bit(state, &in->bit[25]);
    out->bit[51] = qs_gf263_xor_bits(state, &in->bit[157], &in->bit[242]);
    out->bit[52] = qs_gf263_copy_bit(state, &in->bit[26]);
    out->bit[53] = qs_gf263_xor_bits(state, &in->bit[158], &in->bit[243]);
    out->bit[54] = qs_gf263_copy_bit(state, &in->bit[27]);
    out->bit[55] = qs_gf263_xor_bits(state, &in->bit[159], &in->bit[244]);
    out->bit[56] = qs_gf263_copy_bit(state, &in->bit[28]);
    out->bit[57] = qs_gf263_xor_bits(state, &in->bit[160], &in->bit[245]);
    out->bit[58] = qs_gf263_copy_bit(state, &in->bit[29]);
    out->bit[59] = qs_gf263_xor_bits(state, &in->bit[161], &in->bit[246]);
    out->bit[60] = qs_gf263_copy_bit(state, &in->bit[30]);
    out->bit[61] = qs_gf263_xor_bits(state, &in->bit[162], &in->bit[247]);
    out->bit[62] = qs_gf263_copy_bit(state, &in->bit[31]);
    out->bit[63] = qs_gf263_xor_bits(state, &in->bit[163], &in->bit[248]);
    out->bit[64] = qs_gf263_copy_bit(state, &in->bit[32]);
    out->bit[65] = qs_gf263_xor_bits(state, &in->bit[164], &in->bit[249]);
    out->bit[66] = qs_gf263_copy_bit(state, &in->bit[33]);
    out->bit[67] = qs_gf263_xor_bits(state, &in->bit[165], &in->bit[250]);
    out->bit[68] = qs_gf263_copy_bit(state, &in->bit[34]);
    out->bit[69] = qs_gf263_xor_bits(state, &in->bit[166], &in->bit[251]);
    out->bit[70] = qs_gf263_copy_bit(state, &in->bit[35]);
    out->bit[71] = qs_gf263_xor_bits(state, &in->bit[167], &in->bit[252]);
    out->bit[72] = qs_gf263_copy_bit(state, &in->bit[36]);
    out->bit[73] = qs_gf263_xor_bits(state, &in->bit[168], &in->bit[253]);
    out->bit[74] = qs_gf263_copy_bit(state, &in->bit[37]);
    out->bit[75] = qs_gf263_xor_bits(state, &in->bit[169], &in->bit[254]);
    out->bit[76] = qs_gf263_copy_bit(state, &in->bit[38]);
    out->bit[77] = qs_gf263_xor_bits(state, &in->bit[170], &in->bit[255]);
    out->bit[78] = qs_gf263_copy_bit(state, &in->bit[39]);
    out->bit[79] = qs_gf263_xor_bits(state, &in->bit[171], &in->bit[256]);
    out->bit[80] = qs_gf263_copy_bit(state, &in->bit[40]);
    out->bit[81] = qs_gf263_xor_bits(state, &in->bit[172], &in->bit[257]);
    out->bit[82] = qs_gf263_copy_bit(state, &in->bit[41]);
    out->bit[83] = qs_gf263_xor_bits(state, &in->bit[173], &in->bit[258]);
    out->bit[84] = qs_gf263_copy_bit(state, &in->bit[42]);
    out->bit[85] = qs_gf263_xor_bits(state, &in->bit[174], &in->bit[259]);
    out->bit[86] = qs_gf263_copy_bit(state, &in->bit[43]);
    out->bit[87] = qs_gf263_xor_bits(state, &in->bit[175], &in->bit[260]);
    out->bit[88] = qs_gf263_copy_bit(state, &in->bit[44]);
    out->bit[89] = qs_gf263_xor_bits(state, &in->bit[176], &in->bit[261]);
    out->bit[90] = qs_gf263_copy_bit(state, &in->bit[45]);
    out->bit[91] = qs_gf263_xor_bits(state, &in->bit[177], &in->bit[262]);
    out->bit[92] = qs_gf263_copy_bit(state, &in->bit[46]);
    out->bit[93] = qs_gf263_copy_bit(state, &in->bit[178]);
    out->bit[94] = qs_gf263_xor_bits(state, &in->bit[47], &in->bit[132]);
    out->bit[94] = qs_gf263_xor_bits(state, &out->bit[94], &in->bit[217]);
    out->bit[95] = qs_gf263_copy_bit(state, &in->bit[179]);
    out->bit[96] = qs_gf263_xor_bits(state, &in->bit[48], &in->bit[133]);
    out->bit[96] = qs_gf263_xor_bits(state, &out->bit[96], &in->bit[218]);
    out->bit[97] = qs_gf263_copy_bit(state, &in->bit[180]);
    out->bit[98] = qs_gf263_xor_bits(state, &in->bit[49], &in->bit[134]);
    out->bit[98] = qs_gf263_xor_bits(state, &out->bit[98], &in->bit[219]);
    out->bit[99] = qs_gf263_copy_bit(state, &in->bit[181]);
    out->bit[100] = qs_gf263_xor_bits(state, &in->bit[50], &in->bit[135]);
    out->bit[100] = qs_gf263_xor_bits(state, &out->bit[100], &in->bit[220]);
    out->bit[101] = qs_gf263_copy_bit(state, &in->bit[182]);
    out->bit[102] = qs_gf263_xor_bits(state, &in->bit[51], &in->bit[136]);
    out->bit[102] = qs_gf263_xor_bits(state, &out->bit[102], &in->bit[221]);
    out->bit[103] = qs_gf263_copy_bit(state, &in->bit[183]);
    out->bit[104] = qs_gf263_xor_bits(state, &in->bit[52], &in->bit[137]);
    out->bit[104] = qs_gf263_xor_bits(state, &out->bit[104], &in->bit[222]);
    out->bit[105] = qs_gf263_copy_bit(state, &in->bit[184]);
    out->bit[106] = qs_gf263_xor_bits(state, &in->bit[53], &in->bit[138]);
    out->bit[106] = qs_gf263_xor_bits(state, &out->bit[106], &in->bit[223]);
    out->bit[107] = qs_gf263_copy_bit(state, &in->bit[185]);
    out->bit[108] = qs_gf263_xor_bits(state, &in->bit[54], &in->bit[139]);
    out->bit[108] = qs_gf263_xor_bits(state, &out->bit[108], &in->bit[224]);
    out->bit[109] = qs_gf263_copy_bit(state, &in->bit[186]);
    out->bit[110] = qs_gf263_xor_bits(state, &in->bit[55], &in->bit[140]);
    out->bit[110] = qs_gf263_xor_bits(state, &out->bit[110], &in->bit[225]);
    out->bit[111] = qs_gf263_copy_bit(state, &in->bit[187]);
    out->bit[112] = qs_gf263_xor_bits(state, &in->bit[56], &in->bit[141]);
    out->bit[112] = qs_gf263_xor_bits(state, &out->bit[112], &in->bit[226]);
    out->bit[113] = qs_gf263_copy_bit(state, &in->bit[188]);
    out->bit[114] = qs_gf263_xor_bits(state, &in->bit[57], &in->bit[142]);
    out->bit[114] = qs_gf263_xor_bits(state, &out->bit[114], &in->bit[227]);
    out->bit[115] = qs_gf263_copy_bit(state, &in->bit[189]);
    out->bit[116] = qs_gf263_xor_bits(state, &in->bit[58], &in->bit[143]);
    out->bit[116] = qs_gf263_xor_bits(state, &out->bit[116], &in->bit[228]);
    out->bit[117] = qs_gf263_copy_bit(state, &in->bit[190]);
    out->bit[118] = qs_gf263_xor_bits(state, &in->bit[59], &in->bit[144]);
    out->bit[118] = qs_gf263_xor_bits(state, &out->bit[118], &in->bit[229]);
    out->bit[119] = qs_gf263_copy_bit(state, &in->bit[191]);
    out->bit[120] = qs_gf263_xor_bits(state, &in->bit[60], &in->bit[145]);
    out->bit[120] = qs_gf263_xor_bits(state, &out->bit[120], &in->bit[230]);
    out->bit[121] = qs_gf263_copy_bit(state, &in->bit[192]);
    out->bit[122] = qs_gf263_xor_bits(state, &in->bit[61], &in->bit[146]);
    out->bit[122] = qs_gf263_xor_bits(state, &out->bit[122], &in->bit[231]);
    out->bit[123] = qs_gf263_copy_bit(state, &in->bit[193]);
    out->bit[124] = qs_gf263_xor_bits(state, &in->bit[62], &in->bit[147]);
    out->bit[124] = qs_gf263_xor_bits(state, &out->bit[124], &in->bit[232]);
    out->bit[125] = qs_gf263_copy_bit(state, &in->bit[194]);
    out->bit[126] = qs_gf263_xor_bits(state, &in->bit[63], &in->bit[148]);
    out->bit[126] = qs_gf263_xor_bits(state, &out->bit[126], &in->bit[233]);
    out->bit[127] = qs_gf263_copy_bit(state, &in->bit[195]);
    out->bit[128] = qs_gf263_xor_bits(state, &in->bit[64], &in->bit[149]);
    out->bit[128] = qs_gf263_xor_bits(state, &out->bit[128], &in->bit[234]);
    out->bit[129] = qs_gf263_copy_bit(state, &in->bit[196]);
    out->bit[130] = qs_gf263_xor_bits(state, &in->bit[65], &in->bit[150]);
    out->bit[130] = qs_gf263_xor_bits(state, &out->bit[130], &in->bit[235]);
    out->bit[131] = qs_gf263_copy_bit(state, &in->bit[197]);
    out->bit[132] = qs_gf263_xor_bits(state, &in->bit[66], &in->bit[151]);
    out->bit[132] = qs_gf263_xor_bits(state, &out->bit[132], &in->bit[236]);
    out->bit[133] = qs_gf263_copy_bit(state, &in->bit[198]);
    out->bit[134] = qs_gf263_xor_bits(state, &in->bit[67], &in->bit[152]);
    out->bit[134] = qs_gf263_xor_bits(state, &out->bit[134], &in->bit[237]);
    out->bit[135] = qs_gf263_copy_bit(state, &in->bit[199]);
    out->bit[136] = qs_gf263_xor_bits(state, &in->bit[68], &in->bit[153]);
    out->bit[136] = qs_gf263_xor_bits(state, &out->bit[136], &in->bit[238]);
    out->bit[137] = qs_gf263_copy_bit(state, &in->bit[200]);
    out->bit[138] = qs_gf263_xor_bits(state, &in->bit[69], &in->bit[154]);
    out->bit[138] = qs_gf263_xor_bits(state, &out->bit[138], &in->bit[239]);
    out->bit[139] = qs_gf263_copy_bit(state, &in->bit[201]);
    out->bit[140] = qs_gf263_xor_bits(state, &in->bit[70], &in->bit[155]);
    out->bit[140] = qs_gf263_xor_bits(state, &out->bit[140], &in->bit[240]);
    out->bit[141] = qs_gf263_copy_bit(state, &in->bit[202]);
    out->bit[142] = qs_gf263_xor_bits(state, &in->bit[71], &in->bit[156]);
    out->bit[142] = qs_gf263_xor_bits(state, &out->bit[142], &in->bit[241]);
    out->bit[143] = qs_gf263_copy_bit(state, &in->bit[203]);
    out->bit[144] = qs_gf263_xor_bits(state, &in->bit[72], &in->bit[157]);
    out->bit[144] = qs_gf263_xor_bits(state, &out->bit[144], &in->bit[242]);
    out->bit[145] = qs_gf263_copy_bit(state, &in->bit[204]);
    out->bit[146] = qs_gf263_xor_bits(state, &in->bit[73], &in->bit[158]);
    out->bit[146] = qs_gf263_xor_bits(state, &out->bit[146], &in->bit[243]);
    out->bit[147] = qs_gf263_copy_bit(state, &in->bit[205]);
    out->bit[148] = qs_gf263_xor_bits(state, &in->bit[74], &in->bit[159]);
    out->bit[148] = qs_gf263_xor_bits(state, &out->bit[148], &in->bit[244]);
    out->bit[149] = qs_gf263_copy_bit(state, &in->bit[206]);
    out->bit[150] = qs_gf263_xor_bits(state, &in->bit[75], &in->bit[160]);
    out->bit[150] = qs_gf263_xor_bits(state, &out->bit[150], &in->bit[245]);
    out->bit[151] = qs_gf263_copy_bit(state, &in->bit[207]);
    out->bit[152] = qs_gf263_xor_bits(state, &in->bit[76], &in->bit[161]);
    out->bit[152] = qs_gf263_xor_bits(state, &out->bit[152], &in->bit[246]);
    out->bit[153] = qs_gf263_copy_bit(state, &in->bit[208]);
    out->bit[154] = qs_gf263_xor_bits(state, &in->bit[77], &in->bit[162]);
    out->bit[154] = qs_gf263_xor_bits(state, &out->bit[154], &in->bit[247]);
    out->bit[155] = qs_gf263_copy_bit(state, &in->bit[209]);
    out->bit[156] = qs_gf263_xor_bits(state, &in->bit[78], &in->bit[163]);
    out->bit[156] = qs_gf263_xor_bits(state, &out->bit[156], &in->bit[248]);
    out->bit[157] = qs_gf263_copy_bit(state, &in->bit[210]);
    out->bit[158] = qs_gf263_xor_bits(state, &in->bit[79], &in->bit[164]);
    out->bit[158] = qs_gf263_xor_bits(state, &out->bit[158], &in->bit[249]);
    out->bit[159] = qs_gf263_copy_bit(state, &in->bit[211]);
    out->bit[160] = qs_gf263_xor_bits(state, &in->bit[80], &in->bit[165]);
    out->bit[160] = qs_gf263_xor_bits(state, &out->bit[160], &in->bit[250]);
    out->bit[161] = qs_gf263_copy_bit(state, &in->bit[212]);
    out->bit[162] = qs_gf263_xor_bits(state, &in->bit[81], &in->bit[166]);
    out->bit[162] = qs_gf263_xor_bits(state, &out->bit[162], &in->bit[251]);
    out->bit[163] = qs_gf263_copy_bit(state, &in->bit[213]);
    out->bit[164] = qs_gf263_xor_bits(state, &in->bit[82], &in->bit[167]);
    out->bit[164] = qs_gf263_xor_bits(state, &out->bit[164], &in->bit[252]);
    out->bit[165] = qs_gf263_copy_bit(state, &in->bit[214]);
    out->bit[166] = qs_gf263_xor_bits(state, &in->bit[83], &in->bit[168]);
    out->bit[166] = qs_gf263_xor_bits(state, &out->bit[166], &in->bit[253]);
    out->bit[167] = qs_gf263_copy_bit(state, &in->bit[215]);
    out->bit[168] = qs_gf263_xor_bits(state, &in->bit[84], &in->bit[169]);
    out->bit[168] = qs_gf263_xor_bits(state, &out->bit[168], &in->bit[254]);
    out->bit[169] = qs_gf263_copy_bit(state, &in->bit[216]);
    out->bit[170] = qs_gf263_xor_bits(state, &in->bit[85], &in->bit[170]);
    out->bit[170] = qs_gf263_xor_bits(state, &out->bit[170], &in->bit[255]);
    out->bit[171] = qs_gf263_copy_bit(state, &in->bit[217]);
    out->bit[172] = qs_gf263_xor_bits(state, &in->bit[86], &in->bit[171]);
    out->bit[172] = qs_gf263_xor_bits(state, &out->bit[172], &in->bit[256]);
    out->bit[173] = qs_gf263_copy_bit(state, &in->bit[218]);
    out->bit[174] = qs_gf263_xor_bits(state, &in->bit[87], &in->bit[172]);
    out->bit[174] = qs_gf263_xor_bits(state, &out->bit[174], &in->bit[257]);
    out->bit[175] = qs_gf263_copy_bit(state, &in->bit[219]);
    out->bit[176] = qs_gf263_xor_bits(state, &in->bit[88], &in->bit[173]);
    out->bit[176] = qs_gf263_xor_bits(state, &out->bit[176], &in->bit[258]);
    out->bit[177] = qs_gf263_copy_bit(state, &in->bit[220]);
    out->bit[178] = qs_gf263_xor_bits(state, &in->bit[89], &in->bit[174]);
    out->bit[178] = qs_gf263_xor_bits(state, &out->bit[178], &in->bit[259]);
    out->bit[179] = qs_gf263_copy_bit(state, &in->bit[221]);
    out->bit[180] = qs_gf263_xor_bits(state, &in->bit[90], &in->bit[175]);
    out->bit[180] = qs_gf263_xor_bits(state, &out->bit[180], &in->bit[260]);
    out->bit[181] = qs_gf263_copy_bit(state, &in->bit[222]);
    out->bit[182] = qs_gf263_xor_bits(state, &in->bit[91], &in->bit[176]);
    out->bit[182] = qs_gf263_xor_bits(state, &out->bit[182], &in->bit[261]);
    out->bit[183] = qs_gf263_copy_bit(state, &in->bit[223]);
    out->bit[184] = qs_gf263_xor_bits(state, &in->bit[92], &in->bit[177]);
    out->bit[184] = qs_gf263_xor_bits(state, &out->bit[184], &in->bit[262]);
    out->bit[185] = qs_gf263_copy_bit(state, &in->bit[224]);
    out->bit[186] = qs_gf263_xor_bits(state, &in->bit[93], &in->bit[178]);
    out->bit[187] = qs_gf263_copy_bit(state, &in->bit[225]);
    out->bit[188] = qs_gf263_xor_bits(state, &in->bit[94], &in->bit[179]);
    out->bit[189] = qs_gf263_copy_bit(state, &in->bit[226]);
    out->bit[190] = qs_gf263_xor_bits(state, &in->bit[95], &in->bit[180]);
    out->bit[191] = qs_gf263_copy_bit(state, &in->bit[227]);
    out->bit[192] = qs_gf263_xor_bits(state, &in->bit[96], &in->bit[181]);
    out->bit[193] = qs_gf263_copy_bit(state, &in->bit[228]);
    out->bit[194] = qs_gf263_xor_bits(state, &in->bit[97], &in->bit[182]);
    out->bit[195] = qs_gf263_copy_bit(state, &in->bit[229]);
    out->bit[196] = qs_gf263_xor_bits(state, &in->bit[98], &in->bit[183]);
    out->bit[197] = qs_gf263_copy_bit(state, &in->bit[230]);
    out->bit[198] = qs_gf263_xor_bits(state, &in->bit[99], &in->bit[184]);
    out->bit[199] = qs_gf263_copy_bit(state, &in->bit[231]);
    out->bit[200] = qs_gf263_xor_bits(state, &in->bit[100], &in->bit[185]);
    out->bit[201] = qs_gf263_copy_bit(state, &in->bit[232]);
    out->bit[202] = qs_gf263_xor_bits(state, &in->bit[101], &in->bit[186]);
    out->bit[203] = qs_gf263_copy_bit(state, &in->bit[233]);
    out->bit[204] = qs_gf263_xor_bits(state, &in->bit[102], &in->bit[187]);
    out->bit[205] = qs_gf263_copy_bit(state, &in->bit[234]);
    out->bit[206] = qs_gf263_xor_bits(state, &in->bit[103], &in->bit[188]);
    out->bit[207] = qs_gf263_copy_bit(state, &in->bit[235]);
    out->bit[208] = qs_gf263_xor_bits(state, &in->bit[104], &in->bit[189]);
    out->bit[209] = qs_gf263_copy_bit(state, &in->bit[236]);
    out->bit[210] = qs_gf263_xor_bits(state, &in->bit[105], &in->bit[190]);
    out->bit[211] = qs_gf263_copy_bit(state, &in->bit[237]);
    out->bit[212] = qs_gf263_xor_bits(state, &in->bit[106], &in->bit[191]);
    out->bit[213] = qs_gf263_copy_bit(state, &in->bit[238]);
    out->bit[214] = qs_gf263_xor_bits(state, &in->bit[107], &in->bit[192]);
    out->bit[215] = qs_gf263_copy_bit(state, &in->bit[239]);
    out->bit[216] = qs_gf263_xor_bits(state, &in->bit[108], &in->bit[193]);
    out->bit[217] = qs_gf263_copy_bit(state, &in->bit[240]);
    out->bit[218] = qs_gf263_xor_bits(state, &in->bit[109], &in->bit[194]);
    out->bit[219] = qs_gf263_copy_bit(state, &in->bit[241]);
    out->bit[220] = qs_gf263_xor_bits(state, &in->bit[110], &in->bit[195]);
    out->bit[221] = qs_gf263_copy_bit(state, &in->bit[242]);
    out->bit[222] = qs_gf263_xor_bits(state, &in->bit[111], &in->bit[196]);
    out->bit[223] = qs_gf263_copy_bit(state, &in->bit[243]);
    out->bit[224] = qs_gf263_xor_bits(state, &in->bit[112], &in->bit[197]);
    out->bit[225] = qs_gf263_copy_bit(state, &in->bit[244]);
    out->bit[226] = qs_gf263_xor_bits(state, &in->bit[113], &in->bit[198]);
    out->bit[227] = qs_gf263_copy_bit(state, &in->bit[245]);
    out->bit[228] = qs_gf263_xor_bits(state, &in->bit[114], &in->bit[199]);
    out->bit[229] = qs_gf263_copy_bit(state, &in->bit[246]);
    out->bit[230] = qs_gf263_xor_bits(state, &in->bit[115], &in->bit[200]);
    out->bit[231] = qs_gf263_copy_bit(state, &in->bit[247]);
    out->bit[232] = qs_gf263_xor_bits(state, &in->bit[116], &in->bit[201]);
    out->bit[233] = qs_gf263_copy_bit(state, &in->bit[248]);
    out->bit[234] = qs_gf263_xor_bits(state, &in->bit[117], &in->bit[202]);
    out->bit[235] = qs_gf263_copy_bit(state, &in->bit[249]);
    out->bit[236] = qs_gf263_xor_bits(state, &in->bit[118], &in->bit[203]);
    out->bit[237] = qs_gf263_copy_bit(state, &in->bit[250]);
    out->bit[238] = qs_gf263_xor_bits(state, &in->bit[119], &in->bit[204]);
    out->bit[239] = qs_gf263_copy_bit(state, &in->bit[251]);
    out->bit[240] = qs_gf263_xor_bits(state, &in->bit[120], &in->bit[205]);
    out->bit[241] = qs_gf263_copy_bit(state, &in->bit[252]);
    out->bit[242] = qs_gf263_xor_bits(state, &in->bit[121], &in->bit[206]);
    out->bit[243] = qs_gf263_copy_bit(state, &in->bit[253]);
    out->bit[244] = qs_gf263_xor_bits(state, &in->bit[122], &in->bit[207]);
    out->bit[245] = qs_gf263_copy_bit(state, &in->bit[254]);
    out->bit[246] = qs_gf263_xor_bits(state, &in->bit[123], &in->bit[208]);
    out->bit[247] = qs_gf263_copy_bit(state, &in->bit[255]);
    out->bit[248] = qs_gf263_xor_bits(state, &in->bit[124], &in->bit[209]);
    out->bit[249] = qs_gf263_copy_bit(state, &in->bit[256]);
    out->bit[250] = qs_gf263_xor_bits(state, &in->bit[125], &in->bit[210]);
    out->bit[251] = qs_gf263_copy_bit(state, &in->bit[257]);
    out->bit[252] = qs_gf263_xor_bits(state, &in->bit[126], &in->bit[211]);
    out->bit[253] = qs_gf263_copy_bit(state, &in->bit[258]);
    out->bit[254] = qs_gf263_xor_bits(state, &in->bit[127], &in->bit[212]);
    out->bit[255] = qs_gf263_copy_bit(state, &in->bit[259]);
    out->bit[256] = qs_gf263_xor_bits(state, &in->bit[128], &in->bit[213]);
    out->bit[257] = qs_gf263_copy_bit(state, &in->bit[260]);
    out->bit[258] = qs_gf263_xor_bits(state, &in->bit[129], &in->bit[214]);
    out->bit[259] = qs_gf263_copy_bit(state, &in->bit[261]);
    out->bit[260] = qs_gf263_xor_bits(state, &in->bit[130], &in->bit[215]);
    out->bit[261] = qs_gf263_copy_bit(state, &in->bit[262]);
    out->bit[262] = qs_gf263_xor_bits(state, &in->bit[131], &in->bit[216]);
}

void qs_gf263_square(
    const quicksilver_state* state, qs_gf263_bits* out, const qs_gf263_bits* in)
{
    if (out == in) {
        qs_gf263_bits tmp;
        qs_gf263_square_noalias(state, &tmp, in);
        *out = tmp;
    } else {
        qs_gf263_square_noalias(state, out, in);
    }
}

void qs_gf263_frobenius(
    const quicksilver_state* state, qs_gf263_bits* out,
    const qs_gf263_bits* in, unsigned exponent)
{
    if (exponent == 0) {
        for (size_t i = 0; i < BLOCK263_BITS; ++i)
            out->bit[i] = qs_gf263_copy_bit(state, &in->bit[i]);
        return;
    }
    if (exponent == 1) {
        qs_gf263_square(state, out, in);
        return;
    }

    qs_gf263_bits scratch[2];
    const qs_gf263_bits* current = in;
    for (unsigned round = 0; round < exponent - 1; ++round) {
        qs_gf263_bits* next = &scratch[round & 1];
        qs_gf263_square_noalias(state, next, current);
        current = next;
    }
    qs_gf263_square_noalias(state, out, current);
}

void qs_gf263_shift_reduce(
    const quicksilver_state* state, qs_gf263_bits* out,
    const qs_gf263_bits* in, size_t shift)
{
    assert(shift < BLOCK263_BITS);
    quicksilver_vec_gf2 tmp[2 * BLOCK263_BITS - 1];

    for (size_t i = 0; i < 2 * BLOCK263_BITS - 1; ++i) {
        tmp[i] = quicksilver_zero_gf2();
    }
    for (size_t bit = 0; bit < BLOCK263_BITS; ++bit) {
        tmp[bit + shift] = quicksilver_add_gf2(state, tmp[bit + shift], in->bit[bit]);
    }
    for (int pos = 2 * BLOCK263_BITS - 2; pos >= BLOCK263_BITS; --pos) {
        const quicksilver_vec_gf2 term = tmp[pos];
        tmp[pos] = quicksilver_zero_gf2();
        tmp[pos - BLOCK263_BITS + BLOCK263_REDUCTION_TERM] = quicksilver_add_gf2(
            state, tmp[pos - BLOCK263_BITS + BLOCK263_REDUCTION_TERM], term);
        tmp[pos - BLOCK263_BITS] = quicksilver_add_gf2(
            state, tmp[pos - BLOCK263_BITS], term);
    }
    for (size_t bit = 0; bit < BLOCK263_BITS; ++bit) {
        out->bit[bit] = tmp[bit];
    }
}

GW_STATIC_ASSERT(SECURITY_PARAM == 256 && POLY_VEC_LEN == 1,
               "GF263 hash fusion requires scalar GF256 authentication");
GW_STATIC_ASSERT(BLOCK263_BITS == 263 && BLOCK263_REDUCTION_TERM == 93,
               "GF263 projection recurrence assumes t^263 + t^93 + 1");
GW_STATIC_ASSERT(HASHER_GFSECPAR_KEY_POWS == 2,
               "GF263 block append assumes two-coefficient hash groups");

typedef struct {
    poly_secpar_vec weights[2 * BLOCK263_BITS - 1];
    poly_secpar_vec r, defect, r262, r263, r264;
} qs_gf263_projection_key;

static inline poly_secpar_vec qs_gf263_mac_mul(poly_secpar_vec a, poly_secpar_vec b)
{
    return poly_2secpar_reduce_secpar(poly_secpar_mul(a, b));
}

static void qs_gf263_projection_init(qs_gf263_projection_key* key, poly_secpar_vec r)
{

    key->r = r;
    key->weights[262] = poly_secpar_set_low32(1);
    for (int k = 261; k >= 0; --k)
        key->weights[k] = qs_gf263_mac_mul(r, key->weights[k + 1]);
    for (size_t k = 263; k <= 524; ++k)
        key->weights[k] = poly_secpar_add(key->weights[k - 263], key->weights[k - 170]);
    key->r262 = key->weights[0];
    key->r263 = qs_gf263_mac_mul(r, key->r262);
    key->r264 = qs_gf263_mac_mul(r, key->r263);
    key->defect = poly_secpar_add(poly_secpar_set_low32(1),
        poly_secpar_add(key->r263, key->weights[92]));
}

static poly_secpar_vec qs_gf263_project_product(
    const qs_gf263_projection_key* key,
    const poly_secpar_vec a[BLOCK263_BITS], const poly_secpar_vec b[BLOCK263_BITS])
{

    poly_2secpar_vec initial = poly_2secpar_from_secpar(b[0]);
    for (size_t j = 1; j < BLOCK263_BITS; ++j)
        initial = poly_2secpar_add(initial, poly_secpar_mul(key->weights[262 + j], b[j]));
    poly_secpar_vec v = poly_2secpar_reduce_secpar(initial);
    poly_2secpar_vec result = poly_secpar_mul(a[262], v);

    for (int i = 261; i >= 0; --i) {
        poly_secpar_vec correction = b[262 - i];
        if (i >= 170)
            correction = poly_secpar_add(correction, b[432 - i]);
        v = poly_2secpar_reduce_secpar(poly_2secpar_add(
            poly_secpar_mul(key->r, v), poly_secpar_mul(key->defect, correction)));
        result = poly_2secpar_add(result, poly_secpar_mul(a[i], v));
    }
    return poly_2secpar_reduce_secpar(result);
}

static void qs_gf263_hash_append(
    hasher_gfsecpar_state* state, const qs_gf263_projection_key* key,
    poly_secpar_vec contribution)
{

    const poly_secpar_vec previous = poly_2secpar_reduce_secpar(state->state);
    poly_secpar_vec next;
    if (state->pow == 0) {
        next = poly_secpar_add(qs_gf263_mac_mul(key->r262, previous), contribution);
        state->pow = -1;
    } else {
        assert(state->pow == -1 || state->pow == 1);
        const poly_secpar_vec factor = state->pow == -1 ? key->r264 : key->r262;
        next = poly_2secpar_reduce_secpar(poly_2secpar_add(
            poly_secpar_mul(factor, previous), poly_secpar_mul(key->r, contribution)));
        state->pow = 0;
    }
    state->state = poly_2secpar_from_secpar(next);
}

static void qs_gf263_hash64_append(
    hasher_gfsecpar_64_state* state, const qs_gf263_projection_key* key,
    poly_secpar_vec contribution)
{

    state->state = poly_secpar_add(qs_gf263_mac_mul(key->r263, state->state), contribution);
}

void qs_gf263_mul_constraints(
    quicksilver_state* state, const qs_gf263_bits* x,
    const qs_gf263_bits* y, const qs_gf263_bits* z, size_t count)
{
    if (count == 0)
        return;
    qs_gf263_projection_key keys[2];
    qs_gf263_projection_init(&keys[0], state->key_secpar.key_pows[0]);
    qs_gf263_projection_init(&keys[1], poly_secpar_from_64(state->key_64.key));

    for (size_t relation = 0; relation < count; ++relation) {
        poly_secpar_vec a[BLOCK263_BITS], b[BLOCK263_BITS];
        poly_secpar_vec a_plus_value[BLOCK263_BITS], b_plus_value[BLOCK263_BITS];
        for (size_t i = 0; i < BLOCK263_BITS; ++i) {
            a[i] = x[relation].bit[i].mac;
            b[i] = y[relation].bit[i].mac;
            if (!state->verifier) {
                a_plus_value[i] = poly_secpar_add(a[i], poly_secpar_from_1(x[relation].bit[i].value));
                b_plus_value[i] = poly_secpar_add(b[i], poly_secpar_from_1(y[relation].bit[i].value));
            }
        }

        for (size_t h = 0; h < 2; ++h) {
            const qs_gf263_projection_key* key = &keys[h];
            poly_secpar_vec constant = qs_gf263_project_product(key, a, b);
            poly_2secpar_vec z_sum = poly_2secpar_set_zero();
            for (size_t i = 0; i < BLOCK263_BITS; ++i) {
                poly_secpar_vec zi = z[relation].bit[i].mac;
                if (!state->verifier)
                    zi = poly_secpar_add(zi, poly_secpar_from_1(z[relation].bit[i].value));
                z_sum = poly_2secpar_add(z_sum, poly_secpar_mul(key->weights[i], zi));
            }
            const poly_secpar_vec z_projection = poly_2secpar_reduce_secpar(z_sum);
            poly_secpar_vec linear = poly_secpar_set_zero();
            if (state->verifier) {

                constant = poly_secpar_add(constant, qs_gf263_mac_mul(state->delta, z_projection));
            } else {

                linear = poly_secpar_add(constant, poly_secpar_add(z_projection,
                    qs_gf263_project_product(key, a_plus_value, b_plus_value)));
            }
            if (h == 0) {
                qs_gf263_hash_append(&state->state_secpar_const, key, constant);
                if (!state->verifier)
                    qs_gf263_hash_append(&state->state_secpar_linear, key, linear);
            } else {
                qs_gf263_hash64_append(&state->state_64_const, key, constant);
                if (!state->verifier)
                    qs_gf263_hash64_append(&state->state_64_linear, key, linear);
            }
        }
    }
}

void qs_gf263_mul_constraint(
    quicksilver_state* state, const qs_gf263_bits* x,
    const qs_gf263_bits* y, const qs_gf263_bits* z)
{
    qs_gf263_mul_constraints(state, x, y, z, 1);
}

void qs_gf263_dot_constraint(
    quicksilver_state* state, const qs_gf263_bits* y, const block263* alpha)
{
    quicksilver_vec_gf2 dot = quicksilver_zero_gf2();
    for (size_t bit = 0; bit < BLOCK263_BITS; ++bit) {
        if ((alpha->data[bit >> 6] >> (bit & 63)) & 1ULL) {
            dot = quicksilver_add_gf2(state, dot, y->bit[bit]);
        }
    }
    quicksilver_inverse_constraint(
        state, quicksilver_combine_1_bit(state, dot), quicksilver_one_gfsecpar(state));
}
