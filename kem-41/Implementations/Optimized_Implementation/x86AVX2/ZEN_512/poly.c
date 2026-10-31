/*
Copyright (c) 2026 Yu Zhang.
Organization: State Key Laboratory of Cyberspace Security Defense,Institute of Information Engineering, CAS
              School of Cyber Security, University of Chinese Academy of Sciences  
File Description: Declares the ZEN key-encapsulation mechanism layer for the optimized ZEN-512 instance.
*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <immintrin.h>
#include "params.h"
#include "auxfunc.h"
#include "sample.h"
#include "symmetric.h"
#include "ntt.h"

void montmul(__m256i *tmp_c, __m256i *tmp_a, __m256i *tmp_b, __m256i *tmp_Q, __m256i *tmp_QINV)
{
    __m256i tmp_c0, tmp_c1, tmp_x;
    tmp_c0 = _mm256_mullo_epi16(*tmp_a, *tmp_b);
    tmp_c1 = _mm256_mulhi_epi16(*tmp_a, *tmp_b);
    tmp_x = _mm256_mullo_epi16(tmp_c0, *tmp_QINV);
    tmp_x = _mm256_mulhi_epi16(tmp_x, *tmp_Q);
    *tmp_c = _mm256_sub_epi16(tmp_c1, tmp_x);
}

void poly_basemul_ntt(int16_t *r, int16_t *a, int16_t *b, int16_t *muldata)
{
    int i, j, k;
    __m256i zeta, prod, tmp, t0;
    __m256i av[16], bv[16], as[8], bs[8];
    __m256i p0[15], p1[15], pm[15], cross[15];
    __m256i tmp_Q = _mm256_set1_epi16(769);
    __m256i tmp_QINV = _mm256_set1_epi16(-767);
    __m256i set1 = _mm256_set1_epi16(171);
    __m256i zero = _mm256_setzero_si256();

    for(i = 0; i < ZEN_N; i += 256)
    {
        zeta = _mm256_load_si256((__m256i *)(muldata + i / 16));

        av[0] = _mm256_load_si256((__m256i *)(a + i));
        av[1] = _mm256_load_si256((__m256i *)(a + i + 16));
        av[2] = _mm256_load_si256((__m256i *)(a + i + 32));
        av[3] = _mm256_load_si256((__m256i *)(a + i + 48));
        av[4] = _mm256_load_si256((__m256i *)(a + i + 128));
        av[5] = _mm256_load_si256((__m256i *)(a + i + 144));
        av[6] = _mm256_load_si256((__m256i *)(a + i + 160));
        av[7] = _mm256_load_si256((__m256i *)(a + i + 176));
        av[8] = _mm256_load_si256((__m256i *)(a + i + 64));
        av[9] = _mm256_load_si256((__m256i *)(a + i + 80));
        av[10] = _mm256_load_si256((__m256i *)(a + i + 96));
        av[11] = _mm256_load_si256((__m256i *)(a + i + 112));
        av[12] = _mm256_load_si256((__m256i *)(a + i + 192));
        av[13] = _mm256_load_si256((__m256i *)(a + i + 208));
        av[14] = _mm256_load_si256((__m256i *)(a + i + 224));
        av[15] = _mm256_load_si256((__m256i *)(a + i + 240));

        bv[0] = _mm256_load_si256((__m256i *)(b + i));
        bv[1] = _mm256_load_si256((__m256i *)(b + i + 16));
        bv[2] = _mm256_load_si256((__m256i *)(b + i + 32));
        bv[3] = _mm256_load_si256((__m256i *)(b + i + 48));
        bv[4] = _mm256_load_si256((__m256i *)(b + i + 128));
        bv[5] = _mm256_load_si256((__m256i *)(b + i + 144));
        bv[6] = _mm256_load_si256((__m256i *)(b + i + 160));
        bv[7] = _mm256_load_si256((__m256i *)(b + i + 176));
        bv[8] = _mm256_load_si256((__m256i *)(b + i + 64));
        bv[9] = _mm256_load_si256((__m256i *)(b + i + 80));
        bv[10] = _mm256_load_si256((__m256i *)(b + i + 96));
        bv[11] = _mm256_load_si256((__m256i *)(b + i + 112));
        bv[12] = _mm256_load_si256((__m256i *)(b + i + 192));
        bv[13] = _mm256_load_si256((__m256i *)(b + i + 208));
        bv[14] = _mm256_load_si256((__m256i *)(b + i + 224));
        bv[15] = _mm256_load_si256((__m256i *)(b + i + 240));

        for(j = 0; j < 8; j++)
        {
            as[j] = _mm256_add_epi16(av[j], av[j + 8]);
            bs[j] = _mm256_add_epi16(bv[j], bv[j + 8]);
        }

        for(j = 0; j < 15; j++)
        {
            p0[j] = zero;
            p1[j] = zero;
            pm[j] = zero;
        }

        for(j = 0; j < 8; j++)
        {
            for(k = 0; k < 8; k++)
            {
                montmul(&prod, &av[j], &bv[k], &tmp_Q, &tmp_QINV);
                p0[j + k] = _mm256_add_epi16(p0[j + k], prod);

                montmul(&prod, &av[j + 8], &bv[k + 8], &tmp_Q, &tmp_QINV);
                p1[j + k] = _mm256_add_epi16(p1[j + k], prod);

                montmul(&prod, &as[j], &bs[k], &tmp_Q, &tmp_QINV);
                pm[j + k] = _mm256_add_epi16(pm[j + k], prod);
            }
        }

        for(j = 0; j < 15; j++)
        {
            cross[j] = _mm256_sub_epi16(_mm256_sub_epi16(pm[j], p0[j]), p1[j]);
        }

        montmul(&tmp, &p1[0], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[8], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[0], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i), t0);

        montmul(&tmp, &p1[1], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[9], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[1], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 16), t0);

        montmul(&tmp, &p1[2], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[10], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[2], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 32), t0);

        montmul(&tmp, &p1[3], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[11], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[3], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 48), t0);

        montmul(&tmp, &p1[4], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[12], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[4], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 128), t0);

        montmul(&tmp, &p1[5], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[13], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[5], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 144), t0);

        montmul(&tmp, &p1[6], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[14], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[6], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 160), t0);

        montmul(&tmp, &p1[7], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(p0[7], tmp);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 176), t0);

        montmul(&tmp, &p1[8], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[8], tmp), cross[0]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 64), t0);

        montmul(&tmp, &p1[9], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[9], tmp), cross[1]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 80), t0);

        montmul(&tmp, &p1[10], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[10], tmp), cross[2]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 96), t0);

        montmul(&tmp, &p1[11], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[11], tmp), cross[3]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 112), t0);

        montmul(&tmp, &p1[12], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[12], tmp), cross[4]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 192), t0);

        montmul(&tmp, &p1[13], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[13], tmp), cross[5]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 208), t0);

        montmul(&tmp, &p1[14], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[14], tmp), cross[6]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 224), t0);

        montmul(&t0, &cross[7], &set1, &tmp_Q, &tmp_QINV);
        _mm256_store_si256((__m256i *)(r + i + 240), t0);
    }
}

void poly_basemul_ntt_mq(int16_t *r, int16_t *a, int16_t *b, int16_t *muldata)
{
    int i, j, k;
    __m256i zeta, prod, tmp, t0;
    __m256i av[16], bv[16], as[8], bs[8];
    __m256i p0[15], p1[15], pm[15], cross[15];
    __m256i tmp_Q = _mm256_set1_epi16(769);
    __m256i tmp_QINV = _mm256_set1_epi16(-767);
    __m256i set1 = _mm256_set1_epi16(19);
    __m256i zero = _mm256_setzero_si256();

    for(i = 0; i < ZEN_N; i += 256)
    {
        zeta = _mm256_load_si256((__m256i *)(muldata + i / 16));

        av[0] = _mm256_load_si256((__m256i *)(a + i));
        av[1] = _mm256_load_si256((__m256i *)(a + i + 16));
        av[2] = _mm256_load_si256((__m256i *)(a + i + 32));
        av[3] = _mm256_load_si256((__m256i *)(a + i + 48));
        av[4] = _mm256_load_si256((__m256i *)(a + i + 128));
        av[5] = _mm256_load_si256((__m256i *)(a + i + 144));
        av[6] = _mm256_load_si256((__m256i *)(a + i + 160));
        av[7] = _mm256_load_si256((__m256i *)(a + i + 176));
        av[8] = _mm256_load_si256((__m256i *)(a + i + 64));
        av[9] = _mm256_load_si256((__m256i *)(a + i + 80));
        av[10] = _mm256_load_si256((__m256i *)(a + i + 96));
        av[11] = _mm256_load_si256((__m256i *)(a + i + 112));
        av[12] = _mm256_load_si256((__m256i *)(a + i + 192));
        av[13] = _mm256_load_si256((__m256i *)(a + i + 208));
        av[14] = _mm256_load_si256((__m256i *)(a + i + 224));
        av[15] = _mm256_load_si256((__m256i *)(a + i + 240));

        bv[0] = _mm256_load_si256((__m256i *)(b + i));
        bv[1] = _mm256_load_si256((__m256i *)(b + i + 16));
        bv[2] = _mm256_load_si256((__m256i *)(b + i + 32));
        bv[3] = _mm256_load_si256((__m256i *)(b + i + 48));
        bv[4] = _mm256_load_si256((__m256i *)(b + i + 128));
        bv[5] = _mm256_load_si256((__m256i *)(b + i + 144));
        bv[6] = _mm256_load_si256((__m256i *)(b + i + 160));
        bv[7] = _mm256_load_si256((__m256i *)(b + i + 176));
        bv[8] = _mm256_load_si256((__m256i *)(b + i + 64));
        bv[9] = _mm256_load_si256((__m256i *)(b + i + 80));
        bv[10] = _mm256_load_si256((__m256i *)(b + i + 96));
        bv[11] = _mm256_load_si256((__m256i *)(b + i + 112));
        bv[12] = _mm256_load_si256((__m256i *)(b + i + 192));
        bv[13] = _mm256_load_si256((__m256i *)(b + i + 208));
        bv[14] = _mm256_load_si256((__m256i *)(b + i + 224));
        bv[15] = _mm256_load_si256((__m256i *)(b + i + 240));

        for(j = 0; j < 8; j++)
        {
            as[j] = _mm256_add_epi16(av[j], av[j + 8]);
            bs[j] = _mm256_add_epi16(bv[j], bv[j + 8]);
        }

        for(j = 0; j < 15; j++)
        {
            p0[j] = zero;
            p1[j] = zero;
            pm[j] = zero;
        }

        for(j = 0; j < 8; j++)
        {
            for(k = 0; k < 8; k++)
            {
                montmul(&prod, &av[j], &bv[k], &tmp_Q, &tmp_QINV);
                p0[j + k] = _mm256_add_epi16(p0[j + k], prod);

                montmul(&prod, &av[j + 8], &bv[k + 8], &tmp_Q, &tmp_QINV);
                p1[j + k] = _mm256_add_epi16(p1[j + k], prod);

                montmul(&prod, &as[j], &bs[k], &tmp_Q, &tmp_QINV);
                pm[j + k] = _mm256_add_epi16(pm[j + k], prod);
            }
        }

        for(j = 0; j < 15; j++)
        {
            cross[j] = _mm256_sub_epi16(_mm256_sub_epi16(pm[j], p0[j]), p1[j]);
        }

        montmul(&tmp, &p1[0], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[8], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[0], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i), t0);

        montmul(&tmp, &p1[1], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[9], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[1], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 16), t0);

        montmul(&tmp, &p1[2], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[10], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[2], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 32), t0);

        montmul(&tmp, &p1[3], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[11], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[3], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 48), t0);

        montmul(&tmp, &p1[4], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[12], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[4], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 128), t0);

        montmul(&tmp, &p1[5], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[13], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[5], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 144), t0);

        montmul(&tmp, &p1[6], &zeta, &tmp_Q, &tmp_QINV);
        montmul(&prod, &cross[14], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[6], tmp), prod);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 160), t0);

        montmul(&tmp, &p1[7], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(p0[7], tmp);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 176), t0);

        montmul(&tmp, &p1[8], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[8], tmp), cross[0]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 64), t0);

        montmul(&tmp, &p1[9], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[9], tmp), cross[1]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 80), t0);

        montmul(&tmp, &p1[10], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[10], tmp), cross[2]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 96), t0);

        montmul(&tmp, &p1[11], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[11], tmp), cross[3]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 112), t0);

        montmul(&tmp, &p1[12], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[12], tmp), cross[4]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 192), t0);

        montmul(&tmp, &p1[13], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[13], tmp), cross[5]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 208), t0);

        montmul(&tmp, &p1[14], &zeta, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(_mm256_add_epi16(p0[14], tmp), cross[6]);
        montmul(&t0, &t0, &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 224), t0);

        montmul(&t0, &cross[7], &set1, &tmp_Q, &tmp_QINV);
        t0 = _mm256_add_epi16(t0, _mm256_and_si256(_mm256_srai_epi16(t0, 15), tmp_Q));
        _mm256_store_si256((__m256i *)(r + i + 240), t0);
    }
}

/* NTT inversion via even/odd norms and hierarchical batch inversion.
 * Adapted from Kim, Cho and Park, "Accelerating NTRU+ Key Generation
 * via Hierarchical Batch Inversion", Sections III-IV, to q=769.
 * For a=A(y)+xB(y), y=x^2, use a^-1=(A-xB)/(A^2-yB^2).
 * Each norm introduces a factor R^-1: a degree-d denominator
 * is Delta/R^(d-1).  Inverting that stored value and lifting with the
 * same Montgomery products returns the ordinary, unscaled inverse.
 * Input coefficients must be in [-768,768], as supplied by poly_ntt_mq.
 * Noninvertible blocks produce zero; processing still takes the full path.
 */

