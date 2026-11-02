#ifndef GREATWALL160_POLYNOMIALS_IMPL_H
#define GREATWALL160_POLYNOMIALS_IMPL_H

#include "block.h"

typedef uint8_t poly1_vec;
typedef __m128i poly64_vec;
typedef __m128i poly128_vec;
typedef struct { __m128i data[2]; } poly160_vec;
typedef struct { __m128i data[2]; } poly224_vec;
typedef struct { __m128i data[3]; } poly320_vec;

inline poly1_vec poly1_set_all(uint8_t x) { return x; }
inline poly1_vec poly1_load(unsigned long x, unsigned int offset)
{
    assert(offset < sizeof(x)*8);
    return (poly1_vec)(0u - (unsigned)((x >> offset)&1));
}
inline poly1_vec poly1_load_offset8(const void* p, unsigned int offset)
{
    assert(offset < 8);
    return poly1_load(*(const uint8_t*)p,offset);
}
inline poly64_vec poly64_load(const void* p)
{
    uint64_t v; memcpy(&v,p,8); return _mm_cvtsi64_si128(v);
}
inline poly128_vec poly128_load(const void* p)
{
    __m128i v; memcpy(&v,p,16); return v;
}
inline poly64_vec poly64_load_dup(const void* p) { return poly64_load(p); }
inline poly128_vec poly128_load_dup(const void* p) { return poly128_load(p); }
inline void poly64_store(void* p, poly64_vec x) { uint64_t v=(uint64_t)_mm_cvtsi128_si64(x); memcpy(p,&v,8); }
inline void poly128_store(void* p, poly128_vec x) { memcpy(p,&x,16); }
inline void poly128_store1(void* p, poly128_vec x) { poly128_store(p,x); }
inline poly64_vec poly64_set_zero(void) { return _mm_setzero_si128(); }
inline poly128_vec poly128_set_zero(void) { return _mm_setzero_si128(); }
inline poly64_vec poly64_set_low32(uint32_t x) { return _mm_cvtsi32_si128(x); }
inline poly128_vec poly128_set_low32(uint32_t x) { return _mm_cvtsi32_si128(x); }
inline poly64_vec poly64_add(poly64_vec x, poly64_vec y) { return _mm_xor_si128(x,y); }
inline poly128_vec poly128_add(poly128_vec x, poly128_vec y) { return _mm_xor_si128(x,y); }
inline poly128_vec poly128_from_64(poly64_vec x) { return _mm_unpacklo_epi64(x,_mm_setzero_si128()); }
inline poly128_vec poly64_mul(poly64_vec x, poly64_vec y) { return _mm_clmulepi64_si128(x,y,0); }
inline poly64_vec poly64_extract(poly64_vec x, size_t i) { assert(i==0); (void)i; return x; }
inline poly128_vec poly128_extract(poly128_vec x, size_t i) { assert(i==0); (void)i; return x; }
inline bool poly64_eq(poly64_vec x, poly64_vec y) { return _mm_cvtsi128_si64(_mm_xor_si128(x,y))==0; }
inline bool poly128_eq(poly128_vec x, poly128_vec y) { return _mm_testz_si128(_mm_xor_si128(x,y),_mm_set1_epi32(-1))!=0; }

inline poly64_vec poly128_reduce64(poly128_vec x)
{
    const __m128i modulus = _mm_cvtsi32_si128(0x1b);
    const __m128i first = _mm_clmulepi64_si128(modulus,x,0x10);
    const __m128i second = _mm_clmulepi64_si128(modulus,first,0x10);
    return poly128_from_64(_mm_xor_si128(x,_mm_xor_si128(first,second)));
}
inline poly64_vec poly64_mul_a64_reduce64(poly64_vec x)
{
    return poly128_reduce64(_mm_clmulepi64_si128(x,_mm_cvtsi32_si128(0x1b),0));
}
inline poly64_vec poly64_exp(poly64_vec x, size_t exponent)
{
    poly64_vec z=poly64_set_low32(1);
    while(exponent) {
        if(exponent&1) z=poly128_reduce64(poly64_mul(z,x));
        x=poly128_reduce64(poly64_mul(x,x));
        exponent>>=1;
    }
    return z;
}

