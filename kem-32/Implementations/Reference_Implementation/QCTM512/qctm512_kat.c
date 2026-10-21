#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rng.h"
#include "qctm512_kem_api.h"

#define KAT_SUCCESS          0
#define KAT_FILE_OPEN_ERROR -1
#define KAT_CRYPTO_FAILURE  -4

static void fprintBstr(FILE *fp, const char *S, const unsigned char *A, unsigned long long L);

int main(void)
{
    char fn_rsp[96];
    FILE *fp_rsp = NULL;
    unsigned char seed[48];
    unsigned char entropy_input[48];
    unsigned char *ct = NULL;
    unsigned char *ss = NULL;
    unsigned char *ss1 = NULL;
    unsigned char *pk = NULL;
    unsigned char *sk = NULL;
    int ret_val;
    int exit_code = KAT_SUCCESS;

    snprintf(fn_rsp, sizeof(fn_rsp), "KAT_KEM_%s.txt", PKC_ALGNAME);
    fp_rsp = fopen(fn_rsp, "w");
    if (fp_rsp == NULL) {
        printf("Couldn't open <%s> for write\n", fn_rsp);
        return KAT_FILE_OPEN_ERROR;
    }

    ct = calloc((size_t)KEM_CIPHERTEXTBYTES, 1);
    ss = calloc((size_t)KEM_BYTES, 1);
    ss1 = calloc((size_t)KEM_BYTES, 1);
    pk = calloc((size_t)KEM_PUBLICKEYBYTES, 1);
    sk = calloc((size_t)KEM_SECRETKEYBYTES, 1);
    if (ct == NULL || ss == NULL || ss1 == NULL || pk == NULL || sk == NULL) {
        printf("allocation failed\n");
        exit_code = KAT_CRYPTO_FAILURE;
        goto cleanup;
    }

    for (int i = 0; i < 48; i++) {
        entropy_input[i] = (unsigned char)i;
    }

    randombytes_init(entropy_input, NULL, 256);

    fprintf(fp_rsp, "# KAT_KEM_%s\n", PKC_ALGNAME);
    fprintf(fp_rsp, "# Algorithm = %s\n", PKC_ALGNAME);
    fprintf(fp_rsp, "# Function = KEM\n");
    fprintf(fp_rsp, "# Interface = %s KEM API wrapper\n\n", PKC_ALGNAME);

    for (int count = 0; count < 10; count++) {
        randombytes(seed, 48);

        fprintf(fp_rsp, "count = %d\n", count);
        fprintBstr(fp_rsp, "seed = ", seed, 48);

        randombytes_init(seed, NULL, 256);

        ret_val = KEM_KeyGen(pk, sk);
        if (ret_val != 0) {
            printf("KEM_KeyGen returned <%d>\n", ret_val);
            exit_code = KAT_CRYPTO_FAILURE;
            goto cleanup;
        }
        fprintBstr(fp_rsp, "pk = ", pk, KEM_PUBLICKEYBYTES);
        fprintBstr(fp_rsp, "sk = ", sk, KEM_SECRETKEYBYTES);

        ret_val = KEM_Encaps(ct, ss, pk);
        if (ret_val != 0) {
            printf("KEM_Encaps returned <%d>\n", ret_val);
            exit_code = KAT_CRYPTO_FAILURE;
            goto cleanup;
        }
        fprintBstr(fp_rsp, "ct = ", ct, KEM_CIPHERTEXTBYTES);
        fprintBstr(fp_rsp, "ss = ", ss, KEM_BYTES);

        ret_val = KEM_Decaps(ss1, ct, sk);
        if (ret_val != 0) {
            printf("KEM_Decaps returned <%d>\n", ret_val);
            exit_code = KAT_CRYPTO_FAILURE;
            goto cleanup;
        }

        if (memcmp(ss, ss1, KEM_BYTES) != 0) {
            printf("KEM_Decaps returned bad 'ss' value\n");
            exit_code = KAT_CRYPTO_FAILURE;
            goto cleanup;
        }

        fprintf(fp_rsp, "\n");
    }

cleanup:
    if (fp_rsp != NULL) {
        fclose(fp_rsp);
    }
    free(ct);
    free(ss);
    free(ss1);
    free(pk);
    free(sk);
    return exit_code;
}

static void fprintBstr(FILE *fp, const char *S, const unsigned char *A, unsigned long long L)
{
    unsigned long long i;

    fprintf(fp, "%s", S);

    for (i = 0; i < L; i++) {
        fprintf(fp, "%02X", A[i]);
    }

    fprintf(fp, "\n");
}