static inline __m256i inv_add(__m256i a, __m256i b)
{ return _mm256_add_epi16(a, b); }
static inline __m256i inv_sub(__m256i a, __m256i b)
{ return _mm256_sub_epi16(a, b); }
static inline __m256i inv_neg(__m256i a)
{ return _mm256_sub_epi16(_mm256_setzero_si256(), a); }
static inline __m256i inv_mul(__m256i a, __m256i b)
{
    __m256i lo = _mm256_mullo_epi16(a, b);
    __m256i hi = _mm256_mulhi_epi16(a, b);
    __m256i t = _mm256_mullo_epi16(lo, _mm256_set1_epi16(-767));
    return inv_sub(hi, _mm256_mulhi_epi16(t, _mm256_set1_epi16(769)));
}
static inline __m256i inv_reduce(__m256i a)
{
    /* Rounded Barrett reduction: |1-769*43/32768|=299/32768.
     * For every signed 16-bit input, the remainder has magnitude <684.
     * This preserves the Montgomery scale, without a Montgomery product. */
    __m256i q = _mm256_mulhrs_epi16(a, _mm256_set1_epi16(43));
    return inv_sub(a, _mm256_mullo_epi16(q, _mm256_set1_epi16(769)));
}

/* Fixed addition chain for q-2 = 767: 13 Montgomery products.
 * Interpreting a as a Montgomery value gives R^2/a; the last product
 * removes R^2 (R = 171, R^-1 = 9 mod 769).  Zero maps to zero. */
static inline __m256i inv_scalar(__m256i a)
{
    __m256i a2 = inv_mul(a, a);
    __m256i a3 = inv_mul(a2, a);
    __m256i a5 = inv_mul(a3, a2);
    __m256i t = inv_mul(a5, a5);  /* a^10 */
    __m256i a15 = inv_mul(t, a5);
    t = inv_mul(a15, a15);     /* a^30 */
    t = inv_mul(t, t);         /* a^60 */
    t = inv_mul(t, t);         /* a^120 */
    t = inv_mul(t, t);         /* a^240 */
    t = inv_mul(t, a15);       /* a^255 */
    a3 = inv_mul(t, t);        /* a^510 */
    t = inv_mul(a3, t);        /* a^765 */
    t = inv_mul(t, a2);        /* a^767 */
    return inv_mul(t, _mm256_set1_epi16(9));
}

/* Eight vectors, four independent two-vector chains, then 4 -> 2 -> 1.
 * The root uses one fixed 16-lane exponentiation.  Product/recovery
 * stages use 21 vector Montgomery products (3*(8-1)), excluding masks.
 * A zero in any lane is isolated from the other seven blocks in it. */
static void inv_batch(__m256i d[8])
{
    __m256i tree[16], zero[8];
    unsigned int i;
    for(i = 0; i < 8; i++)
    {
        __m256i x = inv_reduce(d[i]);
        zero[i] = _mm256_cmpeq_epi16(x, _mm256_setzero_si256());
        tree[8 + i] = _mm256_blendv_epi8(x, _mm256_set1_epi16(171), zero[i]);
    }
    for(i = 7; i > 0; i--)
        tree[i] = inv_mul(tree[2 * i], tree[2 * i + 1]);
    tree[1] = inv_scalar(tree[1]);
    for(i = 1; i < 8; i++)
    {
        __m256i left = tree[2 * i];
        tree[2 * i] = inv_mul(tree[i], tree[2 * i + 1]);
        tree[2 * i + 1] = inv_mul(tree[i], left);
    }
    for(i = 0; i < 8; i++)
        d[i] = _mm256_andnot_si256(zero[i], tree[8 + i]);
}

/* Fixed-size convolution kernels.  The degree-8 norm/lift are fused.
 * Through 8 -> 4 -> 2 -> 4 -> 8, lazy representatives stay bounded:
 * |n4|<=3148, |n2|<=1643, |t4|<=1240, |t8|<=2451.  The degree-16
 * norm uses Barrett reduction.  The largest intermediate
 * magnitude is <=14742, below signed 16-bit overflow (tests/bounds.py).
 * Larger Karatsuba kernels stay out of line to avoid excessive register
 * spills under the existing -O3/-flto compiler options. */
static inline void inv_square2(__m256i *r, const __m256i *a)
{
    __m256i t = inv_mul(a[0], a[1]);
    r[0] = inv_mul(a[0], a[0]);
    r[1] = inv_add(t, t);
    r[2] = inv_mul(a[1], a[1]);
}

static inline void inv_product2(__m256i *r, const __m256i *a, const __m256i *b)
{
    __m256i p = inv_mul(a[0], b[0]), q = inv_mul(a[1], b[1]);
    r[0] = p;
    r[1] = inv_sub(inv_sub(inv_mul(inv_add(a[0], a[1]), inv_add(b[0], b[1])), p), q);
    r[2] = q;
}

static __attribute__((noinline)) void inv_square4(__m256i *r, const __m256i *a)
{
    __m256i lo[3], hi[3], mid[3], sa[2];
    sa[0] = inv_add(a[0], a[2]);
    sa[1] = inv_add(a[1], a[3]);
    inv_square2(lo, a);
    inv_square2(hi, a + 2);
    inv_square2(mid, sa);
    r[0] = lo[0];
    r[1] = lo[1];
    r[2] = inv_add(lo[2], inv_sub(inv_sub(mid[0], lo[0]), hi[0]));
    r[3] = inv_sub(inv_sub(mid[1], lo[1]), hi[1]);
    r[4] = inv_add(hi[0], inv_sub(inv_sub(mid[2], lo[2]), hi[2]));
    r[5] = hi[1];
    r[6] = hi[2];
}

static __attribute__((noinline)) void inv_product4(__m256i *r, const __m256i *a, const __m256i *b)
{
    __m256i lo[3], hi[3], mid[3], sa[2], sb[2];
    sa[0] = inv_add(a[0], a[2]);
    sb[0] = inv_add(b[0], b[2]);
    sa[1] = inv_add(a[1], a[3]);
    sb[1] = inv_add(b[1], b[3]);
    inv_product2(lo, a, b);
    inv_product2(hi, a + 2, b + 2);
    inv_product2(mid, sa, sb);
    r[0] = lo[0];
    r[1] = lo[1];
    r[2] = inv_add(lo[2], inv_sub(inv_sub(mid[0], lo[0]), hi[0]));
    r[3] = inv_sub(inv_sub(mid[1], lo[1]), hi[1]);
    r[4] = inv_add(hi[0], inv_sub(inv_sub(mid[2], lo[2]), hi[2]));
    r[5] = hi[1];
    r[6] = hi[2];
}

static __attribute__((noinline)) void inv_square8(__m256i *r, const __m256i *a)
{
    __m256i lo[7], hi[7], mid[7], sa[4];
    sa[0] = inv_add(a[0], a[4]);
    sa[1] = inv_add(a[1], a[5]);
    sa[2] = inv_add(a[2], a[6]);
    sa[3] = inv_add(a[3], a[7]);
    inv_square4(lo, a);
    inv_square4(hi, a + 4);
    inv_square4(mid, sa);
    r[0] = lo[0];
    r[1] = lo[1];
    r[2] = lo[2];
    r[3] = lo[3];
    r[4] = inv_add(lo[4], inv_sub(inv_sub(mid[0], lo[0]), hi[0]));
    r[5] = inv_add(lo[5], inv_sub(inv_sub(mid[1], lo[1]), hi[1]));
    r[6] = inv_add(lo[6], inv_sub(inv_sub(mid[2], lo[2]), hi[2]));
    r[7] = inv_sub(inv_sub(mid[3], lo[3]), hi[3]);
    r[8] = inv_add(hi[0], inv_sub(inv_sub(mid[4], lo[4]), hi[4]));
    r[9] = inv_add(hi[1], inv_sub(inv_sub(mid[5], lo[5]), hi[5]));
    r[10] = inv_add(hi[2], inv_sub(inv_sub(mid[6], lo[6]), hi[6]));
    r[11] = hi[3];
    r[12] = hi[4];
    r[13] = hi[5];
    r[14] = hi[6];
}

static __attribute__((noinline)) void inv_product8(__m256i *r, const __m256i *a, const __m256i *b)
{
    __m256i lo[7], hi[7], mid[7], sa[4], sb[4];
    sa[0] = inv_add(a[0], a[4]);
    sb[0] = inv_add(b[0], b[4]);
    sa[1] = inv_add(a[1], a[5]);
    sb[1] = inv_add(b[1], b[5]);
    sa[2] = inv_add(a[2], a[6]);
    sb[2] = inv_add(b[2], b[6]);
    sa[3] = inv_add(a[3], a[7]);
    sb[3] = inv_add(b[3], b[7]);
    inv_product4(lo, a, b);
    inv_product4(hi, a + 4, b + 4);
    inv_product4(mid, sa, sb);
    r[0] = lo[0];
    r[1] = lo[1];
    r[2] = lo[2];
    r[3] = lo[3];
    r[4] = inv_add(lo[4], inv_sub(inv_sub(mid[0], lo[0]), hi[0]));
    r[5] = inv_add(lo[5], inv_sub(inv_sub(mid[1], lo[1]), hi[1]));
    r[6] = inv_add(lo[6], inv_sub(inv_sub(mid[2], lo[2]), hi[2]));
    r[7] = inv_sub(inv_sub(mid[3], lo[3]), hi[3]);
    r[8] = inv_add(hi[0], inv_sub(inv_sub(mid[4], lo[4]), hi[4]));
    r[9] = inv_add(hi[1], inv_sub(inv_sub(mid[5], lo[5]), hi[5]));
    r[10] = inv_add(hi[2], inv_sub(inv_sub(mid[6], lo[6]), hi[6]));
    r[11] = hi[3];
    r[12] = hi[4];
    r[13] = hi[5];
    r[14] = hi[6];
}

static inline void inv_norm2(__m256i *r, const __m256i *a, const __m256i *factor)
{
    __m256i zeta = *factor;
    r[0] = inv_sub(inv_mul(a[0], a[0]), inv_mul(inv_mul(a[1], a[1]), zeta));
}

static inline void inv_norm4(__m256i *r, const __m256i *a, const __m256i *factor)
{
    __m256i zeta = *factor;
    __m256i even[2], odd[2], pe[3], po[3];
    even[0] = a[0]; odd[0] = a[1];
    even[1] = a[2]; odd[1] = a[3];
    inv_square2(pe, even);
    inv_square2(po, odd);
    r[0] = inv_add(pe[0], inv_mul(inv_sub(pe[2], po[1]), zeta));
    r[1] = inv_add(inv_sub(pe[1], po[0]), inv_mul(inv_neg(po[2]), zeta));
}

/* Fused degree-8 even/odd norm, with four bounded lazy coefficients.
 * Avoid materializing two length-7 square arrays. */
