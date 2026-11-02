#include "sampler.h"
#include "fixpoint.h"
#include "gauss_q80_constants.h"
#include "symmetric.h"
#include <stdint.h>

/*************************************************
 * Name:        rej_uniform
 *
 * Description: Sample uniformly random coefficients in [0, Q-1] by
 *              performing rejection sampling on array of random bytes.
 *
 * Arguments:   - int32_t *a: pointer to output array (allocated)
 *              - unsigned int len: number of coefficients to be sampled
 *              - const uint8_t *buf: array of random bytes
 *              - unsigned int buflen: length of array of random bytes
 *
 * Returns number of sampled coefficients. Can be smaller than len 
 * if not enough random bytes were given.
 **************************************************/
unsigned int rej_uniform(int32_t *a, unsigned int len,
                         const uint8_t *buf, unsigned int buflen)
{
    unsigned int ctr = 0, pos = 0;

#if Q_BITS == 18
    while (ctr < len && pos + 9 <= buflen) {
        uint32_t t0 =  (uint32_t)buf[pos]
                      | ((uint32_t)buf[pos+1] << 8)
                      | (((uint32_t)buf[pos+2] & 0x03) << 16);

        uint32_t t1 =  ((uint32_t)buf[pos+2] >> 2)
                      | ((uint32_t)buf[pos+3] << 6)
                      | (((uint32_t)buf[pos+4] & 0x0F) << 14);

        uint32_t t2 =  ((uint32_t)buf[pos+4] >> 4)
                      | ((uint32_t)buf[pos+5] << 4)
                      | (((uint32_t)buf[pos+6] & 0x3F) << 12);

        uint32_t t3 =  ((uint32_t)buf[pos+6] >> 6)
                      | ((uint32_t)buf[pos+7] << 2)
                      | ((uint32_t)buf[pos+8] << 10);

        pos += 9;

        if (t0 < Q && ctr < len) a[ctr++] = (int32_t)t0;
        if (t1 < Q && ctr < len) a[ctr++] = (int32_t)t1;
        if (t2 < Q && ctr < len) a[ctr++] = (int32_t)t2;
        if (t3 < Q && ctr < len) a[ctr++] = (int32_t)t3;
    }

#elif Q_BITS == 17
    while (ctr < len && pos + 17 <= buflen) {
        const uint8_t *b = buf + pos;
        uint32_t t[8];

        t[0] =  (uint32_t)b[0]        | ((uint32_t)b[1]  << 8) | (((uint32_t)b[2]  & 0x01) << 16);
        t[1] = ((uint32_t)b[2]  >> 1) | ((uint32_t)b[3]  << 7) | (((uint32_t)b[4]  & 0x03) << 15);
        t[2] = ((uint32_t)b[4]  >> 2) | ((uint32_t)b[5]  << 6) | (((uint32_t)b[6]  & 0x07) << 14);
        t[3] = ((uint32_t)b[6]  >> 3) | ((uint32_t)b[7]  << 5) | (((uint32_t)b[8]  & 0x0F) << 13);
        t[4] = ((uint32_t)b[8]  >> 4) | ((uint32_t)b[9]  << 4) | (((uint32_t)b[10] & 0x1F) << 12);
        t[5] = ((uint32_t)b[10] >> 5) | ((uint32_t)b[11] << 3) | (((uint32_t)b[12] & 0x3F) << 11);
        t[6] = ((uint32_t)b[12] >> 6) | ((uint32_t)b[13] << 2) | (((uint32_t)b[14] & 0x7F) << 10);
        t[7] = ((uint32_t)b[14] >> 7) | ((uint32_t)b[15] << 1) | ((uint32_t)b[16]  << 9);

        pos += 17;

        for (unsigned int i = 0; i < 8 && ctr < len; i++) {
            if (t[i] < Q)
                a[ctr++] = (int32_t)t[i];
        }
    }

#else
    #error "Q_BITS must be 17 or 18"
#endif
    {
        uint32_t acc = 0;
        unsigned int acc_bits = 0;
        const uint32_t mask = (1u << Q_BITS) - 1u;

        while (ctr < len) {
            while (acc_bits < Q_BITS && pos < buflen) {
                acc |= ((uint32_t)buf[pos++]) << acc_bits;
                acc_bits += 8;
            }
            if (acc_bits < Q_BITS) break;

            uint32_t t = acc & mask;
            acc >>= Q_BITS;
            acc_bits -= Q_BITS;

            if (t < Q)
                a[ctr++] = (int32_t)t;
        }
    }

    return ctr;
}

