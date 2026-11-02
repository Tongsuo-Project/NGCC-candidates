/*
The software is provided by the Institute of Commercial Cryptography Standards
(ICCS), and is used for algorithm submissions in the Next-generation Commercial
Cryptographic Algorithms Program (NGCC).

ICCS doesn't represent or warrant that the operation of the software will be
uninterrupted or error-free in all cases. ICCS will take no responsibility for
the use of the software or the results thereof, if the software is used for any
other purposes.
*/

/*
Copyright (c) 2026 Yu Zhang.
Organization: State Key Laboratory of Cyberspace Security Defense,Institute of Information Engineering, CAS
              School of Cyber Security, University of Chinese Academy of Sciences  
File Description: Declares the ZEN key-encapsulation mechanism layer for the optimized ZEN-256 instance.
*/

#include "KEM_AlgorithmInstance.h"
#include "drng.h"
#include "params.h"

#include <string.h>
#include "pke.h"
#include "auxfunc.h"

#include <stdio.h>

// DRNG_ctx for generating pseudorandom numbers within the KEM scheme
extern DRNG_ctx drng_algorithm;

// The following should be used to get pseudorandom numbers
// get_random_number(&drng_algorithm, random_number, random_number_len_bits);

unsigned long long kem_get_pk_len_bytes()
{
    return ZEN_PUBLICKEY_LEN_BYTES;
}

unsigned long long kem_get_sk_len_bytes()
{
    return ZEN_SECREKEY_LEN_BYTES;
}

unsigned long long kem_get_ss_len_bytes()
{
    return ZEN_SHAREDKEY_LEN_BYTES;
}

unsigned long long kem_get_ct_len_bytes()
{
    return ZEN_CIPHERTEXT_LEN_BYTES;
}

int kem_keygen_derand(
    unsigned char *pk, unsigned long long *pk_len_bytes,
    unsigned char *sk, unsigned long long *sk_len_bytes,
    const unsigned char *seed)
{
    if (pk == NULL || pk_len_bytes == NULL || sk == NULL || sk_len_bytes == NULL || seed == NULL) {
        return -1;
    }

    if (pke_keygen_derand(pk, sk, seed) != 0)
        goto cleanup;
    memcpy(sk + ZEN_INDCPA_SECREKEY_LEN_BYTES, pk, ZEN_INDCPA_PUBLICKEY_LEN_BYTES);
    if (sm3hash(ZEN_SYM_LEN_BYTES*8, pk, ZEN_INDCPA_PUBLICKEY_LEN_BYTES*8, sk+ZEN_INDCPA_SECREKEY_LEN_BYTES+ZEN_INDCPA_PUBLICKEY_LEN_BYTES) != 0)
        goto cleanup;
    if (get_random_number(&drng_algorithm, sk+ZEN_INDCPA_SECREKEY_LEN_BYTES+ZEN_INDCPA_PUBLICKEY_LEN_BYTES+ZEN_SYM_LEN_BYTES, ZEN_SHAREDKEY_LEN_BYTES*8) != 0)
        goto cleanup;
    
    *pk_len_bytes = kem_get_pk_len_bytes();
    *sk_len_bytes = kem_get_sk_len_bytes();
    return 0;

cleanup:
    *pk_len_bytes = 0;
    *sk_len_bytes = 0;
    memset(pk, 0, ZEN_PUBLICKEY_LEN_BYTES);
    memset(sk, 0, ZEN_SECREKEY_LEN_BYTES);
    return ZEN_ERR_INTERNAL;
}

int kem_keygen(
    unsigned char *pk, unsigned long long *pk_len_bytes,
    unsigned char *sk, unsigned long long *sk_len_bytes)
{
    if (pk == NULL || pk_len_bytes == NULL || sk == NULL || sk_len_bytes == NULL) {
        return -1;
    }

    if (pke_keygen(pk, sk) != 0)
        goto cleanup;
    memcpy(sk + ZEN_INDCPA_SECREKEY_LEN_BYTES, pk, ZEN_INDCPA_PUBLICKEY_LEN_BYTES);
    if (sm3hash(ZEN_SYM_LEN_BYTES*8, pk, ZEN_INDCPA_PUBLICKEY_LEN_BYTES*8, sk+ZEN_INDCPA_SECREKEY_LEN_BYTES+ZEN_INDCPA_PUBLICKEY_LEN_BYTES) != 0)
        goto cleanup;
    if (get_random_number(&drng_algorithm, sk+ZEN_INDCPA_SECREKEY_LEN_BYTES+ZEN_INDCPA_PUBLICKEY_LEN_BYTES+ZEN_SYM_LEN_BYTES, ZEN_SHAREDKEY_LEN_BYTES*8) != 0)
        goto cleanup;
    
    *pk_len_bytes = kem_get_pk_len_bytes();
    *sk_len_bytes = kem_get_sk_len_bytes();
    return 0;

cleanup:
    *pk_len_bytes = 0;
    *sk_len_bytes = 0;
    memset(pk, 0, ZEN_PUBLICKEY_LEN_BYTES);
    memset(sk, 0, ZEN_SECREKEY_LEN_BYTES);
    return ZEN_ERR_INTERNAL;
}

