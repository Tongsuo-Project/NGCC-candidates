#ifndef NTT_H
#define NTT_H

#include <stdint.h>
#include "params.h"

#define zetas KEM_NAMESPACE(zetas)
extern const int16_t zetas[128];
#define zetas_inv KEM_NAMESPACE(zetas_inv)
extern const int16_t zetas_inv[128];

#define fqinv KEM_NAMESPACE(fqinv)
int16_t fqinv(int16_t a);
#define fqinv_table KEM_NAMESPACE(fqinv_table)
extern const int16_t fqinv_table[769];

#define ntt KEM_NAMESPACE(ntt)
void ntt(int16_t poly[N]);
#define invntt_tomont KEM_NAMESPACE(invntt_tomont)
void invntt_tomont(int16_t poly[N]);

#if KEM_MODE == 512
/* Recursive Karatsuba inversion used by the mode-512 AVX2 path. */
#define baseinv_K KEM_NAMESPACE(baseinv_K)
int baseinv_K(int half, int16_t *b, const int16_t *a, int16_t zeta);

/* AVX2 K(4) multiply: 8-element arrays, returns YMM */
#include <immintrin.h>
#define k4_ymm KEM_NAMESPACE(k4_ymm)
__m256i k4_ymm(const int16_t a[8], const int16_t b[8],
               int16_t zeta, __m256i q, __m256i qinv, __m256i v);
#endif

#endif
