#include <stdint.h>
#include <string.h>
#include <immintrin.h>
#include "params.h"
#include "poly.h"
#include "ntt.h"
#include "ntt_avx.h"
#include "basemul_avx.h"
#include "consts.h"
#include "reduce.h"
#include "symmetric.h"

/* Barrett reduction followed by branch-free canonicalization to [0, Q). */
static inline __m256i freeze_avx(__m256i a, __m256i q, __m256i v)
{
    __m256i t = _mm256_mulhi_epi16(v, a);
    t = _mm256_srai_epi16(t, 8);
    a = _mm256_sub_epi16(a, _mm256_mullo_epi16(q, t));

    __m256i sign = _mm256_srai_epi16(a, 15);
    a = _mm256_add_epi16(a, _mm256_and_si256(sign, q));
    return _mm256_min_epu16(a, _mm256_sub_epi16(a, q));
}

#if D == 9
/* Quantize and pack eight canonical q=3329 coefficients into 72 bits. */
static inline void compress9_8(uint8_t out[9], __m128i c16)
{
    const __m256i scale = _mm256_set1_epi32(161271);
    const __m256i bias  = _mm256_set1_epi32(524160);
    const __m256i mask9 = _mm256_set1_epi32(0x1FF);
    const uint64_t lane_mask = 0x01FF01FF01FF01FFULL;
    __m256i c32 = _mm256_cvtepi16_epi32(c16);
    __m256i t32 = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_mullo_epi32(c32, scale), bias), 20);
    t32 = _mm256_and_si256(t32, mask9);
    __m128i t16 = _mm_packus_epi32(_mm256_castsi256_si128(t32),
                                   _mm256_extracti128_si256(t32, 1));
    uint64_t lo = (uint64_t)_mm_cvtsi128_si64(t16);
    uint64_t hi = (uint64_t)_mm_extract_epi64(t16, 1);
    uint64_t p0 = _pext_u64(lo, lane_mask);
    uint64_t p1 = _pext_u64(hi, lane_mask);
    uint64_t packed = p0 | (p1 << 36);

    memcpy(out, &packed, sizeof(packed));
    out[8] = (uint8_t)(p1 >> 28);
}
#endif

#if KEM_MODE == 128 || KEM_MODE == 256
/*
 * Exact division by 97 for every 33-bit value accepted by the wire format.
 * The floor reciprocal can undershoot by at most one; the final comparison
 * corrects both quotient and remainder without a hardware DIV.
 */
static inline uint64_t divmod97(uint64_t x, uint16_t *remainder)
{
    const uint64_t reciprocal = 708448213ULL; /* floor(2^36 / 97) */
    uint64_t quotient = (x * reciprocal) >> 36;
    uint64_t rem = x - quotient * 97;
    uint64_t adjust = (uint64_t)(rem >= 97);

    *remainder = (uint16_t)(rem - adjust * 97);
    return quotient + adjust;
}
#endif

/*************************************************
* Name:        poly_freeze
*
* Description: Applies Barrett reduction to all coefficients of a polynomial
*              using the parameters shared with reduce_asm.S
*
* Arguments:   - poly *r: pointer to input/output polynomial
**************************************************/
void poly_freeze(poly *r)
{
    const __m256i q = _mm256_load_si256((const __m256i *)(qdata + _16XQ));
    const __m256i v = _mm256_load_si256((const __m256i *)(qdata + _16XV));
    for (unsigned int i = 0; i < N; i += 16) {
        __m256i a = _mm256_load_si256((const __m256i *)(r->coeffs + i));
        a = freeze_avx(a, q, v);
        _mm256_store_si256((__m256i *)(r->coeffs + i), a);
    }
}


/*************************************************
* Name:        poly_ntt
*
* Description: Computes negacyclic number-theoretic transform (NTT) of
*              a polynomial in place;
*              inputs assumed to be in normal order, output in bitreversed order
*
* Arguments:   - uint16_t *r: pointer to in/output polynomial
**************************************************/
void poly_ntt(poly *r)
{
    /*
     * All production callers provide either ternary coefficients or the
     * bounded output of poly_unpack_and_decompress().  Keeping those small
     * representatives avoids a redundant full-polynomial freeze pass.
     */
    ntt_avx(r->coeffs, qdata);
}

/*************************************************
* Name:        poly_invntt_tomont
*
* Description: Computes inverse of negacyclic number-theoretic transform (NTT)
*              of a polynomial in place;
*              inputs assumed to be in bitreversed order, output in normal order
*
* Arguments:   - uint16_t *r: pointer to in/output polynomial
**************************************************/
void poly_invntt_tomont(poly *r)
{
    invntt_avx(r->coeffs, qdata);
}

/*************************************************
* Name:        poly_basemul_montgomery
*
* Description: Multiplication of two polynomials in NTT domain
*
* Arguments:   - poly *r:       pointer to output polynomial
*              - const poly *a: pointer to first input polynomial
*              - const poly *b: pointer to second input polynomial
**************************************************/
void poly_basemul_montgomery(poly *r, const poly *a, const poly *b)
{
    poly_basemul_montgomery_avx(r->coeffs, a->coeffs, b->coeffs, qdata);
}

