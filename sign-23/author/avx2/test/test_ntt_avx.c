/*
 * test_ntt_avx.c -- AVX2 backend correctness for the SHUTTLE NTT shim.
 *
 * Built per mode with the AVX2 flags, linking the per-mode vendored asm:
 *   gcc -mavx2 ... -DSHUTTLE_MODE=<m> -DUSE_AVX2_NTT
 *     avx2/test/test_ntt_avx.c avx2/poly_ntt.c ref/reduce.c
 *     ref/ntt/<qset>/ntt_ref.c avx2/<qset>/ntt.S avx2/<qset>/ntt_consts.c
 *
 * Asserts (fails=0):
 *   [1] AVX2 round-trip via the shim:
 * poly_invntt_tomont_simd(poly_ntt_simd(a)) == a*R (and output is [0,q)
 * after the signed canonicalization). [2] negacyclic product via the AVX2
 * shim == polymul_schoolbook. [3] nttunpack(ntt_ref(p)) == ntt_avx(p)
 * (mod q) -- the canonical-> backend-native reconciliation, via
 * poly_ntt_simd_import(poly_ntt_canonical)
 *       == poly_ntt.
 *   [4] full-layer ref == avx2 byte-exact -- the SCALAR poly_ntt then
 *       poly_invntt_tomont equals the AVX2 result
 * coefficient-by-coefficient in [0,q) (random + edge: all-0, all-(q-1),
 * alternating).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "params.h"
#include "poly_ntt.h"

/* The genuine AVX2 SIMD NTT kernels, exported by avx2/poly_ntt.c for this
 * byte-exactness validation.  The scheme-facing shim (poly_ntt /
 * poly_invntt_tomont / poly_pointwise_montgomery / poly_ntt_import) routes
 * to the SCALAR oracle so the integrated KAT is byte-exact to ref; the
 * SIMD asm is exercised HERE through these *_simd_* entries and proven
 * bit-identical to that scalar oracle (which authorizes the perf wiring
 * of the SIMD path into a forked polyvec/sign). */
void poly_ntt_simd(poly16 *a);
void poly_invntt_tomont_simd(poly16 *a);
void poly_pointwise_montgomery_simd(poly16 *c, const poly16 *a,
                                    const poly16 *b);
void poly_ntt_simd_import(poly16 *a);

#define MONT16 ((uint16_t)((1u << 16) % Q))
static uint16_t U(uint16_t x)
{
    return (uint16_t)(x % Q);
}

/* PER-CONFIG NTT-domain readback: a RAW NTT-domain lane (output of
 * poly_ntt / nttunpack, NOT yet canonicalized by poly_invntt_tomont) is a
 * TRUE signed int16 for the signed q15361 config (smod), but a
 * raw-bits-unsigned value for the unsigned valley configs ((uint16_t)x % q
 * -- the q>2^15 trap). */
static uint16_t rb(uint16_t raw)
{
#if SHUTTLE_NTT_SIGNED
    int r = (int)(int16_t)raw % Q;
    return (uint16_t)(r < 0 ? r + Q : r);
#else
    return (uint16_t)(raw % Q);
#endif
}

/* ---- a private SCALAR reference of the shim, to cross-check the AVX2
 * shim (the linked poly_ntt.c is the AVX2 fork; we call the s<n>_*_ref
 * kernels directly for the scalar oracle). ---- */
static void ref_ntt(poly16 *a)
{
    ntt_ref(a->coeffs);
}
static void ref_inv(poly16 *a)
{
    invntt_tomont_ref(a->coeffs);
}
static void ref_pw(poly16 *c, const poly16 *a, const poly16 *b)
{
    pointwise_ref(a->coeffs, b->coeffs, c->coeffs);
}