/*************************************************
 * Name:        rej_p
 *
 * Description: Sample coefficients in {-1,0,1} with
 *              Pr[X = 1] = Pr[X = -1] = P, Pr[X = 0] = 1 - 2P
 *              by performing rejection sampling on array of random bytes.
 *
 * Arguments:   - int32_t *a: pointer to output array (allocated)
 *              - unsigned int len: number of coefficients to be sampled
 *              - const uint8_t *buf: array of random bytes
 *              - unsigned int buflen: length of array of random bytes
 *
 * Returns number of sampled coefficients. Can be smaller than len 
 * if not enough random bytes were given.
 **************************************************/
static inline int32_t floor10(uint16_t t) {
    return (int32_t)((t * 205u) >> 11);
}

static inline int32_t mod10(uint16_t t) {
    int32_t r;
    r = (t & 31) + ((t >> 5) << 1);
    r = (r & 31) + ((r >> 5) << 1);
    r = (r & 15) + ((r >> 4) * 6);
    r -= 10 * (r >= 10);
    return r - 10 * (r >= 10);
}

unsigned int rej_p(int32_t *a, unsigned int len,
                   const uint8_t *buf, unsigned int buflen)
{
    unsigned int ctr = 0, pos = 0, acc_bits = 0;
    uint32_t acc = 0;

    if (P == 0.15 || P == 0.2)
    {
        const int32_t thresh = (P == 0.15) ? 3 : 4;

        while (ctr < len) {
            while (acc_bits < 13 && pos < buflen) {
                acc |= ((uint32_t)buf[pos++]) << acc_bits;
                acc_bits += 8;
            }
            if (acc_bits < 13) break;

            uint16_t t = acc & 0x3FFu;
            acc >>= 10;
            acc_bits -= 10;

            if (t >= 1000u) continue;

            int32_t nz, s;

            nz = (int32_t)(mod10(t) < thresh);
            s  = (int32_t)(acc & 1u) * 2 - 1;
            a[ctr++] = nz * s;
            if (ctr >= len) break;
            acc >>= (nz & 1);
            acc_bits -= (nz & 1);
            t = floor10(t);

            nz = (int32_t)(mod10(t) < thresh);
            s  = (int32_t)(acc & 1u) * 2 - 1;
            a[ctr++] = nz * s;
            if (ctr >= len) break;
            acc >>= (nz & 1);
            acc_bits -= (nz & 1);
            t = floor10(t);

            nz = (int32_t)(t < thresh);
            s  = (int32_t)(acc & 1u) * 2 - 1;
            a[ctr++] = nz * s;
            if (ctr >= len) break;
            acc >>= (nz & 1);
            acc_bits -= (nz & 1);
        }
    }
    else if (P == 0.3125)
    {
        while (ctr < len) {
            while (acc_bits < 15 && pos < buflen) {
                acc |= ((uint32_t)buf[pos++]) << acc_bits;
                acc_bits += 8;
            }
            if (acc_bits < 15) break;

            uint32_t t = acc & 0xFFFu;
            acc >>= 12;
            acc_bits -= 12;

            int32_t nz, s;
            uint32_t d;

            d  = t & 0xFu;
            nz = (int32_t)(d < 10u);
            s  = (int32_t)(acc & 1u) * 2 - 1;
            a[ctr++] = nz * s;
            if (ctr >= len) break;
            acc >>= (nz & 1);
            acc_bits -= (nz & 1);

            d  = (t >> 4) & 0xFu;
            nz = (int32_t)(d < 10u);
            s  = (int32_t)(acc & 1u) * 2 - 1;
            a[ctr++] = nz * s;
            if (ctr >= len) break;
            acc >>= (nz & 1);
            acc_bits -= (nz & 1);

            d  = (t >> 8) & 0xFu;
            nz = (int32_t)(d < 10u);
            s  = (int32_t)(acc & 1u) * 2 - 1;
            a[ctr++] = nz * s;
            if (ctr >= len) break;
            acc >>= (nz & 1);
            acc_bits -= (nz & 1);
        }
    }

    return ctr;
}