/*************************************************
* Name:        poly_tomont
*
* Description: Inplace conversion of all coefficients of a polynomial
*              from normal domain to Montgomery domain
*
* Arguments:   - poly *r: pointer to input/output polynomial
**************************************************/
void poly_tomont(poly *r)
{
    const __m256i montsq = _mm256_set1_epi16(MONTSQ);
    const __m256i q      = _mm256_set1_epi16(Q);
    const __m256i qinv   = _mm256_set1_epi16(QINV);
    for (unsigned int i = 0; i < N; i += 16) {
        __m256i a = _mm256_load_si256((const __m256i *)(r->coeffs + i));
        __m256i lo = _mm256_mullo_epi16(montsq, a);
        __m256i hi = _mm256_mulhi_epi16(montsq, a);
        __m256i u  = _mm256_mullo_epi16(lo, qinv);
        __m256i uq = _mm256_mulhi_epi16(q, u);
        a = _mm256_sub_epi16(hi, uq);
        _mm256_store_si256((__m256i *)(r->coeffs + i), a);
    }
}

/*************************************************
* Name:        poly_baseinv
*
* Description: Inversion of polynomial used for inversion
*              of element in Rq in NTT domain
*  
* Arguments:   - poly *b: pointer to the output polynomial
*              - const poly *a: pointer to the input polynomial
***************************************************/
int poly_baseinv(poly *b, const poly *a)
{
    return poly_baseinv_avx(b->coeffs, a->coeffs, qdata);
}

/*************************************************
* Name:        poly_add
*
* Description: Add two polynomials
*
* Arguments: - poly *r:       pointer to output polynomial
*            - const poly *a: pointer to first input polynomial
*            - const poly *b: pointer to second input polynomial
**************************************************/
void poly_add(poly *r, const poly *a, const poly *b)
{
    for (unsigned int i = 0; i < N; i += 16) {
        __m256i va = _mm256_load_si256((const __m256i *)(a->coeffs + i));
        __m256i vb = _mm256_load_si256((const __m256i *)(b->coeffs + i));
        __m256i vr = _mm256_add_epi16(va, vb);
        _mm256_store_si256((__m256i *)(r->coeffs + i), vr);
    }
}


/*************************************************
* Name:        poly_compress_and_pack
*
* Description: Compression and subsequent serialization of a polynomial
*
* Arguments:   - uint8_t *r: pointer to output byte array
*                            (of length KEM_POLYCOMPRESSEDBYTES)
*              - poly *a:    pointer to input polynomial
**************************************************/
void poly_compress_and_pack(uint8_t r[KEM_POLYCOMPRESSEDBYTES], poly *a)
{
    unsigned int i;
    
#if D == 7
    poly_freeze(a);
#endif

#if D == 7
    {
        const __m256i v32   = _mm256_set1_epi32(5585134);
        const __m256i half32 = _mm256_set1_epi32(Q / 2);
        uint16_t t[16] __attribute__((aligned(32)));

        for(i = 0; i < N/8; i++) {
            __m128i c16 = _mm_load_si128((const __m128i *)(a->coeffs + 8*i));
            __m256i c32 = _mm256_cvtepi16_epi32(c16);
            c32 = _mm256_add_epi32(_mm256_slli_epi32(c32, D), half32);

            __m256i c_odd = _mm256_srli_epi64(c32, 32);
            __m256i q_even = _mm256_srli_epi64(_mm256_mul_epu32(c32, v32), 32);
            __m256i q_odd  = _mm256_srli_epi64(_mm256_mul_epu32(c_odd, v32), 32);

            /* packus is lane-interleaved; permute fixes to sequential */
            __m256i packed = _mm256_packus_epi32(q_even, q_odd);
            packed = _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));
            _mm256_store_si256((__m256i *)t, packed);

            uint16_t v0 = t[0], v1 = t[8], v2 = t[2], v3 = t[10];
            uint16_t v4 = t[4], v5 = t[12], v6 = t[6], v7 = t[14];

            r[7*i+0] =  v0 | (v1 << 7);
            r[7*i+1] = (v1 >> 1) | (v2 << 6);
            r[7*i+2] = (v2 >> 2) | (v3 << 5);
            r[7*i+3] = (v3 >> 3) | (v4 << 4);
            r[7*i+4] = (v4 >> 4) | (v5 << 3);
            r[7*i+5] = (v5 >> 5) | (v6 << 2);
            r[7*i+6] = (v6 >> 6) | (v7 << 1);
        }
    }
