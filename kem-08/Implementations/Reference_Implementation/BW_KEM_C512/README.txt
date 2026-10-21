BW_KEM_C512 Reference Implementation

This directory contains the reference implementation of BW_KEM_C512 KEM algorithm for the Next-generation Commercial Cryptographic Algorithms Program (NGCC).

Key parameters:
- KYBER_K = 4
- KYBER_N = 512
- KYBER_ETA1 = 2
- KYBER_ETA2 = 2
- KYBER_POLYVECCOMPRESSEDBYTES = KYBER_K * 640 (u:10-bit)
- KYBER_POLYCOMPRESSEDBYTES = 384 (v:6-bit)

Build instructions:
1. Run 'make' to build the KAT generator
2. Run 'make kat' to generate test vectors and copy them to ../../../Test_Vector/KAT_KEM_BW_KEM_C512.txt

Implementation notes:
- Uses ICCS auxiliary functions for symmetric operations (sm3hash, pseudohash, pseudoXOF)
- Fixed parameter set for BW_KEM_C512 submission bundle
- Only supports the C512 parameter set