#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <assert.h>
#include <string.h>

#include "api.h"
#include "faest.h"
#include "randomness.h"
#include "timing.h"
#include "keygen_gf163.h"
#include "greatwall.h"
#include <limits.h>

#if FAEST_ENABLE_TIMING
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>

static uint64_t keygen_timing_now_ns(void)
{
	struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
	const clockid_t clock_id = CLOCK_MONOTONIC_RAW;
#else
	const clockid_t clock_id = CLOCK_MONOTONIC;
#endif
	if (clock_gettime(clock_id, &ts) != 0)
		abort();
	return (uint64_t) ts.tv_sec * UINT64_C(1000000000) + (uint64_t) ts.tv_nsec;
}
#endif

int crypto_sign_keypair(unsigned char* pk, unsigned char* sk)
{
	if (!greatwall160_constants_available()) return GREATWALL160_ERR_MISSING_CONSTANTS;
	if (!pk || !sk) return -1;
#if FAEST_ENABLE_TIMING
	const uint64_t keygen_start = keygen_timing_now_ns();
	uint64_t random_ns = 0, canonicalize_ns = 0, public_key_ns = 0, candidates = 0;
#endif
	bool candidate_valid;
	do
	{
#if FAEST_ENABLE_TIMING
		const uint64_t random_start = keygen_timing_now_ns();
#endif
		if (rand_bytes(sk, GREATWALL_SECRET_KEY_BYTES) != 0)
			return -1;
#if FAEST_ENABLE_TIMING
		const uint64_t canonicalize_start = keygen_timing_now_ns();
		random_ns += canonicalize_start - random_start;
#endif
		const uint8_t top_mask = 0x07;
		sk[GREATWALL_FIELD_BYTES - 1] &= top_mask;
		sk[GREATWALL_SECRET_KEY_BYTES - 1] &= top_mask;
#if FAEST_ENABLE_TIMING
		const uint64_t public_key_start = keygen_timing_now_ns();
		canonicalize_ns += public_key_start - canonicalize_start;
#endif
#if SECURITY_PARAM == 160 && GREATWALL_KEYGEN_GF163_ISOLATED
		candidate_valid = greatwall_keygen_pubkey_163(pk, sk);
#else
		candidate_valid = faest_pubkey(pk, sk);
#endif
#if FAEST_ENABLE_TIMING
		public_key_ns += keygen_timing_now_ns() - public_key_start;
		++candidates;
#endif
	} while (!candidate_valid);
#if FAEST_ENABLE_TIMING

	const uint64_t total_ns = keygen_timing_now_ns() - keygen_start;
	fprintf(stderr, "[time] keygen/random_bytes_total: %.6f ms\n", random_ns / 1000000.0);
	fprintf(stderr, "[time] keygen/canonicalize_total: %.6f ms\n", canonicalize_ns / 1000000.0);
	fprintf(stderr, "[time] keygen/derive_public_key_total: %.6f ms\n", public_key_ns / 1000000.0);
	fprintf(stderr, "[time] keygen/total: %.6f ms\n", total_ns / 1000000.0);
	fprintf(stderr, "[count] keygen/candidates: %" PRIu64 "\n", candidates);
	fprintf(stderr, "[count] keygen/rejected_candidates: %" PRIu64 "\n", candidates - 1);
#endif
	return 0;
}

int crypto_sign(
    unsigned char* sm, unsigned long long* smlen,
    const unsigned char* m, unsigned long long mlen, const unsigned char* sk)
{
    if (!smlen) return -1;
    *smlen = 0;
    if (!greatwall160_constants_available()) return GREATWALL160_ERR_MISSING_CONSTANTS;
    if (!sm || !sk || (!m && mlen) ||
        mlen > SIZE_MAX - FAEST_SIGNATURE_BYTES ||
        mlen > ULLONG_MAX - FAEST_SIGNATURE_BYTES) return -1;
    uint8_t random_seed[SECURITY_PARAM / 8];
    if (rand_bytes(random_seed, sizeof(random_seed)) != 0) return -1;
    if (mlen) memmove(sm, m, (size_t)mlen);
    if (!faest_sign(sm + mlen, sm, (size_t)mlen, sk, random_seed, sizeof(random_seed)))
        return -1;
    *smlen = mlen + FAEST_SIGNATURE_BYTES;
    return 0;
}

int crypto_sign_open(
    unsigned char* m, unsigned long long* mlen,
    const unsigned char* sm, unsigned long long smlen, const unsigned char* pk)
{
    if (!mlen) return -1;
    *mlen = 0;
    if (!greatwall160_constants_available()) return GREATWALL160_ERR_MISSING_CONSTANTS;
    if (!sm || !pk || smlen < FAEST_SIGNATURE_BYTES || smlen > SIZE_MAX || smlen > PTRDIFF_MAX) return -1;
    const size_t message_len = (size_t)smlen - FAEST_SIGNATURE_BYTES;
    if ((!m && message_len) || !faest_verify(sm + message_len, sm, message_len, pk))
        return -1;
    if (message_len) memmove(m, sm, message_len);
    *mlen = message_len;
    return 0;
}