/* Q63 one-sided degree-12 approximation of exp(-x). */
static uint64_t approx_exp(const uint64_t x) {
    int64_t result = EXP_C12;
    uint64_t threshold;

    result = smulh63_ceil(result, x) + EXP_C11;
    result = smulh63_ceil(result, x) + EXP_C10;
    result = smulh63_ceil(result, x) + EXP_C9;
    result = smulh63_ceil(result, x) + EXP_C8;
    result = smulh63_ceil(result, x) + EXP_C7;
    result = smulh63_ceil(result, x) + EXP_C6;
    result = smulh63_ceil(result, x) + EXP_C5;
    result = smulh63_ceil(result, x) + EXP_C4;
    result = smulh63_ceil(result, x) + EXP_C3;
    result = smulh63_ceil(result, x) + EXP_C2;
    result = smulh63_ceil(result, x) + EXP_C1;
    result = smulh63_ceil(result, x);

    /* Unsigned addition gives EXP_C0 + result even when result is negative. */
    threshold = EXP_C0 + (uint64_t)result;
    if (threshold > (UINT64_C(1) << EXP_FRAC_BITS))
        threshold = UINT64_C(1) << EXP_FRAC_BITS;
    return threshold;
}

/* Constant-time inverse-CDF sampling from the generated 112-bit CDT. */
static uint64_t sample_gauss112(const uint64_t rand_lo,
                                const uint64_t rand_hi) {
    unsigned int i;
    uint64_t r = 0;

    for (i = 0; i < CDTLEN; i++) {
        const uint64_t borrow = rand_lo > CDT112_LO[i];
        const uint64_t hi_diff = CDT112_HI[i] - rand_hi - borrow;
        r += hi_diff >> 63;
    }
    return r;
}

static uint64_t load64_le(const uint8_t src[8]) {
    return (uint64_t)src[0] | ((uint64_t)src[1] << 8) |
           ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) |
           ((uint64_t)src[6] << 48) | ((uint64_t)src[7] << 56);
}

#define GAUSS_RAND (72 + CDT_BITS + 64)
#define GAUSS_RAND_BYTES ((GAUSS_RAND + 7) / 8) /* 31 */
static int sample_gauss_sigma76(uint64_t *r, fp96_76 *sqr,
                                const uint8_t rand[GAUSS_RAND_BYTES]) {
    const uint64_t rand_gauss_lo = load64_le(rand);
    const uint64_t rand_gauss_hi = load64_le(rand + 8) &
                                   ((UINT64_C(1) << 48) - 1);
    const uint64_t rejection_word = load64_le(rand + 14);
    const uint64_t rand_rej = rejection_word & INT64_MAX;
    const uint64_t zero_coin = rejection_word >> 63;
    uint64_t x, exp_in, exp_threshold, diff_hi;
    fp96_76 y;

    // sample x
    x = sample_gauss112(rand_gauss_lo, rand_gauss_hi);

    // y := append x to y
    // leave 16 bit for carries
    y.limb48[0] = load64_le(rand + 22) & ((UINT64_C(1) << 48) - 1);
    y.limb48[1] = rand[28] | ((uint64_t)rand[29] << 8) |
                  ((uint64_t)rand[30] << 16) | (x << 24);

    /* Round y without losing the high bit when x >= 128. */
    *r = ((y.limb48[0] >> 15) + 1) >> 1;
    *r += y.limb48[1] << 32;

    // sqr := y*y
    fixpoint_square(sqr, &y);

    /* If D=(sqr-x^2*2^68), Q63 exp_in is round(D/2^14). */
    diff_hi = sqr->limb48[1] - ((x * x) << 20);
    exp_in = (diff_hi << 34) | (sqr->limb48[0] >> 14);
    exp_in += (sqr->limb48[0] >> 13) & 1;

    exp_threshold = approx_exp(exp_in);
    return (int)(((rand_rej - exp_threshold) >> 63) &
                 (((*r | (UINT64_C(0) - *r)) >> 63) | zero_coin));
}