#elif D == 8
    {
        const __m256i scale  = _mm256_set1_epi16(21789);
        const __m256i two    = _mm256_set1_epi16(2);
        const __m256i q      = _mm256_load_si256((const __m256i *)(qdata + _16XQ));
        const __m256i v      = _mm256_load_si256((const __m256i *)(qdata + _16XV));
        const __m256i mask8  = _mm256_set1_epi16(0x00FF);
        const __m256i zero   = _mm256_setzero_si256();

        for (i = 0; i < N; i += 16) {
            __m256i c = _mm256_load_si256((const __m256i *)(a->coeffs + i));
            /* Fuse canonicalization with compression to avoid another pass. */
            c = freeze_avx(c, q, v);
            _mm256_store_si256((__m256i *)(a->coeffs + i), c);

            /*
             * Exact for every canonical q=769 coefficient:
             * round(256*c/q) mod 256 = mulhi_u16(c + 2, 21789) mod 256.
             */
            __m256i packed = _mm256_mulhi_epu16(
                _mm256_add_epi16(c, two), scale);
            packed = _mm256_and_si256(packed, mask8);
            packed = _mm256_packus_epi16(packed, zero);
            packed = _mm256_permute4x64_epi64(
                packed, _MM_SHUFFLE(3, 1, 2, 0));
            _mm_storeu_si128((__m128i *)(r + i),
                             _mm256_castsi256_si128(packed));
        }
    }
#elif D == 9
    {
        const __m256i q = _mm256_load_si256((const __m256i *)(qdata + _16XQ));
        const __m256i v = _mm256_load_si256((const __m256i *)(qdata + _16XV));

        for (i = 0; i < N; i += 16) {
            __m256i c = _mm256_load_si256((const __m256i *)(a->coeffs + i));
            c = freeze_avx(c, q, v);
            _mm256_store_si256((__m256i *)(a->coeffs + i), c);
            compress9_8(r + (9 * i) / 8, _mm256_castsi256_si128(c));
            compress9_8(r + (9 * i) / 8 + 9,
                        _mm256_extracti128_si256(c, 1));
        }
    }
#endif
}

/*************************************************
* Name:        poly_unpack_and_decompress
*
* Description: De-serialization and subsequent decompression of a polynomial;
*              approximate inverse of poly_compress_and_pack
*
* Arguments:   - poly *r:          pointer to output polynomial
*              - const uint8_t *a: pointer to input byte array
*                                  (of length KEM_POLYCOMPRESSEDBYTES bytes)
**************************************************/
void poly_unpack_and_decompress(poly *r, const uint8_t a[KEM_POLYCOMPRESSEDBYTES])
{
    unsigned int i;
#if D == 7
    {
        const __m128i q32 = _mm_set1_epi32(Q);
        const __m128i half32 = _mm_set1_epi32(1 << (D-1));
        for (i = 0; i < N/8; i++) {
            uint16_t t[8] __attribute__((aligned(16)));
            t[0] = (a[7*i+0] & 0x7F);
            t[1] = ((a[7*i+0] >> 7) | ((a[7*i+1] & 0x3F) << 1));
            t[2] = ((a[7*i+1] >> 6) | ((a[7*i+2] & 0x1F) << 2));
            t[3] = ((a[7*i+2] >> 5) | ((a[7*i+3] & 0x0F) << 3));
            t[4] = ((a[7*i+3] >> 4) | ((a[7*i+4] & 0x07) << 4));
            t[5] = ((a[7*i+4] >> 3) | ((a[7*i+5] & 0x03) << 5));
            t[6] = ((a[7*i+5] >> 2) | ((a[7*i+6] & 0x01) << 6));
            t[7] = (a[7*i+6] >> 1);
            __m128i tv = _mm_load_si128((const __m128i *)t);
            __m128i t32_lo = _mm_cvtepi16_epi32(tv);
            __m128i t32_hi = _mm_cvtepi16_epi32(_mm_unpackhi_epi64(tv, tv));
            t32_lo = _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(t32_lo, q32), half32), D);
            t32_hi = _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(t32_hi, q32), half32), D);
            __m128i result = _mm_packus_epi32(t32_lo, t32_hi);
            _mm_store_si128((__m128i *)&r->coeffs[8*i], result);
        }
    }
#elif D == 8
    {
        const __m256i q16 = _mm256_set1_epi16(Q);
        for (i = 0; i < N; i += 16) {
            /* The ciphertext API does not promise 16-byte input alignment. */
            __m128i a8 = _mm_loadu_si128((const __m128i *)&a[i]);
            __m256i a16 = _mm256_slli_epi16(
                _mm256_cvtepu8_epi16(a8), 7);
            __m256i result = _mm256_mulhrs_epi16(a16, q16);
            _mm256_store_si256((__m256i *)&r->coeffs[i], result);
        }
    }
#elif D == 9
    {
        const __m128i q16 = _mm_set1_epi16(Q);
        const uint64_t lane_mask = 0x01FF01FF01FF01FFULL;
        for (i = 0; i < N/8; i++) {
            uint64_t packed;
            memcpy(&packed, a + 9*i, sizeof(packed));
            uint64_t upper = (packed >> 36) | ((uint64_t)a[9*i+8] << 28);
            __m128i t16 = _mm_set_epi64x(
                (int64_t)_pdep_u64(upper, lane_mask),
                (int64_t)_pdep_u64(packed, lane_mask));
            t16 = _mm_slli_epi16(t16, 6);
            __m128i result = _mm_mulhrs_epi16(t16, q16);
            _mm_store_si128((__m128i *)&r->coeffs[8*i], result);
        }
    }