static inline void inv_norm8(__m256i *r, const __m256i *a, const __m256i *factor)
{
    __m256i zeta = *factor, t, u;
    t = inv_add(inv_mul(a[4], a[4]), inv_add(inv_sub(inv_sub(inv_mul(a[2], a[6]), inv_mul(a[1], a[7])), inv_mul(a[3], a[5])), inv_sub(inv_sub(inv_mul(a[2], a[6]), inv_mul(a[1], a[7])), inv_mul(a[3], a[5]))));
    r[0] = inv_add(inv_mul(a[0], a[0]), inv_mul(t, zeta));
    t = inv_sub(inv_add(inv_sub(inv_mul(a[4], a[6]), inv_mul(a[3], a[7])), inv_sub(inv_mul(a[4], a[6]), inv_mul(a[3], a[7]))), inv_mul(a[5], a[5]));
    u = inv_sub(inv_add(inv_mul(a[0], a[2]), inv_mul(a[0], a[2])), inv_mul(a[1], a[1]));
    r[1] = inv_add(u, inv_mul(t, zeta));
    t = inv_sub(inv_mul(a[6], a[6]), inv_add(inv_mul(a[5], a[7]), inv_mul(a[5], a[7])));
    u = inv_add(inv_add(inv_sub(inv_mul(a[0], a[4]), inv_mul(a[1], a[3])), inv_sub(inv_mul(a[0], a[4]), inv_mul(a[1], a[3]))), inv_mul(a[2], a[2]));
    r[2] = inv_add(u, inv_mul(t, zeta));
    t = inv_sub(inv_add(inv_sub(inv_add(inv_mul(a[0], a[6]), inv_mul(a[2], a[4])), inv_mul(a[1], a[5])), inv_sub(inv_add(inv_mul(a[0], a[6]), inv_mul(a[2], a[4])), inv_mul(a[1], a[5]))), inv_mul(a[3], a[3]));
    r[3] = inv_sub(t, inv_mul(inv_mul(a[7], a[7]), zeta));
}

static inline void inv_norm16(__m256i *r, const __m256i *a, const __m256i *factor)
{
    __m256i zeta = *factor;
    __m256i even[8], odd[8], pe[15], po[15];
    even[0] = a[0]; odd[0] = a[1];
    even[1] = a[2]; odd[1] = a[3];
    even[2] = a[4]; odd[2] = a[5];
    even[3] = a[6]; odd[3] = a[7];
    even[4] = a[8]; odd[4] = a[9];
    even[5] = a[10]; odd[5] = a[11];
    even[6] = a[12]; odd[6] = a[13];
    even[7] = a[14]; odd[7] = a[15];
    inv_square8(pe, even);
    inv_square8(po, odd);
    r[0] = inv_reduce(inv_add(pe[0], inv_mul(inv_sub(pe[8], po[7]), zeta)));
    r[1] = inv_reduce(inv_add(inv_sub(pe[1], po[0]), inv_mul(inv_sub(pe[9], po[8]), zeta)));
    r[2] = inv_reduce(inv_add(inv_sub(pe[2], po[1]), inv_mul(inv_sub(pe[10], po[9]), zeta)));
    r[3] = inv_reduce(inv_add(inv_sub(pe[3], po[2]), inv_mul(inv_sub(pe[11], po[10]), zeta)));
    r[4] = inv_reduce(inv_add(inv_sub(pe[4], po[3]), inv_mul(inv_sub(pe[12], po[11]), zeta)));
    r[5] = inv_reduce(inv_add(inv_sub(pe[5], po[4]), inv_mul(inv_sub(pe[13], po[12]), zeta)));
    r[6] = inv_reduce(inv_add(inv_sub(pe[6], po[5]), inv_mul(inv_sub(pe[14], po[13]), zeta)));
    r[7] = inv_reduce(inv_add(inv_sub(pe[7], po[6]), inv_mul(inv_neg(po[14]), zeta)));
}

static inline void inv_lift4(__m256i *r, const __m256i *a,
                              const __m256i *b, const __m256i *factor)
{
    __m256i zeta = *factor;
    __m256i even[2], odd[2], pe[3], po[3];
    even[0] = a[0]; odd[0] = a[1];
    even[1] = a[2]; odd[1] = a[3];
    inv_product2(pe, even, b);
    inv_product2(po, odd, b);
    r[0] = inv_add(pe[0], inv_mul(pe[2], zeta));
    r[1] = inv_neg(inv_add(po[0], inv_mul(po[2], zeta)));
    r[2] = pe[1];
    r[3] = inv_neg(po[1]);
}

/* Fused cyclic degree-4 products for the even and odd halves.
 * This uses one extra field product per half versus linear Karatsuba,
 * but avoids its coefficient arrays, spills and out-of-line calls. */
static inline void inv_lift8(__m256i *r, const __m256i *a,
                              const __m256i *b, const __m256i *factor)
{
    __m256i zeta = *factor;
    __m256i p0, p1, p2, p3, t, u;
    p0 = inv_mul(a[0], b[0]);
    p1 = inv_mul(a[2], b[1]);
    p2 = inv_mul(a[4], b[2]);
    p3 = inv_mul(a[6], b[3]);
    t = inv_add(inv_sub(inv_sub(inv_mul(inv_add(a[2], a[6]), inv_add(b[1], b[3])), p1), p3), p2);
    r[0] = inv_add(p0, inv_mul(t, zeta));
    t = inv_sub(inv_sub(inv_mul(inv_add(a[4], a[6]), inv_add(b[2], b[3])), p2), p3);
    u = inv_sub(inv_sub(inv_mul(inv_add(a[0], a[2]), inv_add(b[0], b[1])), p0), p1);
    r[2] = inv_add(u, inv_mul(t, zeta));
    t = inv_sub(inv_sub(inv_mul(inv_add(a[0], a[4]), inv_add(b[0], b[2])), p0), p2);
    r[4] = inv_add(inv_add(t, p1), inv_mul(p3, zeta));
    t = inv_sub(inv_sub(inv_mul(inv_add(a[0], a[6]), inv_add(b[0], b[3])), p0), p3);
    u = inv_sub(inv_sub(inv_mul(inv_add(a[2], a[4]), inv_add(b[1], b[2])), p1), p2);
    r[6] = inv_add(t, u);
    p0 = inv_mul(a[1], b[0]);
    p1 = inv_mul(a[3], b[1]);
    p2 = inv_mul(a[5], b[2]);
    p3 = inv_mul(a[7], b[3]);
    t = inv_add(inv_sub(inv_sub(inv_mul(inv_add(a[3], a[7]), inv_add(b[1], b[3])), p1), p3), p2);
    r[1] = inv_neg(inv_add(p0, inv_mul(t, zeta)));
    t = inv_sub(inv_sub(inv_mul(inv_add(a[5], a[7]), inv_add(b[2], b[3])), p2), p3);
    u = inv_sub(inv_sub(inv_mul(inv_add(a[1], a[3]), inv_add(b[0], b[1])), p0), p1);
    r[3] = inv_neg(inv_add(u, inv_mul(t, zeta)));
    t = inv_sub(inv_sub(inv_mul(inv_add(a[1], a[5]), inv_add(b[0], b[2])), p0), p2);
    r[5] = inv_neg(inv_add(inv_add(t, p1), inv_mul(p3, zeta)));
    t = inv_sub(inv_sub(inv_mul(inv_add(a[1], a[7]), inv_add(b[0], b[3])), p0), p3);
    u = inv_sub(inv_sub(inv_mul(inv_add(a[3], a[5]), inv_add(b[1], b[2])), p1), p2);
    r[7] = inv_neg(inv_add(t, u));
}

static inline void inv_lift16(__m256i *r, const __m256i *a,
                              const __m256i *b, const __m256i *factor)
{
    __m256i zeta = *factor;
    __m256i even[8], odd[8], pe[15], po[15];
    even[0] = a[0]; odd[0] = a[1];
    even[1] = a[2]; odd[1] = a[3];
    even[2] = a[4]; odd[2] = a[5];
    even[3] = a[6]; odd[3] = a[7];
    even[4] = a[8]; odd[4] = a[9];
    even[5] = a[10]; odd[5] = a[11];
    even[6] = a[12]; odd[6] = a[13];
    even[7] = a[14]; odd[7] = a[15];
    inv_product8(pe, even, b);
    inv_product8(po, odd, b);
    r[0] = inv_add(pe[0], inv_mul(pe[8], zeta));
    r[1] = inv_neg(inv_add(po[0], inv_mul(po[8], zeta)));
    r[2] = inv_add(pe[1], inv_mul(pe[9], zeta));
    r[3] = inv_neg(inv_add(po[1], inv_mul(po[9], zeta)));
    r[4] = inv_add(pe[2], inv_mul(pe[10], zeta));
    r[5] = inv_neg(inv_add(po[2], inv_mul(po[10], zeta)));
    r[6] = inv_add(pe[3], inv_mul(pe[11], zeta));
    r[7] = inv_neg(inv_add(po[3], inv_mul(po[11], zeta)));
    r[8] = inv_add(pe[4], inv_mul(pe[12], zeta));
    r[9] = inv_neg(inv_add(po[4], inv_mul(po[12], zeta)));
    r[10] = inv_add(pe[5], inv_mul(pe[13], zeta));
    r[11] = inv_neg(inv_add(po[5], inv_mul(po[13], zeta)));
    r[12] = inv_add(pe[6], inv_mul(pe[14], zeta));
    r[13] = inv_neg(inv_add(po[6], inv_mul(po[14], zeta)));
    r[14] = pe[7];
    r[15] = inv_neg(po[7]);
}

void poly_baseinv_ntt(int16_t *r, int16_t *a)
{
    unsigned int i;
    __m256i den[8], n2[8][2], t2[2];
    __m256i n4[8][4], t4[4];
    __m256i n8[8][8], t8[8];
    __m256i av[16], rv[16], zeta;

    for(i = 0; i < 8; i++)
    {
        unsigned int off = 256 * i;
        zeta = inv_neg(_mm256_load_si256((const __m256i *)(invdata + 32 * i)));
        av[0] = _mm256_loadu_si256((const __m256i *)(a + off + 0));
        av[1] = _mm256_loadu_si256((const __m256i *)(a + off + 16));
        av[2] = _mm256_loadu_si256((const __m256i *)(a + off + 32));
        av[3] = _mm256_loadu_si256((const __m256i *)(a + off + 48));
        av[4] = _mm256_loadu_si256((const __m256i *)(a + off + 128));
        av[5] = _mm256_loadu_si256((const __m256i *)(a + off + 144));
        av[6] = _mm256_loadu_si256((const __m256i *)(a + off + 160));
        av[7] = _mm256_loadu_si256((const __m256i *)(a + off + 176));
        av[8] = _mm256_loadu_si256((const __m256i *)(a + off + 64));
        av[9] = _mm256_loadu_si256((const __m256i *)(a + off + 80));
        av[10] = _mm256_loadu_si256((const __m256i *)(a + off + 96));
        av[11] = _mm256_loadu_si256((const __m256i *)(a + off + 112));
        av[12] = _mm256_loadu_si256((const __m256i *)(a + off + 192));
        av[13] = _mm256_loadu_si256((const __m256i *)(a + off + 208));
        av[14] = _mm256_loadu_si256((const __m256i *)(a + off + 224));
        av[15] = _mm256_loadu_si256((const __m256i *)(a + off + 240));
        inv_norm16(n8[i], av, &zeta);
        inv_norm8(n4[i], n8[i], &zeta);
        inv_norm4(n2[i], n4[i], &zeta);
        inv_norm2(den + i, n2[i], &zeta);
    }
    inv_batch(den);
    for(i = 0; i < 8; i++)
    {
        unsigned int off = 256 * i;
        zeta = inv_neg(_mm256_load_si256((const __m256i *)(invdata + 32 * i)));
        av[0] = _mm256_loadu_si256((const __m256i *)(a + off + 0));
        av[1] = _mm256_loadu_si256((const __m256i *)(a + off + 16));
        av[2] = _mm256_loadu_si256((const __m256i *)(a + off + 32));
        av[3] = _mm256_loadu_si256((const __m256i *)(a + off + 48));
        av[4] = _mm256_loadu_si256((const __m256i *)(a + off + 128));
        av[5] = _mm256_loadu_si256((const __m256i *)(a + off + 144));
        av[6] = _mm256_loadu_si256((const __m256i *)(a + off + 160));
        av[7] = _mm256_loadu_si256((const __m256i *)(a + off + 176));
        av[8] = _mm256_loadu_si256((const __m256i *)(a + off + 64));
        av[9] = _mm256_loadu_si256((const __m256i *)(a + off + 80));
        av[10] = _mm256_loadu_si256((const __m256i *)(a + off + 96));
        av[11] = _mm256_loadu_si256((const __m256i *)(a + off + 112));
        av[12] = _mm256_loadu_si256((const __m256i *)(a + off + 192));
        av[13] = _mm256_loadu_si256((const __m256i *)(a + off + 208));
        av[14] = _mm256_loadu_si256((const __m256i *)(a + off + 224));
        av[15] = _mm256_loadu_si256((const __m256i *)(a + off + 240));
        t2[0] = inv_mul(n2[i][0], den[i]);
        t2[1] = inv_neg(inv_mul(n2[i][1], den[i]));
        inv_lift4(t4, n4[i], t2, &zeta);
        inv_lift8(t8, n8[i], t4, &zeta);
        inv_lift16(rv, av, t8, &zeta);
        _mm256_storeu_si256((__m256i *)(r + off + 0), rv[0]);
        _mm256_storeu_si256((__m256i *)(r + off + 16), rv[1]);
        _mm256_storeu_si256((__m256i *)(r + off + 32), rv[2]);
        _mm256_storeu_si256((__m256i *)(r + off + 48), rv[3]);
        _mm256_storeu_si256((__m256i *)(r + off + 128), rv[4]);
        _mm256_storeu_si256((__m256i *)(r + off + 144), rv[5]);
        _mm256_storeu_si256((__m256i *)(r + off + 160), rv[6]);
        _mm256_storeu_si256((__m256i *)(r + off + 176), rv[7]);
        _mm256_storeu_si256((__m256i *)(r + off + 64), rv[8]);
        _mm256_storeu_si256((__m256i *)(r + off + 80), rv[9]);
        _mm256_storeu_si256((__m256i *)(r + off + 96), rv[10]);
        _mm256_storeu_si256((__m256i *)(r + off + 112), rv[11]);
        _mm256_storeu_si256((__m256i *)(r + off + 192), rv[12]);
        _mm256_storeu_si256((__m256i *)(r + off + 208), rv[13]);
        _mm256_storeu_si256((__m256i *)(r + off + 224), rv[14]);
        _mm256_storeu_si256((__m256i *)(r + off + 240), rv[15]);
    }
}

