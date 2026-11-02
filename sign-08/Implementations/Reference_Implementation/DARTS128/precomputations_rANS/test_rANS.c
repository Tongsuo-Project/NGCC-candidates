#include "params.h"

#ifndef MUBYTES
#define MUBYTES CRHBYTES
#endif
#include "poly.h"
#include "polyfix.h"
#include "polyvec.h"
#include "polymat.h"
#include "packing.h"
#include "sign.h"
#include "drng.h"
#include "symmetric.h"
#include "encoding.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef NUM_TESTS
#define NUM_TESTS 200000
#endif

#ifndef KEY_ROTATION_INTERVAL
#define KEY_ROTATION_INTERVAL 256
#endif

/*
 * Use, for example, CPPFLAGS=-DRANS_DRNG_SEED_BYTE=0x56 to obtain a
 * deterministic sample stream independent from the default training stream.
 * All 48 seed bytes are filled with this value.
 */
#ifndef RANS_DRNG_SEED_BYTE
#define RANS_DRNG_SEED_BYTE 0x55
#endif

#if NUM_TESTS < 1
#error "NUM_TESTS must be positive"
#endif

#if KEY_ROTATION_INTERVAL < 1
#error "KEY_ROTATION_INTERVAL must be positive"
#endif

#if RANS_DRNG_SEED_BYTE < 0 || RANS_DRNG_SEED_BYTE > 255
#error "RANS_DRNG_SEED_BYTE must fit in one byte"
#endif

#define MESSAGE_BYTES 32
#define RANS_SCALE 1024
#define H_SIZE_DIST_LENGTH (N * K + 1)
#define HB_Z1_SIZE_DIST_LENGTH (N * L + 1)
#define TOTAL_SIZE_DIST_LENGTH (N * (K + L) + 1)

extern DRNG_ctx drng_algorithm;

#if DARTS_MODE == 128
#define STAT_M_H 23
#define STAT_H_CENTER 6
#define STAT_H_EDGE 5
#define STAT_H_NEG 1023
#define STAT_H_POS 1019
#define STAT_H_NEG_START 13
#define STAT_H_POS_START 18
#define STAT_M_HB_Z1 11
#define STAT_HB_Z1_POS 1019
#define STAT_HB_Z1_POS_START 6
#elif DARTS_MODE == 256
#define STAT_M_H 27
#define STAT_H_CENTER 7
#define STAT_H_EDGE 6
#define STAT_H_NEG 511
#define STAT_H_POS 506
#define STAT_H_NEG_START 15
#define STAT_H_POS_START 21
#define STAT_M_HB_Z1 13
#define STAT_HB_Z1_POS 506
#define STAT_HB_Z1_POS_START 7
#elif DARTS_MODE == 512
#define STAT_M_H 71
#define STAT_H_CENTER 18
#define STAT_H_EDGE 17
#define STAT_H_NEG 1023
#define STAT_H_POS 1007
#define STAT_H_NEG_START 37
#define STAT_H_POS_START 54
#define STAT_M_HB_Z1 35
#define STAT_HB_Z1_POS 1007
#define STAT_HB_Z1_POS_START 18
#else
#error "Unsupported DARTS_MODE"
#endif

static int map_h_symbol(int32_t value)
{
    if (value >= -STAT_H_CENTER && value <= STAT_H_CENTER) {
        return value + STAT_H_CENTER;
    }
    if (value >= -STAT_H_NEG && value < -STAT_H_NEG + STAT_H_EDGE) {
        return value + STAT_H_NEG + STAT_H_NEG_START;
    }
    if (value >= STAT_H_POS && value < STAT_H_POS + STAT_H_EDGE) {
        return value - STAT_H_POS + STAT_H_POS_START;
    }
    return -1;
}

static int map_hb_z1_symbol(int32_t value)
{
    if (value >= 0 && value < STAT_HB_Z1_POS_START) {
        return value;
    }
    if (value >= STAT_HB_Z1_POS &&
        value < STAT_HB_Z1_POS + STAT_HB_Z1_POS_START - 1) {
        return value - STAT_HB_Z1_POS + STAT_HB_Z1_POS_START;
    }
    return -1;
}

/*
 * Match the historical table-generation rule used by prec_encoding.c:
 * floor(count / total * 1024), give every symbol at least one slot, then
 * put the rounding difference into the most frequent symbol.
 */