#endif
}

/*************************************************
* Name:        poly_to_bytes
*
* Description: Serialization of a polynomial
*
* Arguments:   - uint8_t *r: pointer to output byte array
*                            (needs space for KEM_POLYBYTES bytes)
*              - poly *a:    pointer to input polynomial
**************************************************/
void poly_to_bytes(uint8_t r[KEM_POLYBYTES], const poly *a)
{
    unsigned int i;
#if KEM_MODE == 128
    {
        const int16_t *coeffs = a->coeffs;

        for (i = 0; i < 102; i += 2) {
            /* group A: coeffs[0..4], group B: coeffs[5..9] */
            uint16_t ca0 = (uint16_t)coeffs[5*i + 0];
            uint16_t ca1 = (uint16_t)coeffs[5*i + 1];
            uint16_t ca2 = (uint16_t)coeffs[5*i + 2];
            uint16_t ca3 = (uint16_t)coeffs[5*i + 3];
            uint16_t ca4 = (uint16_t)coeffs[5*i + 4];
            uint16_t cb0 = (uint16_t)coeffs[5*i + 5];
            uint16_t cb1 = (uint16_t)coeffs[5*i + 6];
            uint16_t cb2 = (uint16_t)coeffs[5*i + 7];
            uint16_t cb3 = (uint16_t)coeffs[5*i + 8];
            uint16_t cb4 = (uint16_t)coeffs[5*i + 9];

            /* L: interleaved compute */
            uint32_t L_a = (ca0 & 7) | ((ca1 & 7) << 3) | ((ca2 & 7) << 6) |
                           ((ca3 & 7) << 9) | ((ca4 & 7) << 12);
            uint32_t L_b = (cb0 & 7) | ((cb1 & 7) << 3) | ((cb2 & 7) << 6) |
                           ((cb3 & 7) << 9) | ((cb4 & 7) << 12);

            /* H: interleaved multiply-accumulate to hide latency */
            uint64_t ha0 = ca0 >> 3;
            uint64_t hb0 = cb0 >> 3;
            uint64_t ha1 = (ca1 >> 3) * 97ULL;
            uint64_t hb1 = (cb1 >> 3) * 97ULL;
            uint64_t H_a = ha0 + ha1;
            uint64_t H_b = hb0 + hb1;

            ha1 = (ca2 >> 3) * 9409ULL;
            hb1 = (cb2 >> 3) * 9409ULL;
            H_a += ha1; H_b += hb1;

            ha1 = (ca3 >> 3) * 912673ULL;
            hb1 = (cb3 >> 3) * 912673ULL;
            H_a += ha1; H_b += hb1;

            ha1 = (ca4 >> 3) * 88529281ULL;
            hb1 = (cb4 >> 3) * 88529281ULL;
            H_a += ha1; H_b += hb1;

            uint64_t code_a = (H_a << 15) | L_a;
            uint64_t code_b = (H_b << 15) | L_b;

            memcpy(r + 6*i,      &code_a, 6);
            memcpy(r + 6*i + 6,  &code_b, 6);
        }

        /* tail: 2 coefficients → 20 bits */
        {
            uint32_t t;
            t  = (uint32_t)(uint16_t)a->coeffs[510];
            t |= (uint32_t)(uint16_t)a->coeffs[511] << 10;
            memcpy(r + 612, &t, 3);
        }
    }
#elif KEM_MODE == 256
    {
        const int16_t *coeffs = a->coeffs;

        for (i = 0; i < 204; i += 2) {
            uint16_t ca0 = (uint16_t)coeffs[5*i + 0];
            uint16_t ca1 = (uint16_t)coeffs[5*i + 1];
            uint16_t ca2 = (uint16_t)coeffs[5*i + 2];
            uint16_t ca3 = (uint16_t)coeffs[5*i + 3];
            uint16_t ca4 = (uint16_t)coeffs[5*i + 4];
            uint16_t cb0 = (uint16_t)coeffs[5*i + 5];
            uint16_t cb1 = (uint16_t)coeffs[5*i + 6];
            uint16_t cb2 = (uint16_t)coeffs[5*i + 7];
            uint16_t cb3 = (uint16_t)coeffs[5*i + 8];
            uint16_t cb4 = (uint16_t)coeffs[5*i + 9];

            uint32_t L_a = (ca0 & 7) | ((ca1 & 7) << 3) | ((ca2 & 7) << 6) |
                           ((ca3 & 7) << 9) | ((ca4 & 7) << 12);
            uint32_t L_b = (cb0 & 7) | ((cb1 & 7) << 3) | ((cb2 & 7) << 6) |
                           ((cb3 & 7) << 9) | ((cb4 & 7) << 12);

            uint64_t ha0 = ca0 >> 3;
            uint64_t hb0 = cb0 >> 3;
            uint64_t ha1 = (ca1 >> 3) * 97ULL;
            uint64_t hb1 = (cb1 >> 3) * 97ULL;
            uint64_t H_a = ha0 + ha1;
            uint64_t H_b = hb0 + hb1;

            ha1 = (ca2 >> 3) * 9409ULL;
            hb1 = (cb2 >> 3) * 9409ULL;
            H_a += ha1; H_b += hb1;

            ha1 = (ca3 >> 3) * 912673ULL;
            hb1 = (cb3 >> 3) * 912673ULL;
            H_a += ha1; H_b += hb1;

            ha1 = (ca4 >> 3) * 88529281ULL;
            hb1 = (cb4 >> 3) * 88529281ULL;
            H_a += ha1; H_b += hb1;

            uint64_t code_a = (H_a << 15) | L_a;
            uint64_t code_b = (H_b << 15) | L_b;

            memcpy(r + 6*i,      &code_a, 6);
            memcpy(r + 6*i + 6,  &code_b, 6);
        }

        /* tail: 4 coefficients → 40 bits */
        {
            uint64_t t;
            t  = (uint64_t)(uint16_t)a->coeffs[1020];
            t |= (uint64_t)(uint16_t)a->coeffs[1021] << 10;
            t |= (uint64_t)(uint16_t)a->coeffs[1022] << 20;
            t |= (uint64_t)(uint16_t)a->coeffs[1023] << 30;
            memcpy(r + 1224, &t, 5);
        }
    }

#elif KEM_MODE == 512
    {
        uint16_t c0, c1;

        for (i = 0; i < N / 2; i++) {
            c0 = (uint16_t)a->coeffs[2*i + 0];
            c1 = (uint16_t)a->coeffs[2*i + 1];

            r[3*i + 0] = (uint8_t)(c0);
            r[3*i + 1] = (uint8_t)((c0 >> 8) | (c1 << 4));
            r[3*i + 2] = (uint8_t)(c1 >> 4);
        }
    }
#else
    #error "Unsupported KEM_MODE: must be 128, 256, or 512"
#endif
}