int check_poly_inv_Zq(int16_t *a)
{
    unsigned int i;
    uint32_t m;

    __m256i acc;
    __m256i zero;
    __m256i one;

    __m256i x0, x1, x2, x3;
    __m256i s0, s1, s2, s3;
    __m256i t0, t1, t2, t3;
    __m256i c0, c1, c2, c3;

    acc  = _mm256_setzero_si256();
    zero = _mm256_setzero_si256();
    one  = _mm256_set1_epi16(1);

    for(i = 0; i < ZEN_N; i += 64)
    {
        x0 = _mm256_load_si256((const __m256i *)(a + i +  0));
        x1 = _mm256_load_si256((const __m256i *)(a + i + 16));
        x2 = _mm256_load_si256((const __m256i *)(a + i + 32));
        x3 = _mm256_load_si256((const __m256i *)(a + i + 48));

        s0 = _mm256_madd_epi16(x0, one);
        s1 = _mm256_madd_epi16(x1, one);
        s2 = _mm256_madd_epi16(x2, one);
        s3 = _mm256_madd_epi16(x3, one);

        t0 = _mm256_permute2x128_si256(s0, s0, 0x01);
        t1 = _mm256_permute2x128_si256(s1, s1, 0x01);
        t2 = _mm256_permute2x128_si256(s2, s2, 0x01);
        t3 = _mm256_permute2x128_si256(s3, s3, 0x01);

        s0 = _mm256_add_epi32(s0, t0);
        s1 = _mm256_add_epi32(s1, t1);
        s2 = _mm256_add_epi32(s2, t2);
        s3 = _mm256_add_epi32(s3, t3);

        s0 = _mm256_hadd_epi32(s0, s0);
        s1 = _mm256_hadd_epi32(s1, s1);
        s2 = _mm256_hadd_epi32(s2, s2);
        s3 = _mm256_hadd_epi32(s3, s3);

        s0 = _mm256_hadd_epi32(s0, s0);
        s1 = _mm256_hadd_epi32(s1, s1);
        s2 = _mm256_hadd_epi32(s2, s2);
        s3 = _mm256_hadd_epi32(s3, s3);

        c0 = _mm256_cmpeq_epi32(s0, zero);
        c1 = _mm256_cmpeq_epi32(s1, zero);
        c2 = _mm256_cmpeq_epi32(s2, zero);
        c3 = _mm256_cmpeq_epi32(s3, zero);

        acc = _mm256_or_si256(acc, c0);
        acc = _mm256_or_si256(acc, c1);
        acc = _mm256_or_si256(acc, c2);
        acc = _mm256_or_si256(acc, c3);
    }

    m = (uint32_t)_mm256_movemask_epi8(acc);

    return (int)((m | (0u - m)) >> 31);
}


int check_poly_inv_Z2(int16_t *a)
{
    unsigned int i;
    __m256i acc = _mm256_setzero_si256();
    __m128i lo, hi, x;
    uint32_t v;

    for(i = 0; i < ZEN_N4; i += 64)
    {
        __m256i x0, x1, x2, x3;

        x0 = _mm256_load_si256((const __m256i *)(a + i +  0));
        x1 = _mm256_load_si256((const __m256i *)(a + i + 16));
        x2 = _mm256_load_si256((const __m256i *)(a + i + 32));
        x3 = _mm256_load_si256((const __m256i *)(a + i + 48));

        acc = _mm256_xor_si256(acc, x0);
        acc = _mm256_xor_si256(acc, x1);
        acc = _mm256_xor_si256(acc, x2);
        acc = _mm256_xor_si256(acc, x3);
    }

    lo = _mm256_castsi256_si128(acc);
    hi = _mm256_extracti128_si256(acc, 1);
    x = _mm_xor_si128(lo, hi);

    x = _mm_xor_si128(x, _mm_srli_si128(x, 8));
    x = _mm_xor_si128(x, _mm_srli_si128(x, 4));
    x = _mm_xor_si128(x, _mm_srli_si128(x, 2));

    v = (uint32_t)(uint16_t)_mm_extract_epi16(x, 0);

    return (int)((((v | (0u - v)) >> 31) ^ 1u) & 1u);
}

#include <stdint.h>
#include <immintrin.h>

static inline uint64_t pack64_bits_avx2(const int16_t *a)
{
    __m256i x0, x1, x2, x3;
    __m256i y0, y1;
    __m256i one;
    uint32_t m0, m1;

    one = _mm256_set1_epi16(1);

    x0 = _mm256_loadu_si256((const __m256i *)(a +  0));
    x1 = _mm256_loadu_si256((const __m256i *)(a + 16));
    x2 = _mm256_loadu_si256((const __m256i *)(a + 32));
    x3 = _mm256_loadu_si256((const __m256i *)(a + 48));

    x0 = _mm256_and_si256(x0, one);
    x1 = _mm256_and_si256(x1, one);
    x2 = _mm256_and_si256(x2, one);
    x3 = _mm256_and_si256(x3, one);

    y0 = _mm256_packus_epi16(x0, x1);
    y1 = _mm256_packus_epi16(x2, x3);

    y0 = _mm256_permute4x64_epi64(y0, 0xD8);
    y1 = _mm256_permute4x64_epi64(y1, 0xD8);

    y0 = _mm256_slli_epi16(y0, 7);
    y1 = _mm256_slli_epi16(y1, 7);

    m0 = (uint32_t)_mm256_movemask_epi8(y0);
    m1 = (uint32_t)_mm256_movemask_epi8(y1);

    return (uint64_t)m0 | ((uint64_t)m1 << 32);
}

static inline void unpack64_bits_avx2(int16_t *r, uint64_t x)
{
    __m256i bitmask;
    __m256i zero;
    __m256i one;
    __m256i v0, v1, v2, v3;

    bitmask = _mm256_setr_epi16(
        0x0001, 0x0002, 0x0004, 0x0008,
        0x0010, 0x0020, 0x0040, 0x0080,
        0x0100, 0x0200, 0x0400, 0x0800,
        0x1000, 0x2000, 0x4000, (int16_t)0x8000
    );

    zero = _mm256_setzero_si256();
    one  = _mm256_set1_epi16(1);

    v0 = _mm256_set1_epi16((int16_t)(x));
    v1 = _mm256_set1_epi16((int16_t)(x >> 16));
    v2 = _mm256_set1_epi16((int16_t)(x >> 32));
    v3 = _mm256_set1_epi16((int16_t)(x >> 48));

    v0 = _mm256_and_si256(v0, bitmask);
    v1 = _mm256_and_si256(v1, bitmask);
    v2 = _mm256_and_si256(v2, bitmask);
    v3 = _mm256_and_si256(v3, bitmask);

    v0 = _mm256_cmpeq_epi16(v0, zero);
    v1 = _mm256_cmpeq_epi16(v1, zero);
    v2 = _mm256_cmpeq_epi16(v2, zero);
    v3 = _mm256_cmpeq_epi16(v3, zero);

    v0 = _mm256_andnot_si256(v0, one);
    v1 = _mm256_andnot_si256(v1, one);
    v2 = _mm256_andnot_si256(v2, one);
    v3 = _mm256_andnot_si256(v3, one);

    _mm256_storeu_si256((__m256i *)(r +  0), v0);
    _mm256_storeu_si256((__m256i *)(r + 16), v1);
    _mm256_storeu_si256((__m256i *)(r + 32), v2);
    _mm256_storeu_si256((__m256i *)(r + 48), v3);
}

static inline uint64_t parity64_u64(uint64_t x)
{
    x ^= x >> 32;
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x ^= x >> 2;
    x ^= x >> 1;

    return x & 1ULL;
}

static inline void clmul32x4_radix16_avx2_vec(__m256i a, __m256i b, __m256i *r)
{
    __m256i mask32;
    __m256i mask64;
    __m256i a0, a1, a2, a3;
    __m256i b0, b1, b2, b3;
    __m256i p;
    __m256i acc;

    mask32 = _mm256_set1_epi64x(0x11111111ULL);
    mask64 = _mm256_set1_epi64x(0x1111111111111111ULL);

    a0 = _mm256_and_si256(a, mask32);
    a1 = _mm256_and_si256(_mm256_srli_epi64(a, 1), mask32);
    a2 = _mm256_and_si256(_mm256_srli_epi64(a, 2), mask32);
    a3 = _mm256_and_si256(_mm256_srli_epi64(a, 3), mask32);

    b0 = _mm256_and_si256(b, mask32);
    b1 = _mm256_and_si256(_mm256_srli_epi64(b, 1), mask32);
    b2 = _mm256_and_si256(_mm256_srli_epi64(b, 2), mask32);
    b3 = _mm256_and_si256(_mm256_srli_epi64(b, 3), mask32);

    acc = _mm256_setzero_si256();

#define CLMUL32_STEP(A, B, SH) do {                     \
    p = _mm256_mul_epu32((A), (B));                     \
    p = _mm256_and_si256(p, mask64);                    \
    if((SH) != 0)                                       \
    {                                                   \
        p = _mm256_slli_epi64(p, (SH));                 \
    }                                                   \
    acc = _mm256_xor_si256(acc, p);                     \
} while (0)

    CLMUL32_STEP(a0, b0, 0);
    CLMUL32_STEP(a0, b1, 1);
    CLMUL32_STEP(a0, b2, 2);
    CLMUL32_STEP(a0, b3, 3);

    CLMUL32_STEP(a1, b0, 1);
    CLMUL32_STEP(a1, b1, 2);
    CLMUL32_STEP(a1, b2, 3);
    CLMUL32_STEP(a1, b3, 4);

    CLMUL32_STEP(a2, b0, 2);
    CLMUL32_STEP(a2, b1, 3);
    CLMUL32_STEP(a2, b2, 4);
    CLMUL32_STEP(a2, b3, 5);

    CLMUL32_STEP(a3, b0, 3);
    CLMUL32_STEP(a3, b1, 4);
    CLMUL32_STEP(a3, b2, 5);
    CLMUL32_STEP(a3, b3, 6);

#undef CLMUL32_STEP

    *r = acc;
}

static inline void clmul64x4_avx2_vec(__m256i a, __m256i b, __m256i *lo, __m256i *hi)
{
    __m256i mask32;
    __m256i a0, a1;
    __m256i b0, b1;
    __m256i z0, z1, z2;

    mask32 = _mm256_set1_epi64x(0xFFFFFFFFULL);

    a0 = _mm256_and_si256(a, mask32);
    a1 = _mm256_srli_epi64(a, 32);

    b0 = _mm256_and_si256(b, mask32);
    b1 = _mm256_srli_epi64(b, 32);

    clmul32x4_radix16_avx2_vec(a0, b0, &z0);
    clmul32x4_radix16_avx2_vec(a1, b1, &z2);
    clmul32x4_radix16_avx2_vec(_mm256_xor_si256(a0, a1),
                               _mm256_xor_si256(b0, b1),
                               &z1);

    z1 = _mm256_xor_si256(z1, z0);
    z1 = _mm256_xor_si256(z1, z2);

    *lo = _mm256_xor_si256(z0, _mm256_slli_epi64(z1, 32));
    *hi = _mm256_xor_si256(z2, _mm256_srli_epi64(z1, 32));
}

