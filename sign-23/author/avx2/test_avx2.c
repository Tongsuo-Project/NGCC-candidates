/*
 * Correctness + performance test for the 8-way AVX2 SM3 XOF.
 *
 * Ground truth is the scalar reference (SHUTTLE/ref/auxfunc.c): for every
 * test vector we run the reference pseudoXOF()/sm3hash() once per lane and
 * require the 8-way result to be byte-identical.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>

#include "auxfunc.h"      /* reference: pseudoXOF, sm3hash */
#include "auxfunc_avx2.h" /* 8-way:     pseudoXOF_avx2, sm3hash_avx2 */

#define NWAY 8

/* deterministic xorshift so failures reproduce */
static uint64_t rng_state = 0x123456789abcdef0ULL;
static unsigned char rb(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (unsigned char)(rng_state >> 24);
}

static int test_xof(unsigned long long msg_len_bits,
                    unsigned long long out_len_bits)
{
    size_t mbytes = (size_t)((msg_len_bits + 7) / 8);
    size_t obytes = (size_t)((out_len_bits + 7) / 8);
    unsigned char *msg[NWAY], *out_ref[NWAY], *out_avx[NWAY];
    const unsigned char *cmsg[NWAY];
    unsigned char *omsg[NWAY];

    for (int k = 0; k < NWAY; k++) {
        msg[k] = malloc(mbytes ? mbytes : 1);
        out_ref[k] = malloc(obytes ? obytes : 1);
        out_avx[k] = malloc(obytes ? obytes : 1);
        for (size_t i = 0; i < mbytes; i++)
            msg[k][i] = rb();
        memset(out_ref[k], 0xAA, obytes);
        memset(out_avx[k], 0x55, obytes);
        cmsg[k] = msg[k];
        omsg[k] = out_avx[k];
    }

    for (int k = 0; k < NWAY; k++)
        pseudoXOF(out_len_bits, msg[k], msg_len_bits, out_ref[k]);
    pseudoXOF_avx2(out_len_bits, cmsg, msg_len_bits, omsg);

    int ok = 1;
    for (int k = 0; k < NWAY && ok; k++)
        if (memcmp(out_ref[k], out_avx[k], obytes) != 0)
            ok = 0;

    for (int k = 0; k < NWAY; k++) {
        free(msg[k]);
        free(out_ref[k]);
        free(out_avx[k]);
    }
    if (!ok)
        printf("  [FAIL] XOF msg_len_bits=%llu out_len_bits=%llu\n",
               msg_len_bits, out_len_bits);
    return ok;
}

static int test_hash(unsigned long long msg_len_bits)
{
    size_t mbytes = (size_t)((msg_len_bits + 7) / 8);
    unsigned char *msg[NWAY], dref[NWAY][32], davx[NWAY][32];
    const unsigned char *cmsg[NWAY];
    unsigned char *cd[NWAY];
    for (int k = 0; k < NWAY; k++) {
        msg[k] = malloc(mbytes ? mbytes : 1);
        for (size_t i = 0; i < mbytes; i++)
            msg[k][i] = rb();
        cmsg[k] = msg[k];
        cd[k] = davx[k];
    }
    for (int k = 0; k < NWAY; k++)
        sm3hash(256, msg[k], msg_len_bits, dref[k]);
    sm3hash_avx2(cmsg, msg_len_bits, cd);

    int ok = 1;
    for (int k = 0; k < NWAY && ok; k++)
        if (memcmp(dref[k], davx[k], 32) != 0)
            ok = 0;
    for (int k = 0; k < NWAY; k++)
        free(msg[k]);
    if (!ok)
        printf("  [FAIL] HASH msg_len_bits=%llu\n", msg_len_bits);
    return ok;
}