/*************************************************
* Name:        poly_from_bytes
*
* Description: De-serialization of a polynomial;
*              inverse of poly_to_bytes
*
* Arguments:   - poly *r:          pointer to output polynomial
*              - const uint8_t *a: pointer to input byte array
*                                  (of KEM_POLYBYTES bytes)
**************************************************/
void poly_from_bytes(poly *r, const uint8_t a[KEM_POLYBYTES])
{
    unsigned int i;

#if KEM_MODE == 128
    {
        int16_t *coeffs = r->coeffs;

        for (i = 0; i < 102; i += 2) {
            uint64_t code0, code1;
            memcpy(&code0, a + 6*i, 8);
            memcpy(&code1, a + 6*i + 6, 8);
            code0 &= 0xFFFFFFFFFFFFULL;
            code1 &= 0xFFFFFFFFFFFFULL;

            uint32_t L0 = (uint32_t)(code0 & 0x7FFF);
            uint64_t H0 = code0 >> 15;
            uint32_t L1 = (uint32_t)(code1 & 0x7FFF);
            uint64_t H1 = code1 >> 15;

            /* j=0: 64-bit division for both groups */
            uint16_t l00 = L0 & 7;
            uint16_t h00;
            H0 = divmod97(H0, &h00);
            uint16_t l10 = L1 & 7;
            uint16_t h10;
            H1 = divmod97(H1, &h10);
            coeffs[5*i + 0] = (int16_t)((h00 << 3) | l00);
            coeffs[5*i + 5] = (int16_t)((h10 << 3) | l10);

            /* j=1..4: H < 2^27, use 32-bit division */
            uint32_t H0_32 = (uint32_t)H0;
            uint32_t H1_32 = (uint32_t)H1;

            /* j=1 */
            uint16_t l01 = (L0 >> 3) & 7;
            uint16_t h01;
            H0_32 = (uint32_t)divmod97(H0_32, &h01);
            uint16_t l11 = (L1 >> 3) & 7;
            uint16_t h11;
            H1_32 = (uint32_t)divmod97(H1_32, &h11);
            coeffs[5*i + 1] = (int16_t)((h01 << 3) | l01);
            coeffs[5*i + 6] = (int16_t)((h11 << 3) | l11);

            /* j=2 */
            uint16_t l02 = (L0 >> 6) & 7;
            uint16_t h02;
            H0_32 = (uint32_t)divmod97(H0_32, &h02);
            uint16_t l12 = (L1 >> 6) & 7;
            uint16_t h12;
            H1_32 = (uint32_t)divmod97(H1_32, &h12);
            coeffs[5*i + 2] = (int16_t)((h02 << 3) | l02);
            coeffs[5*i + 7] = (int16_t)((h12 << 3) | l12);

            /* j=3 */
            uint16_t l03 = (L0 >> 9) & 7;
            uint16_t h03;
            H0_32 = (uint32_t)divmod97(H0_32, &h03);
            uint16_t l13 = (L1 >> 9) & 7;
            uint16_t h13;
            H1_32 = (uint32_t)divmod97(H1_32, &h13);
            coeffs[5*i + 3] = (int16_t)((h03 << 3) | l03);
            coeffs[5*i + 8] = (int16_t)((h13 << 3) | l13);

            /* j=4 */
            uint16_t l04 = (L0 >> 12) & 7;
            uint16_t h04;
            (void)divmod97(H0_32, &h04);
            uint16_t l14 = (L1 >> 12) & 7;
            uint16_t h14;
            (void)divmod97(H1_32, &h14);
            coeffs[5*i + 4] = (int16_t)((h04 << 3) | l04);
            coeffs[5*i + 9] = (int16_t)((h14 << 3) | l14);
        }

        {
            uint32_t t;
            t  = (uint32_t)a[612];
            t |= (uint32_t)a[613] <<  8;
            t |= (uint32_t)a[614] << 16;
            r->coeffs[510] = (int16_t)( t        & 0x3FF);
            r->coeffs[511] = (int16_t)((t >> 10) & 0x3FF);
        }
    }
#elif KEM_MODE == 256
    {
        int16_t *coeffs = r->coeffs;

        for (i = 0; i < 204; i += 2) {
            uint64_t code0, code1;
            memcpy(&code0, a + 6*i, 8);
            memcpy(&code1, a + 6*i + 6, 8);
            code0 &= 0xFFFFFFFFFFFFULL;
            code1 &= 0xFFFFFFFFFFFFULL;

            uint32_t L0 = (uint32_t)(code0 & 0x7FFF);
            uint64_t H0 = code0 >> 15;
            uint32_t L1 = (uint32_t)(code1 & 0x7FFF);
            uint64_t H1 = code1 >> 15;

            /* j=0: 64-bit division for both groups */
            uint16_t l00 = L0 & 7;
            uint16_t h00;
            H0 = divmod97(H0, &h00);
            uint16_t l10 = L1 & 7;
            uint16_t h10;
            H1 = divmod97(H1, &h10);
            coeffs[5*i + 0] = (int16_t)((h00 << 3) | l00);
            coeffs[5*i + 5] = (int16_t)((h10 << 3) | l10);

            /* j=1..4: H < 2^27, 32-bit division */
            uint32_t H0_32 = (uint32_t)H0;
            uint32_t H1_32 = (uint32_t)H1;

            /* j=1 */
            uint16_t l01 = (L0 >> 3) & 7;
            uint16_t h01;
            H0_32 = (uint32_t)divmod97(H0_32, &h01);
            uint16_t l11 = (L1 >> 3) & 7;
            uint16_t h11;
            H1_32 = (uint32_t)divmod97(H1_32, &h11);
            coeffs[5*i + 1] = (int16_t)((h01 << 3) | l01);
            coeffs[5*i + 6] = (int16_t)((h11 << 3) | l11);

            /* j=2 */
            uint16_t l02 = (L0 >> 6) & 7;
            uint16_t h02;
            H0_32 = (uint32_t)divmod97(H0_32, &h02);
            uint16_t l12 = (L1 >> 6) & 7;
            uint16_t h12;
            H1_32 = (uint32_t)divmod97(H1_32, &h12);
            coeffs[5*i + 2] = (int16_t)((h02 << 3) | l02);
            coeffs[5*i + 7] = (int16_t)((h12 << 3) | l12);

            /* j=3 */
            uint16_t l03 = (L0 >> 9) & 7;
            uint16_t h03;
            H0_32 = (uint32_t)divmod97(H0_32, &h03);
            uint16_t l13 = (L1 >> 9) & 7;
            uint16_t h13;
            H1_32 = (uint32_t)divmod97(H1_32, &h13);
            coeffs[5*i + 3] = (int16_t)((h03 << 3) | l03);
            coeffs[5*i + 8] = (int16_t)((h13 << 3) | l13);

            /* j=4 */
            uint16_t l04 = (L0 >> 12) & 7;
            uint16_t h04;
            (void)divmod97(H0_32, &h04);
            uint16_t l14 = (L1 >> 12) & 7;
            uint16_t h14;
            (void)divmod97(H1_32, &h14);
            coeffs[5*i + 4] = (int16_t)((h04 << 3) | l04);
            coeffs[5*i + 9] = (int16_t)((h14 << 3) | l14);
        }

        {
            uint64_t t;
            t  = (uint64_t)a[1224];
            t |= (uint64_t)a[1225] <<  8;
            t |= (uint64_t)a[1226] << 16;
            t |= (uint64_t)a[1227] << 24;
            t |= (uint64_t)a[1228] << 32;
            r->coeffs[1020] = (int16_t)( t        & 0x3FF);
            r->coeffs[1021] = (int16_t)((t >> 10) & 0x3FF);
            r->coeffs[1022] = (int16_t)((t >> 20) & 0x3FF);
            r->coeffs[1023] = (int16_t)((t >> 30) & 0x3FF);
        }
    }
#elif KEM_MODE == 512
    for (i = 0; i < N / 2; i++) {
        r->coeffs[2*i + 0] = (int16_t)( (uint16_t) a[3*i + 0]             |
                                        ((uint16_t)(a[3*i + 1] & 0x0F) << 8));
        r->coeffs[2*i + 1] = (int16_t)(((uint16_t) a[3*i + 1] >> 4)       |
                                        ((uint16_t) a[3*i + 2]        << 4));
    }

#else
    #error "Unsupported KEM_MODE"
#endif
}