static int normalize_frequencies(uint32_t *frequencies,
                                 const uint64_t *counts,
                                 size_t alphabet_size)
{
    uint64_t total = 0;
    uint32_t max_frequency = 0;
    uint32_t sum = 0;
    size_t max_index = 0;

    for (size_t i = 0; i < alphabet_size; ++i) {
        total += counts[i];
    }
    if (total == 0) {
        return -1;
    }

    for (size_t i = 0; i < alphabet_size; ++i) {
        if (counts[i] > UINT64_MAX / RANS_SCALE) {
            return -1;
        }
        frequencies[i] =
            (uint32_t)((counts[i] * RANS_SCALE) / total);
        if (frequencies[i] == 0) {
            frequencies[i] = 1;
        }
        if (frequencies[i] > max_frequency) {
            max_frequency = frequencies[i];
            max_index = i;
        }
        sum += frequencies[i];
    }

    {
        int64_t adjusted =
            (int64_t)frequencies[max_index] + (int64_t)RANS_SCALE - sum;
        if (adjusted <= 0 || adjusted > RANS_SCALE) {
            return -1;
        }
        frequencies[max_index] = (uint32_t)adjusted;
    }

    sum = 0;
    for (size_t i = 0; i < alphabet_size; ++i) {
        sum += frequencies[i];
    }
    return sum == RANS_SCALE ? 0 : -1;
}

static void print_u64_array(const char *name,
                            const uint64_t *values,
                            size_t length)
{
    printf("%s={", name);
    for (size_t i = 0; i < length; ++i) {
        printf("%" PRIu64 "%s", values[i], i + 1 == length ? "" : ", ");
    }
    printf("}\n");
}

static void print_u32_array(const char *name,
                            const uint32_t *values,
                            size_t length)
{
    printf("%s={", name);
    for (size_t i = 0; i < length; ++i) {
        printf("%" PRIu32 "%s", values[i], i + 1 == length ? "" : ", ");
    }
    printf("}\n");
}

static void print_size_distribution(const char *title,
                                    const uint64_t *distribution,
                                    size_t length,
                                    uint64_t total,
                                    int print_cumulative)
{
    uint64_t cumulative = 0;

    printf("\n=== %s ===\n", title);
    if (print_cumulative) {
        printf("尺寸(字节) |    频数     |    比例    |  累计比例\n");
        printf("-----------|-------------|------------|------------\n");
    } else {
        printf("尺寸(字节) |    频数     |    比例\n");
        printf("-----------|-------------|------------\n");
    }

    for (size_t size = 0; size < length; ++size) {
        if (distribution[size] == 0) {
            continue;
        }
        cumulative += distribution[size];
        if (print_cumulative) {
            printf("%10zu | %11" PRIu64 " | %9.4f%% | %9.4f%%\n",
                   size, distribution[size],
                   100.0 * (double)distribution[size] / (double)total,
                   100.0 * (double)cumulative / (double)total);
        } else {
            printf("%10zu | %11" PRIu64 " | %9.4f%%\n", size,
                   distribution[size],
                   100.0 * (double)distribution[size] / (double)total);
        }
    }
}

static size_t histogram_quantile(const uint64_t *distribution,
                                 size_t length,
                                 uint64_t total,
                                 uint32_t numerator,
                                 uint32_t denominator)
{
    uint64_t target;
    uint64_t cumulative = 0;

    if (total == 0 || denominator == 0 || numerator == 0 ||
        numerator > denominator) {
        return 0;
    }

    target = (total / denominator) * numerator;
    target += ((total % denominator) * numerator + denominator - 1) /
              denominator;

    for (size_t size = 0; size < length; ++size) {
        cumulative += distribution[size];
        if (cumulative >= target) {
            return size;
        }
    }

    return length - 1;
}

static void print_machine_size_statistics(const char *name,
                                          const uint64_t *distribution,
                                          size_t length,
                                          uint64_t total)
{
    static const struct {
        const char *label;
        uint32_t numerator;
        uint32_t denominator;
    } quantiles[] = {
        {"Q50", 50, 100},
        {"Q90", 90, 100},
        {"Q95", 95, 100},
        {"Q99", 99, 100},
        {"Q999", 999, 1000},
        {"Q9999", 9999, 10000},
        {"Q100", 1, 1},
    };

    printf("PREPACK_%s_COUNT=%" PRIu64 "\n", name, total);
    for (size_t size = 0; size < length; ++size) {
        if (distribution[size] != 0) {
            printf("PREPACK_%s_HIST=%zu,%" PRIu64 "\n",
                   name, size, distribution[size]);
        }
    }
    for (size_t q = 0; q < sizeof(quantiles) / sizeof(quantiles[0]); ++q) {
        printf("PREPACK_%s_%s=%zu\n", name, quantiles[q].label,
               histogram_quantile(distribution, length, total,
                                  quantiles[q].numerator,
                                  quantiles[q].denominator));
    }
}

