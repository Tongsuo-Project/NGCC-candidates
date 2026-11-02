#ifndef SIGN_PARAMS_H
#define SIGN_PARAMS_H

#include "config.h"

#define CRHBYTES 64 

/* Message representative length. Keep it independent from seed/hash lengths. */
#if DARTS_MODE == 512
#define MUBYTES 128
#else
#define MUBYTES CRHBYTES
#endif

/*-----------------128 bits security parameters ----------------------*/    
#if DARTS_MODE == 128 

#define SEEDBYTES 32

#define N 512
#define Q 130817
#define HALF_Q 65409 // Q+1 / 2
#define ROOT_OF_UNITY 667 // 256-th primitive root of unity module q
#define DQ (Q << 1)  // 2Q
#define K 1
#define L 2

#define P 0.15  // probability of 1 (or -1) 
#define TAU 30
#define B0 5866.64  // 小球的半径（小）
#define B 6057.90   // 超球采样半径（大）
#define B1 6054.88  // 中球的半径（中）
#define B11 7500.00 // 验签时的半径
#define GAMMA 34.92
/* Derived bounds retained for parameter auditing; not used at runtime. */
#define SC_BOUND 191.26
#define B_SIS 10435.93
#define D 10
#define SQNM 39.19 // sqrt(n*(k+l))

#define BASE_ENC_HB_Z1  220
#define BASE_ENC_H 80

#define LN 16384
#define LNHALF 8192
#define LNBITS 14

#define B0SQ ((uint64_t)(B0*B0))
#define B1SQ ((uint64_t)(B1*B1))
#define B11SQ ((uint64_t)(B11*B11))
#define BSQ ((uint64_t)(B*B))

#define Q_BITS 17

#define POLY_Q_PACKEDBYTES 1088 // 17 * n / 8 bytes
#define POLY_S_PACKEDBYTES 128  // 2 * n / 8 bytes
#define POLY_C_PACKEDBYTES 64   // size of c (n bits)

#define POLY_HIGHBITS_PACKEDBYTES ((N * D) / 8)  // size of highbits of poly
#define POLY_LOWBITS_PACKEDBYTES ((N * 57) / 64) // size of lowbits of poly (BAT)

#define ENCODE_MAXSIZE 471 // 对 HighBits(z1) 和 h 的rANS编码的最大尺寸

#define POLYVECK_HIGHBITS_PACKEDBYTES (K * POLY_HIGHBITS_PACKEDBYTES) // 多项式向量高位封装尺寸

#define CRYPTO_PUBLICKEYBYTES (SEEDBYTES + K * POLY_Q_PACKEDBYTES)  // pk = (seed, A_0)
#define CRYPTO_SECRETKEYBYTES (CRYPTO_PUBLICKEYBYTES + SEEDBYTES + (K+L) * POLY_S_PACKEDBYTES)  // sk = (s, K, pk)

#define CRYPTO_SIGNATUREBYTES (POLY_C_PACKEDBYTES + L * POLY_LOWBITS_PACKEDBYTES + 2 + ENCODE_MAXSIZE) // 签名尺寸 = c + LowBits(z1) + rANS_length +Encode(HighBits(z1)) + Encode(h)

#endif


/*-----------------256 bits security parameters ----------------------*/
#if DARTS_MODE == 256

#define SEEDBYTES 32

#define N 512
#define Q 130817
#define HALF_Q 65409 // Q+1 / 2
#define ROOT_OF_UNITY 667 // 256-th primitive root of unity module q
#define DQ (Q << 1)  // 2Q
#define K 2
#define L 3
#define P 0.3125  // probability of 1 (or -1) 
#define TAU 59
#define B0 18440.32  // 小球的半径（小）
#define B 18916.70   // 超球采样半径（大）
#define B1 18910.70  // 中球的半径（中）
#define B11 23000.00 // 验签时的半径
#define GAMMA 62.02
/* Derived bounds retained for parameter auditing; not used at runtime. */
#define SC_BOUND 476.38
#define B_SIS 31240.06
#define D 9
#define SQNM 50.60 // sqrt(n*(k+l))

#define BASE_ENC_HB_Z1 400 
#define BASE_ENC_H 300

#define LN 16384
#define LNHALF 8192
#define LNBITS 14

#define B0SQ ((uint64_t)(B0*B0))
#define B1SQ ((uint64_t)(B1*B1))
#define B11SQ ((uint64_t)(B11*B11))
#define BSQ ((uint64_t)(B*B))

