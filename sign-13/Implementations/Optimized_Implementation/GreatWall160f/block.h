#ifndef GREATWALL160_BLOCK_H
#define GREATWALL160_BLOCK_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "config.h"
#include "gf163.h"
#include "block_impl.h"

typedef struct { uint32_t data[5]; } block160;
typedef struct { uint32_t data[10]; } block320;
typedef block160 block_secpar;
typedef block320 block_2secpar;
#define BLOCK_SECPAR_BYTES 20
#define BLOCK_2SECPAR_BYTES 40

#define BLOCK_SECPAR_LEN 2
#define BLOCK_2SECPAR_LEN 3
GW_STATIC_ASSERT(sizeof(block_secpar) == 20, "GF160 blocks must have 20-byte stride.");
GW_STATIC_ASSERT(sizeof(block_2secpar) == 40, "GF320 blocks must have 40-byte stride.");
GW_STATIC_ASSERT(sizeof(block128) == 16, "Padding in block128.");
GW_STATIC_ASSERT(sizeof(block256) == 32, "Padding in block256.");

inline block160 block160_xor(block160 x, block160 y) { block160 z; for(size_t i=0;i<5;++i) z.data[i]=x.data[i]^y.data[i]; return z; }
inline block160 block160_and(block160 x, block160 y) { block160 z; for(size_t i=0;i<5;++i) z.data[i]=x.data[i]&y.data[i]; return z; }
inline block160 block160_set_zero(void) { block160 z={{0}}; return z; }
inline block160 block160_set_all_8(uint8_t x) { block160 z; memset(&z,x,sizeof(z)); return z; }
inline block160 block160_set_low32(uint32_t x) { block160 z={{0}}; z.data[0]=x; return z; }
inline block160 block160_set_low64(uint64_t x) { block160 z={{0}}; memcpy(&z,&x,8); return z; }
inline bool block160_any_zeros(block160 x) { const uint8_t* p=(const uint8_t*)&x; unsigned any=0; for(size_t i=0;i<sizeof(x);++i) any|=(p[i]==0); return any!=0; }

inline block320 block320_xor(block320 x, block320 y) { block320 z; for(size_t i=0;i<10;++i) z.data[i]=x.data[i]^y.data[i]; return z; }
inline block320 block320_and(block320 x, block320 y) { block320 z; for(size_t i=0;i<10;++i) z.data[i]=x.data[i]&y.data[i]; return z; }
inline block320 block320_set_zero(void) { block320 z={{0}}; return z; }
inline block320 block320_set_all_8(uint8_t x) { block320 z; memset(&z,x,sizeof(z)); return z; }
inline block320 block320_set_low32(uint32_t x) { block320 z={{0}}; z.data[0]=x; return z; }
inline block320 block320_set_low64(uint64_t x) { block320 z={{0}}; memcpy(&z,&x,8); return z; }
inline bool block320_any_zeros(block320 x) { const uint8_t* p=(const uint8_t*)&x; unsigned any=0; for(size_t i=0;i<sizeof(x);++i) any|=(p[i]==0); return any!=0; }

inline block192 block192_xor(block192 x, block192 y) { block192 z; for(size_t i=0;i<3;++i) z.data[i]=x.data[i]^y.data[i]; return z; }
inline block192 block192_and(block192 x, block192 y) { block192 z; for(size_t i=0;i<3;++i) z.data[i]=x.data[i]&y.data[i]; return z; }
inline block192 block192_set_zero(void) { block192 z={{0}}; return z; }
inline block192 block192_set_all_8(uint8_t x) { block192 z; memset(&z,x,sizeof(z)); return z; }
inline block192 block192_set_low32(uint32_t x) { block192 z={{0}}; z.data[0]=x; return z; }
inline block192 block192_set_low64(uint64_t x) { block192 z={{0}}; memcpy(&z,&x,8); return z; }
inline bool block192_any_zeros(block192 x) { const uint8_t* p=(const uint8_t*)&x; unsigned any=0; for(size_t i=0;i<sizeof(x);++i) any|=(p[i]==0); return any!=0; }
inline block_secpar block_secpar_xor(block_secpar x, block_secpar y) { return block160_xor(x,y); }
inline block_secpar block_secpar_and(block_secpar x, block_secpar y) { return block160_and(x,y); }
inline block_secpar block_secpar_set_all_8(uint8_t x) { return block160_set_all_8(x); }
inline block_secpar block_secpar_set_low32(uint32_t x) { return block160_set_low32(x); }
inline block_secpar block_secpar_set_low64(uint64_t x) { return block160_set_low64(x); }
inline block_secpar block_secpar_set_zero(void) { return block160_set_zero(); }
inline block_2secpar block_2secpar_xor(block_2secpar x, block_2secpar y) { return block320_xor(x,y); }
inline block_2secpar block_2secpar_and(block_2secpar x, block_2secpar y) { return block320_and(x,y); }
inline block_2secpar block_2secpar_set_all_8(uint8_t x) { return block320_set_all_8(x); }
inline block_2secpar block_2secpar_set_low32(uint32_t x) { return block320_set_low32(x); }
inline block_2secpar block_2secpar_set_low64(uint64_t x) { return block320_set_low64(x); }
inline block_2secpar block_2secpar_set_zero(void) { return block320_set_zero(); }
inline bool block_secpar_any_zeros(block_secpar x) { return block160_any_zeros(x); }

#endif
