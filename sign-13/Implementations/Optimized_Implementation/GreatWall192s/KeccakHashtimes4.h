#ifndef _KeccakHashInterfacetimes4_h_
#define _KeccakHashInterfacetimes4_h_

#include "config.h"
#ifdef XKCP_has_KeccakP1600times4

#if !defined(SUPERCOP)
#include "KeccakHash.h"
#else
#include <libkeccak.a.headers/KeccakHash.h>
#endif
#include "KeccakSpongetimes4.h"

typedef struct {
    KeccakWidth1600times4_SpongeInstance sponge;
    unsigned int fixedOutputLength;
    unsigned char delimitedSuffix;
} Keccak_HashInstancetimes4;

HashReturn Keccak_HashInitializetimes4(Keccak_HashInstancetimes4* instance, unsigned int rate,
    unsigned int capacity, unsigned int hashbitlen, unsigned char delimitedSuffix);
HashReturn Keccak_HashUpdatetimes4(Keccak_HashInstancetimes4* instance, const BitSequence** data, BitLength databitlen);
HashReturn Keccak_HashFinaltimes4(Keccak_HashInstancetimes4* instance, BitSequence** hashval);
HashReturn Keccak_HashSqueezetimes4(Keccak_HashInstancetimes4* instance, BitSequence** data, BitLength databitlen);

#define Keccak_HashInitializetimes4_SHAKE128(hashInstance)        Keccak_HashInitializetimes4(hashInstance, 1344,  256,   0, 0x1F)

#define Keccak_HashInitializetimes4_SHAKE256(hashInstance)        Keccak_HashInitializetimes4(hashInstance, 1088,  512,   0, 0x1F)

#define Keccak_HashInitializetimes4_SHA3_224(hashInstance)        Keccak_HashInitializetimes4(hashInstance, 1152,  448, 224, 0x06)

#define Keccak_HashInitializetimes4_SHA3_256(hashInstance)        Keccak_HashInitializetimes4(hashInstance, 1088,  512, 256, 0x06)

#define Keccak_HashInitializetimes4_SHA3_384(hashInstance)        Keccak_HashInitializetimes4(hashInstance,  832,  768, 384, 0x06)

#define Keccak_HashInitializetimes4_SHA3_512(hashInstance)        Keccak_HashInitializetimes4(hashInstance,  576, 1024, 512, 0x06)

#else
#error This requires an implementation of Keccak-p[1600]x4
#endif

#endif
