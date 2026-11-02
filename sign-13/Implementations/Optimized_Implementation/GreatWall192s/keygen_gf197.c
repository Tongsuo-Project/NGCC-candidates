#include "keygen_gf197.h"
#include "gf197.h"
#include "greatwall.h"
#include "owf_alpha.h"
#include "timing.h"

GW_STATIC_ASSERT(SECURITY_PARAM == 192, "keygen_gf197 is only for GreatWall192");
GW_STATIC_ASSERT(OWF_BLOCK_SIZE == BLOCK197_BYTES, "GF197 encoding mismatch");

#define GREATWALL_KEYGEN_GF197_LOCAL_ARITH 1

static inline void keygen_gf197_square(block197* out,const block197* in) {
#if GREATWALL_KEYGEN_GF197_LOCAL_ARITH
    assert(block197_is_canonical(in));
    const uint64_t x0=in->data[0],x1=in->data[1],x2=in->data[2],x3=in->data[3];
    const __m128i a=_mm_set_epi64x((long long)x1,(long long)x0);
    const __m128i b=_mm_set_epi64x((long long)x3,(long long)x2);
    const __m128i tab=_mm_set_epi64x(0x5554515045444140,0x1514111005040100);
    const __m128i mask=_mm_set1_epi8(15);
    const __m128i ae=_mm_shuffle_epi8(tab,_mm_and_si128(a,mask));
    const __m128i ao=_mm_shuffle_epi8(tab,_mm_and_si128(_mm_srli_epi64(a,4),mask));
    const __m128i be=_mm_shuffle_epi8(tab,_mm_and_si128(b,mask));
    const __m128i bo=_mm_shuffle_epi8(tab,_mm_and_si128(_mm_srli_epi64(b,4),mask));
    const __m128i p0=_mm_unpacklo_epi8(ae,ao),p1=_mm_unpackhi_epi8(ae,ao);
    const __m128i p2=_mm_unpacklo_epi8(be,bo),p3=_mm_unpackhi_epi8(be,bo);
    const uint64_t w0=_mm_cvtsi128_si64(p0),w1=_mm_extract_epi64(p0,1);
    const uint64_t w2=_mm_cvtsi128_si64(p1),w3=_mm_extract_epi64(p1,1);
    const uint64_t w4=_mm_cvtsi128_si64(p2),w5=_mm_extract_epi64(p2,1),w6=_mm_cvtsi128_si64(p3);
    const uint64_t h0=(w3>>5)|(w4<<59), h1=(w4>>5)|(w5<<59);
    const uint64_t h2=(w5>>5)|(w6<<59), h3=w6>>5;
    const uint64_t t0=w0^h0^(h0<<1)^(h0<<2)^(h0<<21);
    const uint64_t t1=w1^h1^(h1<<1)^(h1<<2)^(h1<<21)^(h0>>63)^(h0>>62)^(h0>>43);
    const uint64_t t2=w2^h2^(h2<<1)^(h2<<2)^(h2<<21)^(h1>>63)^(h1>>62)^(h1>>43);
    const uint64_t t3=(w3&UINT64_C(0x1f))^h3^(h3<<1)^(h3<<2)^(h3<<21)^(h2>>63)^(h2>>62)^(h2>>43);
    const uint64_t u=t3>>5;
    out->data[0]=t0^u^(u<<1)^(u<<2)^(u<<21);
    out->data[1]=t1; out->data[2]=t2; out->data[3]=t3&UINT64_C(0x1f);
#else
    block197_sqr(out,in);
#endif
}