#define Q_BITS 17

#define POLY_Q_PACKEDBYTES 1088 // 17 * n / 8 bytes
#define POLY_S_PACKEDBYTES 128  // 2 * n / 8 bytes
#define POLY_C_PACKEDBYTES 64   // size of c (n bits)

#define POLY_HIGHBITS_PACKEDBYTES ((N * D) / 8)  // size of highbits of poly
#define POLY_LOWBITS_PACKEDBYTES ((N * 65) / 64) // size of lowbits of poly (BAT)

#define ENCODE_MAXSIZE 863 // 对 HighBits(z1) 和 h 的rANS编码的最大尺寸

#define POLYVECK_HIGHBITS_PACKEDBYTES (K * POLY_HIGHBITS_PACKEDBYTES) // 多项式向量高位封装尺寸

#define CRYPTO_PUBLICKEYBYTES (SEEDBYTES + K * POLY_Q_PACKEDBYTES)  // pk = (seed, A_0)
#define CRYPTO_SECRETKEYBYTES (CRYPTO_PUBLICKEYBYTES + SEEDBYTES + (K+L) * POLY_S_PACKEDBYTES)  // sk = (s, K, pk)

#define CRYPTO_SIGNATUREBYTES (POLY_C_PACKEDBYTES + L * POLY_LOWBITS_PACKEDBYTES + 2 + ENCODE_MAXSIZE) // 签名尺寸 = c + LowBits(z1) + rANS_length +Encode(HighBits(z1)) + Encode(h)

#endif

/*-----------------512 bits security parameters ----------------------*/
#if DARTS_MODE == 512

#define SEEDBYTES 64

#define N 1024
#define Q 260609
#define HALF_Q 130305 // Q+1 / 2
#define ROOT_OF_UNITY 1092 // 512-th primitive root of unity module q
#define DQ (Q << 1)  // 2Q
#define K 2
#define L 3
#define P 0.2  // probability of 1 (or -1) 
#define TAU 115
#define B0 73440.24  // 小球的半径（小）
#define B 74220.97   // 超球采样半径（大）
#define B1 74216.86  // 中球的半径（中）
#define B11 80000.00 // 验签时的半径
#define GAMMA 72.80
/* Derived bounds retained for parameter auditing; not used at runtime. */
#define SC_BOUND 780.72
#define B_SIS 91607.91
#define D 10
#define SQNM 71.55 // sqrt(n*(k+l))

#define BASE_ENC_HB_Z1 1500
#define BASE_ENC_H 1000

#define LN 16384
#define LNHALF 8192
#define LNBITS 14

#define B0SQ ((uint64_t)(B0*B0))
#define B1SQ ((uint64_t)(B1*B1))
#define B11SQ ((uint64_t)(B11*B11))
#define BSQ ((uint64_t)(B*B))

#define Q_BITS 18

#define POLY_Q_PACKEDBYTES 2304 // 18 * n / 8 bytes
#define POLY_S_PACKEDBYTES 256  // 2 * n / 8 bytes
#define POLY_C_PACKEDBYTES 128   // size of c (n bits)

#define POLY_HIGHBITS_PACKEDBYTES ((N * D) / 8)  // size of highbits of poly
#define POLY_LOWBITS_PACKEDBYTES (N) // size of lowbits of poly (BAT)

#define ENCODE_MAXSIZE 2650  // 对 HighBits(z1) 和 h 的rANS编码的最大尺寸

#define POLYVECK_HIGHBITS_PACKEDBYTES (K * POLY_HIGHBITS_PACKEDBYTES) // 多项式向量高位封装尺寸

#define CRYPTO_PUBLICKEYBYTES (SEEDBYTES + K * POLY_Q_PACKEDBYTES)  // pk = (seed, A_0)
#define CRYPTO_SECRETKEYBYTES (CRYPTO_PUBLICKEYBYTES + SEEDBYTES + (K+L) * POLY_S_PACKEDBYTES)  // sk = (s, K, pk)

#define CRYPTO_SIGNATUREBYTES (POLY_C_PACKEDBYTES + L * POLY_LOWBITS_PACKEDBYTES + 2 + ENCODE_MAXSIZE) // 签名尺寸 = c + LowBits(z1) + rANS_length +Encode(HighBits(z1)) + Encode(h)

#endif


#endif
