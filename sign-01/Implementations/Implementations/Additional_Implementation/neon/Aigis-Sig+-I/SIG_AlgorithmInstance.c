/*
The software is provided by the Institute of Commercial Cryptography Standards
(ICCS), and is used for algorithm submissions in the Next-generation Commercial
Cryptographic Algorithms Program (NGCC).

ICCS doesn't represent or warrant that the operation of the software will be
uninterrupted or error-free in all cases. ICCS will take no responsibility for
the use of the software or the results thereof, if the software is used for any
other purposes.
*/

#include "SIG_AlgorithmInstance.h"
#include "drng.h"
#include "api.h"

// DRNG_ctx for generating pseudorandom numbers within the SIG scheme
extern DRNG_ctx drng_algorithm;

// The following should be used to get pseudorandom numbers
// get_random_number(&drng_algorithm, random_number, random_number_len_bits);

unsigned long long sig_get_pk_len_bytes()
{
	return SIG_PUBLICKEYBYTES;
}

unsigned long long sig_get_sk_len_bytes()
{
	return SIG_SECRETKEYBYTES;
}

unsigned long long sig_get_sn_len_bytes()
{
	return SIG_BYTES;
}

int sig_keygen(
	unsigned char *pk, unsigned long long *pk_len_bytes,
	unsigned char *sk, unsigned long long *sk_len_bytes)
{
	*pk_len_bytes = SIG_PUBLICKEYBYTES;
	*sk_len_bytes = SIG_SECRETKEYBYTES;
	return msig_keygen(pk, sk);
}

int sig_sign(
	unsigned char *sk, unsigned long long sk_len_bytes,
	unsigned char *m, unsigned long long m_len_bytes,
	unsigned char *sn, unsigned long long *sn_len_bytes)
{
	int32_t smlen = 0;
	int ret;
	if (sn_len_bytes == NULL) return -1;
	*sn_len_bytes = 0;
	if (sk_len_bytes != SIG_SECRETKEYBYTES) return -1;
	if (m_len_bytes > SIG_MAX_MESSAGE_BYTES) return -1;
	ret = msig_sign(sk, m, (int32_t)m_len_bytes, sn, &smlen);
	if (ret == 0)
		*sn_len_bytes = (unsigned long long)smlen;
	return ret;
}

int sig_verify(
	unsigned char *pk, unsigned long long pk_len_bytes,
	unsigned char *sn, unsigned long long sn_len_bytes,
	unsigned char *m, unsigned long long m_len_bytes)
{
	if (pk_len_bytes != SIG_PUBLICKEYBYTES) return -1;
	if (sn_len_bytes != SIG_BYTES || m_len_bytes > SIG_MAX_MESSAGE_BYTES) return -1;
	return msig_verf(pk,
		sn, (int32_t)sn_len_bytes,
		m, (int32_t)m_len_bytes);
}