static inline void keygen_gf197_multiply(block197* out,const block197* a,const block197* b) {
#if GREATWALL_KEYGEN_GF197_LOCAL_ARITH
    assert(block197_is_canonical(a)&&block197_is_canonical(b));
    const __m128i a0=_mm_cvtsi64_si128((long long)a->data[0]);
    const __m128i a1=_mm_cvtsi64_si128((long long)a->data[1]);
    const __m128i a2=_mm_cvtsi64_si128((long long)a->data[2]);
    const __m128i a3=_mm_cvtsi64_si128((long long)a->data[3]);
    const __m128i b0=_mm_cvtsi64_si128((long long)b->data[0]);
    const __m128i b1=_mm_cvtsi64_si128((long long)b->data[1]);
    const __m128i b2=_mm_cvtsi64_si128((long long)b->data[2]);
    const __m128i b3=_mm_cvtsi64_si128((long long)b->data[3]);
    const __m128i p00=_mm_clmulepi64_si128(a0,b0,0);
    const __m128i p01=_mm_clmulepi64_si128(a0,b1,0);
    const __m128i p02=_mm_clmulepi64_si128(a0,b2,0);
    const __m128i p03=_mm_clmulepi64_si128(a0,b3,0);
    const __m128i p10=_mm_clmulepi64_si128(a1,b0,0);
    const __m128i p11=_mm_clmulepi64_si128(a1,b1,0);
    const __m128i p12=_mm_clmulepi64_si128(a1,b2,0);
    const __m128i p13=_mm_clmulepi64_si128(a1,b3,0);
    const __m128i p20=_mm_clmulepi64_si128(a2,b0,0);
    const __m128i p21=_mm_clmulepi64_si128(a2,b1,0);
    const __m128i p22=_mm_clmulepi64_si128(a2,b2,0);
    const __m128i p23=_mm_clmulepi64_si128(a2,b3,0);
    const __m128i p30=_mm_clmulepi64_si128(a3,b0,0);
    const __m128i p31=_mm_clmulepi64_si128(a3,b1,0);
    const __m128i p32=_mm_clmulepi64_si128(a3,b2,0);
    const __m128i p33=_mm_clmulepi64_si128(a3,b3,0);
    const uint64_t w0=(uint64_t)_mm_cvtsi128_si64(p00);
    const uint64_t w1=(uint64_t)_mm_extract_epi64(p00,1)^(uint64_t)_mm_cvtsi128_si64(p01)^(uint64_t)_mm_cvtsi128_si64(p10);
    const uint64_t w2=(uint64_t)_mm_extract_epi64(p01,1)^(uint64_t)_mm_cvtsi128_si64(p02)^(uint64_t)_mm_extract_epi64(p10,1)^(uint64_t)_mm_cvtsi128_si64(p11)^(uint64_t)_mm_cvtsi128_si64(p20);
    const uint64_t w3=(uint64_t)_mm_extract_epi64(p02,1)^(uint64_t)_mm_cvtsi128_si64(p03)^(uint64_t)_mm_extract_epi64(p11,1)^(uint64_t)_mm_cvtsi128_si64(p12)^(uint64_t)_mm_extract_epi64(p20,1)^(uint64_t)_mm_cvtsi128_si64(p21)^(uint64_t)_mm_cvtsi128_si64(p30);
    const uint64_t w4=(uint64_t)_mm_extract_epi64(p03,1)^(uint64_t)_mm_extract_epi64(p12,1)^(uint64_t)_mm_cvtsi128_si64(p13)^(uint64_t)_mm_extract_epi64(p21,1)^(uint64_t)_mm_cvtsi128_si64(p22)^(uint64_t)_mm_extract_epi64(p30,1)^(uint64_t)_mm_cvtsi128_si64(p31);
    const uint64_t w5=(uint64_t)_mm_extract_epi64(p13,1)^(uint64_t)_mm_extract_epi64(p22,1)^(uint64_t)_mm_cvtsi128_si64(p23)^(uint64_t)_mm_extract_epi64(p31,1)^(uint64_t)_mm_cvtsi128_si64(p32);
    const uint64_t w6=(uint64_t)_mm_extract_epi64(p23,1)^(uint64_t)_mm_extract_epi64(p32,1)^(uint64_t)_mm_cvtsi128_si64(p33);
    const uint64_t h0=(w3>>5)|(w4<<59), h1=(w4>>5)|(w5<<59);
    const uint64_t h2=(w5>>5)|(w6<<59), h3=w6>>5;
    const uint64_t t0=w0^h0^(h0<<1)^(h0<<2)^(h0<<21);
    const uint64_t t1=w1^h1^(h1<<1)^(h1<<2)^(h1<<21)^(h0>>63)^(h0>>62)^(h0>>43);
    const uint64_t t2=w2^h2^(h2<<1)^(h2<<2)^(h2<<21)^(h1>>63)^(h1>>62)^(h1>>43);
    const uint64_t t3=(w3&UINT64_C(0x1f))^h3^(h3<<1)^(h3<<2)^(h3<<21)^(h2>>63)^(h2>>62)^(h2>>43);
    const uint64_t u=t3>>5;
    out->data[0]=t0^u^(u<<1)^(u<<2)^(u<<21);
    out->data[1]=t1; out->data[2]=t2; out->data[3]=t3&UINT64_C(0x1f);
#else
    block197_mul(out,a,b);
#endif
}

