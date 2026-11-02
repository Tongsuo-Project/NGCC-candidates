#!/usr/bin/env bash
# DKEM-256 Optimized (x86-64 AVX2 NTT: mlkem-asm N=256, packed-512 N=512).
# Per req 3.4(5) ALL hashing/XOF uses the official auxfunc (no hand-vectorized SM3);
# the sm3x8/sm3x4 parallel-noise code has been removed from the sources.
set -e
CC=${CC:-gcc}
CF="-std=c99 -O3 -fcommon -I. -mavx2 -DDKM_LINUX -DDKE_AVX2_NTT256_ASM -DDKE_NTT512_PACKED -DDKE_MODE=256 -DDKE_HASH=0 -DDKE_RANDOM=0 -Wpedantic -Wall -Wextra -flto -fomit-frame-pointer -march=x86-64 -mtune=native"
AIF="-Iavx2-linux -Iavx2 -Iavx2-dkek -Iavx2-512p -DDKEK_K=3"
rm -rf build; mkdir -p build; i=0
for s in sm3.c dke_sm3.c dke_hash.c randombytes.c randombytes_sys.c reduce.c ntt.c  poly.c polyvec.c random_sampling.c dke_utils.c packing.c verify.c dkecpa.c dkecca.c  KEM_AlgorithmInstance.c drng.c auxfunc.c KAT_KEM.c avx2/ntt_avx2.c avx2/poly_avx2.c avx2/consts_avx2.c  avx2/consts512_avx2.c avx2/verify_avx2.c avx2/rejsample_avx2.c  avx2-512p/consts.c avx2-dkek/consts.c; do $CC $CF -c "$s" -o build/c$i.o; i=$((i+1)); done
for s in avx2-linux/ntt256_avx2.S avx2-linux/ntt_dkek_avx2.S avx2-linux/ntt_tobytes_avx2.S  avx2-linux/invntt256_avx2.S avx2-linux/invntt_dkek_avx2.S avx2-linux/ntt512_avx2.S  avx2-linux/invntt512_avx2.S avx2-linux/basemul_avx2.S avx2-linux/basemul_dkek_avx2.S  avx2-linux/basemul512_avx2.S avx2-512/ntt512.S avx2-512p/ntt.S avx2-512p/invntt.S  avx2-512p/basemul.S avx2-512p/shuffle.S avx2-512p/fq.S avx2-linux/poly_ops_avx2.S  avx2-linux/ntt_large_avx2.S avx2-linux/ntt_large512_avx2.S avx2-linux/ntt_unrolled_avx2.S  avx2-dkek/ntt.S avx2-dkek/invntt.S avx2-dkek/basemul.S avx2-dkek/shuffle.S avx2-dkek/fq.S;        do $CC $CF $AIF -c "$s" -o build/a$i.o; i=$((i+1)); done
$CC -O3 -flto -fomit-frame-pointer build/*.o -o kat_DKEM-256 -lm
./kat_DKEM-256
echo "KAT written to output/KAT_KEM_DKEM-256.txt"
