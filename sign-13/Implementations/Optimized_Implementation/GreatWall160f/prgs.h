#ifndef PRGS_H
#define PRGS_H

#include "aes.h"
#include "hash.h"

#define DEFINE_PRG_AES192_CTR(name) \
    typedef aes_round_keys prg_##name##_key; \
    typedef block128 prg_##name##_iv; \
    typedef block128 prg_##name##_block; \
    typedef char prg_##name##_fixed_key; \
    static ALWAYS_INLINE void prg_##name##_init( \
        prg_##name##_key* prgs, const prg_##name##_fixed_key* fixed_key, \
        const block_secpar* keys, const prg_##name##_iv* ivs, size_t num_keys, \
        uint32_t num_blocks, uint32_t counter, prg_##name##_block* output) \
    { \
        (void)fixed_key; \
        if (!num_keys) return; \
        assert(num_blocks <= 3); \
        if (!num_blocks) { \
            for (size_t i = 0; i < num_keys; ++i) { \
                aes_keygen(&prgs[i], keys[i]); prgs[i].iv = ivs[i]; \
            } \
            return; \
        } \
        for (size_t i = 0; i < num_keys; i += AES_PREFERRED_WIDTH) { \
            size_t n = num_keys - i; \
            if (n > AES_PREFERRED_WIDTH) n = AES_PREFERRED_WIDTH; \
            aes_keygen_ctr(prgs + i, keys + i, ivs + i, n, num_blocks, counter, output + i * num_blocks); \
        } \
    } \
    static ALWAYS_INLINE void prg_##name##_gen( \
        const prg_##name##_key* prgs, const prg_##name##_fixed_key* fixed_key, \
        size_t num_keys, uint32_t num_blocks, uint32_t counter, prg_##name##_block* output) \
    { \
        (void)fixed_key; \
        if (!num_keys || !num_blocks) return; \
        assert(num_blocks <= 3); \
        for (size_t i = 0; i < num_keys; i += AES_PREFERRED_WIDTH) { \
            size_t n = num_keys - i; \
            if (n > AES_PREFERRED_WIDTH) n = AES_PREFERRED_WIDTH; \
            aes_ctr(prgs + i, n, num_blocks, counter, output + i * num_blocks); \
        } \
    }

#define PRG_VOLE_PREFERRED_WIDTH AES_PREFERRED_WIDTH
#define PRG_VOLE_PREFERRED_WIDTH_SHIFT AES_PREFERRED_WIDTH_SHIFT
#define PRG_TREE_PREFERRED_WIDTH AES_PREFERRED_WIDTH
#define PRG_TREE_PREFERRED_WIDTH_SHIFT AES_PREFERRED_WIDTH_SHIFT
DEFINE_PRG_AES192_CTR(vole)
DEFINE_PRG_AES192_CTR(tree)
#undef DEFINE_PRG_AES192_CTR

typedef char prg_leaf_key;
typedef block128 prg_leaf_iv;
typedef block_secpar prg_leaf_block;
typedef char prg_leaf_fixed_key;
#define PRG_LEAF_PREFERRED_WIDTH_SHIFT 3
#define PRG_LEAF_PREFERRED_WIDTH (1 << PRG_LEAF_PREFERRED_WIDTH_SHIFT)
static ALWAYS_INLINE void prg_leaf_init(
    prg_leaf_key* prgs, const prg_leaf_fixed_key* fixed_key,
    const block_secpar* keys, const prg_leaf_iv* ivs, size_t num_keys,
    uint32_t num_blocks, uint32_t counter, prg_leaf_block* output)
{
    (void)prgs; (void)fixed_key;
    assert(counter == 0);
    shake_prg(keys, ivs, num_keys, num_blocks * sizeof(block_secpar), (uint8_t*)output);
}
static ALWAYS_INLINE void prg_leaf_gen(
    const prg_leaf_key* prgs, const prg_leaf_fixed_key* fixed_key,
    size_t num_keys, uint32_t num_blocks, uint32_t counter, prg_leaf_block* output)
{
    (void)prgs; (void)fixed_key; (void)counter; (void)output;
    assert(num_keys == 0 || num_blocks == 0);
}
static ALWAYS_INLINE void init_fixed_keys(
    prg_tree_fixed_key* tree, prg_leaf_fixed_key* leaf, block_secpar iv)
{
    (void)tree; (void)leaf; (void)iv;
}
#endif