static inline void keygen_gf197_square_n(block197* out, const block197* in, unsigned count)
{
    *out = *in;
    for (unsigned i = 0; i < count; ++i)
        keygen_gf197_square(out, out);
}

static inline void keygen_gf197_inverse1(block197* block)
{
#if GREATWALL_GF197_INVERSE_CHAIN

    const block197 x = *block;
    block197 t, x3, x7, x14, p, r2, r4, r8, r10, b;
    keygen_gf197_square(&t, &x);
    keygen_gf197_multiply(&x3, &t, &x);
    keygen_gf197_square(&t, &x3);
    keygen_gf197_multiply(&x7, &t, &x);
    keygen_gf197_square(&x14, &x7);
    keygen_gf197_square_n(&t, &x7, 4);
    keygen_gf197_multiply(&t, &t, &x7);
    keygen_gf197_square_n(&t, &t, 2);
    keygen_gf197_multiply(&p, &t, &x);
    keygen_gf197_square_n(&r2, &p, 9);
    keygen_gf197_multiply(&r2, &r2, &p);
    keygen_gf197_square_n(&r4, &r2, 18);
    keygen_gf197_multiply(&r4, &r4, &r2);
    keygen_gf197_square_n(&r8, &r4, 36);
    keygen_gf197_multiply(&r8, &r8, &r4);
    keygen_gf197_square_n(&r10, &r8, 18);
    keygen_gf197_multiply(&r10, &r10, &r2);
    keygen_gf197_square_n(&b, &r10, 4);
    keygen_gf197_multiply(&b, &b, &x14);
    keygen_gf197_square_n(&t, &b, 94);
    keygen_gf197_multiply(&t, &t, &b);
    keygen_gf197_square_n(&t, &t, 9);
    keygen_gf197_multiply(block, &t, &p);
#else
    block197_mersenne_inverse1_binary(block);
#endif
}

