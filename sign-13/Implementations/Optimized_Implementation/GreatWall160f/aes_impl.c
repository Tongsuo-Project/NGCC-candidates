#include "aes.h"

#define KEYGEN_WIDTH 4

typedef struct {
    block128 key_slices[6];
    block128 next_sbox;
} aes_keygen_state;

ALWAYS_INLINE void cumulative_xor(block128* x, size_t n)
{
    for (size_t i = 0; i + 1 < n; ++i)
        x[i + 1] = block128_xor(x[i], x[i + 1]);
}

ALWAYS_INLINE void shift2_mod6(block128* x)
{
    block128 output[6];
    memcpy(output + 2, x, 4 * sizeof(block128));
    memcpy(output, x + 4, 2 * sizeof(block128));
    memcpy(x, output, sizeof(output));
}

ALWAYS_INLINE void aes_keygen_init(
    aes_keygen_state* states, aes_round_keys* aeses,
    const block_secpar* seeds, size_t num_keys)
{
    for (size_t i = 0; i < num_keys; i += KEYGEN_WIDTH) {
        aes_keygen_state* state = &states[i / KEYGEN_WIDTH];
        size_t n = num_keys - i < KEYGEN_WIDTH ? num_keys - i : KEYGEN_WIDTH;
        block192 keys[KEYGEN_WIDTH] = {0};
        for (size_t j = 0; j < n; ++j) {

            memcpy(&keys[j], &seeds[i + j], sizeof(block_secpar));
            memcpy(&aeses[i + j].keys[0], &keys[j], sizeof(block192));
        }
        block128 low[KEYGEN_WIDTH];
        for (size_t j = 0; j < KEYGEN_WIDTH; ++j)
            memcpy(&low[j], &keys[j], sizeof(block128));
        block128 hi01 = _mm_set_epi64x(keys[1].data[2], keys[0].data[2]);
        block128 hi23 = _mm_set_epi64x(keys[3].data[2], keys[2].data[2]);
        transpose4x4_32(state->key_slices, low);
        transpose4x2_32(state->key_slices + 4, hi01, hi23);
        state->next_sbox = state->key_slices[5];
        shift2_mod6(state->key_slices);
        cumulative_xor(state->key_slices + 2, 4);
    }
}

ALWAYS_INLINE void aes_keygen_round(
    aes_keygen_state* states, aes_round_keys* aeses, size_t num_keys, int round)
{
    for (size_t i = 0; i < num_keys; i += KEYGEN_WIDTH) {
        aes_keygen_state* state = &states[i / KEYGEN_WIDTH];
        size_t n = num_keys - i < KEYGEN_WIDTH ? num_keys - i : KEYGEN_WIDTH;
        if (round % 3 < 2) {
            const block128 rot_word_inv_shift_rows = _mm_setr_epi8(
                1,14,11,4, 5,2,15,8, 9,6,3,12, 13,10,7,0);
            int idx = (2 * round + 1) / 3;
            block128 constant = _mm_set1_epi32(aes_round_constants[idx - 1]);
            block128 sbox = _mm_aesenclast_si128(
                _mm_shuffle_epi8(state->next_sbox, rot_word_inv_shift_rows), constant);
            size_t first = round % 3 == 1 ? 2 : 0;
            for (size_t j = first; j < 6; ++j)
                state->key_slices[j] = block128_xor(state->key_slices[j], sbox);
        }
        if (round % 3 == 2)
            state->next_sbox = state->key_slices[3];
        else if (round % 3 == 0)
            state->next_sbox = state->key_slices[5];

        block128 round_keys[KEYGEN_WIDTH];
        transpose4x4_32(round_keys, state->key_slices);
        for (size_t j = 0; j < n; ++j)
            aeses[i + j].keys[round] = round_keys[j];
        shift2_mod6(state->key_slices);
        size_t next = (round % 3 + 1) * 2;
        cumulative_xor(state->key_slices + 1, next - 1);
        cumulative_xor(state->key_slices + next, 6 - next);
    }
}

ALWAYS_INLINE void aes_keygen_impl(
    aes_round_keys* aeses, const block_secpar* keys,
    size_t num_keys, uint32_t num_blocks, block128* output)
{
    aes_keygen_state states[AES_PREFERRED_WIDTH / KEYGEN_WIDTH];
    aes_keygen_init(states, aeses, keys, num_keys);
    aes_round(aeses, output, num_keys, num_blocks, 0);

    for (int round = 1; round <= AES_ROUNDS - 1; round += 3) {
        aes_keygen_round(states, aeses, num_keys, round);
        aes_round(aeses, output, num_keys, num_blocks, round);
        aes_keygen_round(states, aeses, num_keys, round + 1);
        aes_round(aeses, output, num_keys, num_blocks, round + 1);
        if (round + 2 < AES_ROUNDS) {
            aes_keygen_round(states, aeses, num_keys, round + 2);
            aes_round(aeses, output, num_keys, num_blocks, round + 2);
        }
    }
    aes_keygen_round(states, aeses, num_keys, AES_ROUNDS);
    aes_round(aeses, output, num_keys, num_blocks, AES_ROUNDS);
}

#define DEF_AES_KEYGEN_IMPL_KB(n, b) \
    void aes_keygen_impl_##n##_##b(aes_round_keys* restrict aeses, \
        const block_secpar* restrict keys, block128* restrict output) \
    { \
        assert(n <= AES_PREFERRED_WIDTH); \
        if (n <= AES_PREFERRED_WIDTH) aes_keygen_impl(aeses, keys, n, b, output); \
    }
#define DEF_AES_KEYGEN_IMPL_K(n) \
    DEF_AES_KEYGEN_IMPL_KB(n, 1) \
    DEF_AES_KEYGEN_IMPL_KB(n, 2) \
    DEF_AES_KEYGEN_IMPL_KB(n, 3)

DEF_AES_KEYGEN_IMPL_K(1)
DEF_AES_KEYGEN_IMPL_K(2)
DEF_AES_KEYGEN_IMPL_K(3)
DEF_AES_KEYGEN_IMPL_K(4)
DEF_AES_KEYGEN_IMPL_K(5)
DEF_AES_KEYGEN_IMPL_K(6)
DEF_AES_KEYGEN_IMPL_K(7)
DEF_AES_KEYGEN_IMPL_K(8)
DEF_AES_KEYGEN_IMPL_K(9)
DEF_AES_KEYGEN_IMPL_K(10)
DEF_AES_KEYGEN_IMPL_K(11)
DEF_AES_KEYGEN_IMPL_K(12)
DEF_AES_KEYGEN_IMPL_K(13)
DEF_AES_KEYGEN_IMPL_K(14)
DEF_AES_KEYGEN_IMPL_K(15)
DEF_AES_KEYGEN_IMPL_K(16)

void aes_keygen(aes_round_keys* round_keys, block_secpar key)
{
    block128 unused;
    aes_keygen_impl(round_keys, &key, 1, 0, &unused);
}