int kem_enc(
    unsigned char *pk, unsigned long long pk_len_bytes,
    unsigned char *ss, unsigned long long *ss_len_bytes,
    unsigned char *ct, unsigned long long *ct_len_bytes)
{
    uint8_t buf[ZEN_INDCPA_MSG_LEN_BYTES+2*ZEN_SYM_LEN_BYTES];
    uint8_t kr[2*ZEN_SYM_LEN_BYTES+SEED_LEN_BYTES];

    if (pk == NULL || ss == NULL || ss_len_bytes == NULL || ct == NULL || ct_len_bytes == NULL) {
        return -1;
    }
    if (pk_len_bytes != kem_get_pk_len_bytes()) {
        return -2;
    }

    //m
    if (get_random_number(&drng_algorithm, buf, ZEN_INDCPA_MSG_LEN_BYTES*8) != 0)
        goto cleanup;
    //hash(pk)
    if (sm3hash(ZEN_SYM_LEN_BYTES*8, pk, ZEN_INDCPA_PUBLICKEY_LEN_BYTES*8, buf+ZEN_INDCPA_MSG_LEN_BYTES) != 0)
        goto cleanup;
    //hash(m||hash(pk))
    if (pseudohash((ZEN_SYM_LEN_BYTES+SEED_LEN_BYTES)*8, buf, (ZEN_INDCPA_MSG_LEN_BYTES+ZEN_SYM_LEN_BYTES)*8, kr) != 0)
        goto cleanup;
    if (pke_enc(pk, buf, kr+ZEN_SYM_LEN_BYTES, ct) != 0)
        goto cleanup;
    memcpy(ss, kr, ZEN_SYM_LEN_BYTES);

    *ss_len_bytes = kem_get_ss_len_bytes();
    *ct_len_bytes = kem_get_ct_len_bytes();
    return 0;

cleanup:
    *ss_len_bytes = 0;
    *ct_len_bytes = 0;
    memset(ct, 0, ZEN_CIPHERTEXT_LEN_BYTES);
    memset(ss, 0, ZEN_SHAREDKEY_LEN_BYTES);
    return ZEN_ERR_INTERNAL;
}

int kem_dec(
    unsigned char *sk, unsigned long long sk_len_bytes,
    unsigned char *ct, unsigned long long ct_len_bytes,
    unsigned char *ss, unsigned long long *ss_len_bytes)
{
    unsigned int i;
    uint32_t diff;
    uint8_t mask;
    uint8_t buf[ZEN_INDCPA_MSG_LEN_BYTES+ZEN_SYM_LEN_BYTES];
    uint8_t kr[ZEN_SYM_LEN_BYTES+SEED_LEN_BYTES];
    uint8_t zc[ZEN_SHAREDKEY_LEN_BYTES+ZEN_CIPHERTEXT_LEN_BYTES];
    uint8_t ctp[ZEN_CIPHERTEXT_LEN_BYTES];
    uint8_t ssp[2*ZEN_SHAREDKEY_LEN_BYTES];
    uint8_t *pk;

    if (sk == NULL || ct == NULL || ss == NULL || ss_len_bytes == NULL) {
        return -2;
    }
    if (sk_len_bytes != kem_get_sk_len_bytes() || ct_len_bytes != kem_get_ct_len_bytes()) {
        return -3;
    }
    pk = sk + ZEN_INDCPA_SECREKEY_LEN_BYTES;

    pke_dec(sk, ct, buf);
    memcpy(buf+ZEN_INDCPA_MSG_LEN_BYTES, sk+ZEN_INDCPA_SECREKEY_LEN_BYTES+ZEN_INDCPA_PUBLICKEY_LEN_BYTES, ZEN_SYM_LEN_BYTES);
    if (pseudohash((ZEN_SYM_LEN_BYTES+SEED_LEN_BYTES)*8, buf, (ZEN_INDCPA_MSG_LEN_BYTES+ZEN_SYM_LEN_BYTES)*8, kr) != 0)
        goto cleanup;

    memcpy(zc, sk+ZEN_INDCPA_SECREKEY_LEN_BYTES+ZEN_INDCPA_PUBLICKEY_LEN_BYTES+ZEN_SYM_LEN_BYTES, ZEN_SHAREDKEY_LEN_BYTES);
    memcpy(zc+ZEN_SHAREDKEY_LEN_BYTES, ct, ZEN_CIPHERTEXT_LEN_BYTES);

    if (sm3hash(ZEN_SYM_LEN_BYTES*8, zc, (ZEN_SHAREDKEY_LEN_BYTES+ZEN_CIPHERTEXT_LEN_BYTES)*8, ssp) != 0)
        goto cleanup;

    if (pke_enc(pk, buf, kr+ZEN_SYM_LEN_BYTES, ctp) != 0)
        goto cleanup;	

    diff = 0;
    for(i = 0; i < ZEN_CIPHERTEXT_LEN_BYTES; i++)
    {
        diff |= (uint32_t)(ct[i] ^ ctp[i]);
    }
    mask = (uint8_t)(0u - ((diff | (0u - diff)) >> 31));

    /* GCC/Clang value barrier: keep the mask opaque so selection cannot
     * become a choice of source address. Both candidate secrets must be read.
     * The empty asm emits no instructions and needs no memory clobber. */
    __asm__ __volatile__("" : "+r" (mask));

    for(i = 0; i < ZEN_SHAREDKEY_LEN_BYTES; i++)
    {
        ss[i] = (uint8_t)((ssp[i] & mask) | (kr[i] & ~mask));
    }

    *ss_len_bytes = kem_get_ss_len_bytes();
    return 0;

cleanup:
    *ss_len_bytes = 0;
    memset(ss, 0, ZEN_SHAREDKEY_LEN_BYTES);
    return ZEN_ERR_INTERNAL;
}
