/*
Copyright (c) 2026 Yu Zhang.
Organization: State Key Laboratory of Cyberspace Security Defense,Institute of Information Engineering, CAS
              School of Cyber Security, University of Chinese Academy of Sciences  
File Description: Declares the ZEN key-encapsulation mechanism layer for the optimized ZEN-512 instance.
*/
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "params.h"
#include "poly.h"

void tenary1_8(int16_t *r, const uint8_t *buf)
{
    unsigned int i;

    for(i = 0; i < ZEN_N_LEN_BYTES; i++)
    {
        uint8_t x0 = buf[i];
        uint8_t x1 = buf[i + ZEN_N_LEN_BYTES];
        uint8_t x2 = buf[i + 2*ZEN_N_LEN_BYTES];

#define STORE_TERNARY1_8(j)                                                \
        r[8*i + (j)] = (int16_t)((((x0 >> (j)) & 1)                       \
                                   - ((x1 >> (j)) & 1))                    \
                                  * ((x2 >> (j)) & 1))
        STORE_TERNARY1_8(0);
        STORE_TERNARY1_8(1);
        STORE_TERNARY1_8(2);
        STORE_TERNARY1_8(3);
        STORE_TERNARY1_8(4);
        STORE_TERNARY1_8(5);
        STORE_TERNARY1_8(6);
        STORE_TERNARY1_8(7);
#undef STORE_TERNARY1_8
    }
}

void tenary3_32(int16_t *r, const uint8_t *buf)
{
    unsigned int i;

    for(i = 0; i < ZEN_N_LEN_BYTES; i++)
    {
        uint8_t x0 = buf[i];
        uint8_t x1 = buf[i + ZEN_N_LEN_BYTES];
        uint8_t x2 = buf[i + 2*ZEN_N_LEN_BYTES];
        uint8_t x3 = buf[i + 3*ZEN_N_LEN_BYTES];
        uint8_t x4 = buf[i + 4*ZEN_N_LEN_BYTES];

#define STORE_TERNARY3_32(j)                                               \
        r[8*i + (j)] = (int16_t)((((((x0 >> (j)) & (x1 >> (j))) & 1)      \
                                    - (((x2 >> (j)) & (x3 >> (j))) & 1))   \
                                   * ((x4 >> (j)) & 1)))
        STORE_TERNARY3_32(0);
        STORE_TERNARY3_32(1);
        STORE_TERNARY3_32(2);
        STORE_TERNARY3_32(3);
        STORE_TERNARY3_32(4);
        STORE_TERNARY3_32(5);
        STORE_TERNARY3_32(6);
        STORE_TERNARY3_32(7);
#undef STORE_TERNARY3_32
    }
}