static void bench(unsigned long long msg_len_bits,
                  unsigned long long out_len_bits, int iters)
{
    size_t mbytes = (size_t)((msg_len_bits + 7) / 8);
    size_t obytes = (size_t)((out_len_bits + 7) / 8);
    unsigned char *msg[NWAY], *out[NWAY];
    const unsigned char *cmsg[NWAY];
    unsigned char *omsg[NWAY];
    for (int k = 0; k < NWAY; k++) {
        msg[k] = malloc(mbytes ? mbytes : 1);
        out[k] = malloc(obytes ? obytes : 1);
        for (size_t i = 0; i < mbytes; i++)
            msg[k][i] = rb();
        cmsg[k] = msg[k];
        omsg[k] = out[k];
    }
    /* warm up */
    pseudoXOF_avx2(out_len_bits, cmsg, msg_len_bits, omsg);
    for (int k = 0; k < NWAY; k++)
        pseudoXOF(out_len_bits, msg[k], msg_len_bits, out[k]);

    unsigned long long t0, t1, ref_c, avx_c;
    t0 = __rdtsc();
    for (int it = 0; it < iters; it++)
        for (int k = 0; k < NWAY; k++)
            pseudoXOF(out_len_bits, msg[k], msg_len_bits, out[k]);
    t1 = __rdtsc();
    ref_c = (t1 - t0) / iters;

    t0 = __rdtsc();
    for (int it = 0; it < iters; it++)
        pseudoXOF_avx2(out_len_bits, cmsg, msg_len_bits, omsg);
    t1 = __rdtsc();
    avx_c = (t1 - t0) / iters;

    printf(
        "  msg=%5llub out=%6llub : ref(8x)=%8llu cyc  avx2(8-way)=%8llu "
        "cyc  speedup=%5.2fx\n",
        msg_len_bits, out_len_bits, ref_c, avx_c,
        (double)ref_c / (double)avx_c);
    for (int k = 0; k < NWAY; k++) {
        free(msg[k]);
        free(out[k]);
    }
}

int main(void)
{
    int pass = 0, total = 0;
    /* message lengths: empty, sub-byte, byte-aligned, block-boundary,
       multi-block, and the 2-final-block window (msg_tail_bits > 415). */
    unsigned long long mlens[] = {
        0,   1,    7,    8,    15,   100,  255,  256, 257, 263,
        416, 447,  448,  480,  500,  511,  512,  513, 519, 543,
        575, 1000, 1024, 1031, 2048, 4096, 4097, 8000};
    unsigned long long olens[] = {1,    100,  255,  256,  257,  512,  768,
                                  1000, 1024, 2048, 4096, 8192, 12345};
    int nm = sizeof(mlens) / sizeof(mlens[0]);
    int no = sizeof(olens) / sizeof(olens[0]);

    printf("== correctness: pseudoXOF_avx2 vs reference ==\n");
    for (int a = 0; a < nm; a++)
        for (int b = 0; b < no; b++) {
            total++;
            pass += test_xof(mlens[a], olens[b]);
        }
    printf("== correctness: sm3hash_avx2 vs reference ==\n");
    for (int a = 0; a < nm; a++) {
        total++;
        pass += test_hash(mlens[a]);
    }
    printf("XOF/HASH correctness: %d/%d passed\n\n", pass, total);

    /* exhaustive sweep: every msg bit-length across two full blocks,
       hitting all (msg%8, msg%512) alignments and both 1- and
       2-final-block cases. */
    printf("== exhaustive sweep msg_len_bits in [0,1088] ==\n");
    unsigned long long sweep_o[] = {256, 512, 1024};
    int sp = 0, st = 0;
    for (unsigned long long m = 0; m <= 1088; m++) {
        for (int b = 0; b < 3; b++) {
            st++;
            sp += test_xof(m, sweep_o[b]);
        }
        st++;
        sp += test_hash(m);
    }
    printf("sweep correctness: %d/%d passed\n\n", sp, st);
    pass += sp;
    total += st;

    printf(
        "== performance (cycles per 8 XOF calls; lower is better) ==\n");
    bench(256, 256, 20000);
    bench(256, 1024, 8000);
    bench(512, 2048, 4000);
    bench(2048, 4096, 2000);
    bench(8000, 8192, 1000);

    if (pass != total) {
        printf("\nRESULT: FAIL\n");
        return 1;
    }
    printf("\nRESULT: PASS\n");
    return 0;
}