static inline void clmul64x4_avx2(uint64_t a0, uint64_t a1,
                                  uint64_t a2, uint64_t a3,
                                  uint64_t b0, uint64_t b1,
                                  uint64_t b2, uint64_t b3,
                                  uint64_t *lo0, uint64_t *hi0,
                                  uint64_t *lo1, uint64_t *hi1,
                                  uint64_t *lo2, uint64_t *hi2,
                                  uint64_t *lo3, uint64_t *hi3)
{
    __m256i va;
    __m256i vb;
    __m256i vlo;
    __m256i vhi;

    va = _mm256_set_epi64x((long long)a3, (long long)a2,
                           (long long)a1, (long long)a0);
    vb = _mm256_set_epi64x((long long)b3, (long long)b2,
                           (long long)b1, (long long)b0);

    clmul64x4_avx2_vec(va, vb, &vlo, &vhi);

    *lo0 = (uint64_t)_mm256_extract_epi64(vlo, 0);
    *lo1 = (uint64_t)_mm256_extract_epi64(vlo, 1);
    *lo2 = (uint64_t)_mm256_extract_epi64(vlo, 2);
    *lo3 = (uint64_t)_mm256_extract_epi64(vlo, 3);

    *hi0 = (uint64_t)_mm256_extract_epi64(vhi, 0);
    *hi1 = (uint64_t)_mm256_extract_epi64(vhi, 1);
    *hi2 = (uint64_t)_mm256_extract_epi64(vhi, 2);
    *hi3 = (uint64_t)_mm256_extract_epi64(vhi, 3);
}

static inline void clmul64_mod_full_avx2(uint64_t a, uint64_t b, uint64_t *r)
{
    uint64_t p0, p1;
    uint64_t d0, d1, d2, d3, d4, d5;

    clmul64x4_avx2(a, 0, 0, 0,
                   b, 0, 0, 0,
                   &p0, &p1,
                   &d0, &d1,
                   &d2, &d3,
                   &d4, &d5);

    *r = p0 ^ p1;
}

static inline void clmul128_avx2(uint64_t a0, uint64_t a1,
                                 uint64_t b0, uint64_t b1,
                                 uint64_t *p0, uint64_t *p1,
                                 uint64_t *p2, uint64_t *p3)
{
    uint64_t z00, z01;
    uint64_t z20, z21;
    uint64_t t0, t1;
    uint64_t d0, d1;
    uint64_t m0, m1;

    clmul64x4_avx2(a0, a1, a0 ^ a1, 0,
                   b0, b1, b0 ^ b1, 0,
                   &z00, &z01,
                   &z20, &z21,
                   &t0,  &t1,
                   &d0,  &d1);

    m0 = t0 ^ z00 ^ z20;
    m1 = t1 ^ z01 ^ z21;

    *p0 = z00;
    *p1 = z01 ^ m0;
    *p2 = z20 ^ m1;
    *p3 = z21;
}

static inline void clmul128_mod_avx2(uint64_t a0, uint64_t a1,
                                     uint64_t b0, uint64_t b1,
                                     uint64_t *r0, uint64_t *r1)
{
    uint64_t p0, p1, p2, p3;

    clmul128_avx2(a0, a1, b0, b1, &p0, &p1, &p2, &p3);

    *r0 = p0 ^ p2;
    *r1 = p1 ^ p3;
}

static inline void clmul256_avx2(uint64_t a0, uint64_t a1,
                                 uint64_t a2, uint64_t a3,
                                 uint64_t b0, uint64_t b1,
                                 uint64_t b2, uint64_t b3,
                                 uint64_t *p0, uint64_t *p1,
                                 uint64_t *p2, uint64_t *p3,
                                 uint64_t *p4, uint64_t *p5,
                                 uint64_t *p6, uint64_t *p7)
{
    uint64_t z00, z01, z02, z03;
    uint64_t z20, z21, z22, z23;
    uint64_t t0, t1, t2, t3;
    uint64_t m0, m1, m2, m3;

    clmul128_avx2(a0, a1, b0, b1, &z00, &z01, &z02, &z03);
    clmul128_avx2(a2, a3, b2, b3, &z20, &z21, &z22, &z23);
    clmul128_avx2(a0 ^ a2, a1 ^ a3, b0 ^ b2, b1 ^ b3, &t0, &t1, &t2, &t3);

    m0 = t0 ^ z00 ^ z20;
    m1 = t1 ^ z01 ^ z21;
    m2 = t2 ^ z02 ^ z22;
    m3 = t3 ^ z03 ^ z23;

    *p0 = z00;
    *p1 = z01;
    *p2 = z02 ^ m0;
    *p3 = z03 ^ m1;
    *p4 = z20 ^ m2;
    *p5 = z21 ^ m3;
    *p6 = z22;
    *p7 = z23;
}

static inline void clmul256_mod_avx2(uint64_t a0, uint64_t a1,
                                     uint64_t a2, uint64_t a3,
                                     uint64_t b0, uint64_t b1,
                                     uint64_t b2, uint64_t b3,
                                     uint64_t *r0, uint64_t *r1,
                                     uint64_t *r2, uint64_t *r3)
{
    uint64_t p0, p1, p2, p3;
    uint64_t p4, p5, p6, p7;

    clmul256_avx2(a0, a1, a2, a3,
                  b0, b1, b2, b3,
                  &p0, &p1, &p2, &p3,
                  &p4, &p5, &p6, &p7);

    *r0 = p0 ^ p4;
    *r1 = p1 ^ p5;
    *r2 = p2 ^ p6;
    *r3 = p3 ^ p7;
}

static inline void clmul512_avx2(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                                 uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7,
                                 uint64_t b0, uint64_t b1, uint64_t b2, uint64_t b3,
                                 uint64_t b4, uint64_t b5, uint64_t b6, uint64_t b7,
                                 uint64_t *p0, uint64_t *p1, uint64_t *p2, uint64_t *p3,
                                 uint64_t *p4, uint64_t *p5, uint64_t *p6, uint64_t *p7,
                                 uint64_t *p8, uint64_t *p9, uint64_t *p10, uint64_t *p11,
                                 uint64_t *p12, uint64_t *p13, uint64_t *p14, uint64_t *p15)
{
    uint64_t z00, z01, z02, z03, z04, z05, z06, z07;
    uint64_t z20, z21, z22, z23, z24, z25, z26, z27;
    uint64_t t0, t1, t2, t3, t4, t5, t6, t7;
    uint64_t m0, m1, m2, m3, m4, m5, m6, m7;

    clmul256_avx2(a0, a1, a2, a3,
                  b0, b1, b2, b3,
                  &z00, &z01, &z02, &z03,
                  &z04, &z05, &z06, &z07);

    clmul256_avx2(a4, a5, a6, a7,
                  b4, b5, b6, b7,
                  &z20, &z21, &z22, &z23,
                  &z24, &z25, &z26, &z27);

    clmul256_avx2(a0 ^ a4, a1 ^ a5, a2 ^ a6, a3 ^ a7,
                  b0 ^ b4, b1 ^ b5, b2 ^ b6, b3 ^ b7,
                  &t0, &t1, &t2, &t3,
                  &t4, &t5, &t6, &t7);

    m0 = t0 ^ z00 ^ z20;
    m1 = t1 ^ z01 ^ z21;
    m2 = t2 ^ z02 ^ z22;
    m3 = t3 ^ z03 ^ z23;
    m4 = t4 ^ z04 ^ z24;
    m5 = t5 ^ z05 ^ z25;
    m6 = t6 ^ z06 ^ z26;
    m7 = t7 ^ z07 ^ z27;

    *p0  = z00;
    *p1  = z01;
    *p2  = z02;
    *p3  = z03;
    *p4  = z04 ^ m0;
    *p5  = z05 ^ m1;
    *p6  = z06 ^ m2;
    *p7  = z07 ^ m3;
    *p8  = z20 ^ m4;
    *p9  = z21 ^ m5;
    *p10 = z22 ^ m6;
    *p11 = z23 ^ m7;
    *p12 = z24;
    *p13 = z25;
    *p14 = z26;
    *p15 = z27;
}

void mul_in_R2_1024(int16_t *a, int16_t *b, int16_t *res)
{
    uint64_t a0, a1, a2, a3, a4, a5, a6, a7;
    uint64_t a8, a9, a10, a11, a12, a13, a14, a15;
    uint64_t b0, b1, b2, b3, b4, b5, b6, b7;
    uint64_t b8, b9, b10, b11, b12, b13, b14, b15;

    uint64_t z00, z01, z02, z03, z04, z05, z06, z07;
    uint64_t z08, z09, z10, z11, z12, z13, z14, z15;
    uint64_t z20, z21, z22, z23, z24, z25, z26, z27;
    uint64_t z28, z29, z30, z31, z32, z33, z34, z35;
    uint64_t t0, t1, t2, t3, t4, t5, t6, t7;
    uint64_t t8, t9, t10, t11, t12, t13, t14, t15;
    uint64_t m0, m1, m2, m3, m4, m5, m6, m7;
    uint64_t m8, m9, m10, m11, m12, m13, m14, m15;

    uint64_t r0, r1, r2, r3, r4, r5, r6, r7;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

    a0  = pack64_bits_avx2(a +   0);
    a1  = pack64_bits_avx2(a +  64);
    a2  = pack64_bits_avx2(a + 128);
    a3  = pack64_bits_avx2(a + 192);
    a4  = pack64_bits_avx2(a + 256);
    a5  = pack64_bits_avx2(a + 320);
    a6  = pack64_bits_avx2(a + 384);
    a7  = pack64_bits_avx2(a + 448);
    a8  = pack64_bits_avx2(a + 512);
    a9  = pack64_bits_avx2(a + 576);
    a10 = pack64_bits_avx2(a + 640);
    a11 = pack64_bits_avx2(a + 704);
    a12 = pack64_bits_avx2(a + 768);
    a13 = pack64_bits_avx2(a + 832);
    a14 = pack64_bits_avx2(a + 896);
    a15 = pack64_bits_avx2(a + 960);

    b0  = pack64_bits_avx2(b +   0);
    b1  = pack64_bits_avx2(b +  64);
    b2  = pack64_bits_avx2(b + 128);
    b3  = pack64_bits_avx2(b + 192);
    b4  = pack64_bits_avx2(b + 256);
    b5  = pack64_bits_avx2(b + 320);
    b6  = pack64_bits_avx2(b + 384);
    b7  = pack64_bits_avx2(b + 448);
    b8  = pack64_bits_avx2(b + 512);
    b9  = pack64_bits_avx2(b + 576);
    b10 = pack64_bits_avx2(b + 640);
    b11 = pack64_bits_avx2(b + 704);
    b12 = pack64_bits_avx2(b + 768);
    b13 = pack64_bits_avx2(b + 832);
    b14 = pack64_bits_avx2(b + 896);
    b15 = pack64_bits_avx2(b + 960);

    clmul512_avx2(a0, a1, a2, a3, a4, a5, a6, a7,
                  b0, b1, b2, b3, b4, b5, b6, b7,
                  &z00, &z01, &z02, &z03, &z04, &z05, &z06, &z07,
                  &z08, &z09, &z10, &z11, &z12, &z13, &z14, &z15);

    clmul512_avx2(a8, a9, a10, a11, a12, a13, a14, a15,
                  b8, b9, b10, b11, b12, b13, b14, b15,
                  &z20, &z21, &z22, &z23, &z24, &z25, &z26, &z27,
                  &z28, &z29, &z30, &z31, &z32, &z33, &z34, &z35);

    clmul512_avx2(a0 ^ a8, a1 ^ a9, a2 ^ a10, a3 ^ a11,
                  a4 ^ a12, a5 ^ a13, a6 ^ a14, a7 ^ a15,
                  b0 ^ b8, b1 ^ b9, b2 ^ b10, b3 ^ b11,
                  b4 ^ b12, b5 ^ b13, b6 ^ b14, b7 ^ b15,
                  &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7,
                  &t8, &t9, &t10, &t11, &t12, &t13, &t14, &t15);

    m0  = t0  ^ z00 ^ z20;
    m1  = t1  ^ z01 ^ z21;
    m2  = t2  ^ z02 ^ z22;
    m3  = t3  ^ z03 ^ z23;
    m4  = t4  ^ z04 ^ z24;
    m5  = t5  ^ z05 ^ z25;
    m6  = t6  ^ z06 ^ z26;
    m7  = t7  ^ z07 ^ z27;
    m8  = t8  ^ z08 ^ z28;
    m9  = t9  ^ z09 ^ z29;
    m10 = t10 ^ z10 ^ z30;
    m11 = t11 ^ z11 ^ z31;
    m12 = t12 ^ z12 ^ z32;
    m13 = t13 ^ z13 ^ z33;
    m14 = t14 ^ z14 ^ z34;
    m15 = t15 ^ z15 ^ z35;

    r0  = z00 ^ z20 ^ m8;
    r1  = z01 ^ z21 ^ m9;
    r2  = z02 ^ z22 ^ m10;
    r3  = z03 ^ z23 ^ m11;
    r4  = z04 ^ z24 ^ m12;
    r5  = z05 ^ z25 ^ m13;
    r6  = z06 ^ z26 ^ m14;
    r7  = z07 ^ z27 ^ m15;
    r8  = z08 ^ z28 ^ m0;
    r9  = z09 ^ z29 ^ m1;
    r10 = z10 ^ z30 ^ m2;
    r11 = z11 ^ z31 ^ m3;
    r12 = z12 ^ z32 ^ m4;
    r13 = z13 ^ z33 ^ m5;
    r14 = z14 ^ z34 ^ m6;
    r15 = z15 ^ z35 ^ m7;

    unpack64_bits_avx2(res +   0, r0);
    unpack64_bits_avx2(res +  64, r1);
    unpack64_bits_avx2(res + 128, r2);
    unpack64_bits_avx2(res + 192, r3);
    unpack64_bits_avx2(res + 256, r4);
    unpack64_bits_avx2(res + 320, r5);
    unpack64_bits_avx2(res + 384, r6);
    unpack64_bits_avx2(res + 448, r7);
    unpack64_bits_avx2(res + 512, r8);
    unpack64_bits_avx2(res + 576, r9);
    unpack64_bits_avx2(res + 640, r10);
    unpack64_bits_avx2(res + 704, r11);
    unpack64_bits_avx2(res + 768, r12);
    unpack64_bits_avx2(res + 832, r13);
    unpack64_bits_avx2(res + 896, r14);
    unpack64_bits_avx2(res + 960, r15);
}

