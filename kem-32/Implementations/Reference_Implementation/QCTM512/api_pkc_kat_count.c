#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "KEM_AlgorithmInstance.h"
#include "drng.h"

#define SEED_LEN_BYTES 64
#define KAT_KEM_SUCCESS 0
#define KAT_ARGUMENT_INVALID -1
#define KAT_FILE_OPERATE_FAILED -2
#define KAT_KEM_CRYPTO_FAILURE -3
#define KAT_MEMORY_ALLOCATION_FAILED -4
#define KAT_KEM_SS_UNEQUAL -5

DRNG_ctx drng_algorithm;

static int validate_algorithm_instance_name(const char *algorithm)
{
    int i;

    if (algorithm == NULL || strlen(algorithm) > 64) {
        return KAT_ARGUMENT_INVALID;
    }
    for (i = 0; algorithm[i] != '\0'; i++) {
        unsigned char c = (unsigned char)algorithm[i];
        if (!(isalnum(c) || c == '-' || c == '_')) {
            return KAT_ARGUMENT_INVALID;
        }
    }
    return 0;
}

static int create_directory(const char *path)
{
    if (mkdir(path, 0755) == 0) {
        return 0;
    }
    if (errno == EEXIST && access(path, F_OK) == 0) {
        return 0;
    }
    return 1;
}

static void fprintlen(FILE *file_output, const char *identifier,
                      unsigned long long len)
{
    fprintf(file_output, "%s%llu\n", identifier, len);
}

static void fprintstr(FILE *file_output, const char *identifier,
                      const unsigned char *msg, unsigned long long len)
{
    unsigned long long i;

    fprintf(file_output, "%s", identifier);
    for (i = 0; i < len; i++) {
        fprintf(file_output, "%02X", msg[i]);
    }
    fprintf(file_output, "\n");
}

static int generate_official_seed(int count, unsigned char *seed)
{
    DRNG_ctx drng_seed;
    unsigned char nonce[SEED_LEN_BYTES];
    int i;

    if (count < 0 || count > 9 || seed == NULL) {
        return KAT_ARGUMENT_INVALID;
    }
    for (i = 0; i < SEED_LEN_BYTES / 4; i++) {
        memcpy(nonce + 4 * i, "seed", 4);
    }
    init_random_number(&drng_seed, nonce, SEED_LEN_BYTES);
    for (i = 0; i <= count; i++) {
        if (get_random_number(&drng_seed, seed, SEED_LEN_BYTES * 8ULL) != 0) {
            return KAT_KEM_CRYPTO_FAILURE;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    FILE *file_output;
    char file_path[160];
    int count;
    int rtn;
    unsigned char *seed;
    unsigned char *ss;
    unsigned char *ss1;
    unsigned char *ct;
    unsigned char *pk;
    unsigned char *sk;
    unsigned long long pk_len_bytes;
    unsigned long long sk_len_bytes;
    unsigned long long ss_len_bytes;
    unsigned long long ct_len_bytes;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <count 0..9>\n", argv[0]);
        return KAT_ARGUMENT_INVALID;
    }
    count = atoi(argv[1]);
    if (count < 0 || count > 9 ||
        validate_algorithm_instance_name(ALGORITHM_INSTANCE) != 0) {
        return KAT_ARGUMENT_INVALID;
    }

    pk_len_bytes = kem_get_pk_len_bytes();
    sk_len_bytes = kem_get_sk_len_bytes();
    ss_len_bytes = kem_get_ss_len_bytes();
    ct_len_bytes = kem_get_ct_len_bytes();

    pk = calloc(pk_len_bytes, 1);
    sk = calloc(sk_len_bytes, 1);
    ss = calloc(ss_len_bytes, 1);
    ss1 = calloc(ss_len_bytes, 1);
    ct = calloc(ct_len_bytes, 1);
    seed = calloc(SEED_LEN_BYTES, 1);
    if (pk == NULL || sk == NULL || ss == NULL || ss1 == NULL ||
        ct == NULL || seed == NULL) {
        free(seed);
        free(ct);
        free(ss1);
        free(ss);
        free(sk);
        free(pk);
        return KAT_MEMORY_ALLOCATION_FAILED;
    }

    rtn = generate_official_seed(count, seed);
    if (rtn != 0) {
        free(seed);
        free(ct);
        free(ss1);
        free(ss);
        free(sk);
        free(pk);
        return rtn;
    }

    if (create_directory("output") != 0 ||
        create_directory("output/shards") != 0) {
        free(seed);
        free(ct);
        free(ss1);
        free(ss);
        free(sk);
        free(pk);
        return KAT_FILE_OPERATE_FAILED;
    }
    snprintf(file_path, sizeof(file_path),
             "output/shards/KAT_KEM_%s_count_%02d.txt",
             ALGORITHM_INSTANCE, count);
    file_output = fopen(file_path, "wb");
    if (file_output == NULL) {
        free(seed);
        free(ct);
        free(ss1);
        free(ss);
        free(sk);
        free(pk);
        return KAT_FILE_OPERATE_FAILED;
    }

    fprintf(file_output, "Count = %d\n", count);
    fprintf(file_output, "Seed_Len = %d\n", SEED_LEN_BYTES);
    fprintstr(file_output, "Seed = ", seed, SEED_LEN_BYTES);
    fflush(file_output);

    init_random_number(&drng_algorithm, seed, SEED_LEN_BYTES);
    rtn = kem_keygen(pk, &pk_len_bytes, sk, &sk_len_bytes);
    if (rtn != 0) {
        fclose(file_output);
        return KAT_KEM_CRYPTO_FAILURE;
    }
    fprintlen(file_output, "PK_Len = ", pk_len_bytes);
    fprintstr(file_output, "PK = ", pk, pk_len_bytes);
    fprintlen(file_output, "SK_Len = ", sk_len_bytes);
    fprintstr(file_output, "SK = ", sk, sk_len_bytes);

    rtn = kem_enc(pk, pk_len_bytes, ss, &ss_len_bytes, ct, &ct_len_bytes);
    if (rtn != 0) {
        fclose(file_output);
        return KAT_KEM_CRYPTO_FAILURE;
    }
    fprintlen(file_output, "CT_Len = ", ct_len_bytes);
    fprintstr(file_output, "CT = ", ct, ct_len_bytes);
    fprintlen(file_output, "SS_Len = ", ss_len_bytes);
    fprintstr(file_output, "SS = ", ss, ss_len_bytes);

    rtn = kem_dec(sk, sk_len_bytes, ct, ct_len_bytes, ss1, &ss_len_bytes);
    if (rtn != 0) {
        fclose(file_output);
        return KAT_KEM_CRYPTO_FAILURE;
    }
    if (memcmp(ss, ss1, ss_len_bytes) != 0) {
        fclose(file_output);
        return KAT_KEM_SS_UNEQUAL;
    }
    fprintf(file_output, "\n");

    free(seed);
    free(ct);
    free(ss1);
    free(ss);
    free(sk);
    free(pk);

    if (fclose(file_output) != 0) {
        return KAT_FILE_OPERATE_FAILED;
    }
    printf("Wrote %s\n", file_path);
    return KAT_KEM_SUCCESS;
}
