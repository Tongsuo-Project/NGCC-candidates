#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheme_api.h"
#include "rng.h"

int main(void)
{
    unsigned char entropy_input[48];
    unsigned char *pk = NULL;
    unsigned char *sk = NULL;
    unsigned char *ct = NULL;
    unsigned char *ss = NULL;
    unsigned char *ss1 = NULL;
    int i;
    int status = 1;

    for (i = 0; i < 48; i++) {
        entropy_input[i] = (unsigned char)i;
    }
    randombytes_init(entropy_input, NULL, 256);

    pk = calloc((size_t)CRYPTO_PUBLICKEYBYTES, 1);
    sk = calloc((size_t)CRYPTO_SECRETKEYBYTES, 1);
    ct = calloc((size_t)CRYPTO_CIPHERTEXTBYTES, 1);
    ss = calloc((size_t)CRYPTO_BYTES, 1);
    ss1 = calloc((size_t)CRYPTO_BYTES, 1);
    if (pk == NULL || sk == NULL || ct == NULL || ss == NULL || ss1 == NULL) {
        puts("alloc=fail");
        goto cleanup;
    }

    if (crypto_kem_keypair(pk, sk) != SUCCESS) {
        puts("keypair=fail");
        goto cleanup;
    }
    puts("keypair=ok");
    if (crypto_kem_enc(ct, ss, pk) != SUCCESS) {
        puts("enc=fail");
        goto cleanup;
    }
    puts("enc=ok");
    if (crypto_kem_dec(ss1, ct, sk) != SUCCESS) {
        puts("dec=fail");
        goto cleanup;
    }
    puts("dec=ok");
    if (memcmp(ss, ss1, CRYPTO_BYTES) != 0) {
        puts("ss=bad");
        goto cleanup;
    }
    puts("ss=ok");
    status = 0;

cleanup:
    free(pk);
    free(sk);
    free(ct);
    free(ss);
    free(ss1);
    return status;
}