static int test_z1_highbits_distribution(void)
{
    static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    static uint8_t sk[CRYPTO_SECRETKEYBYTES];
    static uint8_t sig[CRYPTO_SIGNATUREBYTES];
    static uint8_t seedbuf[CRHBYTES];
    static uint8_t buf[POLYVECK_HIGHBITS_PACKEDBYTES];
    static uint8_t mu[MUBYTES];
    static uint8_t key[SEEDBYTES];
    static uint8_t message[MESSAGE_BYTES];

    static poly s0;
    static polyvecl_1 s1;
    static polyveck e;
    static polyvecl A[K];
    static polyveck w;

    static polyfixvecl y1, z1, z1tmp;
    static polyfixveck y2, z2, z2tmp;
    static polyvecl z1rnd, highbits_z1, lowbits_z1;
    static polyveck z2rnd, h, htmp, w_copy;

    static poly c, c_hat, c_1_b, c_1_2b;
    static poly w0, w0_cb;
    static poly w0_highbits, w0_cb_highbits;
    static polyvecl cs;
    static polyveck ce;

    static uint64_t distribution[1 << D];
    static uint64_t h_distribution[1 << (D + 1)];
    static uint64_t mapping_distribution[1 << D];
    static uint64_t mapping_h_distribution[1 << (D + 1)];
    static uint64_t unmapped_distribution[1 << D];
    static uint64_t unmapped_h_distribution[1 << (D + 1)];
    static uint64_t h_size_dist[H_SIZE_DIST_LENGTH];
    static uint64_t hb_z1_size_dist[HB_Z1_SIZE_DIST_LENGTH];
    static uint64_t total_size_dist[TOTAL_SIZE_DIST_LENGTH];

    uint64_t total_coeff = 0;
    uint64_t total_h_coeff = 0;
    uint64_t total_mapping_coeff = 0;
    uint64_t total_mapping_h_coeff = 0;
    uint64_t total_attempts = 0;
    uint64_t reject_compress_count = 0;
    uint64_t reject1_count = 0;
    uint64_t reject23_count = 0;
    uint64_t reject_pack_count = 0;
    uint64_t reject_pack_mapping_count = 0;
    uint64_t reject_pack_size_count = 0;
    uint64_t compress_attempts_by_b[2] = {0, 0};
    uint64_t compress_rejects_by_b[2] = {0, 0};
    uint64_t accepted_by_b[2] = {0, 0};

    uint64_t min_h = UINT64_MAX, max_h = 0, sum_h = 0;
    uint64_t min_hb_z1 = UINT64_MAX, max_hb_z1 = 0, sum_hb_z1 = 0;
    uint64_t min_total = UINT64_MAX, max_total = 0;
    uint64_t encode_success = 0;
    uint64_t decode_h_success = 0, decode_hb_z1_success = 0;
    uint64_t decode_h_fail = 0, decode_hb_z1_fail = 0;

    int64_t z1_min = INT64_MAX, z1_max = INT64_MIN;
    sm3_xof_state state;
    unsigned int i, j;

    memset(distribution, 0, sizeof(distribution));
    memset(h_distribution, 0, sizeof(h_distribution));
    memset(mapping_distribution, 0, sizeof(mapping_distribution));
    memset(mapping_h_distribution, 0, sizeof(mapping_h_distribution));
    memset(unmapped_distribution, 0, sizeof(unmapped_distribution));
    memset(unmapped_h_distribution, 0, sizeof(unmapped_h_distribution));
    memset(h_size_dist, 0, sizeof(h_size_dist));
    memset(hb_z1_size_dist, 0, sizeof(hb_z1_size_dist));
    memset(total_size_dist, 0, sizeof(total_size_dist));

    printf("参数: N=%d, Q=%d, D=%d, L=%d, K=%d\n", N, Q, D, L, K);
    printf("LN=%d, LNBITS=%d\n", LN, LNBITS);
    printf("B0=%.2f, B1=%.2f, B=%.2f\n", B0, B1, B);
    printf("采样次数: %u\n", (unsigned int)NUM_TESTS);
    printf("每 %u 个签名轮换一次有效密钥\n\n",
           (unsigned int)KEY_ROTATION_INTERVAL);
    printf("DRNG seed: 48 bytes of 0x%02X\n\n",
           (unsigned int)RANS_DRNG_SEED_BYTE);

    for (unsigned int sample = 0; sample < (unsigned int)NUM_TESTS; ++sample) {
        uint8_t b;
        uint16_t counter = 0;
        uint64_t reject1, reject2, reject3;
        int candidate_has_unmapped_symbol;

        if (sample % ((unsigned int)NUM_TESTS / 50 + 1) == 0 ||
            sample + 1 == (unsigned int)NUM_TESTS) {
            unsigned int progress =
                (sample + 1) * 50 / (unsigned int)NUM_TESTS;
            fprintf(stderr, "\r进度: [");
            for (unsigned int p = 0; p < 50; ++p) {
                if (p < progress) {
                    fputc('=', stderr);
                } else if (p == progress) {
                    fputc('>', stderr);
                } else {
                    fputc(' ', stderr);
                }
            }
            fprintf(stderr, "] %3u%% (%u/%u)",
                    (sample + 1) * 100 / (unsigned int)NUM_TESTS,
                    sample + 1, (unsigned int)NUM_TESTS);
            fflush(stderr);
        }

        if (sample % (unsigned int)KEY_ROTATION_INTERVAL == 0) {
            if (crypto_sign_keypair(pk, sk) != 0) {
                fprintf(stderr, "crypto_sign_keypair failed\n");
                return -1;
            }
            unpack_sk(A, &s0, &s1, &e, key, sk);
            poly_ntt(&s0);
            polyvecl_1_ntt(&s1);
            polyveck_ntt(&e);
        }

        if (get_random_number(&drng_algorithm, message,
                              sizeof(message) * 8) != 0) {
            fprintf(stderr, "message generation failed\n");
            return -1;
        }

        sm3_xof_absorb_twice(&state, sk, CRYPTO_PUBLICKEYBYTES,
                             message, sizeof(message));
        sm3_xof_squeeze(mu, MUBYTES, &state);
        sm3_xof_absorb_twice(&state, key, SEEDBYTES, mu, MUBYTES);
        sm3_xof_squeeze(seedbuf, CRHBYTES, &state);

reject:
        candidate_has_unmapped_symbol = 0;
        ++total_attempts;
        counter =
            polyfixveclk_sample_hyperball(&y1, &y2, &b, seedbuf, counter);

        polyfixvecl_round(&z1rnd, &y1);
        polyfixveck_round(&z2rnd, &y2);

        polyvecl_ntt(&z1rnd);
        polymatkl_pointwise_montgomery(&w, A, &z1rnd);
        polyveck_invntt_tomont(&w);
        polyveck_add(&w, &w, &z2rnd);
        polyveck_freeze(&w);
        w_copy = w;

        polyveck_pack_highbits(buf, &w);
        poly_challenge(&c, buf, mu);

        /*
         * Symmetric Step 9:
         *   Comp(w0) == Comp(w0 + (1-2b)c).
         * Keep c_1_b unchanged for the later hint computation.
         */
        w0 = w.vec[0];
        poly_mul_1_b(&c_1_b, &c, b & 1);
        poly_add(&c_1_2b, &c_1_b, &c_1_b);
        poly_sub(&c_1_2b, &c_1_2b, &c);
        poly_add(&w0_cb, &w0, &c_1_2b);
        poly_freeze(&w0);
        poly_freeze(&w0_cb);
        poly_compress(&w0_highbits, &w0);
        poly_compress(&w0_cb_highbits, &w0_cb);

        ++compress_attempts_by_b[b & 1];
        if (poly_equal(&w0_highbits, &w0_cb_highbits) == 0) {
            ++reject_compress_count;
            ++compress_rejects_by_b[b & 1];
            goto reject;
        }

        c_hat = c;
        poly_ntt(&c_hat);
        poly_basemul_montgomery(&cs.vec[0], &c_hat, &s0);
        for (i = 1; i < L; ++i) {
            poly_basemul_montgomery(&cs.vec[i], &c_hat, &s1.vec[i - 1]);
        }
        for (i = 0; i < K; ++i) {
            poly_basemul_montgomery(&ce.vec[i], &c_hat, &e.vec[i]);
        }

        polyvecl_invntt_tomont(&cs);
        polyveck_invntt_tomont(&ce);

        polyvecl_cneg(&cs, b & 1);
        polyveck_cneg(&ce, b & 1);
        polyfixvecl_add(&z1, &y1, &cs);
        polyfixveck_add(&z2, &y2, &ce);

        reject1 =
            ((uint64_t)B1SQ * LN * LN - polyfixveclk_sqnorm2(&z1, &z2)) >>
            63;
        reject1 &= 1;

        polyfixvecl_double(&z1tmp, &z1);
        polyfixveck_double(&z2tmp, &z2);
        polyfixfixvecl_sub(&z1tmp, &z1tmp, &y1);
        polyfixfixveck_sub(&z2tmp, &z2tmp, &y2);

        reject2 =
            (polyfixveclk_sqnorm2(&z1tmp, &z2tmp) -
             (uint64_t)BSQ * LN * LN) >>
            63;
        reject2 &= 1;
        reject3 =
            ((uint64_t)B0SQ * LN * LN - polyfixveclk_sqnorm2(&z1, &z2)) >>
            63;
        reject3 &= 1;

        if (reject1) {
            ++reject1_count;
            goto reject;
        }
        if (reject2 && reject3 && (b & 0x2) == 0) {
            ++reject23_count;
            goto reject;
        }

        polyfixvecl_round(&z1rnd, &z1);
        polyfixveck_round(&z2rnd, &z2);
        polyvecl_freeze(&z1rnd);
        polyveck_freeze(&z2rnd);
        polyvecl_lowbits(&lowbits_z1, &z1rnd);
        polyvecl_compress(&highbits_z1, &z1rnd);

        /*
         * The hint intentionally continues to use (1-b)c. This is distinct
         * from the symmetric Step-9 check above.
         */
        polyveck_compress(&htmp, &w_copy);
        polyveck_sub(&w_copy, &w_copy, &z2rnd);
        poly_add(&w_copy.vec[0], &w_copy.vec[0], &c_1_b);
        polyveck_freeze(&w_copy);
        polyveck_compress(&h, &w_copy);
        polyveck_sub(&h, &htmp, &h);

        /* First decide whether the entire candidate fits the fixed alphabet. */
        for (i = 0; i < L; ++i) {
            for (j = 0; j < N; ++j) {
                int32_t value = highbits_z1.vec[i].coeffs[j];
                if (value < 0 || value >= (1 << D)) {
                    fprintf(stderr, "HighBits(z1) outside [0, 2^D)\n");
                    return -1;
                }
                if (map_hb_z1_symbol(value) < 0) {
                    candidate_has_unmapped_symbol = 1;
                    ++unmapped_distribution[value];
                }
            }
        }
        for (i = 0; i < K; ++i) {
            for (j = 0; j < N; ++j) {
                int32_t index = h.vec[i].coeffs[j] + (1 << D);
                if (index < 0 || index >= (1 << (D + 1))) {
                    fprintf(stderr, "h histogram index out of range\n");
                    return -1;
                }
                if (map_h_symbol(h.vec[i].coeffs[j]) < 0) {
                    candidate_has_unmapped_symbol = 1;
                    ++unmapped_h_distribution[index];
                }
            }
        }

        /*
         * Train only on candidates whose complete coefficient vectors fit
         * the production alphabet. Do this before pack_sig so candidates
         * rejected only by the old probability table's encoded length still
         * contribute to its replacement.
         */
        if (!candidate_has_unmapped_symbol) {
            for (i = 0; i < L; ++i) {
                for (j = 0; j < N; ++j) {
                    int32_t value = highbits_z1.vec[i].coeffs[j];
                    ++mapping_distribution[value];
                    ++total_mapping_coeff;
                }
            }
            for (i = 0; i < K; ++i) {
                for (j = 0; j < N; ++j) {
                    int32_t index = h.vec[i].coeffs[j] + (1 << D);
                    ++mapping_h_distribution[index];
                    ++total_mapping_h_coeff;
                }
            }

            /*
             * Measure every algorithm-accepted, alphabet-representable
             * candidate before pack_sig applies BASE_ENC_* and
             * ENCODE_MAXSIZE. These distributions are therefore not
             * censored by the old signature-size limit.
             */
            {
                uint8_t encoded_h[N * K];
                uint8_t encoded_hb_z1[N * L];
                int32_t decoded_h[N * K];
                int32_t decoded_hb_z1[N * L];
                uint16_t size_h_enc =
                    encode_h(encoded_h, (const int32_t *)&h);
                uint16_t size_hb_z1_enc =
                    encode_hb_z1(encoded_hb_z1,
                                 (const int32_t *)&highbits_z1);
                uint64_t total_size;

                if (size_h_enc == 0 || size_hb_z1_enc == 0) {
                    fprintf(stderr,
                            "mapped pre-pack candidate failed direct "
                            "encoding\n");
                    return -1;
                }

                ++encode_success;
                sum_h += size_h_enc;
                sum_hb_z1 += size_hb_z1_enc;
                if (size_h_enc < min_h) min_h = size_h_enc;
                if (size_h_enc > max_h) max_h = size_h_enc;
                if (size_hb_z1_enc < min_hb_z1) {
                    min_hb_z1 = size_hb_z1_enc;
                }
                if (size_hb_z1_enc > max_hb_z1) {
                    max_hb_z1 = size_hb_z1_enc;
                }

                if (size_h_enc >= H_SIZE_DIST_LENGTH ||
                    size_hb_z1_enc >= HB_Z1_SIZE_DIST_LENGTH) {
                    fprintf(stderr, "pre-pack size histogram overflow\n");
                    return -1;
                }
                ++h_size_dist[size_h_enc];
                ++hb_z1_size_dist[size_hb_z1_enc];

                total_size = (uint64_t)size_h_enc + size_hb_z1_enc;
                if (total_size >= TOTAL_SIZE_DIST_LENGTH) {
                    fprintf(stderr,
                            "pre-pack total-size histogram overflow\n");
                    return -1;
                }
                ++total_size_dist[total_size];
                if (total_size < min_total) min_total = total_size;
                if (total_size > max_total) max_total = total_size;

                if (decode_h(decoded_h, encoded_h, size_h_enc) == 0) {
                    int match = 1;
                    for (size_t index = 0; index < N * K; ++index) {
                        if (decoded_h[index] !=
                            ((const int32_t *)&h)[index]) {
                            match = 0;
                            break;
                        }
                    }
                    if (match) {
                        ++decode_h_success;
                    } else {
                        ++decode_h_fail;
                    }
                } else {
                    ++decode_h_fail;
                }

                if (decode_hb_z1(decoded_hb_z1, encoded_hb_z1,
                                 size_hb_z1_enc) == 0) {
                    int match = 1;
                    for (size_t index = 0; index < N * L; ++index) {
                        if (decoded_hb_z1[index] !=
                            ((const int32_t *)&highbits_z1)[index]) {
                            match = 0;
                            break;
                        }
                    }
                    if (match) {
                        ++decode_hb_z1_success;
                    } else {
                        ++decode_hb_z1_fail;
                    }
                } else {
                    ++decode_hb_z1_fail;
                }
            }
        }

        /*
         * Only count signatures that the production packer can emit. A
         * mapping miss or an overlong encoding is a normal signing retry.
         */
        if (pack_sig(sig, &c, &lowbits_z1, &highbits_z1, &h) != 0) {
            ++reject_pack_count;
            if (candidate_has_unmapped_symbol) {
                ++reject_pack_mapping_count;
            } else {
                ++reject_pack_size_count;
            }
            goto reject;
        }

        if (candidate_has_unmapped_symbol) {
            fprintf(stderr, "pack_sig accepted an unmapped rANS symbol\n");
            return -1;
        }

        /*
         * Keep this hand-instrumented signing path locked to the production
         * verifier. A failure here means the model would be trained on an
         * object that is not an actual DARTS signature.
         */
        if (crypto_sign_verify(sig, CRYPTO_SIGNATUREBYTES,
                               message, sizeof(message), pk) != 0) {
            fprintf(stderr,
                    "pack_sig produced a signature rejected by "
                    "crypto_sign_verify\n");
            return -1;
        }

        ++accepted_by_b[b & 1];

        for (i = 0; i < L; ++i) {
            for (j = 0; j < N; ++j) {
                int32_t value = highbits_z1.vec[i].coeffs[j];
                if (z1.vec[i].coeffs[j] < z1_min) {
                    z1_min = z1.vec[i].coeffs[j];
                }
                if (z1.vec[i].coeffs[j] > z1_max) {
                    z1_max = z1.vec[i].coeffs[j];
                }
                if (value < 0 || value >= (1 << D)) {
                    fprintf(stderr, "HighBits(z1) outside [0, 2^D)\n");
                    return -1;
                }
                ++distribution[value];
                ++total_coeff;
            }
        }

        for (i = 0; i < K; ++i) {
            for (j = 0; j < N; ++j) {
                int32_t value = h.vec[i].coeffs[j];
                int32_t index = value + (1 << D);
                if (index < 0 || index >= (1 << (D + 1))) {
                    fprintf(stderr, "h histogram index out of range\n");
                    return -1;
                }
                ++h_distribution[index];
                ++total_h_coeff;
            }
        }

    }

    fputc('\n', stderr);

    printf("\n=== z1 高位系数分布 (总系数: %" PRIu64 ") ===\n",
           total_coeff);
    printf("高位值 |        频数        |   比例\n");
    printf("-------|--------------------|------------\n");
    for (i = 0; i < (1u << D); ++i) {
        if (distribution[i] != 0) {
            printf("  %4u | %18" PRIu64 " | %10.6f%%\n", i,
                   distribution[i],
                   100.0 * (double)distribution[i] / (double)total_coeff);
        }
    }

    printf("\n=== h 系数分布 (总系数: %" PRIu64 ") ===\n",
           total_h_coeff);
    printf("  h值  |        频数        |   比例\n");
    printf("-------|--------------------|------------\n");
    for (i = 0; i < (1u << (D + 1)); ++i) {
        if (h_distribution[i] != 0) {
            int32_t value = (int32_t)i - (1 << D);
            printf("  %4d | %18" PRIu64 " | %10.6f%%\n", value,
                   h_distribution[i],
                   100.0 * (double)h_distribution[i] /
                       (double)total_h_coeff);
        }
    }

    printf("\n=== 拒绝采样统计 ===\n");
    printf("成功签名数: %u\n", (unsigned int)NUM_TESTS);
    printf("总尝试次数: %" PRIu64 "\n", total_attempts);
    printf("平均尝试次数: %.4f\n",
           (double)total_attempts / (double)NUM_TESTS);
    printf("Reject Step9: %" PRIu64 " (%.4f%%)\n",
           reject_compress_count,
           100.0 * (double)reject_compress_count / (double)total_attempts);
    printf("  b=0: %" PRIu64 "/%" PRIu64 " (%.4f%%)\n",
           compress_rejects_by_b[0], compress_attempts_by_b[0],
           compress_attempts_by_b[0] == 0
               ? 0.0
               : 100.0 * (double)compress_rejects_by_b[0] /
                     (double)compress_attempts_by_b[0]);
    printf("  b=1: %" PRIu64 "/%" PRIu64 " (%.4f%%)\n",
           compress_rejects_by_b[1], compress_attempts_by_b[1],
           compress_attempts_by_b[1] == 0
               ? 0.0
               : 100.0 * (double)compress_rejects_by_b[1] /
                     (double)compress_attempts_by_b[1]);
    printf("Reject1 (|z| >= B1): %" PRIu64 "\n", reject1_count);
    printf("Reject2&3: %" PRIu64 "\n", reject23_count);
    printf("Reject pack_sig: %" PRIu64 "\n", reject_pack_count);
    printf("  alphabet outside: %" PRIu64 "\n",
           reject_pack_mapping_count);
    printf("  encoded size/other: %" PRIu64 "\n",
           reject_pack_size_count);
    printf("最终 b=0/b=1: %" PRIu64 "/%" PRIu64 "\n",
           accepted_by_b[0], accepted_by_b[1]);
    printf("z1 坐标范围: [%" PRId64 ", %" PRId64 "]\n", z1_min, z1_max);

    printf("\n=== pre-pack rANS 编码统计（未经过旧尺寸上限）===\n");
    printf("可映射候选/成功编码: %" PRIu64 "/%" PRIu64 "\n",
           encode_success,
           (uint64_t)NUM_TESTS + reject_pack_size_count);
    printf("h 解码: 成功 %" PRIu64 ", 失败 %" PRIu64 "\n",
           decode_h_success, decode_h_fail);
    printf("hb_z1 解码: 成功 %" PRIu64 ", 失败 %" PRIu64 "\n",
           decode_hb_z1_success, decode_hb_z1_fail);
    if (encode_success != 0) {
        printf("h 长度: min=%" PRIu64 ", max=%" PRIu64
               ", avg=%.4f\n",
               min_h, max_h, (double)sum_h / (double)encode_success);
        printf("hb_z1 长度: min=%" PRIu64 ", max=%" PRIu64
               ", avg=%.4f\n",
               min_hb_z1, max_hb_z1,
               (double)sum_hb_z1 / (double)encode_success);
        printf("总长度: min=%" PRIu64 ", max=%" PRIu64
               ", avg=%.4f\n",
               min_total, max_total,
               (double)(sum_h + sum_hb_z1) / (double)encode_success);

        print_size_distribution(
            "pre-pack h 编码尺寸分布", h_size_dist,
            sizeof(h_size_dist) / sizeof(h_size_dist[0]),
            encode_success, 0);
        print_size_distribution(
            "pre-pack hb_z1 编码尺寸分布", hb_z1_size_dist,
            sizeof(hb_z1_size_dist) / sizeof(hb_z1_size_dist[0]),
            encode_success, 0);
        print_size_distribution(
            "pre-pack 总编码尺寸分布 (h + hb_z1)", total_size_dist,
            sizeof(total_size_dist) / sizeof(total_size_dist[0]),
            encode_success, 1);

        printf("\n=== 机器可读 pre-pack 尺寸统计 ===\n");
        printf("PREPACK_MAPPED_CANDIDATES=%" PRIu64 "\n",
               encode_success);
        printf("PREPACK_PACK_SIZE_REJECTIONS=%" PRIu64 "\n",
               reject_pack_size_count);
        print_machine_size_statistics(
            "H", h_size_dist,
            sizeof(h_size_dist) / sizeof(h_size_dist[0]), encode_success);
        print_machine_size_statistics(
            "HB_Z1", hb_z1_size_dist,
            sizeof(hb_z1_size_dist) / sizeof(hb_z1_size_dist[0]),
            encode_success);
        print_machine_size_statistics(
            "TOTAL", total_size_dist,
            sizeof(total_size_dist) / sizeof(total_size_dist[0]),
            encode_success);
    }

    {
        uint64_t counts_h[STAT_M_H] = {0};
        uint64_t counts_hb_z1[STAT_M_HB_Z1] = {0};
        uint32_t frequencies_h[STAT_M_H];
        uint32_t frequencies_hb_z1[STAT_M_HB_Z1];
        uint64_t unmapped_h = 0;
        uint64_t unmapped_hb_z1 = 0;
        uint64_t mapped_h = 0;
        uint64_t mapped_hb_z1 = 0;

        for (i = 0; i < (1u << (D + 1)); ++i) {
            int32_t value = (int32_t)i - (1 << D);
            int symbol = map_h_symbol(value);
            if (symbol >= 0 && symbol < STAT_M_H) {
                counts_h[symbol] += mapping_h_distribution[i];
            } else if (mapping_h_distribution[i] != 0) {
                fprintf(stderr, "mapped h histogram contains an invalid symbol\n");
                return -1;
            }
            unmapped_h += unmapped_h_distribution[i];
            if (unmapped_h_distribution[i] != 0) {
                printf("UNMAPPED_H_VALUE[%" PRId32 "]=%" PRIu64 "\n",
                       value, unmapped_h_distribution[i]);
            }
        }
        for (i = 0; i < (1u << D); ++i) {
            int symbol = map_hb_z1_symbol((int32_t)i);
            if (symbol >= 0 && symbol < STAT_M_HB_Z1) {
                counts_hb_z1[symbol] += mapping_distribution[i];
            } else if (mapping_distribution[i] != 0) {
                fprintf(stderr,
                        "mapped HighBits(z1) histogram contains an invalid symbol\n");
                return -1;
            }
            unmapped_hb_z1 += unmapped_distribution[i];
            if (unmapped_distribution[i] != 0) {
                printf("UNMAPPED_HB_Z1_VALUE[%u]=%" PRIu64 "\n",
                       i, unmapped_distribution[i]);
            }
        }

        for (i = 0; i < STAT_M_H; ++i) {
            mapped_h += counts_h[i];
        }
        for (i = 0; i < STAT_M_HB_Z1; ++i) {
            mapped_hb_z1 += counts_hb_z1[i];
        }
        if (mapped_h != total_mapping_h_coeff ||
            mapped_hb_z1 != total_mapping_coeff) {
            fprintf(stderr, "rANS mapping histogram accounting mismatch\n");
            return -1;
        }

        printf("\n=== 机器可读 rANS 映射输入 ===\n");
        printf("MAPPING_H_TOTAL=%" PRIu64 "\n", total_mapping_h_coeff);
        printf("MAPPING_HB_Z1_TOTAL=%" PRIu64 "\n", total_mapping_coeff);
        print_u64_array("COUNTS_H", counts_h, STAT_M_H);
        print_u64_array("COUNTS_HB_Z1", counts_hb_z1, STAT_M_HB_Z1);
        printf("UNMAPPED_H=%" PRIu64 "\n", unmapped_h);
        printf("UNMAPPED_HB_Z1=%" PRIu64 "\n", unmapped_hb_z1);

        /*
         * Values outside the deliberately finite production alphabet are
         * normal signing retries. Exclude their entire candidate, but retain
         * every fully representable candidate (including size-rejected ones)
         * so the old probability table cannot censor its replacement.
         */
        if (normalize_frequencies(frequencies_h, counts_h, STAT_M_H) != 0 ||
            normalize_frequencies(frequencies_hb_z1, counts_hb_z1,
                                  STAT_M_HB_Z1) != 0) {
            fprintf(stderr, "cannot normalize the rANS frequency map\n");
            return -1;
        }

        print_u32_array("F_H", frequencies_h, STAT_M_H);
        print_u32_array("F_HB_Z1", frequencies_hb_z1, STAT_M_HB_Z1);
    }

    {
        uint64_t h_size_count = 0;
        uint64_t hb_z1_size_count = 0;
        uint64_t total_size_count = 0;
        uint64_t h_size_weighted = 0;
        uint64_t hb_z1_size_weighted = 0;
        uint64_t total_size_weighted = 0;
        const uint64_t emitted = (uint64_t)NUM_TESTS;
        const uint64_t mapped_candidates =
            emitted + reject_pack_size_count;

        for (i = 0; i < sizeof(h_size_dist) / sizeof(h_size_dist[0]); ++i) {
            h_size_count += h_size_dist[i];
            h_size_weighted += (uint64_t)i * h_size_dist[i];
        }
        for (i = 0;
             i < sizeof(hb_z1_size_dist) / sizeof(hb_z1_size_dist[0]);
             ++i) {
            hb_z1_size_count += hb_z1_size_dist[i];
            hb_z1_size_weighted += (uint64_t)i * hb_z1_size_dist[i];
        }
        for (i = 0;
             i < sizeof(total_size_dist) / sizeof(total_size_dist[0]);
             ++i) {
            total_size_count += total_size_dist[i];
            total_size_weighted += (uint64_t)i * total_size_dist[i];
        }

        if (h_size_count != encode_success ||
            hb_z1_size_count != encode_success ||
            total_size_count != encode_success ||
            h_size_weighted != sum_h ||
            hb_z1_size_weighted != sum_hb_z1 ||
            total_size_weighted != sum_h + sum_hb_z1 ||
            total_coeff != emitted * (uint64_t)L * N ||
            total_h_coeff != emitted * (uint64_t)K * N ||
            total_mapping_coeff !=
                mapped_candidates * (uint64_t)L * N ||
            total_mapping_h_coeff !=
                mapped_candidates * (uint64_t)K * N) {
            fprintf(stderr, "statistics accounting mismatch\n");
            return -1;
        }
    }

    if (total_attempts != (uint64_t)NUM_TESTS + reject_compress_count +
                              reject1_count + reject23_count +
                              reject_pack_count ||
        compress_attempts_by_b[0] + compress_attempts_by_b[1] !=
            total_attempts ||
        compress_rejects_by_b[0] + compress_rejects_by_b[1] !=
            reject_compress_count ||
        accepted_by_b[0] + accepted_by_b[1] != (uint64_t)NUM_TESTS ||
        reject_pack_count !=
            reject_pack_mapping_count + reject_pack_size_count ||
        encode_success !=
            (uint64_t)NUM_TESTS + reject_pack_size_count ||
        decode_h_fail != 0 || decode_hb_z1_fail != 0 ||
        decode_h_success != encode_success ||
        decode_hb_z1_success != encode_success) {
        return -1;
    }

    return 0;
}

int main(void)
{
    unsigned char drng_seed[48];

    memset(drng_seed, RANS_DRNG_SEED_BYTE, sizeof(drng_seed));
    if (init_random_number(&drng_algorithm, drng_seed,
                           sizeof(drng_seed)) != 0) {
        fprintf(stderr, "DRNG initialization failed\n");
        return 1;
    }

    return test_z1_highbits_distribution() == 0 ? 0 : 1;
}
