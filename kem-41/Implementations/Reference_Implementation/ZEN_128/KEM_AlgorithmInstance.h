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
File Description: Declares the ZEN key-encapsulation mechanism layer for the optimized ZEN-128 instance.
*/

#ifndef KEM_ALGORITHM_INSTANCE_H
#define KEM_ALGORITHM_INSTANCE_H

/* Resource/primitive failure or exhausted key-generation nonces. */
#define ZEN_ERR_INTERNAL (-4)

// Set "OUTPUT_BLANK_TEST_VECTORS" as 0 to generate test vector files
// Set "OUTPUT_BLANK_TEST_VECTORS" as 1 to generate blank template (default)
#define OUTPUT_BLANK_TEST_VECTORS 0

// Set "ALGORITHM_INSTANCE" as your algorithm instance name (no more than 64 bytes)
// Only letters, numbers, '-' or '_' are permitted
#define ALGORITHM_INSTANCE "ZEN_128"

/*
 * Byte buffers (pk, sk, ct, ss and seed) may have any byte alignment.
 * Allocate the documented byte lengths; no SIMD padding is required.
 * On ZEN_ERR_INTERNAL, output lengths are zero and output key/ct/ss buffers
 * are cleared. Invalid ciphertexts still use normal implicit rejection.
 * Check every return value before using outputs.
 * Length pointers must point to valid unsigned long long objects.
 */

#ifdef __cplusplus
extern "C"
{
#endif

	/// @brief Obtain the claimed byte length of the public key
	/// @return Claimed byte length of the public key
	unsigned long long kem_get_pk_len_bytes();

	/// @brief Obtain the claimed byte length of the private key
	/// @return Claimed byte length of the private key
	unsigned long long kem_get_sk_len_bytes();

	/// @brief Obtain the claimed byte length of the shared secret key (encapsulated key)
	/// @return Claimed byte length of the shared secret key
	unsigned long long kem_get_ss_len_bytes();

	/// @brief Obtain the claimed byte length of the ciphertext
	/// @return Claimed byte length of the ciphertext
	unsigned long long kem_get_ct_len_bytes();

	/// @brief Deterministically key generate from an input seed
	/// @param[out] pk Public key
	/// @param[out] pk_len_bytes Byte length of the public key
	/// @param[out] sk Private key
	/// @param[out] sk_len_bytes Byte length of the private key
	/// @param[in] seed Base address of input seed byte array used for deterministic key generation
	/// @return If run successfully, return 0; otherwise, return a self-defined negative (-1 to -99) error code
	int kem_keygen_derand(
		unsigned char *pk, unsigned long long *pk_len_bytes,
		unsigned char *sk, unsigned long long *sk_len_bytes,
		const unsigned char *seed);

	/// @brief Key generate
	/// @param[out] pk Public key
	/// @param[out] pk_len_bytes Byte length of the public key
	/// @param[out] sk Private key
	/// @param[out] sk_len_bytes Byte length of the private key
	/// @return If run successfully, return 0; otherwise, return a self-defined negative (-1 to -99) error code
	int kem_keygen(
		unsigned char *pk, unsigned long long *pk_len_bytes,
		unsigned char *sk, unsigned long long *sk_len_bytes);

	/// @brief Encapsulate
	/// @param[in] pk Public key
	/// @param[in] pk_len_bytes Byte length of the public key
	/// @param[out] ss Shared secret key
	/// @param[out] ss_len_bytes Byte length of the shared secret key
	/// @param[out] ct Ciphertext
	/// @param[out] ct_len_bytes Byte length of the ciphertext
	/// @return If run successfully, return 0; otherwise, return a self-defined negative (-1 to -99) error code
	int kem_enc(
		unsigned char *pk, unsigned long long pk_len_bytes,
		unsigned char *ss, unsigned long long *ss_len_bytes,
		unsigned char *ct, unsigned long long *ct_len_bytes);

	/// @brief Decapsulate
	/// @param[in] sk Private key
	/// @param[in] sk_len_bytes Byte length of the private key
	/// @param[in] ct Ciphertext
	/// @param[in] ct_len_bytes Byte length of the ciphertext
	/// @param[out] ss Shared secret key
	/// @param[out] ss_len_bytes Byte length of the shared secret key
	/// @return 0 for completed decapsulation (including implicit rejection); negative for API/internal errors
	int kem_dec(
		unsigned char *sk, unsigned long long sk_len_bytes,
		unsigned char *ct, unsigned long long ct_len_bytes,
		unsigned char *ss, unsigned long long *ss_len_bytes);

#ifdef __cplusplus
}
#endif
#endif