int main(void)
{
    srand(20260624);
    ntt_ref_init();

    /* [1] AVX2 round-trip */
    int f1 = 0;
    for (int it = 0; it < 5000; it++) {
        poly16 a, ref;
        for (int i = 0; i < N; i++) {
            uint16_t v = (uint16_t)(rand() % Q);
            a.coeffs[i] = v;
            ref.coeffs[i] = (uint16_t)((uint32_t)v * MONT16 % Q);
        }
        poly_ntt_simd(&a);
        poly_invntt_tomont_simd(&a);
        for (int i = 0; i < N; i++)
            if (a.coeffs[i] >= (uint16_t)Q ||
                a.coeffs[i] != ref.coeffs[i]) {
                if (f1 < 4)
                    printf("  [rt] it=%d i=%d got=%u exp=%u\n", it, i,
                           a.coeffs[i], ref.coeffs[i]);
                f1++;
                break;
            }
    }
    printf(
        "[1] AVX2 round-trip invntt_tomont(ntt(a)) == a*R in [0,q), 5000: "
        "%s (%d)\n",
        f1 ? "FAIL" : "PASS", f1);

    /* [2] AVX2 negacyclic product == schoolbook */
    int f2 = 0;
    for (int it = 0; it < 2000; it++) {
        poly16 a, b, na, nb, c;
        uint16_t cref[N];
        for (int i = 0; i < N; i++) {
            a.coeffs[i] = (uint16_t)(rand() % Q);
            b.coeffs[i] = (uint16_t)(rand() % Q);
            na.coeffs[i] = a.coeffs[i];
            nb.coeffs[i] = b.coeffs[i];
        }
        poly_ntt_simd(&na);
        poly_ntt_simd(&nb);
        poly_pointwise_montgomery_simd(&c, &na, &nb);
        poly_invntt_tomont_simd(&c);
        polymul_schoolbook(a.coeffs, b.coeffs, cref);
        for (int i = 0; i < N; i++)
            if (c.coeffs[i] != cref[i]) {
                if (f2 < 4)
                    printf("  [mul] it=%d i=%d got=%u exp=%u\n", it, i,
                           c.coeffs[i], cref[i]);
                f2++;
                break;
            }
    }
    printf("[2] AVX2 negacyclic product == schoolbook, 2000: %s (%d)\n",
           f2 ? "FAIL" : "PASS", f2);

    /* [3] nttunpack(ntt_ref(p)) == ntt_avx(p).  poly_ntt_canonical =
     * scalar ntt_ref order; poly_ntt_import = nttunpack into AVX2 layout;
     * poly_ntt = the AVX2 forward.  They must agree mod q
     * coefficient-by-coefficient. */
    int f3 = 0;
    for (int it = 0; it < 5000; it++) {
        poly16 p, want;
        for (int i = 0; i < N; i++) {
            uint16_t v = (uint16_t)(rand() % Q);
            p.coeffs[i] = v;
            want.coeffs[i] = v;
        }
        poly_ntt_canonical(&p);
        poly_ntt_simd_import(&p); /* AVX2 nttunpack */
        poly_ntt_simd(&want);     /* AVX2 forward */
        /* RAW NTT-domain lanes: read back per-config (signed smod /
         * unsigned). */
        for (int i = 0; i < N; i++)
            if (rb(p.coeffs[i]) != rb(want.coeffs[i])) {
                if (f3 < 4)
                    printf("  [unpack] it=%d i=%d got=%u exp=%u\n", it, i,
                           rb(p.coeffs[i]), rb(want.coeffs[i]));
                f3++;
                break;
            }
    }
    printf("[3] nttunpack(ntt_ref(p)) == ntt_avx(p), 5000: %s (%d)\n",
           f3 ? "FAIL" : "PASS", f3);

    /* [4] full-layer scalar == AVX2 byte-exact in [0,q), random +
     * edge. */
    int f4 = 0;
    int edges = 4;
    for (int it = 0; it < 3000 + edges; it++) {
        poly16 a, sa;
        for (int i = 0; i < N; i++) {
            uint16_t v;
            if (it == 3000)
                v = 0;
            else if (it == 3001)
                v = (uint16_t)(Q - 1);
            else if (it == 3002)
                v = (uint16_t)(i & 1 ? Q - 1 : 0);
            else if (it == 3003)
                v = (uint16_t)((i * 37 + 11) % Q);
            else
                v = (uint16_t)(rand() % Q);
            a.coeffs[i] = v;
            sa.coeffs[i] = v;
        }
        /* AVX2 path */
        poly_ntt_simd(&a);
        poly_invntt_tomont_simd(&a);
        /* scalar oracle (already [0,q)) */
        ref_ntt(&sa);
        ref_inv(&sa);
        for (int i = 0; i < N; i++)
            if (a.coeffs[i] != U(sa.coeffs[i])) {
                if (f4 < 6)
                    printf("  [eq] it=%d i=%d avx2=%u scalar=%u\n", it, i,
                           a.coeffs[i], U(sa.coeffs[i]));
                f4++;
                break;
            }
    }
    printf("[4] scalar==AVX2 byte-exact (round-trip), %d: %s (%d)\n",
           3000 + edges, f4 ? "FAIL" : "PASS", f4);

    /* [4b] pointwise layer scalar == AVX2 (in matching NTT orders):
     * compare the full product output, which is order-agnostic. */
    int f5 = 0;
    for (int it = 0; it < 2000; it++) {
        poly16 na, nb, c, sa, sb, sc;
        for (int i = 0; i < N; i++) {
            uint16_t v = (uint16_t)(rand() % Q),
                     w = (uint16_t)(rand() % Q);
            na.coeffs[i] = v;
            nb.coeffs[i] = w;
            sa.coeffs[i] = v;
            sb.coeffs[i] = w;
        }
        poly_ntt_simd(&na);
        poly_ntt_simd(&nb);
        poly_pointwise_montgomery_simd(&c, &na, &nb);
        poly_invntt_tomont_simd(&c);
        ref_ntt(&sa);
        ref_ntt(&sb);
        ref_pw(&sc, &sa, &sb);
        ref_inv(&sc);
        for (int i = 0; i < N; i++)
            if (c.coeffs[i] != U(sc.coeffs[i])) {
                f5++;
                break;
            }
    }
    printf(
        "[4b] scalar==AVX2 byte-exact (full product), 2000: %s (%d)\n",
        f5 ? "FAIL" : "PASS", f5);

    int fails = f1 + f2 + f3 + f4 + f5;
    printf("\nSUMMARY test_ntt_avx (SHUTTLE-%d, AVX2) fails=%d\n",
           SHUTTLE_MODE, fails);
    return fails ? 1 : 0;
}