inline poly160_vec poly160_set_zero(void) { poly160_vec z; memset(&z,0,sizeof(z)); return z; }
inline poly160_vec poly160_load(const void* p) { poly160_vec z=poly160_set_zero(); memcpy(&z,p,20); return z; }
inline poly160_vec poly160_load_dup(const void* p) { return poly160_load(p); }
inline void poly160_store(void* p, poly160_vec x) { memcpy(p,&x,20); }
inline void poly160_store1(void* p, poly160_vec x) { poly160_store(p,x); }
inline poly160_vec poly160_set_low32(uint32_t x) { poly160_vec z=poly160_set_zero(); z.data[0]=_mm_cvtsi32_si128(x); return z; }
inline poly160_vec poly160_add(poly160_vec x, poly160_vec y) { poly160_vec z; for(size_t i=0;i<2;++i) z.data[i]=_mm_xor_si128(x.data[i],y.data[i]); return z; }
inline bool poly160_eq(poly160_vec x, poly160_vec y) { __m128i d=_mm_setzero_si128(); for(size_t i=0;i<2;++i) d=_mm_or_si128(d,_mm_xor_si128(x.data[i],y.data[i])); return _mm_testz_si128(d,d)!=0; }
inline poly160_vec poly160_extract(poly160_vec x, size_t index) { assert(index==0); (void)index; return x; }
inline poly160_vec poly1x160_mul(poly1_vec x, poly160_vec y)
{

    const __m128i mask=_mm_set1_epi8((char)(0u-(unsigned)(x!=0)));
    poly160_vec z;
    for(size_t i=0;i<2;++i) z.data[i]=_mm_and_si128(mask,y.data[i]);
    return z;
}

inline poly224_vec poly224_set_zero(void) { poly224_vec z; memset(&z,0,sizeof(z)); return z; }
inline poly224_vec poly224_load(const void* p) { poly224_vec z=poly224_set_zero(); memcpy(&z,p,28); return z; }
inline poly224_vec poly224_load_dup(const void* p) { return poly224_load(p); }
inline void poly224_store(void* p, poly224_vec x) { memcpy(p,&x,28); }
inline void poly224_store1(void* p, poly224_vec x) { poly224_store(p,x); }
inline poly224_vec poly224_set_low32(uint32_t x) { poly224_vec z=poly224_set_zero(); z.data[0]=_mm_cvtsi32_si128(x); return z; }
inline poly224_vec poly224_add(poly224_vec x, poly224_vec y) { poly224_vec z; for(size_t i=0;i<2;++i) z.data[i]=_mm_xor_si128(x.data[i],y.data[i]); return z; }
inline bool poly224_eq(poly224_vec x, poly224_vec y) { __m128i d=_mm_setzero_si128(); for(size_t i=0;i<2;++i) d=_mm_or_si128(d,_mm_xor_si128(x.data[i],y.data[i])); return _mm_testz_si128(d,d)!=0; }
inline poly224_vec poly224_extract(poly224_vec x, size_t index) { assert(index==0); (void)index; return x; }
inline poly224_vec poly1x224_mul(poly1_vec x, poly224_vec y)
{

    const __m128i mask=_mm_set1_epi8((char)(0u-(unsigned)(x!=0)));
    poly224_vec z;
    for(size_t i=0;i<2;++i) z.data[i]=_mm_and_si128(mask,y.data[i]);
    return z;
}