/*************************************************
* Name:        poly_from_msg
*
* Description: Convert message to polynomial
*
* Arguments:   - poly *r:            pointer to output polynomial
*              - const uint8_t *msg: pointer to input message
**************************************************/
void poly_from_msg(poly *r, const uint8_t msg[KEM_CPAPKE_MSGBYTES])
{
    unsigned int i;
    const __m256i hq = _mm256_set1_epi16(HALF_Q);
    const __m256i z  = _mm256_setzero_si256();

#if REPETITIONS == 2
    for (i = 0; i < KEM_CPAPKE_MSGBYTES; i += 2) {
        uint64_t lo = _pdep_u64(msg[i],     0x0101010101010101ULL);
        uint64_t hi = _pdep_u64(msg[i + 1], 0x0101010101010101ULL);
        __m128i b8  = _mm_set_epi64x(hi, lo);
        __m256i b16 = _mm256_cvtepu8_epi16(b8);
        __m256i val = _mm256_and_si256(_mm256_cmpgt_epi16(b16, z), hq);
        _mm256_store_si256((__m256i *)&r->coeffs[8*i], val);
        _mm256_store_si256((__m256i *)&r->coeffs[8*i + N/2], val);
    }
#elif REPETITIONS == 4
    for (i = 0; i < KEM_CPAPKE_MSGBYTES; i += 2) {
        uint64_t lo = _pdep_u64(msg[i],     0x0101010101010101ULL);
        uint64_t hi = _pdep_u64(msg[i + 1], 0x0101010101010101ULL);
        __m128i b8  = _mm_set_epi64x(hi, lo);
        __m256i b16 = _mm256_cvtepu8_epi16(b8);
        __m256i val = _mm256_and_si256(_mm256_cmpgt_epi16(b16, z), hq);
        _mm256_store_si256((__m256i *)&r->coeffs[8*i], val);
        _mm256_store_si256((__m256i *)&r->coeffs[8*i + N/4], val);
        _mm256_store_si256((__m256i *)&r->coeffs[8*i + N/2], val);
        _mm256_store_si256((__m256i *)&r->coeffs[8*i + 3*N/4], val);
    }
#endif
}

