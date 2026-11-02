/*
 * Derived from HAETAE v2.0, Copyright (c) 2023 Team HAETAE.
 * Modifications Copyright (c) 2026 Hang Zhang.
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <immintrin.h>
#include <string.h>
#include "fips202.h"
#include "fips202x4.h"
#include "../common/xof_permutation_count.h"

static const uint64_t KeccakF_RoundConstants[24] = {
  0x0000000000000001ULL, 0x0000000000008082ULL,
  0x800000000000808aULL, 0x8000000080008000ULL,
  0x000000000000808bULL, 0x0000000080000001ULL,
  0x8000000080008081ULL, 0x8000000000008009ULL,
  0x000000000000008aULL, 0x0000000000000088ULL,
  0x0000000080008009ULL, 0x000000008000000aULL,
  0x000000008000808bULL, 0x800000000000008bULL,
  0x8000000000008089ULL, 0x8000000000008003ULL,
  0x8000000000008002ULL, 0x8000000000000080ULL,
  0x000000000000800aULL, 0x800000008000000aULL,
  0x8000000080008081ULL, 0x8000000000008080ULL,
  0x0000000080000001ULL, 0x8000000080008008ULL
};

static void keccakx4_absorb_once(__m256i s[25],
                                 unsigned int r,
                                 const uint8_t *in0,
                                 const uint8_t *in1,
                                 const uint8_t *in2,
                                 const uint8_t *in3,
                                 size_t inlen,
                                 uint8_t p)
{
  size_t i;
  uint64_t pos = 0;
  __m256i t, idx;

  for(i = 0; i < 25; ++i)
    s[i] = _mm256_setzero_si256();

  idx = _mm256_set_epi64x((long long)in3, (long long)in2, (long long)in1, (long long)in0);
  while(inlen >= r) {
    for(i = 0; i < r/8; ++i) {
      t = _mm256_i64gather_epi64((long long *)pos, idx, 1);
      s[i] = _mm256_xor_si256(s[i], t);
      pos += 8;
    }
    inlen -= r;

    bit_xof_count_x4_permutation();
    f1600x4(s, KeccakF_RoundConstants);
  }

  for(i = 0; i < inlen/8; ++i) {
    t = _mm256_i64gather_epi64((long long *)pos, idx, 1);
    s[i] = _mm256_xor_si256(s[i], t);
    pos += 8;
  }
  inlen -= 8*i;

  if(inlen) {
    uint64_t tail[4] = {0, 0, 0, 0};

    memcpy(&tail[0], in0 + pos, inlen);
    memcpy(&tail[1], in1 + pos, inlen);
    memcpy(&tail[2], in2 + pos, inlen);
    memcpy(&tail[3], in3 + pos, inlen);
    t = _mm256_set_epi64x((long long)tail[3], (long long)tail[2],
                          (long long)tail[1], (long long)tail[0]);
    s[i] = _mm256_xor_si256(s[i], t);
  }

  t = _mm256_set1_epi64x((uint64_t)p << 8*inlen);
  s[i] = _mm256_xor_si256(s[i], t);
  t = _mm256_set1_epi64x(1ULL << 63);
  s[r/8 - 1] = _mm256_xor_si256(s[r/8 - 1], t);
}

static void keccakx4_squeezeblocks(uint8_t *out0,
                                   uint8_t *out1,
                                   uint8_t *out2,
                                   uint8_t *out3,
                                   size_t nblocks,
                                   unsigned int r,
                                   __m256i s[25])
{
  unsigned int i;
    __m128i t;

  while(nblocks > 0) {
    bit_xof_count_x4_permutation();
    f1600x4(s, KeccakF_RoundConstants);
    for(i=0; i < r/8; ++i) {
      t = _mm256_castsi256_si128(s[i]);
      _mm_storeu_si64(&out0[8*i], t);
      _mm_storeu_si64(&out1[8*i], _mm_srli_si128(t, 8));
      t = _mm256_extracti128_si256(s[i],1);
      _mm_storeu_si64(&out2[8*i], t);
      _mm_storeu_si64(&out3[8*i], _mm_srli_si128(t, 8));
    }

    out0 += r;
    out1 += r;
    out2 += r;
    out3 += r;
    --nblocks;
  }
}

static void keccakx4_squeezeblocks_vec(__m256i *out,
                                   size_t nblocks,
                                   unsigned int r,
                                   __m256i s[25])
{
  unsigned int i;

  while(nblocks > 0) {
    bit_xof_count_x4_permutation();
    f1600x4(s, KeccakF_RoundConstants);
    for(i=0; i < r/8; ++i) {
      out[i] = s[i];
    }

    out += r/8;
    --nblocks;
  }
}

void shake128x4_absorb_once(keccakx4_state *state,
                            const uint8_t *in0,
                            const uint8_t *in1,
                            const uint8_t *in2,
                            const uint8_t *in3,
                            size_t inlen)
{
  keccakx4_absorb_once(state->s, SHAKE128_RATE, in0, in1, in2, in3, inlen, 0x1F);
}

void shake128x4_squeezeblocks(uint8_t *out0,
                              uint8_t *out1,
                              uint8_t *out2,
                              uint8_t *out3,
                              size_t nblocks,
                              keccakx4_state *state)
{
  keccakx4_squeezeblocks(out0, out1, out2, out3, nblocks, SHAKE128_RATE, state->s);
}

void shake256x4_absorb_once(keccakx4_state *state,
                            const uint8_t *in0,
                            const uint8_t *in1,
                            const uint8_t *in2,
                            const uint8_t *in3,
                            size_t inlen)
{
  keccakx4_absorb_once(state->s, SHAKE256_RATE, in0, in1, in2, in3, inlen, 0x1F);
}

void shake256x4_squeezeblocks(uint8_t *out0,
                              uint8_t *out1,
                              uint8_t *out2,
                              uint8_t *out3,
                              size_t nblocks,
                              keccakx4_state *state)
{
  keccakx4_squeezeblocks(out0, out1, out2, out3, nblocks, SHAKE256_RATE, state->s);
}

void shake256x4_squeezeblocks_vec(__m256i *buf,
                              size_t nblocks,
                              keccakx4_state *state)
{
  keccakx4_squeezeblocks_vec(buf, nblocks, SHAKE256_RATE, state->s);
}

void shake256x4_squeezeblock_state(keccakx4_state *state)
{
  bit_xof_count_x4_permutation();
  f1600x4(state->s, KeccakF_RoundConstants);
}

void shake256x4(uint8_t *out0,
                uint8_t *out1,
                uint8_t *out2,
                uint8_t *out3,
                size_t outlen,
                const uint8_t *in0,
                const uint8_t *in1,
                const uint8_t *in2,
                const uint8_t *in3,
                size_t inlen)
{
  unsigned int i;
  size_t nblocks = outlen/SHAKE256_RATE;
  uint8_t t[4][SHAKE256_RATE];
  keccakx4_state state;

  shake256x4_absorb_once(&state, in0, in1, in2, in3, inlen);
  shake256x4_squeezeblocks(out0, out1, out2, out3, nblocks, &state);

  out0 += nblocks*SHAKE256_RATE;
  out1 += nblocks*SHAKE256_RATE;
  out2 += nblocks*SHAKE256_RATE;
  out3 += nblocks*SHAKE256_RATE;
  outlen -= nblocks*SHAKE256_RATE;

  if(outlen) {
    shake256x4_squeezeblocks(t[0], t[1], t[2], t[3], 1, &state);
    for(i = 0; i < outlen; ++i) {
      out0[i] = t[0][i];
      out1[i] = t[1][i];
      out2[i] = t[2][i];
      out3[i] = t[3][i];
    }
  }
}
