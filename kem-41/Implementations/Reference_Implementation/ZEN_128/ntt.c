/*
Copyright (c) 2026 Yu Zhang.
Organization: State Key Laboratory of Cyberspace Security Defense,Institute of Information Engineering, CAS
              School of Cyber Security, University of Chinese Academy of Sciences  
File Description: Declares the ZEN key-encapsulation mechanism layer for the optimized ZEN-128 instance.
*/
#include <stdint.h>
#include <stdio.h>
#include "params.h"
#include "ntt.h"

/// @brief Multiply two coefficients in the finite field modulo ZEN_Q using Montgomery reduction
/// @param[in] a First input coefficient
/// @param[in] b Second input coefficient
/// @return Product of a and b reduced modulo ZEN_Q
static inline int16_t fqmul(int16_t a, int16_t b) 
{
  return montgomery_reduce((int32_t)a * b);
}

void poly_ntt(int16_t *a) 
{
    unsigned int len, start, j, k;
    int16_t t0, t1, t2, t3, zeta;

    k = 1;
    for(len = ZEN_N >> 1; len >= 4; len >>= 1) 
    {
        for(start = 0; start < ZEN_N; start = j + len) 
        {
            zeta = f[k++];
            for(j = start; j < start + len; j += 4)
            {
                t0 = fqmul(zeta, a[j + len]);
                t1 = fqmul(zeta, a[j + len + 1]);
                t2 = fqmul(zeta, a[j + len + 2]);
                t3 = fqmul(zeta, a[j + len + 3]);
                a[j + len] = a[j] - t0;
                a[j + len + 1] = a[j + 1] - t1;
                a[j + len + 2] = a[j + 2] - t2;
                a[j + len + 3] = a[j + 3] - t3;
                a[j] += t0;
                a[j + 1] += t1;
                a[j + 2] += t2;
                a[j + 3] += t3;
            }
        }
    }
}

void poly_ntt_mq(int16_t *a) 
{
    unsigned int len, start, j, k;
    int16_t t0, t1, t2, t3, zeta;

    k = 1;
    for(len = ZEN_N >> 1; len >= 4; len >>= 1) 
    {
        for(start = 0; start < ZEN_N; start = j + len) 
        {
            zeta = f[k++];
            for(j = start; j < start + len; j += 4)
            {
                t0 = fqmul(zeta, a[j + len]);
                t1 = fqmul(zeta, a[j + len + 1]);
                t2 = fqmul(zeta, a[j + len + 2]);
                t3 = fqmul(zeta, a[j + len + 3]);
                a[j + len] = a[j] - t0;
                a[j + len + 1] = a[j + 1] - t1;
                a[j + len + 2] = a[j + 2] - t2;
                a[j + len + 3] = a[j + 3] - t3;
                a[j] += t0;
                a[j + 1] += t1;
                a[j + 2] += t2;
                a[j + 3] += t3;
            }
        }
    }

    for(j = 0; j < ZEN_N; j += 4)
    {
        t0 = fqmul(a[j], 171);
        t1 = fqmul(a[j + 1], 171);
        t2 = fqmul(a[j + 2], 171);
        t3 = fqmul(a[j + 3], 171);
        a[j] = t0 + ((t0 >> 15) & ZEN_Q);
        a[j + 1] = t1 + ((t1 >> 15) & ZEN_Q);
        a[j + 2] = t2 + ((t2 >> 15) & ZEN_Q);
        a[j + 3] = t3 + ((t3 >> 15) & ZEN_Q);
    }
}

void poly_intt(int16_t *a) 
{
    unsigned int start, len, j, k;
    int16_t t, u, zeta, scale;

    k = 0;
    for(len = 4; len < (ZEN_N >> 1); len <<= 1)
    {
        for(start = 0; start < ZEN_N; start =j + len)
        {
            zeta = fn[k++];
            for(j = start; j < start + len; j++) 
            {
                t = a[j];
                a[j] = (t + a[j + len]);
                a[j + len] = t - a[j + len];
                a[j + len] = fqmul(zeta, a[j + len]);
            }
        }
    }

    /* Fuse the final inverse layer with the global scaling.  The upper
     * twiddle already contains one Montgomery reduction, so combine it
     * with the scale once rather than scaling every upper coefficient. */
    len = ZEN_N >> 1;
    scale = fn[127];
    zeta = fqmul(fn[k], scale);
    for(j = 0; j < len; j++)
    {
        t = a[j];
        u = a[j + len];
        a[j] = fqmul(t + u, scale);
        a[j + len] = fqmul(t - u, zeta);
    }
}