#define PREFIX64(X) do {                  \
    (X) ^= (X) << 1;                      \
    (X) ^= (X) << 2;                      \
    (X) ^= (X) << 4;                      \
    (X) ^= (X) << 8;                      \
    (X) ^= (X) << 16;                     \
    (X) ^= (X) << 32;                     \
} while (0)

#define XOR_SHIFT512(SH) do {                                             \
    sh = (SH);                                                            \
    if(sh < 64)                                                           \
    {                                                                     \
        x0 = k0 << sh;                                                    \
        x1 = (k1 << sh) | (k0 >> (64 - sh));                              \
        x2 = (k2 << sh) | (k1 >> (64 - sh));                              \
        x3 = (k3 << sh) | (k2 >> (64 - sh));                              \
        x4 = (k4 << sh) | (k3 >> (64 - sh));                              \
        x5 = (k5 << sh) | (k4 >> (64 - sh));                              \
        x6 = (k6 << sh) | (k5 >> (64 - sh));                              \
        x7 = (k7 << sh) | (k6 >> (64 - sh));                              \
        k0 ^= x0;                                                         \
        k1 ^= x1;                                                         \
        k2 ^= x2;                                                         \
        k3 ^= x3;                                                         \
        k4 ^= x4;                                                         \
        k5 ^= x5;                                                         \
        k6 ^= x6;                                                         \
        k7 ^= x7;                                                         \
    }                                                                     \
    else if(sh == 64)                                                     \
    {                                                                     \
        k7 ^= k6;                                                         \
        k6 ^= k5;                                                         \
        k5 ^= k4;                                                         \
        k4 ^= k3;                                                         \
        k3 ^= k2;                                                         \
        k2 ^= k1;                                                         \
        k1 ^= k0;                                                         \
    }                                                                     \
    else if(sh == 128)                                                    \
    {                                                                     \
        k7 ^= k5;                                                         \
        k6 ^= k4;                                                         \
        k5 ^= k3;                                                         \
        k4 ^= k2;                                                         \
        k3 ^= k1;                                                         \
        k2 ^= k0;                                                         \
    }                                                                     \
    else                                                                  \
    {                                                                     \
        k7 ^= k3;                                                         \
        k6 ^= k2;                                                         \
        k5 ^= k1;                                                         \
        k4 ^= k0;                                                         \
    }                                                                     \
} while (0)

#define BLOCK_PREFIX512(N) do {                                           \
    for(sh = (N); sh < 512; sh <<= 1)                                     \
    {                                                                     \
        XOR_SHIFT512(sh);                                                 \
    }                                                                     \
} while (0)

#define MUL_MOD_SMALL_ROTATE_CONST(AP, TP, N, MASKN, OUT) do {            \
    unsigned int _i;                                                       \
    uint64_t _b;                                                           \
    uint64_t _r;                                                           \
    uint64_t _mask;                                                        \
    uint64_t _wrap;                                                        \
                                                                          \
    _b = (TP) & (MASKN);                                                   \
    _r = 0;                                                               \
                                                                          \
    for(_i = 0; _i < (N); _i++)                                           \
    {                                                                     \
        _mask = 0ULL - (((AP) >> _i) & 1ULL);                             \
        _r ^= _b & _mask;                                                  \
        _wrap = (_b >> ((N) - 1)) & 1ULL;                                  \
        _b = ((_b << 1) | _wrap) & (MASKN);                                \
    }                                                                     \
                                                                          \
    (OUT) = _r & (MASKN);                                                  \
} while (0)

#define MUL_LOW_BY_F512_ROTATE_CONST(BP, BITS) do {                       \
    unsigned int _i;                                                       \
    uint64_t _g0, _g1, _g2, _g3, _g4, _g5, _g6, _g7;                      \
    uint64_t _u0, _u1, _u2, _u3, _u4, _u5, _u6, _u7;                      \
    uint64_t _mask;                                                        \
    uint64_t _wrap;                                                        \
                                                                          \
    _g0 = f0;                                                              \
    _g1 = f1;                                                              \
    _g2 = f2;                                                              \
    _g3 = f3;                                                              \
    _g4 = f4;                                                              \
    _g5 = f5;                                                              \
    _g6 = f6;                                                              \
    _g7 = f7;                                                              \
                                                                          \
    r0 = 0;                                                               \
    r1 = 0;                                                               \
    r2 = 0;                                                               \
    r3 = 0;                                                               \
    r4 = 0;                                                               \
    r5 = 0;                                                               \
    r6 = 0;                                                               \
    r7 = 0;                                                               \
                                                                          \
    for(_i = 0; _i < (BITS); _i++)                                        \
    {                                                                     \
        _mask = 0ULL - (((BP) >> _i) & 1ULL);                             \
                                                                          \
        r0 ^= _g0 & _mask;                                                 \
        r1 ^= _g1 & _mask;                                                 \
        r2 ^= _g2 & _mask;                                                 \
        r3 ^= _g3 & _mask;                                                 \
        r4 ^= _g4 & _mask;                                                 \
        r5 ^= _g5 & _mask;                                                 \
        r6 ^= _g6 & _mask;                                                 \
        r7 ^= _g7 & _mask;                                                 \
                                                                          \
        _wrap = _g7 >> 63;                                                 \
                                                                          \
        _u0 = (_g0 << 1) | _wrap;                                          \
        _u1 = (_g1 << 1) | (_g0 >> 63);                                    \
        _u2 = (_g2 << 1) | (_g1 >> 63);                                    \
        _u3 = (_g3 << 1) | (_g2 >> 63);                                    \
        _u4 = (_g4 << 1) | (_g3 >> 63);                                    \
        _u5 = (_g5 << 1) | (_g4 >> 63);                                    \
        _u6 = (_g6 << 1) | (_g5 >> 63);                                    \
        _u7 = (_g7 << 1) | (_g6 >> 63);                                    \
                                                                          \
        _g0 = _u0;                                                         \
        _g1 = _u1;                                                         \
        _g2 = _u2;                                                         \
        _g3 = _u3;                                                         \
        _g4 = _u4;                                                         \
        _g5 = _u5;                                                         \
        _g6 = _u6;                                                         \
        _g7 = _u7;                                                         \
    }                                                                     \
} while (0)

static inline void mul64_by_f512_avx2(uint64_t bp,
                                      uint64_t f0, uint64_t f1,
                                      uint64_t f2, uint64_t f3,
                                      uint64_t f4, uint64_t f5,
                                      uint64_t f6, uint64_t f7,
                                      uint64_t *r0, uint64_t *r1,
                                      uint64_t *r2, uint64_t *r3,
                                      uint64_t *r4, uint64_t *r5,
                                      uint64_t *r6, uint64_t *r7)
{
    uint64_t p0, p1, q0, q1;
    uint64_t s0, s1, t0, t1;
    uint64_t u0, u1, v0, v1;
    uint64_t w0, w1, y0, y1;

    clmul64x4_avx2(bp, bp, bp, bp,
                   f0, f1, f2, f3,
                   &p0, &p1,
                   &q0, &q1,
                   &s0, &s1,
                   &t0, &t1);

    clmul64x4_avx2(bp, bp, bp, bp,
                   f4, f5, f6, f7,
                   &u0, &u1,
                   &v0, &v1,
                   &w0, &w1,
                   &y0, &y1);

    *r0 = p0 ^ y1;
    *r1 = p1 ^ q0;
    *r2 = q1 ^ s0;
    *r3 = s1 ^ t0;
    *r4 = t1 ^ u0;
    *r5 = u1 ^ v0;
    *r6 = v1 ^ w0;
    *r7 = w1 ^ y0;
}

static inline void mul128_by_f512_avx2(uint64_t bp0, uint64_t bp1,
                                       uint64_t f0, uint64_t f1,
                                       uint64_t f2, uint64_t f3,
                                       uint64_t f4, uint64_t f5,
                                       uint64_t f6, uint64_t f7,
                                       uint64_t *r0, uint64_t *r1,
                                       uint64_t *r2, uint64_t *r3,
                                       uint64_t *r4, uint64_t *r5,
                                       uint64_t *r6, uint64_t *r7)
{
    uint64_t a0, a1, a2, a3;
    uint64_t b0, b1, b2, b3;
    uint64_t c0, c1, c2, c3;
    uint64_t d0, d1, d2, d3;

    clmul128_avx2(bp0, bp1, f0, f1, &a0, &a1, &a2, &a3);
    clmul128_avx2(bp0, bp1, f2, f3, &b0, &b1, &b2, &b3);
    clmul128_avx2(bp0, bp1, f4, f5, &c0, &c1, &c2, &c3);
    clmul128_avx2(bp0, bp1, f6, f7, &d0, &d1, &d2, &d3);

    *r0 = a0 ^ d2;
    *r1 = a1 ^ d3;
    *r2 = a2 ^ b0;
    *r3 = a3 ^ b1;
    *r4 = b2 ^ c0;
    *r5 = b3 ^ c1;
    *r6 = c2 ^ d0;
    *r7 = c3 ^ d1;
}

static inline void mul256_by_f512_avx2(uint64_t bp0, uint64_t bp1,
                                       uint64_t bp2, uint64_t bp3,
                                       uint64_t f0, uint64_t f1,
                                       uint64_t f2, uint64_t f3,
                                       uint64_t f4, uint64_t f5,
                                       uint64_t f6, uint64_t f7,
                                       uint64_t *r0, uint64_t *r1,
                                       uint64_t *r2, uint64_t *r3,
                                       uint64_t *r4, uint64_t *r5,
                                       uint64_t *r6, uint64_t *r7)
{
    uint64_t a0, a1, a2, a3, a4, a5, a6, a7;
    uint64_t b0, b1, b2, b3, b4, b5, b6, b7;

    clmul256_avx2(bp0, bp1, bp2, bp3,
                  f0, f1, f2, f3,
                  &a0, &a1, &a2, &a3,
                  &a4, &a5, &a6, &a7);

    clmul256_avx2(bp0, bp1, bp2, bp3,
                  f4, f5, f6, f7,
                  &b0, &b1, &b2, &b3,
                  &b4, &b5, &b6, &b7);

    *r0 = a0 ^ b4;
    *r1 = a1 ^ b5;
    *r2 = a2 ^ b6;
    *r3 = a3 ^ b7;
    *r4 = a4 ^ b0;
    *r5 = a5 ^ b1;
    *r6 = a6 ^ b2;
    *r7 = a7 ^ b3;
}

