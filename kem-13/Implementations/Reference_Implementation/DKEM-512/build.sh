#!/usr/bin/env bash
# DKEM-512 Reference (portable ISO C). drng/auxfunc/KAT_KEM are the official API_PKC files.
set -e
CC=${CC:-gcc}
$CC -std=c99 -Wpedantic -Wall -Wextra -O2 -fcommon -DDKE_FORCE_SCALAR -DDKE_MODE=512 -DDKE_HASH=0 -DDKE_RANDOM=0 -I. \
   sm3.c dke_sm3.c dke_hash.c randombytes.c randombytes_sys.c reduce.c ntt.c  poly.c polyvec.c random_sampling.c dke_utils.c packing.c verify.c dkecpa.c dkecca.c  KEM_AlgorithmInstance.c drng.c auxfunc.c KAT_KEM.c -o kat_DKEM-512 -lm
./kat_DKEM-512
echo "KAT written to output/KAT_KEM_DKEM-512.txt"