/// @brief Multiply two degree-3 polynomial blocks in the NTT domain with a given twiddle factor
/// @param[out] r Base address of output coefficient array of length 4
/// @param[in] a Base address of first input coefficient array of length 4
/// @param[in] b Base address of second input coefficient array of length 4
/// @param[in] zeta Twiddle factor used in the block multiplication
/// @return None
static inline void base_mul(int16_t *restrict r, const int16_t *restrict a,
                            const int16_t *restrict b, int16_t zeta)
{
    int16_t c0,  c1,  c2,  c3;
    c0 = fqmul(a[0], b[0]);
    c1 = fqmul(a[1], b[1]);
    c2 = fqmul(a[2], b[2]);
    c3 = fqmul(a[3], b[3]);

    r[0] = fqmul((a[1] + a[3]), (b[1] + b[3]));
    r[0] -= c1;
    r[0] -= c3;
    r[0] += c2;
    r[0] = fqmul(r[0], zeta);
    r[0] += c0;

    r[1] = fqmul((a[2] + a[3]), (b[2] + b[3]));
    r[1] -= c2;
    r[1] -= c3;
    r[1] = fqmul(r[1], zeta);
    r[1] += fqmul((a[0] + a[1]), (b[0] + b[1]));
    r[1] -= c0;
    r[1] -= c1;

    r[2] = fqmul(c3, zeta);
    r[2] += c1;
    r[2] += fqmul((a[0] + a[2]), (b[0] + b[2]));
    r[2] -= c0;
    r[2] -= c2;

    r[3] = fqmul((a[0] + a[3]), (b[0] + b[3]));
    r[3] -= c0;
    r[3] -= c3;
    r[3] += fqmul((a[1] + a[2]), (b[1] + b[2]));
    r[3] -= c1;
    r[3] -= c2;
}

void poly_basemul_ntt(int16_t *restrict r, const int16_t *restrict a,
                      const int16_t *restrict b)
{
    unsigned int i;
    for(i = 0; i < ZEN_N / 8; i++) 
    {
        base_mul(r + 8 * i, a + 8 * i, b + 8 * i, f[64 + i]);
        base_mul(r + 8 * i + 4, a + 8 * i + 4, b + 8 * i + 4, -f[64 + i]);
    }
}

/// @brief Multiply two degree-3 polynomial blocks in the NTT domain with a given twiddle factor and reduce coefficients modulo ZEN_Q
/// @param[out] r Base address of output coefficient array of length 4
/// @param[in] a Base address of first input coefficient array of length 4
/// @param[in] b Base address of second input coefficient array of length 4
/// @param[in] zeta Twiddle factor used in the block multiplication
/// @return None
static inline void base_mul_mq(int16_t *restrict r, const int16_t *restrict a,
                               const int16_t *restrict b, int16_t zeta)
{
    int16_t c0,  c1,  c2,  c3;
    c0 = fqmul(a[0], b[0]);
    c1 = fqmul(a[1], b[1]);
    c2 = fqmul(a[2], b[2]);
    c3 = fqmul(a[3], b[3]);

    r[0] = fqmul((a[1] + a[3]), (b[1] + b[3]));
    r[0] -= c1;
    r[0] -= c3;
    r[0] += c2;
    r[0] = fqmul(r[0], zeta);
    r[0] += c0;
    r[0] = fqmul(r[0], 19);
    r[0] += (r[0] >> 15) & ZEN_Q;

    r[1] = fqmul((a[2] + a[3]), (b[2] + b[3]));
    r[1] -= c2;
    r[1] -= c3;
    r[1] = fqmul(r[1], zeta);
    r[1] += fqmul((a[0] + a[1]), (b[0] + b[1]));
    r[1] -= c0;
    r[1] -= c1;
    r[1] = fqmul(r[1], 19);
    r[1] += (r[1] >> 15) & ZEN_Q;

    r[2] = fqmul(c3, zeta);
    r[2] += c1;
    r[2] += fqmul((a[0] + a[2]), (b[0] + b[2]));
    r[2] -= c0;
    r[2] -= c2;
    r[2] = fqmul(r[2], 19);
    r[2] += (r[2] >> 15) & ZEN_Q;

    r[3] = fqmul((a[0] + a[3]), (b[0] + b[3]));
    r[3] -= c0;
    r[3] -= c3;
    r[3] += fqmul((a[1] + a[2]), (b[1] + b[2]));
    r[3] -= c1;
    r[3] -= c2;
    r[3] = fqmul(r[3], 19);
    r[3] += (r[3] >> 15) & ZEN_Q;
}

