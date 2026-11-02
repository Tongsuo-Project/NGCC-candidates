#include "aes.h"

unsigned char aes_round_constants[] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
};

extern inline void aes_round_function(
    const aes_round_keys* restrict round_keys, block128* restrict block,
    block128* restrict after_sbox, int round);
extern ALWAYS_INLINE void aes_keygen_ctr(
    aes_round_keys* restrict aeses, const block_secpar* restrict keys,
    const block128* restrict ivs, size_t num_keys, uint32_t num_blocks,
    uint32_t counter, block128* restrict output);
extern inline void aes_ctr(
    const aes_round_keys* restrict aeses, size_t num_keys, uint32_t num_blocks,
    uint32_t counter, block128* restrict output);
