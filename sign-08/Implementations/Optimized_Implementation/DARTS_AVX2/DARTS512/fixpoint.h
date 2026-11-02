#ifndef FP__H
#define FP__H

#include "config.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef __SIZEOF_INT128__
__extension__ typedef __int128 int128;
__extension__ typedef unsigned __int128 uint128;
#endif

typedef struct {
    uint64_t limb48[2];
} fp96_76;
#define fixpoint_mul_high DARTS_NAMESPACE(fixpoint_mul_high)
#define fixpoint_square DARTS_NAMESPACE(fixpoint_square)
void fixpoint_square(fp96_76 *sqx, const fp96_76 *x);

#define fixpoint_newton_invsqrt DARTS_NAMESPACE(fixpoint_newton_invsqrt)
void fixpoint_newton_invsqrt(fp96_76 *invsqrtx, const fp96_76 *xhalf);

#define fixpoint_mul_rnd DARTS_NAMESPACE(fixpoint_mul_rnd)
int32_t fixpoint_mul_rnd(const uint64_t x, const fp96_76 *y,
                         const uint8_t sign);

#define fixpoint_add DARTS_NAMESPACE(fixpoint_add)
void fixpoint_add(fp96_76 *xy, const fp96_76 *x, const fp96_76 *y);

static inline void renormalize(fp96_76 *x) {
    x->limb48[1] += x->limb48[0] >> 48;
    x->limb48[0] &= (1ULL << 48) - 1;
}

static inline void mul64(uint64_t r[2], const uint64_t b, const uint64_t a) {
#ifndef __SIZEOF_INT128__
    const uint64_t al = (uint32_t)a;
    const uint64_t ah = a >> 32;
    const uint64_t bl = (uint32_t)b;
    const uint64_t bh = b >> 32;
    const uint64_t w0 = al * bl;
    const uint64_t t = ah * bl + (w0 >> 32);
    uint64_t w1 = (uint32_t)t;
    const uint64_t w2 = t >> 32;

    w1 += al * bh;
    r[0] = (w1 << 32) | (uint32_t)w0;
    r[1] = ah * bh + w2 + (w1 >> 32);
#else
    uint128 res = ((uint128)a * (uint128)b);
    r[0] = res;
    r[1] = res >> 64;
#endif
}

/* Return ceil(a*b/2^63), for signed a and 0 <= b < 2^63. */
static inline int64_t smulh63_ceil(const int64_t a, const uint64_t b) {
#ifdef __SIZEOF_INT128__
    const uint64_t sign = (uint64_t)a >> 63;
    const uint64_t mask = (uint64_t)0 - sign;
    const uint64_t magnitude = (((uint64_t)a) ^ mask) + sign;
    const uint128 product = (uint128)magnitude * (uint128)b;

    /* ceil(-p/2^63)=-floor(p/2^63); avoid shifting a negative value. */
    if (sign)
        return -(int64_t)(product >> 63);
    return (int64_t)((product + (((uint128)1 << 63) - 1)) >> 63);
#else
    const uint64_t sign = (uint64_t)a >> 63;
    const uint64_t mask = (uint64_t)0 - sign;
    const uint64_t magnitude = (((uint64_t)a) ^ mask) + sign;
    uint64_t product[2];
    uint64_t quotient;

    mul64(product, magnitude, b);
    quotient = (product[1] << 1) | (product[0] >> 63);
    if (sign)
        return -(int64_t)quotient;
    return (int64_t)(quotient + ((product[0] & INT64_MAX) != 0));
#endif
}

static inline void sq64(uint64_t r[2], const uint64_t a) {
    mul64(r, a, a);
}

static inline void mul48(uint64_t r[2], const uint64_t b, const uint64_t a) {
    mul64(r, b, a);
    r[1] <<= 16;
    r[1] |= r[0] >> 48;
    r[0] &= (1ULL << 48) - 1;
}

static inline void mulacc48(uint64_t r[2], const uint64_t b, const uint64_t a) {
    uint64_t tmp[2];
    mul48(tmp, b, a);
    r[0] += tmp[0];
    r[1] += tmp[1];
}

// (a0 + a1*2^32)^2 = a0^2 + 2^33*a0*a1 + 2^64*a1^2
static inline void sq48(uint64_t r[2], const uint64_t a) {
    sq64(r, a);
    r[1] <<= 16;
    r[1] |= r[0] >> 48;
    r[0] &= (1ULL << 48) - 1;
}

static inline void fixpoint_mul_high(fp96_76 *xy, const fp96_76 *x,
                                     const uint64_t y) {
    uint64_t tmp[2];
    mul48(&xy->limb48[0], x->limb48[0], y); // implicitly shifted right by 48

    mul48(tmp, x->limb48[1], y);
    xy->limb48[1] += tmp[0];

    // shift right by 28, rounding
    xy->limb48[0] += 1UL << 27;
    xy->limb48[0] >>= 28;
    xy->limb48[0] += (xy->limb48[1] << 20) & ((1ULL << 48) - 1);
    xy->limb48[1] >>= 28;

    xy->limb48[1] += tmp[1] << 20;

    renormalize(xy);
}

#endif