void poly_basemul_ntt_mq(int16_t *restrict r, const int16_t *restrict a,
                         const int16_t *restrict b)
{
    unsigned int i;
    for(i = 0; i < ZEN_N / 8; i++) 
    {
        base_mul_mq(r + 8 * i, a + 8 * i, b + 8 * i, f[64 + i]);
        base_mul_mq(r + 8 * i + 4, a + 8 * i + 4, b + 8 * i + 4, -f[64 + i]);
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

/* Fixed addition chain for q-2 = 767: 13 Montgomery products.
 * Interpreting a as a Montgomery value gives R^2/a; the last product
 * removes R^2 (R = 171, R^-1 = 9 mod 769).  Zero maps to zero. */
static inline int16_t inv_scalar(int16_t a)
{
    int16_t a2 = fqmul(a, a);
    int16_t a3 = fqmul(a2, a);
    int16_t a5 = fqmul(a3, a2);
    int16_t t = fqmul(a5, a5);  /* a^10 */
    int16_t a15 = fqmul(t, a5);
    t = fqmul(a15, a15);     /* a^30 */
    t = fqmul(t, t);         /* a^60 */
    t = fqmul(t, t);         /* a^120 */
    t = fqmul(t, t);         /* a^240 */
    t = fqmul(t, a15);       /* a^255 */
    a3 = fqmul(t, t);        /* a^510 */
    t = fqmul(a3, t);        /* a^765 */
    t = fqmul(t, a2);        /* a^767 */
    return fqmul(t, 9);
}

/* Hierarchical Montgomery trick, 128 denominators, eight independent
 * length-16 chains and a binary tree over their eight products.
 * Exactly 3*(128-1) products and one scalar inversion, excluding masks.
 * Zero denominators are replaced by R and masked back to zero so one
 * noninvertible component cannot corrupt the other components.
 * All indices and loop bounds depend only on the public batch size. */
static void inv_batch(int16_t d[128])
{
    int16_t p[128], leaf[128], tree[16];
    uint16_t zero[128];
    unsigned int i, j;
    for(i = 0; i < 128; i++)
    {
        int16_t x = d[i] + ((d[i] >> 15) & ZEN_Q);
        zero[i] = (uint16_t)(((uint32_t)x - 1u) >> 16);
        leaf[i] = (int16_t)(((uint16_t)x & ~zero[i]) | (MONT & zero[i]));
    }
    for(j = 0; j < 8; j++)
        p[j] = leaf[j];
    for(i = 8; i < 128; i += 8)
    {
        p[i + 0] = fqmul(p[i + 0 - 8], leaf[i + 0]);
        p[i + 1] = fqmul(p[i + 1 - 8], leaf[i + 1]);
        p[i + 2] = fqmul(p[i + 2 - 8], leaf[i + 2]);
        p[i + 3] = fqmul(p[i + 3 - 8], leaf[i + 3]);
        p[i + 4] = fqmul(p[i + 4 - 8], leaf[i + 4]);
        p[i + 5] = fqmul(p[i + 5 - 8], leaf[i + 5]);
        p[i + 6] = fqmul(p[i + 6 - 8], leaf[i + 6]);
        p[i + 7] = fqmul(p[i + 7 - 8], leaf[i + 7]);
    }
    for(j = 0; j < 8; j++)
        tree[8 + j] = p[120 + j];
    for(i = 7; i > 0; i--)
        tree[i] = fqmul(tree[2 * i], tree[2 * i + 1]);
    tree[1] = inv_scalar(tree[1]);
    for(i = 1; i < 8; i++)
    {
        int16_t left = tree[2 * i];
        tree[2 * i] = fqmul(tree[i], tree[2 * i + 1]);
        tree[2 * i + 1] = fqmul(tree[i], left);
    }
    for(i = 120; i > 0; i -= 8)
    {
        d[i + 0] = (int16_t)((uint16_t)fqmul(p[i + 0 - 8], tree[8]) & ~zero[i + 0]);
        tree[8] = fqmul(tree[8], leaf[i + 0]);
        d[i + 1] = (int16_t)((uint16_t)fqmul(p[i + 1 - 8], tree[9]) & ~zero[i + 1]);
        tree[9] = fqmul(tree[9], leaf[i + 1]);
        d[i + 2] = (int16_t)((uint16_t)fqmul(p[i + 2 - 8], tree[10]) & ~zero[i + 2]);
        tree[10] = fqmul(tree[10], leaf[i + 2]);
        d[i + 3] = (int16_t)((uint16_t)fqmul(p[i + 3 - 8], tree[11]) & ~zero[i + 3]);
        tree[11] = fqmul(tree[11], leaf[i + 3]);
        d[i + 4] = (int16_t)((uint16_t)fqmul(p[i + 4 - 8], tree[12]) & ~zero[i + 4]);
        tree[12] = fqmul(tree[12], leaf[i + 4]);
        d[i + 5] = (int16_t)((uint16_t)fqmul(p[i + 5 - 8], tree[13]) & ~zero[i + 5]);
        tree[13] = fqmul(tree[13], leaf[i + 5]);
        d[i + 6] = (int16_t)((uint16_t)fqmul(p[i + 6 - 8], tree[14]) & ~zero[i + 6]);
        tree[14] = fqmul(tree[14], leaf[i + 6]);
        d[i + 7] = (int16_t)((uint16_t)fqmul(p[i + 7 - 8], tree[15]) & ~zero[i + 7]);
        tree[15] = fqmul(tree[15], leaf[i + 7]);
    }
    for(j = 0; j < 8; j++)
        d[j] = (int16_t)((uint16_t)tree[8 + j] & ~zero[j]);
}

/* Accumulate integer products before reducing.  With |a_i|<=768,
 * even the degree-16 expressions stay below 2^24 in magnitude.
 * Each resulting norm/lift coefficient has magnitude <769, so the
 * same bound applies at every level.  This saves reductions without
 * changing the multiplication representation. */
static inline void inv_norm2(int16_t *r, const int16_t *a, int16_t zeta)
{
    r[0] = montgomery_reduce(((int32_t)a[0] * a[0]) - (0)
        + (int32_t)zeta * montgomery_reduce((0) - ((int32_t)a[1] * a[1])));
}

static inline void inv_norm4(int16_t *r, const int16_t *a, int16_t zeta)
{
    r[0] = montgomery_reduce(((int32_t)a[0] * a[0]) - (0)
        + (int32_t)zeta * montgomery_reduce(((int32_t)a[2] * a[2]) - (2 * (int32_t)a[1] * a[3])));
    r[1] = montgomery_reduce((2 * (int32_t)a[0] * a[2]) - ((int32_t)a[1] * a[1])
        + (int32_t)zeta * montgomery_reduce((0) - ((int32_t)a[3] * a[3])));
}

static inline void inv_lift4(int16_t *r, const int16_t *a,
                              const int16_t *b, int16_t zeta)
{
    r[0] = montgomery_reduce((int32_t)b[0] * a[0]
        + (int32_t)zeta * montgomery_reduce((int32_t)b[1] * a[2]));
    r[1] = -montgomery_reduce((int32_t)b[0] * a[1]
        + (int32_t)zeta * montgomery_reduce((int32_t)b[1] * a[3]));
    r[2] = montgomery_reduce((int32_t)b[0] * a[2] + (int32_t)b[1] * a[0]);
    r[3] = -montgomery_reduce((int32_t)b[0] * a[3] + (int32_t)b[1] * a[1]);
}

void poly_baseinv_ntt(int16_t *restrict r, const int16_t *restrict a)
{
    unsigned int i;
    int16_t den[128], n2[128][2];
    int16_t t2[2];

    for(i = 0; i < 128; i++)
    {
        int16_t zeta = (i & 1) ? -f[64 + i / 2] : f[64 + i / 2];
        inv_norm4(n2[i], a + 4 * i, zeta);
        inv_norm2(den + i, n2[i], zeta);
    }
    inv_batch(den);
    for(i = 0; i < 128; i++)
    {
        int16_t zeta = (i & 1) ? -f[64 + i / 2] : f[64 + i / 2];
        t2[0] = fqmul(n2[i][0], den[i]);
        t2[1] = -fqmul(n2[i][1], den[i]);
        inv_lift4(r + 4 * i, a + 4 * i, t2, zeta);
    }
}