static inline void keygen_gf197_inverse2(block197* block)
{
#if GREATWALL_GF197_INVERSE_CHAIN

    const block197 x = *block;
    block197 t, a2, a3, a5, a10, a11, d, p, r2, r3, r6, r7, b;
    keygen_gf197_square(&t, &x);
    keygen_gf197_multiply(&a2, &t, &x);
    keygen_gf197_square(&t, &a2);
    keygen_gf197_multiply(&a3, &t, &x);
    keygen_gf197_square_n(&t, &a3, 2);
    keygen_gf197_multiply(&a5, &t, &a2);
    keygen_gf197_square_n(&t, &a5, 5);
    keygen_gf197_multiply(&a10, &t, &a5);
    keygen_gf197_square(&t, &a10);
    keygen_gf197_multiply(&a11, &t, &x);
    keygen_gf197_square_n(&t, &a11, 2);
    keygen_gf197_multiply(&d, &t, &x);
    keygen_gf197_multiply(&p, &d, &x);
    keygen_gf197_square_n(&r2, &p, 13);
    keygen_gf197_multiply(&r2, &r2, &p);
    keygen_gf197_square_n(&r3, &r2, 13);
    keygen_gf197_multiply(&r3, &r3, &p);
    keygen_gf197_square_n(&r6, &r3, 39);
    keygen_gf197_multiply(&r6, &r6, &r3);
    keygen_gf197_square_n(&r7, &r6, 13);
    keygen_gf197_multiply(&r7, &r7, &p);
    keygen_gf197_square(&b, &r7);
    keygen_gf197_multiply(&b, &b, &x);
    keygen_gf197_square_n(&t, &b, 92);
    keygen_gf197_multiply(&t, &t, &b);
    keygen_gf197_square_n(&t, &t, 13);
    keygen_gf197_multiply(block, &t, &d);
#else
    block197_mersenne_inverse2_binary(block);
#endif
}

static inline void keygen_gf197_matrix(block197* x, const uint64_t matrix[197][4])
{
    block197_multiply_with_GF2_matrix(x, matrix);
}

bool greatwall_keygen_pubkey_197(uint8_t* pk, const uint8_t* sk)
{
    block197 key, iv;
    block197_load_to_block(&key, sk);
    block197_load_to_block(&iv, sk + BLOCK197_BYTES);

    FAEST_TIMING_DECLARE();
    FAEST_TIMING_START();
    owf_block alpha0, alpha1;
    if (!derive_owf_alpha(&alpha0, &iv, 0) ||
        !derive_owf_alpha(&alpha1, &iv, 1) ||
        owf_alpha_is_zero(&alpha0) || owf_alpha_is_zero(&alpha1)) {
        FAEST_TIMING_PRINT("keygen_owf/derive_alpha_and_check");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/derive_alpha_and_check");

    FAEST_TIMING_START();
    block197 state = key;
    block197 rc0, rc1;
    block197_load_words(&rc0, greatwall_rc_197[0]);
    block197_load_words(&rc1, greatwall_rc_197[1]);
    FAEST_TIMING_PRINT("keygen_owf/load_constants");

    FAEST_TIMING_START();
    keygen_gf197_matrix(&state, greatwall_mat_197_0);
    state = block197_xor(state, rc0);
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix0_and_add_constant");
    FAEST_TIMING_START();
    keygen_gf197_inverse1(&state);
    FAEST_TIMING_PRINT("keygen_owf/mersenne_inverse1");
    FAEST_TIMING_START();
    if (!owf_alpha_dot_is_one(&alpha0, &state)) {
        FAEST_TIMING_PRINT("keygen_owf/check_alpha0");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/check_alpha0");

    FAEST_TIMING_START();
    state = block197_xor(state, key);
    keygen_gf197_matrix(&state, greatwall_mat_197_1);
    state = block197_xor(state, iv);
    const block197 tmp = state;
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix1_and_mix_input");
    FAEST_TIMING_START();
    keygen_gf197_inverse2(&state);
    FAEST_TIMING_PRINT("keygen_owf/mersenne_inverse2");
    FAEST_TIMING_START();
    if (!owf_alpha_dot_is_one(&alpha1, &state)) {
        FAEST_TIMING_PRINT("keygen_owf/check_alpha1");
        return false;
    }
    FAEST_TIMING_PRINT("keygen_owf/check_alpha1");

    FAEST_TIMING_START();
    keygen_gf197_matrix(&state, greatwall_mat_197_2);
    state = block197_xor(state, rc1);
    state = block197_xor(state, tmp);
    state = block197_xor(state, key);

    block197_store_from_block(pk, &state);
    block197_store_from_block(pk + BLOCK197_BYTES, &iv);
    FAEST_TIMING_PRINT("keygen_owf/linear_matrix2_and_pack_output");
    return true;
}