inline poly320_vec poly320_set_zero(void) { poly320_vec z; memset(&z,0,sizeof(z)); return z; }
inline poly320_vec poly320_load(const void* p) { poly320_vec z=poly320_set_zero(); memcpy(&z,p,40); return z; }
inline poly320_vec poly320_load_dup(const void* p) { return poly320_load(p); }
inline void poly320_store(void* p, poly320_vec x) { memcpy(p,&x,40); }
inline void poly320_store1(void* p, poly320_vec x) { poly320_store(p,x); }
inline poly320_vec poly320_set_low32(uint32_t x) { poly320_vec z=poly320_set_zero(); z.data[0]=_mm_cvtsi32_si128(x); return z; }
inline poly320_vec poly320_add(poly320_vec x, poly320_vec y) { poly320_vec z; for(size_t i=0;i<3;++i) z.data[i]=_mm_xor_si128(x.data[i],y.data[i]); return z; }
inline bool poly320_eq(poly320_vec x, poly320_vec y) { __m128i d=_mm_setzero_si128(); for(size_t i=0;i<3;++i) d=_mm_or_si128(d,_mm_xor_si128(x.data[i],y.data[i])); return _mm_testz_si128(d,d)!=0; }
inline poly320_vec poly320_extract(poly320_vec x, size_t index) { assert(index==0); (void)index; return x; }
inline poly320_vec poly1x320_mul(poly1_vec x, poly320_vec y)
{

    const __m128i mask=_mm_set1_epi8((char)(0u-(unsigned)(x!=0)));
    poly320_vec z;
    for(size_t i=0;i<3;++i) z.data[i]=_mm_and_si128(mask,y.data[i]);
    return z;
}

inline poly160_vec poly160_from_64(poly64_vec x)
{
    poly160_vec z=poly160_set_zero(); z.data[0]=poly128_from_64(x); return z;
}
inline poly160_vec poly160_from_1(poly1_vec x) { return poly160_set_low32(x!=0); }
inline poly224_vec poly224_from_160(poly160_vec x)
{
    poly224_vec z; z.data[0]=x.data[0]; z.data[1]=x.data[1]; return z;
}
inline poly320_vec poly320_from_160(poly160_vec x)
{
    poly320_vec z=poly320_set_zero(); z.data[0]=x.data[0]; z.data[1]=x.data[1]; return z;
}

inline poly320_vec poly160_mul(poly160_vec a, poly160_vec b)
{
    const __m128i a0=a.data[0], b0=b.data[0];
    const __m128i a1=_mm_srli_si128(a0,8), b1=_mm_srli_si128(b0,8);
    const __m128i a2=a.data[1], b2=b.data[1];
    const __m128i p0=_mm_clmulepi64_si128(a0,b0,0x00);
    const __m128i p1=_mm_clmulepi64_si128(a0,b0,0x11);
    const __m128i p2=_mm_clmulepi64_si128(a2,b2,0x00);
    const __m128i p01=_mm_xor_si128(
        _mm_clmulepi64_si128(_mm_xor_si128(a0,a1),_mm_xor_si128(b0,b1),0),
        _mm_xor_si128(p0,p1));
    const __m128i p02=_mm_xor_si128(
        _mm_clmulepi64_si128(_mm_xor_si128(a0,a2),_mm_xor_si128(b0,b2),0),
        _mm_xor_si128(p0,p2));
    const __m128i p12=_mm_xor_si128(
        _mm_clmulepi64_si128(_mm_xor_si128(a1,a2),_mm_xor_si128(b1,b2),0),
        _mm_xor_si128(p1,p2));
    poly320_vec z;
    z.data[0]=_mm_xor_si128(p0,_mm_slli_si128(p01,8));
    z.data[1]=_mm_xor_si128(_mm_xor_si128(p1,p02),
        _mm_xor_si128(_mm_srli_si128(p01,8),_mm_slli_si128(p12,8)));
    z.data[2]=_mm_xor_si128(p2,_mm_srli_si128(p12,8));
    return z;
}
inline poly224_vec poly64x160_mul(poly64_vec a, poly160_vec b)
{
    const __m128i p0=_mm_clmulepi64_si128(a,b.data[0],0x00);
    const __m128i p1=_mm_clmulepi64_si128(a,b.data[0],0x10);
    const __m128i p2=_mm_clmulepi64_si128(a,b.data[1],0x00);
    poly224_vec z;
    z.data[0]=_mm_xor_si128(p0,_mm_slli_si128(p1,8));
    z.data[1]=_mm_xor_si128(p2,_mm_srli_si128(p1,8));
    return z;
}

