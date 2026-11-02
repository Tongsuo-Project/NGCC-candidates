#ifndef GREATWALL160_POLYNOMIALS_H
#define GREATWALL160_POLYNOMIALS_H

#include "block.h"
#define POLY_VEC_LEN (1 << POLY_VEC_LEN_SHIFT)
extern const uint32_t gf64_modulus;
extern const uint32_t gf160_modulus;
#define GF160_REDUCTION_LOW UINT32_C(0x2d)
#include "polynomials_impl.h"

typedef poly160_vec poly_secpar_vec;
typedef poly224_vec poly_secpar_plus_64_vec;
typedef poly320_vec poly_2secpar_vec;
inline poly_secpar_vec poly_secpar_load(const void* p) { return poly160_load(p); }
inline poly_secpar_vec poly_secpar_load_dup(const void* p) { return poly160_load_dup(p); }
inline void poly_secpar_store(void* p, poly_secpar_vec x) { poly160_store(p,x); }
inline void poly_secpar_store1(void* p, poly_secpar_vec x) { poly160_store1(p,x); }
inline poly_secpar_vec poly_secpar_set_zero(void) { return poly160_set_zero(); }
inline poly_secpar_vec poly_secpar_set_low32(uint32_t x) { return poly160_set_low32(x); }
inline poly_secpar_vec poly_secpar_add(poly_secpar_vec x, poly_secpar_vec y) { return poly160_add(x,y); }
inline bool poly_secpar_eq(poly_secpar_vec x, poly_secpar_vec y) { return poly160_eq(x,y); }
inline poly_secpar_vec poly_secpar_extract(poly_secpar_vec x, size_t i) { return poly160_extract(x,i); }
inline poly_secpar_plus_64_vec poly_secpar_plus_64_load(const void* p) { return poly224_load(p); }
inline poly_secpar_plus_64_vec poly_secpar_plus_64_load_dup(const void* p) { return poly224_load_dup(p); }
inline void poly_secpar_plus_64_store(void* p, poly_secpar_plus_64_vec x) { poly224_store(p,x); }
inline void poly_secpar_plus_64_store1(void* p, poly_secpar_plus_64_vec x) { poly224_store1(p,x); }
inline poly_secpar_plus_64_vec poly_secpar_plus_64_set_zero(void) { return poly224_set_zero(); }
inline poly_secpar_plus_64_vec poly_secpar_plus_64_set_low32(uint32_t x) { return poly224_set_low32(x); }
inline poly_secpar_plus_64_vec poly_secpar_plus_64_add(poly_secpar_plus_64_vec x, poly_secpar_plus_64_vec y) { return poly224_add(x,y); }
inline bool poly_secpar_plus_64_eq(poly_secpar_plus_64_vec x, poly_secpar_plus_64_vec y) { return poly224_eq(x,y); }
inline poly_secpar_plus_64_vec poly_secpar_plus_64_extract(poly_secpar_plus_64_vec x, size_t i) { return poly224_extract(x,i); }
inline poly_2secpar_vec poly_2secpar_load(const void* p) { return poly320_load(p); }
inline poly_2secpar_vec poly_2secpar_load_dup(const void* p) { return poly320_load_dup(p); }
inline void poly_2secpar_store(void* p, poly_2secpar_vec x) { poly320_store(p,x); }
inline void poly_2secpar_store1(void* p, poly_2secpar_vec x) { poly320_store1(p,x); }
inline poly_2secpar_vec poly_2secpar_set_zero(void) { return poly320_set_zero(); }
inline poly_2secpar_vec poly_2secpar_set_low32(uint32_t x) { return poly320_set_low32(x); }
inline poly_2secpar_vec poly_2secpar_add(poly_2secpar_vec x, poly_2secpar_vec y) { return poly320_add(x,y); }
inline bool poly_2secpar_eq(poly_2secpar_vec x, poly_2secpar_vec y) { return poly320_eq(x,y); }
inline poly_2secpar_vec poly_2secpar_extract(poly_2secpar_vec x, size_t i) { return poly320_extract(x,i); }

inline poly_2secpar_vec poly_secpar_mul(poly_secpar_vec x, poly_secpar_vec y) { return poly160_mul(x,y); }
inline poly_secpar_vec poly1xsecpar_mul(poly1_vec x, poly_secpar_vec y) { return poly1x160_mul(x,y); }
inline poly_2secpar_vec poly1x2secpar_mul(poly1_vec x, poly_2secpar_vec y) { return poly1x320_mul(x,y); }
inline poly_secpar_plus_64_vec poly64xsecpar_mul(poly64_vec x, poly_secpar_vec y) { return poly64x160_mul(x,y); }
inline poly_2secpar_vec poly_2secpar_shift_left_1(poly_2secpar_vec x) { return poly320_shift_left_1(x); }
inline poly_2secpar_vec poly_2secpar_shift_left_8(poly_2secpar_vec x) { return poly320_shift_left_8(x); }
inline poly_secpar_vec poly_2secpar_reduce_secpar(poly_2secpar_vec x) { return poly320_reduce160(x); }
inline poly_secpar_vec poly_secpar_plus_64_reduce_secpar(poly_secpar_plus_64_vec x) { return poly224_reduce160(x); }
inline poly_secpar_plus_64_vec poly_secpar_plus_64_from_secpar(poly_secpar_vec x) { return poly224_from_160(x); }
inline poly_2secpar_vec poly_2secpar_from_secpar(poly_secpar_vec x) { return poly320_from_160(x); }
inline poly_secpar_vec poly_secpar_from_64(poly64_vec x) { return poly160_from_64(x); }
inline poly_secpar_vec poly_secpar_from_1(poly1_vec x) { return poly160_from_1(x); }
inline poly_secpar_vec poly_secpar_from_byte(uint8_t x) { return poly160_set_low32(x); }
inline poly_secpar_vec poly_secpar_from_secpar_poly1(const poly1_vec* p) { return poly160_pack_bits(p,160); }
inline poly_secpar_vec poly_secpar_from_8_poly1(const poly1_vec* p) { return poly160_pack_bits(p,8); }
inline poly_secpar_vec poly_secpar_from_16_poly1(const poly1_vec* p) { return poly160_pack_bits(p,16); }
inline poly_secpar_vec poly_secpar_from_secpar_poly_secpar(const poly_secpar_vec* p) { return poly160_pack_coefficients(p,160); }
inline poly_secpar_vec poly_secpar_from_8_poly_secpar(const poly_secpar_vec* p) { return poly160_pack_coefficients(p,8); }
inline poly_secpar_vec poly_secpar_from_16_poly_secpar(const poly_secpar_vec* p) { return poly160_pack_coefficients(p,16); }
inline poly_secpar_vec poly_secpar_from_8_poly64(const poly64_vec* p)
{
    poly160_vec lifted[8];
    for(size_t i=0;i<8;++i) lifted[i]=poly160_from_64(p[i]);
    return poly160_pack_coefficients(lifted,8);
}
inline poly_secpar_vec poly_secpar_exp(poly_secpar_vec x, size_t exponent) { return poly160_exp(x,exponent); }
#endif