#define DO_LEVEL_LT64_512(N, MASKN) do {                                \
    t64 = k0 ^ k1 ^ k2 ^ k3 ^ k4 ^ k5 ^ k6 ^ k7;                        \
                                                                         \
    if((N) <= 32) t64 ^= t64 >> 32;                                      \
    if((N) <= 16) t64 ^= t64 >> 16;                                      \
    if((N) <= 8)  t64 ^= t64 >> 8;                                       \
    if((N) <= 4)  t64 ^= t64 >> 4;                                       \
    if((N) <= 2)  t64 ^= t64 >> 2;                                       \
                                                                         \
    tp = t64 & (MASKN);                                                  \
    ap = inv0 & (MASKN);                                                 \
                                                                         \
    MUL_MOD_SMALL_ROTATE_CONST(ap, tp, N, MASKN, bp);                    \
    MUL_LOW_BY_F512_ROTATE_CONST(bp, N);                                 \
                                                                         \
    k0 ^= r0;                                                            \
    k1 ^= r1;                                                            \
    k2 ^= r2;                                                            \
    k3 ^= r3;                                                            \
    k4 ^= r4;                                                            \
    k5 ^= r5;                                                            \
    k6 ^= r6;                                                            \
    k7 ^= r7;                                                            \
                                                                         \
    BLOCK_PREFIX512(N);                                                  \
                                                                         \
    inv0 ^= bp;                                                          \
    inv0 ^= bp << (N);                                                   \
} while (0)

#define DO_LEVEL_64_512() do {                                           \
    tp = k0 ^ k1 ^ k2 ^ k3 ^ k4 ^ k5 ^ k6 ^ k7;                          \
    ap = inv0;                                                           \
                                                                         \
    clmul64_mod_full_avx2(ap, tp, &bp);                                  \
                                                                         \
    mul64_by_f512_avx2(bp, f0, f1, f2, f3, f4, f5, f6, f7,                \
                       &r0, &r1, &r2, &r3,                                \
                       &r4, &r5, &r6, &r7);                               \
                                                                         \
    k0 ^= r0;                                                            \
    k1 ^= r1;                                                            \
    k2 ^= r2;                                                            \
    k3 ^= r3;                                                            \
    k4 ^= r4;                                                            \
    k5 ^= r5;                                                            \
    k6 ^= r6;                                                            \
    k7 ^= r7;                                                            \
                                                                         \
    BLOCK_PREFIX512(64);                                                 \
                                                                         \
    inv0 ^= bp;                                                          \
    inv1 ^= bp;                                                          \
} while (0)

void FastInversion(int16_t *f_inv, int16_t *f)
{
    unsigned int sh;

    uint64_t f0, f1, f2, f3, f4, f5, f6, f7;
    uint64_t k0, k1, k2, k3, k4, k5, k6, k7;
    uint64_t inv0, inv1, inv2, inv3, inv4, inv5, inv6, inv7;

    uint64_t t64;
    uint64_t tp, ap, bp;
    uint64_t tp0, tp1, tp2, tp3;
    uint64_t ap0, ap1, ap2, ap3;
    uint64_t bp0, bp1, bp2, bp3;

    uint64_t r0, r1, r2, r3, r4, r5, r6, r7;
    uint64_t acc64, acc, mask;

    uint64_t x0, x1, x2, x3, x4, x5, x6, x7;

    f0 = pack64_bits_avx2(f +   0);
    f1 = pack64_bits_avx2(f +  64);
    f2 = pack64_bits_avx2(f + 128);
    f3 = pack64_bits_avx2(f + 192);
    f4 = pack64_bits_avx2(f + 256);
    f5 = pack64_bits_avx2(f + 320);
    f6 = pack64_bits_avx2(f + 384);
    f7 = pack64_bits_avx2(f + 448);

    k0 = f0;
    PREFIX64(k0);

    k1 = f1;
    PREFIX64(k1);
    k1 ^= 0ULL - (k0 >> 63);

    k2 = f2;
    PREFIX64(k2);
    k2 ^= 0ULL - (k1 >> 63);

    k3 = f3;
    PREFIX64(k3);
    k3 ^= 0ULL - (k2 >> 63);

    k4 = f4;
    PREFIX64(k4);
    k4 ^= 0ULL - (k3 >> 63);

    k5 = f5;
    PREFIX64(k5);
    k5 ^= 0ULL - (k4 >> 63);

    k6 = f6;
    PREFIX64(k6);
    k6 ^= 0ULL - (k5 >> 63);

    k7 = f7;
    PREFIX64(k7);
    k7 ^= 0ULL - (k6 >> 63);

    acc64 = k0 ^ k1 ^ k2 ^ k3 ^ k4 ^ k5 ^ k6 ^ k7;
    acc = parity64_u64(acc64);

    mask = 0ULL - acc;

    k0 ^= f0 & mask;
    k1 ^= f1 & mask;
    k2 ^= f2 & mask;
    k3 ^= f3 & mask;
    k4 ^= f4 & mask;
    k5 ^= f5 & mask;
    k6 ^= f6 & mask;
    k7 ^= f7 & mask;

    PREFIX64(k0);

    PREFIX64(k1);
    k1 ^= 0ULL - (k0 >> 63);

    PREFIX64(k2);
    k2 ^= 0ULL - (k1 >> 63);

    PREFIX64(k3);
    k3 ^= 0ULL - (k2 >> 63);

    PREFIX64(k4);
    k4 ^= 0ULL - (k3 >> 63);

    PREFIX64(k5);
    k5 ^= 0ULL - (k4 >> 63);

    PREFIX64(k6);
    k6 ^= 0ULL - (k5 >> 63);

    PREFIX64(k7);
    k7 ^= 0ULL - (k6 >> 63);

    inv0 = (acc ^ 1ULL) | (acc << 1);
    inv1 = 0;
    inv2 = 0;
    inv3 = 0;
    inv4 = 0;
    inv5 = 0;
    inv6 = 0;
    inv7 = 0;

    DO_LEVEL_LT64_512(2,  0x0000000000000003ULL);
    DO_LEVEL_LT64_512(4,  0x000000000000000FULL);
    DO_LEVEL_LT64_512(8,  0x00000000000000FFULL);
    DO_LEVEL_LT64_512(16, 0x000000000000FFFFULL);
    DO_LEVEL_LT64_512(32, 0x00000000FFFFFFFFULL);
    DO_LEVEL_64_512();

    tp0 = k0 ^ k2 ^ k4 ^ k6;
    tp1 = k1 ^ k3 ^ k5 ^ k7;

    ap0 = inv0;
    ap1 = inv1;

    clmul128_mod_avx2(ap0, ap1, tp0, tp1, &bp0, &bp1);

    mul128_by_f512_avx2(bp0, bp1,
                        f0, f1, f2, f3, f4, f5, f6, f7,
                        &r0, &r1, &r2, &r3,
                        &r4, &r5, &r6, &r7);

    k0 ^= r0;
    k1 ^= r1;
    k2 ^= r2;
    k3 ^= r3;
    k4 ^= r4;
    k5 ^= r5;
    k6 ^= r6;
    k7 ^= r7;

    BLOCK_PREFIX512(128);

    inv0 ^= bp0;
    inv1 ^= bp1;
    inv2 ^= bp0;
    inv3 ^= bp1;

    tp0 = k0 ^ k4;
    tp1 = k1 ^ k5;
    tp2 = k2 ^ k6;
    tp3 = k3 ^ k7;

    ap0 = inv0;
    ap1 = inv1;
    ap2 = inv2;
    ap3 = inv3;

    clmul256_mod_avx2(ap0, ap1, ap2, ap3,
                      tp0, tp1, tp2, tp3,
                      &bp0, &bp1, &bp2, &bp3);

    mul256_by_f512_avx2(bp0, bp1, bp2, bp3,
                        f0, f1, f2, f3, f4, f5, f6, f7,
                        &r0, &r1, &r2, &r3,
                        &r4, &r5, &r6, &r7);

    k0 ^= r0;
    k1 ^= r1;
    k2 ^= r2;
    k3 ^= r3;
    k4 ^= r4;
    k5 ^= r5;
    k6 ^= r6;
    k7 ^= r7;

    BLOCK_PREFIX512(256);

    inv0 ^= bp0;
    inv1 ^= bp1;
    inv2 ^= bp2;
    inv3 ^= bp3;
    inv4 ^= bp0;
    inv5 ^= bp1;
    inv6 ^= bp2;
    inv7 ^= bp3;

    unpack64_bits_avx2(f_inv +   0, inv0);
    unpack64_bits_avx2(f_inv +  64, inv1);
    unpack64_bits_avx2(f_inv + 128, inv2);
    unpack64_bits_avx2(f_inv + 192, inv3);
    unpack64_bits_avx2(f_inv + 256, inv4);
    unpack64_bits_avx2(f_inv + 320, inv5);
    unpack64_bits_avx2(f_inv + 384, inv6);
    unpack64_bits_avx2(f_inv + 448, inv7);
}

int poly_generate_gf(int16_t *a, const uint8_t *seed, uint8_t nonce)
{
    uint8_t buf[ZEN_N_LEN_BYTES*5];

    if (zen_pseudoXOF(ZEN_N*5, seed, SEED_LEN_BYTES*8, buf, nonce) != 0)
        return -1;
    tenary3_32(a, buf);
    return 0;
}

int poly_generate_se(int16_t *a, const uint8_t *seed, uint8_t nonce)
{
    uint8_t buf[ZEN_N_LEN_BYTES*3];

    if (zen_pseudoXOF(ZEN_N*3, seed, SEED_LEN_BYTES*8, buf, nonce) != 0)
        return -1;
    tenary1_8(a, buf);
    return 0;
}

void poly_bit2byte_pack(uint8_t *pa, const int16_t *a, const unsigned int n)
{
    unsigned int i, j, chunk;
    const unsigned int nbytes = n >> 3;
    const __m256i mask01 = _mm256_set1_epi16(1);

    for(i = 0; i < nbytes; i += 32)
    {
        uint32_t m[8];

        chunk = nbytes - i;
        if(chunk > 32)
            chunk = 32;

        for(j = 0; j < chunk / 4; j++)
        {
            const int16_t *src = a + ((i + 4 * j) << 3);
            __m256i x0, x1, y;

            x0 = _mm256_loadu_si256((const __m256i *)(src +  0));
            x1 = _mm256_loadu_si256((const __m256i *)(src + 16));

            x0 = _mm256_and_si256(x0, mask01);
            x1 = _mm256_and_si256(x1, mask01);

            y = _mm256_packus_epi16(x0, x1);
            y = _mm256_permute4x64_epi64(y, 0xD8);
            y = _mm256_slli_epi16(y, 7);

            m[j] = (uint32_t)_mm256_movemask_epi8(y);
        }

        memcpy(pa + i, m, chunk);
    }
}

void poly_xor_bit2byte_pack(uint8_t *pa, const int16_t *a,
                            const int16_t *b, const unsigned int n)
{
    unsigned int i, j, chunk;
    const unsigned int nbytes = n >> 3;
    const __m256i mask01 = _mm256_set1_epi16(1);

    for(i = 0; i < nbytes; i += 32)
    {
        uint32_t m[8];

        chunk = nbytes - i;
        if(chunk > 32)
            chunk = 32;

        for(j = 0; j < chunk / 4; j++)
        {
            const unsigned int offset = (i + 4 * j) << 3;
            __m256i x0, x1, y;

            x0 = _mm256_loadu_si256((const __m256i *)(a + offset));
            x0 = _mm256_xor_si256(
                x0, _mm256_loadu_si256((const __m256i *)(b + offset)));
            x1 = _mm256_loadu_si256((const __m256i *)(a + offset + 16));
            x1 = _mm256_xor_si256(
                x1, _mm256_loadu_si256((const __m256i *)(b + offset + 16)));

            x0 = _mm256_and_si256(x0, mask01);
            x1 = _mm256_and_si256(x1, mask01);

            y = _mm256_packus_epi16(x0, x1);
            y = _mm256_permute4x64_epi64(y, 0xD8);
            y = _mm256_slli_epi16(y, 7);

            m[j] = (uint32_t)_mm256_movemask_epi8(y);
        }

        memcpy(pa + i, m, chunk);
    }
}

void poly_byte2bit_unpack(int16_t *a, const uint8_t *pa, const unsigned int n)
{
    unsigned int i, j, chunk;
    const unsigned int nbytes = n >> 3;

    const __m256i zero = _mm256_setzero_si256(), one = _mm256_set1_epi16(1),
                  bitmask = _mm256_setr_epi16(
        0x0001, 0x0002, 0x0004, 0x0008,
        0x0010, 0x0020, 0x0040, 0x0080,
        0x0100, 0x0200, 0x0400, 0x0800,
        0x1000, 0x2000, 0x4000, (int16_t)0x8000
    );

    for(i = 0; i < nbytes; i += 32)
    {
        chunk = nbytes - i;
        if(chunk > 32)
            chunk = 32;

        for(j = 0; j < chunk / 4; j++)
        {
            uint32_t w;
            __m256i x0, x1;
            int16_t *dst = a + ((i + 4 * j) << 3);

            w  = (uint32_t)pa[i + 4 * j + 0];
            w |= (uint32_t)pa[i + 4 * j + 1] << 8;
            w |= (uint32_t)pa[i + 4 * j + 2] << 16;
            w |= (uint32_t)pa[i + 4 * j + 3] << 24;

            x0 = _mm256_set1_epi16((int16_t)(w & 0xFFFFu));
            x1 = _mm256_set1_epi16((int16_t)(w >> 16));

            x0 = _mm256_and_si256(x0, bitmask);
            x1 = _mm256_and_si256(x1, bitmask);

            x0 = _mm256_cmpeq_epi16(x0, zero);
            x1 = _mm256_cmpeq_epi16(x1, zero);

            x0 = _mm256_andnot_si256(x0, one);
            x1 = _mm256_andnot_si256(x1, one);

            _mm256_storeu_si256((__m256i *)(dst +  0), x0);
            _mm256_storeu_si256((__m256i *)(dst + 16), x1);
        }
    }
}

