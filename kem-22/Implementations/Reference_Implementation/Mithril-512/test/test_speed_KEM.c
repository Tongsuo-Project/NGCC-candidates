#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "cpucycles.h"
#include "speed_print.h"
#include "drng.h"
#include "parameters.h"
#include "poly.h"
#include "ring.h"
#include "pke.h"
#include "kem.h"

#define NUMBER_OF_TESTS 1000
uint64_t t[NUMBER_OF_TESTS];

#define RNG_SEED_LENGTH 32
DRNG_ctx drng_algorithm;

int main() {

  poly f, g, h;
  ring_element r, a, s;
  ring_element_Awin aw;
  unsigned char seedA[RRLWR_PKE_SEED_A_LEN];
  unsigned char seedS[RRLWR_SEED_S_LEN];
  unsigned char seedSp[RRLWR_SEED_S_LEN];
  unsigned char sk[RRLWR_PKE_SK_LEN];
  unsigned char pk[RRLWR_PKE_PK_LEN];
  unsigned char ct[RRLWR_PKE_CT_LEN];
  unsigned char m[RRLWR_PKE_MESSAGE_LEN];
  unsigned char mp[RRLWR_PKE_MESSAGE_LEN];
  unsigned char kem_sk[RRLWR_KEM_SK_LEN];
  unsigned char kem_pk[RRLWR_KEM_PK_LEN];
  unsigned char kem_ct[RRLWR_KEM_CT_LEN];
  unsigned char ss[RRLWR_KEM_SS_LEN];
  unsigned long long length;

  /* Initialize RNG*/
  const unsigned char seed[RNG_SEED_LENGTH] = {0};
  init_random_number(&drng_algorithm, seed, RNG_SEED_LENGTH);

  get_random_number(&drng_algorithm, seedA,  8*RRLWR_PKE_SEED_A_LEN);
  get_random_number(&drng_algorithm, seedS,  8*RRLWR_SEED_S_LEN);
  get_random_number(&drng_algorithm, seedSp, 8*RRLWR_SEED_S_LEN);
  get_random_number(&drng_algorithm, m,      8*RRLWR_PKE_MESSAGE_LEN);
  ring_uniform_Awin(&aw, RRLWR_PKE_LOGQ, seedA, RRLWR_PKE_SEED_A_LEN);
  for(unsigned int i = 0; i < RRLWR_K; i++) {
    a.x[i] = aw.x[RRLWR_K - 1 - i];
  }
  ring_uniform(&s, RRLWR_PKE_LOG_ETA+1, seedS, RRLWR_SEED_S_LEN);
  f = a.x[0];
  g = s.x[0];

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    poly_mul_toom4(&h, &f, &g);
    t[i] = cpucycles();
  }
  print_results("poly_mul_toom4: ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    poly_add(&h, &f, &g);
    t[i] = cpucycles();
  }
  print_results("poly_add: ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    poly_sub(&h, &f, &g);
    t[i] = cpucycles();
  }
  print_results("poly_sub: ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    ring_mul(r.x, &a, &s, RRLWR_K);
    t[i] = cpucycles();
  }
  print_results("ring_mul (full): ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    ring_mul_Awin(r.x, &aw, &s, RRLWR_K);
    t[i] = cpucycles();
  }
  print_results("ring_mul_Awin (full): ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    ring_mul(r.x, &a, &s, 1);
    t[i] = cpucycles();
  }
  print_results("ring_mul (1 coefficient): ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    ring_mul_Awin(r.x, &aw, &s, 1);
    t[i] = cpucycles();
  }
  print_results("ring_mul_Awin (1 coefficient): ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    pke_keygen(pk, sk, seedA, seedS);
    t[i] = cpucycles();
  }
  print_results("pke_keygen: ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    pke_encrypt(ct, pk, m, seedSp);
    t[i] = cpucycles();
  }
  print_results("pke_encrypt: ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    pke_decrypt(mp, ct, sk);
    t[i] = cpucycles();
  }
  print_results("pke_decrypt: ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    kem_keygen(kem_pk, &length, kem_sk, &length);
    t[i] = cpucycles();
  }
  print_results("kem_keygen: ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    kem_enc(kem_pk, RRLWR_KEM_PK_LEN, ss, &length, kem_ct, &length);
    t[i] = cpucycles();
  }
  print_results("kem_encaps: ", t, NUMBER_OF_TESTS);

  for(unsigned int i=0;i<NUMBER_OF_TESTS;i++) {
    kem_dec(kem_sk, RRLWR_KEM_SK_LEN, kem_ct, RRLWR_KEM_CT_LEN, ss, &length);
    t[i] = cpucycles();
  }
  print_results("kem_decaps: ", t, NUMBER_OF_TESTS);
}