/*************************************************
* Name:        ternary_p
*
* Description: Sample a polynomial with coefficients in {-1,0,1} according to probabilities
*              P(-1) = P(1) = P, P(0) = 1 - 2*P
*
* Arguments:   - poly *r:            pointer to output polynomial
*              - uint8_t *buf:        pointer to input buffer for randomness
*                                      (needs to have enough bytes to sample N coefficients)
**************************************************/
static void ternary_15625(poly *r, const uint8_t *buf)  // 5/32
{
    unsigned int i, j, k;

    for (i = 0; i < N/8; i++) {
        uint64_t w = 0;

        for (j = 0; j < 5; j++)
        {
            w |= (uint64_t)buf[5*i + j] << (8*j);
        }

        for (k = 0; k < 8; k++) {
            uint16_t v = (w >> (5*k)) & 0x1F;
            uint16_t lt5 = (v - 5) >> 15;
            uint16_t lt10 = (v - 10) >> 15;
            r->coeffs[8*i + k] = (int16_t)lt5 - (int16_t)((lt5 ^ 1) & lt10);
        }
    }
}
static inline void ternary_nibbles(poly *r, const uint8_t *buf,
                                   const __m128i lut)
{
    unsigned int i;
    const __m128i nibble_mask = _mm_set1_epi8(0x0F);

    for (i = 0; i < N / 2; i += 16) {
        __m128i w  = _mm_loadu_si128((const __m128i *)(buf + i));
        __m128i lo = _mm_and_si128(w, nibble_mask);
        __m128i hi = _mm_and_si128(_mm_srli_epi16(w, 4), nibble_mask);
        __m128i x0 = _mm_unpacklo_epi8(lo, hi);
        __m128i x1 = _mm_unpackhi_epi8(lo, hi);
        x0 = _mm_shuffle_epi8(lut, x0);
        x1 = _mm_shuffle_epi8(lut, x1);
        _mm256_store_si256((__m256i *)&r->coeffs[2 * i],
                           _mm256_cvtepi8_epi16(x0));
        _mm256_store_si256((__m256i *)&r->coeffs[2 * i + 16],
                           _mm256_cvtepi8_epi16(x1));
    }
}

static void ternary_3125(poly *r, const uint8_t *buf) // 5/16
{
    const __m128i lut = _mm_setr_epi8(
         1,  1,  1,  1,  1, -1, -1, -1,
        -1, -1,  0,  0,  0,  0,  0,  0);
    ternary_nibbles(r, buf, lut);
}