void poly_secretkey_pack(uint8_t *ss, const int16_t *a)
{
    unsigned int i, k;
    const __m256i mask01 = _mm256_set1_epi16(1);

    for(i = 0; i < ZEN_N; i += 32)
    {
        const int16_t *src = a + i;
        uint8_t *dst = ss + (i / 32) * 40;

        __m256i x0 = _mm256_loadu_si256((const __m256i *)(src +  0)),
                x1 = _mm256_loadu_si256((const __m256i *)(src + 16));

        uint32_t w[10];

        for(k = 0; k < 10; k++)
        {
            __m128i cnt = _mm_cvtsi32_si128((int)k);
            __m256i y0, y1, y;

            y0 = _mm256_srl_epi16(x0, cnt);
            y1 = _mm256_srl_epi16(x1, cnt);

            y0 = _mm256_and_si256(y0, mask01);
            y1 = _mm256_and_si256(y1, mask01);

            y = _mm256_packus_epi16(y0, y1);
            y = _mm256_permute4x64_epi64(y, 0xD8);
            y = _mm256_slli_epi16(y, 7);

            w[k] = (uint32_t)_mm256_movemask_epi8(y);
        }

        _mm256_storeu_si256((__m256i *)(dst + 0),
            _mm256_setr_epi32(
                (int)w[0], (int)w[1], (int)w[2], (int)w[3],
                (int)w[4], (int)w[5], (int)w[6], (int)w[7]
            )
        );

        _mm_storel_epi64((__m128i *)(dst + 32),
            _mm_setr_epi32((int)w[8], (int)w[9], 0, 0)
        );
    }
}

void poly_secretkey_unpack(int16_t *a, const uint8_t *ss)
{
    unsigned int i, k;
    const __m256i zero = _mm256_setzero_si256(), one = _mm256_set1_epi16(1),
                  bitmask = _mm256_setr_epi16(
        0x0001, 0x0002, 0x0004, 0x0008,
        0x0010, 0x0020, 0x0040, 0x0080,
        0x0100, 0x0200, 0x0400, 0x0800,
        0x1000, 0x2000, 0x4000, (int16_t)0x8000
    );

    for(i = 0; i < ZEN_N; i += 32)
    {
        int16_t *dst = a + i;
        const uint8_t *src = ss + (i / 32) * 40;

        __m256i acc0 = _mm256_setzero_si256();
        __m256i acc1 = _mm256_setzero_si256();

        for(k = 0; k < 10; k++)
        {
            uint32_t w;
            __m128i cnt;
            __m256i x0, x1;

            w  = (uint32_t)src[4 * k + 0];
            w |= (uint32_t)src[4 * k + 1] << 8;
            w |= (uint32_t)src[4 * k + 2] << 16;
            w |= (uint32_t)src[4 * k + 3] << 24;

            cnt = _mm_cvtsi32_si128((int)k);

            x0 = _mm256_set1_epi16((int16_t)(w & 0xFFFFu));
            x1 = _mm256_set1_epi16((int16_t)(w >> 16));

            x0 = _mm256_and_si256(x0, bitmask);
            x1 = _mm256_and_si256(x1, bitmask);

            x0 = _mm256_cmpeq_epi16(x0, zero);
            x1 = _mm256_cmpeq_epi16(x1, zero);

            x0 = _mm256_andnot_si256(x0, one);
            x1 = _mm256_andnot_si256(x1, one);

            x0 = _mm256_sll_epi16(x0, cnt);
            x1 = _mm256_sll_epi16(x1, cnt);

            acc0 = _mm256_or_si256(acc0, x0);
            acc1 = _mm256_or_si256(acc1, x1);
        }

        _mm256_storeu_si256((__m256i *)(dst +  0), acc0);
        _mm256_storeu_si256((__m256i *)(dst + 16), acc1);
    }
}

static const uint64_t pack_table[] = 
{
    1, 769, 591361, 454756609, 349707832321
};

static inline uint64_t pack5_base769(const int16_t *a)
{
    return (uint64_t)(uint16_t)a[0]
         + (uint64_t)(uint16_t)a[1] * pack_table[1]
         + (uint64_t)(uint16_t)a[2] * pack_table[2]
         + (uint64_t)(uint16_t)a[3] * pack_table[3]
         + (uint64_t)(uint16_t)a[4] * pack_table[4];
}

static inline uint64_t load64_le(const uint8_t *src)
{
    uint64_t x;
    memcpy(&x, src, sizeof(x));
    return x;
}

static inline void store64_le(uint8_t *dst, uint64_t x)
{
    memcpy(dst, &x, sizeof(x));
}

void poly_publickey_pack(uint8_t *pa, const int16_t *a)
{
    int i, out;

    out = 0;
    for(i = 0; i < 2040; i += 20)
    {
        uint64_t x0 = pack5_base769(a + i);
        uint64_t x1 = pack5_base769(a + i + 5);
        uint64_t x2 = pack5_base769(a + i + 10);
        uint64_t x3 = pack5_base769(a + i + 15);

        store64_le(pa + out, x0 | ((x3 & 0xFFFFULL) << 48));
        store64_le(pa + out + 8, x1 | (((x3 >> 16) & 0xFFFFULL) << 48));
        store64_le(pa + out + 16, x2 | (((x3 >> 32) & 0xFFFFULL) << 48));
        out += 24;
    }

    {
        uint64_t x0 = pack5_base769(a + 2040);
        uint64_t tail =
              (uint64_t)(uint16_t)a[ZEN_N - 3]
            + (uint64_t)(uint16_t)a[ZEN_N - 2] * pack_table[1]
            + (uint64_t)(uint16_t)a[ZEN_N - 1] * pack_table[2];

        uint64_t last = (tail >> 16) & 0x1FFFULL;

        store64_le(pa + out, x0 | ((tail & 0xFFFFULL) << 48));
        memcpy(pa + out + 8, &last, 2);
    }
}

static inline void unpack5_base769(int16_t *a, uint64_t x)
{
    const uint64_t div769_m = 374811932576999ULL;
    uint64_t q;

    q = (uint64_t)(((__uint128_t)x * div769_m) >> 58);
    a[0] = (int16_t)(x - q * 769ULL);
    x = q;
    q = (uint64_t)(((__uint128_t)x * div769_m) >> 58);
    a[1] = (int16_t)(x - q * 769ULL);
    x = q;
    q = (uint64_t)(((__uint128_t)x * div769_m) >> 58);
    a[2] = (int16_t)(x - q * 769ULL);
    x = q;
    q = (uint64_t)(((__uint128_t)x * div769_m) >> 58);
    a[3] = (int16_t)(x - q * 769ULL);
    x = q;
    q = (uint64_t)(((__uint128_t)x * div769_m) >> 58);
    a[4] = (int16_t)(x - q * 769ULL);
}

void poly_publickey_unpack(int16_t *a, const uint8_t *pa)
{
    int i, out;
    const uint64_t mask48 = 0x0000FFFFFFFFFFFFULL;
    const uint64_t mask13 = 0x1FFFULL;
    const uint64_t div769_m = 374811932576999ULL;
    uint64_t x, q, r306, r307;

    out = 0;
    for(i = 0; i < 306; i += 3)
    {
        uint64_t r0 = load64_le(pa + 8*i);
        uint64_t r1 = load64_le(pa + 8*i + 8);
        uint64_t r2 = load64_le(pa + 8*i + 16);
        uint64_t x0 = r0 & mask48;
        uint64_t x1 = r1 & mask48;
        uint64_t x2 = r2 & mask48;
        uint64_t x3 = (r0 >> 48) | ((r1 >> 48) << 16) | ((r2 >> 48) << 32);

        unpack5_base769(a + out, x0);
        unpack5_base769(a + out + 5, x1);
        unpack5_base769(a + out + 10, x2);
        unpack5_base769(a + out + 15, x3);
        out += 20;
    }

    r306 = load64_le(pa + 2448);
    r307 = 0;
    memcpy(&r307, pa + 2456, 2);
    unpack5_base769(a + 2040, r306 & mask48);

    x = ((r306 >> 48) & 0xFFFFULL) | ((r307 & mask13) << 16);
    for(i = 0; i < 3; i++)
    {
        q = (uint64_t)(((__uint128_t)x * div769_m) >> 58);
        a[ZEN_N - 3 + i] = (int16_t)(x - q * 769ULL);
        x = q;
    }
}

void poly_ciphertext_pack(uint8_t *pa, const int16_t *a)
{
    unsigned int i;
    const __m256i maskff = _mm256_set1_epi16(0x00FF);
    __m256i x0, x1, y;

    for(i = 0; i < ZEN_N; i += 32)
    {
        x0 = _mm256_load_si256((const __m256i *)(a + i +  0));
        x1 = _mm256_load_si256((const __m256i *)(a + i + 16));

        x0 = _mm256_and_si256(x0, maskff);
        x1 = _mm256_and_si256(x1, maskff);

        y = _mm256_packus_epi16(x0, x1);
        y = _mm256_permute4x64_epi64(y, 0xD8);

        /* The serialized ciphertext may be an unaligned message slice. */
        _mm256_storeu_si256((__m256i *)(pa + i), y);
    }
}

void poly_ciphertext_unpack(int16_t *a, const uint8_t *pa)
{
    unsigned int i;
    __m256i x, y0, y1;

    for(i = 0; i < ZEN_N; i += 32)
    {
        /* Only the internal coefficient array requires SIMD alignment. */
        x = _mm256_loadu_si256((const __m256i *)(pa + i));

        y0 = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(x));
        y1 = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(x, 1));

        _mm256_store_si256((__m256i *)(a + i +  0), y0);
        _mm256_store_si256((__m256i *)(a + i + 16), y1);
    }
}
void poly_compress(int16_t *a)
{
    unsigned int i;

    const __m256i add384 = _mm256_set1_epi32(384), mul10908 = _mm256_set1_epi32(10908),
                  mask255 = _mm256_set1_epi16(0x00FF);
    __m256i v, lo, hi, r;

    for(i = 0; i < ZEN_N; i += 16)
    {
        v = _mm256_load_si256((const __m256i *)(a + i));

        lo = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(v));
        hi = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(v, 1));

        lo = _mm256_slli_epi32(lo, 8);
        hi = _mm256_slli_epi32(hi, 8);

        lo = _mm256_add_epi32(lo, add384);
        hi = _mm256_add_epi32(hi, add384);

        lo = _mm256_mullo_epi32(lo, mul10908);
        hi = _mm256_mullo_epi32(hi, mul10908);

        lo = _mm256_srli_epi32(lo, 23);
        hi = _mm256_srli_epi32(hi, 23);

        r = _mm256_packus_epi32(lo, hi);
        r = _mm256_permute4x64_epi64(r, 0xD8);
        r = _mm256_and_si256(r, mask255);

        _mm256_store_si256((__m256i *)(a + i), r);
    }
}

void poly_decompress(int16_t *a)
{
    unsigned int i;

    const __m256i q769 = _mm256_set1_epi32(769), add128 = _mm256_set1_epi32(128);
    __m256i v, lo, hi, r;

    for(i = 0; i < ZEN_N; i += 16)
    {
        v = _mm256_load_si256((const __m256i *)(a + i));

        lo = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(v));
        hi = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(v, 1));

        lo = _mm256_mullo_epi32(lo, q769);
        hi = _mm256_mullo_epi32(hi, q769);

        lo = _mm256_add_epi32(lo, add128);
        hi = _mm256_add_epi32(hi, add128);

        lo = _mm256_srli_epi32(lo, 8);
        hi = _mm256_srli_epi32(hi, 8);

        r = _mm256_packus_epi32(lo, hi);
        r = _mm256_permute4x64_epi64(r, 0xD8);

        _mm256_store_si256((__m256i *)(a + i), r);
    }
}
