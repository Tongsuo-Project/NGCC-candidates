/*
Copyright (c) 2026 Yu Zhang.
Organization: State Key Laboratory of Cyberspace Security Defense,Institute of Information Engineering, CAS
              School of Cyber Security, University of Chinese Academy of Sciences  
File Description: Declares the ZEN key-encapsulation mechanism layer for the optimized ZEN-128 instance.
*/
#include <stdint.h>
#include <string.h>
#include <immintrin.h>
#include "params.h"
#include "poly.h"

void cbd1(int16_t *r, const uint8_t *buf)
{
    unsigned int i;
    __m256i f0, f1, f2, f3;
    __m256i g0, g1, g2, g3;
    __m256i h0, h1, h2, h3;
    const __m256i mask01 = _mm256_set1_epi32(0x01010101);

    for(i = 0; i < ZEN_N / 128; i++)
    {
        f0 = _mm256_load_si256((const __m256i *)(buf + 32 * i));

        g0 = _mm256_and_si256(f0, mask01);
        g1 = _mm256_srli_epi16(f0, 1);
        g1 = _mm256_and_si256(g1, mask01);
        g0 = _mm256_sub_epi8(g0, g1);

        g1 = _mm256_srli_epi16(f0, 2);
        g2 = _mm256_srli_epi16(f0, 3);
        g1 = _mm256_and_si256(g1, mask01);
        g2 = _mm256_and_si256(g2, mask01);
        g1 = _mm256_sub_epi8(g1, g2);

        g2 = _mm256_srli_epi16(f0, 4);
        g3 = _mm256_srli_epi16(f0, 5);
        g2 = _mm256_and_si256(g2, mask01);
        g3 = _mm256_and_si256(g3, mask01);
        g2 = _mm256_sub_epi8(g2, g3);

        g3 = _mm256_srli_epi16(f0, 6);
        f1 = _mm256_srli_epi16(f0, 7);
        g3 = _mm256_and_si256(g3, mask01);
        f1 = _mm256_and_si256(f1, mask01);
        g3 = _mm256_sub_epi8(g3, f1);

        f0 = _mm256_unpacklo_epi8(g0, g1);
        f1 = _mm256_unpackhi_epi8(g0, g1);
        f2 = _mm256_unpacklo_epi8(g2, g3);
        f3 = _mm256_unpackhi_epi8(g2, g3);

        h0 = _mm256_unpacklo_epi16(f0, f2);
        h1 = _mm256_unpackhi_epi16(f0, f2);
        h2 = _mm256_unpacklo_epi16(f1, f3);
        h3 = _mm256_unpackhi_epi16(f1, f3);

        f0 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(h0));
        f1 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(h1));
        f2 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(h2));
        f3 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(h3));

        _mm256_store_si256((__m256i *)(r + 128 * i +  0), f0);
        _mm256_store_si256((__m256i *)(r + 128 * i + 16), f1);
        _mm256_store_si256((__m256i *)(r + 128 * i + 32), f2);
        _mm256_store_si256((__m256i *)(r + 128 * i + 48), f3);

        f0 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(h0, 1));
        f1 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(h1, 1));
        f2 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(h2, 1));
        f3 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(h3, 1));

        _mm256_store_si256((__m256i *)(r + 128 * i +  64), f0);
        _mm256_store_si256((__m256i *)(r + 128 * i +  80), f1);
        _mm256_store_si256((__m256i *)(r + 128 * i +  96), f2);
        _mm256_store_si256((__m256i *)(r + 128 * i + 112), f3);
    }
}

void cbd2(int16_t *r, const uint8_t *buf)
{
    unsigned int i;
    __m256i f0, f1, f2, f3;
    const __m256i mask55 = _mm256_set1_epi32(0x55555555);
    const __m256i mask33 = _mm256_set1_epi32(0x33333333);
    const __m256i mask03 = _mm256_set1_epi32(0x03030303);
    const __m256i mask0F = _mm256_set1_epi32(0x0F0F0F0F);

    for(i = 0; i < ZEN_N / 64; i++) 
    {
        f0 = _mm256_load_si256((const __m256i *)(buf + 32 * i));

        f1 = _mm256_srli_epi16(f0, 1);
        f0 = _mm256_and_si256(mask55, f0);
        f1 = _mm256_and_si256(mask55, f1);
        f0 = _mm256_add_epi8(f0, f1);

        f1 = _mm256_srli_epi16(f0, 2);
        f0 = _mm256_and_si256(mask33, f0);
        f1 = _mm256_and_si256(mask33, f1);
        f0 = _mm256_add_epi8(f0, mask33);
        f0 = _mm256_sub_epi8(f0, f1);

        f1 = _mm256_srli_epi16(f0, 4);
        f0 = _mm256_and_si256(mask0F, f0);
        f1 = _mm256_and_si256(mask0F, f1);
        f0 = _mm256_sub_epi8(f0, mask03);
        f1 = _mm256_sub_epi8(f1, mask03);

        f2 = _mm256_unpacklo_epi8(f0, f1);
        f3 = _mm256_unpackhi_epi8(f0, f1);

        f0 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(f2));
        f1 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(f2,1));
        f2 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(f3));
        f3 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(f3,1));

        _mm256_store_si256((__m256i *)(r + 64 * i +  0), f0);
        _mm256_store_si256((__m256i *)(r + 64 * i + 16), f2);
        _mm256_store_si256((__m256i *)(r + 64 * i + 32), f1);
        _mm256_store_si256((__m256i *)(r + 64 * i + 48), f3);
    }
}

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