static void ternary_4375(poly *r, const uint8_t *buf) // 7/16
{
    const __m128i lut = _mm_setr_epi8(
         1,  1,  1,  1,  1,  1,  1, -1,
        -1, -1, -1, -1, -1, -1,  0,  0);
    ternary_nibbles(r, buf, lut);
}
static void ternary_25(poly *r, const uint8_t *buf) // 1/4
{
    const __m128i one = _mm_set1_epi8(1);

    for (unsigned int i = 0; i < N / 4; i += 16) {
        __m128i w = _mm_loadu_si128((const __m128i *)(buf + i));
        __m128i c0 = _mm_sub_epi8(
            _mm_and_si128(w, one),
            _mm_and_si128(_mm_srli_epi16(w, 1), one));
        __m128i c1 = _mm_sub_epi8(
            _mm_and_si128(_mm_srli_epi16(w, 2), one),
            _mm_and_si128(_mm_srli_epi16(w, 3), one));
        __m128i c2 = _mm_sub_epi8(
            _mm_and_si128(_mm_srli_epi16(w, 4), one),
            _mm_and_si128(_mm_srli_epi16(w, 5), one));
        __m128i c3 = _mm_sub_epi8(
            _mm_and_si128(_mm_srli_epi16(w, 6), one),
            _mm_and_si128(_mm_srli_epi16(w, 7), one));

        __m128i a0 = _mm_unpacklo_epi8(c0, c1);
        __m128i a1 = _mm_unpackhi_epi8(c0, c1);
        __m128i b0 = _mm_unpacklo_epi8(c2, c3);
        __m128i b1 = _mm_unpackhi_epi8(c2, c3);
        __m128i x0 = _mm_unpacklo_epi16(a0, b0);
        __m128i x1 = _mm_unpackhi_epi16(a0, b0);
        __m128i x2 = _mm_unpacklo_epi16(a1, b1);
        __m128i x3 = _mm_unpackhi_epi16(a1, b1);

        _mm256_store_si256((__m256i *)&r->coeffs[4 * i],
                           _mm256_cvtepi8_epi16(x0));
        _mm256_store_si256((__m256i *)&r->coeffs[4 * i + 16],
                           _mm256_cvtepi8_epi16(x1));
        _mm256_store_si256((__m256i *)&r->coeffs[4 * i + 32],
                           _mm256_cvtepi8_epi16(x2));
        _mm256_store_si256((__m256i *)&r->coeffs[4 * i + 48],
                           _mm256_cvtepi8_epi16(x3));
    }
}
static void ternary_125(poly *r, const uint8_t *buf) // 1/8
{
    unsigned int i,j;

    for (i = 0; i < N/8; i++) {
        uint32_t w = (uint32_t)buf[3*i] | ((uint32_t)buf[3*i+1] << 8) | ((uint32_t)buf[3*i+2] << 16);

        for (j = 0; j < 8; j++) {
            uint16_t v = (w >> (3*j)) & 0x07;
            uint16_t lt3 = (uint16_t)(v - 1) >> 15;
            uint16_t lt6 = (uint16_t)(v - 2) >> 15;
            r->coeffs[8*i + j] = (int16_t)((2*lt3 - 1) * lt6);
        }
    }
}
/*************************************************
* Name:        poly_ternary_p
*
* Description: Dispatch ternary polynomial sampling based on probability.
*              Wrappers pass compile-time constants (PF/PG/PR/PE), so the
*              switch is constant-folded by the optimizer.
*
* Arguments:   - poly *r:            pointer to output polynomial
*              - const uint8_t *seed: pointer to input seed
*              - uint8_t nonce:       domain-separation nonce
*              - int prob:            one of {125, 3125, 25, 15625, 4375}
**************************************************/
static void poly_ternary_p(poly *r, const uint8_t seed[SEEDBYTES],
                            uint8_t nonce, int prob)
{
    switch (prob) {
    case 125: {   // 1/8
        uint8_t buf[N * 3/8];
        prf(buf, sizeof(buf), seed, nonce);
        ternary_125(r, buf);
        break;
    }
    case 3125: {  // 5/16
        uint8_t buf[N * 4/8];
        prf(buf, sizeof(buf), seed, nonce);
        ternary_3125(r, buf);
        break;
    }
    case 25: {    // 1/4
        uint8_t buf[N * 2/8];
        prf(buf, sizeof(buf), seed, nonce);
        ternary_25(r, buf);
        break;
    }
    case 15625: { // 5/32
        uint8_t buf[N * 5/8];
        prf(buf, sizeof(buf), seed, nonce);
        ternary_15625(r, buf);
        break;
    }
    case 4375: {  // 7/16
        uint8_t buf[N * 4/8];
        prf(buf, sizeof(buf), seed, nonce);
        ternary_4375(r, buf);
        break;
    }
    }
}

void poly_f_ternary_p(poly *r, const uint8_t seed[SEEDBYTES], uint8_t nonce) {
    poly_ternary_p(r, seed, nonce, PF);
}
void poly_g_ternary_p(poly *r, const uint8_t seed[SEEDBYTES], uint8_t nonce) {
    poly_ternary_p(r, seed, nonce, PG);
}
void poly_r_ternary_p(poly *r, const uint8_t seed[SEEDBYTES], uint8_t nonce) {
    poly_ternary_p(r, seed, nonce, PR);
}
void poly_e_ternary_p(poly *r, const uint8_t seed[SEEDBYTES], uint8_t nonce) {
    poly_ternary_p(r, seed, nonce, PE);
}
