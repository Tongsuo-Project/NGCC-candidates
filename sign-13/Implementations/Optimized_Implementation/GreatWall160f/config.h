#ifndef CONFIG_H
#define CONFIG_H

#include "c99_compat.h"

/* Experimental construction choices, NOT an authoritative Pylon160 profile. */
#define SECURITY_PARAM 160
#define GREATWALL_INSTANCE_NAME "GreatWall160f"
#define OWF_GREATWALL

#define PRG_AES_CTR
#define TREE_PRG_AES_CTR
#define LEAF_PRG_SHAKE
#define BITS_PER_WITNESS 20
#define ZERO_BITS_IN_CHALLENGE_3 0
#define USE_IMPROVED_VECTOR_COMMITMENTS 1
#define BATCH_VECTOR_OPENING_SEEDS_TRESHOLD 160
#define RANDOM_ORACLE_SHA3
#define XKCP_has_KeccakP1600
#define XKCP_has_KeccakP1600times2
#define XKCP_has_KeccakP1600times4
#define XKCP_has_KeccakP1600times8

#endif