/* Experimental authentication field, distinct from native GF(2^163).
 * f(X)=X^160+X^5+X^3+X^2+1. Rabin irreducibility certificate:
 * X^(2^160) mod f=X; gcd(f,X^(2^80)+X)=gcd(f,X^(2^32)+X)=1.
 * 2 and 5 are all prime divisors of 160. tests/test_gf160.c independently
 * recomputes these checks by bitwise polynomial long division.
 *
 * X^160 = r = 1+X^2+X^3+X^5. Fold H*r into L; only five bits can
 * exceed degree159, and folding those a second time finishes reduction.
 * This accepts every degree<=319 input, not just multiplication outputs. */
inline poly160_vec poly320_reduce160(poly320_vec x)
{
    uint64_t w[5]; memcpy(w,&x,40);
    const uint64_t h0=(w[2]>>32)^(w[3]<<32);
    const uint64_t h1=(w[3]>>32)^(w[4]<<32);
    const uint64_t h2=w[4]>>32;
    uint64_t r[3];
    r[0]=w[0]^h0^(h0<<2)^(h0<<3)^(h0<<5);
    r[1]=w[1]^h1^(h1<<2)^(h1<<3)^(h1<<5)^(h0>>62)^(h0>>61)^(h0>>59);
    r[2]=(w[2]&UINT64_C(0xffffffff))^h2^(h2<<2)^(h2<<3)^(h2<<5)^(h1>>62)^(h1>>61)^(h1>>59);
    const uint64_t overflow=r[2]>>32;
    r[2]&=UINT64_C(0xffffffff);
    r[0]^=overflow^(overflow<<2)^(overflow<<3)^(overflow<<5);
    return poly160_load(r);
}
inline poly160_vec poly224_reduce160(poly224_vec x)
{
    poly320_vec z=poly320_set_zero();
    z.data[0]=x.data[0]; z.data[1]=x.data[1];
    return poly320_reduce160(z);
}
inline poly320_vec poly320_shift_left_1(poly320_vec x)
{
    uint64_t w[5],z[5]; memcpy(w,&x,40);
    z[0]=w[0]<<1; for(size_t i=1;i<5;++i) z[i]=(w[i]<<1)|(w[i-1]>>63);
    return poly320_load(z);
}
inline poly320_vec poly320_shift_left_8(poly320_vec x)
{
    uint64_t w[5],z[5]; memcpy(w,&x,40);
    z[0]=w[0]<<8; for(size_t i=1;i<5;++i) z[i]=(w[i]<<8)|(w[i-1]>>56);
    return poly320_load(z);
}
inline poly160_vec poly160_exp(poly160_vec x, size_t exponent)
{
    poly160_vec z=poly160_set_low32(1);
    while(exponent) {
        if(exponent&1) z=poly320_reduce160(poly160_mul(z,x));
        x=poly320_reduce160(poly160_mul(x,x));
        exponent>>=1;
    }
    return z;
}

inline poly160_vec poly160_pack_bits(const poly1_vec* p, size_t n)
{
    assert(n<=160);
    uint8_t bytes[20]={0};
    for(size_t i=0;i<n;++i) bytes[i/8]|=(uint8_t)((unsigned)(p[i]!=0)<<(i%8));
    return poly160_load(bytes);
}
inline poly160_vec poly160_pack_coefficients(const poly160_vec* p, size_t n)
{
    assert(n<=160);
    uint64_t acc[5]={0};
    for(size_t i=0;i<n;++i) {
        uint64_t w[3]={0}; poly160_store(w,p[i]);
        const size_t word=i/64; const unsigned shift=(unsigned)(i%64);
        for(size_t j=0;j<3;++j) {
            acc[word+j]^=w[j]<<shift;
            if(shift && word+j+1<5) acc[word+j+1]^=w[j]>>(64-shift);
        }
    }
    return poly320_reduce160(poly320_load(acc));
}
#endif
