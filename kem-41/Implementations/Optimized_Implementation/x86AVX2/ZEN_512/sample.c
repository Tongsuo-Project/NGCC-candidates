/*
Copyright (c) 2026 Yu Zhang.
Organization: State Key Laboratory of Cyberspace Security Defense,Institute of Information Engineering, CAS
              School of Cyber Security, University of Chinese Academy of Sciences  
File Description: Declares the ZEN key-encapsulation mechanism layer for the optimized ZEN-512 instance.
*/
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <immintrin.h>
#include "params.h"
#include "poly.h"

static inline __m256i unpack16_bits(const uint8_t *buf,
                                    const __m256i bitmask,
                                    const __m256i zero,
                                    const __m256i one)
{
    uint16_t w = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    __m256i x = _mm256_set1_epi16((int16_t)w);

    x = _mm256_and_si256(x, bitmask);
    x = _mm256_cmpeq_epi16(x, zero);
    return _mm256_andnot_si256(x, one);
}

void tenary1_8(int16_t *r, const uint8_t *buf)
{
    unsigned int i;
    const unsigned int plane_bytes = ZEN_N / 8;
    const __m256i zero = _mm256_setzero_si256();
    const __m256i one = _mm256_set1_epi16(1);
    const __m256i bitmask = _mm256_setr_epi16(
        0x0001, 0x0002, 0x0004, 0x0008,
        0x0010, 0x0020, 0x0040, 0x0080,
        0x0100, 0x0200, 0x0400, 0x0800,
        0x1000, 0x2000, 0x4000, (int16_t)0x8000
    );

    for(i = 0; i < ZEN_N; i += 16)
    {
        __m256i a = unpack16_bits(buf + i/8, bitmask, zero, one);
        __m256i b = unpack16_bits(buf + plane_bytes + i/8, bitmask, zero, one);
        __m256i c = unpack16_bits(buf + 2*plane_bytes + i/8, bitmask, zero, one);

        a = _mm256_sub_epi16(a, b);
        a = _mm256_mullo_epi16(a, c);
        _mm256_store_si256((__m256i *)(r + i), a);
    }
}

void tenary3_32(int16_t *r, const uint8_t *buf)
{
    unsigned int i;
    const unsigned int plane_bytes = ZEN_N / 8;
    const __m256i zero = _mm256_setzero_si256();
    const __m256i one = _mm256_set1_epi16(1);
    const __m256i bitmask = _mm256_setr_epi16(
        0x0001, 0x0002, 0x0004, 0x0008,
        0x0010, 0x0020, 0x0040, 0x0080,
        0x0100, 0x0200, 0x0400, 0x0800,
        0x1000, 0x2000, 0x4000, (int16_t)0x8000
    );

    for(i = 0; i < ZEN_N; i += 16)
    {
        __m256i a = unpack16_bits(buf + i/8, bitmask, zero, one);
        __m256i b = unpack16_bits(buf + plane_bytes + i/8, bitmask, zero, one);
        __m256i c = unpack16_bits(buf + 2*plane_bytes + i/8, bitmask, zero, one);
        __m256i d = unpack16_bits(buf + 3*plane_bytes + i/8, bitmask, zero, one);
        __m256i e = unpack16_bits(buf + 4*plane_bytes + i/8, bitmask, zero, one);

        a = _mm256_and_si256(a, b);
        c = _mm256_and_si256(c, d);
        a = _mm256_sub_epi16(a, c);
        a = _mm256_mullo_epi16(a, e);
        _mm256_store_si256((__m256i *)(r + i), a);
    }
}