int sample_gauss(uint64_t *r, fp96_76 *sqsum, const uint8_t *buf, const size_t buflen, const size_t len, const int dont_write_last)
{
    const uint8_t *pos = buf;
    fp96_76 sqr;
    size_t bytecnt = buflen, coefcnt = 0;
    int accepted;
    uint64_t dummy;
    
    while (coefcnt < len) {
        if (bytecnt < GAUSS_RAND_BYTES) {
          renormalize(sqsum);
          return coefcnt;
        }

        if (dont_write_last && coefcnt == len-1)
        {
          accepted = sample_gauss_sigma76(&dummy, &sqr, pos);
        } else {
          accepted = sample_gauss_sigma76(&r[coefcnt], &sqr, pos);
        }
        coefcnt += accepted;
        pos += GAUSS_RAND_BYTES;
        bytecnt -= GAUSS_RAND_BYTES;

        sqsum->limb48[0] += sqr.limb48[0] & -(int64_t)accepted;
        sqsum->limb48[1] += sqr.limb48[1] & -(int64_t)accepted;
    }

    renormalize(sqsum);
    return len;
}

#define POLY_HYPERBALL_BUFLEN (GAUSS_RAND_BYTES * N)
#define POLY_HYPERBALL_NBLOCKS ((POLY_HYPERBALL_BUFLEN + SM3_XOF_BLOCKBYTES - 1) / SM3_XOF_BLOCKBYTES)

/*************************************************
 * Name:        sample_gauss_N
 *
 * Description: Sample len coefficients from discrete Gaussian with
 *              standard deviation sigma=76/2^13
 *              using output of SM3-XOF(seed|nonce).
 *              Also compute the squared norm of the sampled coefficients.
 *
 * Arguments:   - uint64_t *r: pointer to output array of sampled coefficients
 *              - uint8_t *signs: pointer to output array of sign bits
 *              - fp96_76 *sqsum: pointer to output squared norm
 *              - const uint8_t seed[CRHBYTES]: byte array with seed of length CRHBYTES
 *              - const uint16_t nonce: 2-byte nonce
 *              - const size_t len: number of coefficients to be sampled
 **************************************************/
void sample_gauss_N(uint64_t *r, uint8_t *signs, fp96_76 *sqsum,
                    const uint8_t seed[CRHBYTES], const uint16_t nonce,
                    const size_t len) {
    uint8_t buf[POLY_HYPERBALL_NBLOCKS * SM3_XOF_BLOCKBYTES];
    size_t bytecnt, coefcnt, firstflag = 1;
    sm3_xof_state state;
    sm3_xof_stream_init(&state, seed, CRHBYTES, nonce);

    sm3_xof_squeezeblocks(buf, POLY_HYPERBALL_NBLOCKS, &state);
    for (size_t i = 0; i < len / 8; i++) {
        signs[i] = buf[i];
    }
    bytecnt = POLY_HYPERBALL_NBLOCKS * SM3_XOF_BLOCKBYTES - len / 8;
    coefcnt = sample_gauss(r, sqsum, buf + len / 8, bytecnt, len, len%N);
    while (coefcnt < len) {
        size_t off = bytecnt % GAUSS_RAND_BYTES;
        for (size_t i = 0; i < off; i++) {
            buf[i] = buf[bytecnt + len/8*firstflag - off + i];
        }
        sm3_xof_squeezeblocks(buf + off, 1, &state);
        bytecnt = SM3_XOF_BLOCKBYTES + off;

        coefcnt += sample_gauss(r + coefcnt, sqsum, buf, bytecnt, len - coefcnt, len%N);
        firstflag = 0;
    }
}
