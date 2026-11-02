#!/usr/bin/env bash
# DKEM-256 Additional (arm64, ARMv8.4-A native). Build on an arm64 host:
#   bash build.sh                       (native gcc on AArch64 Linux / Apple Silicon clang)
#   CC=aarch64-linux-gnu-gcc bash build.sh   (cross; run kat_DKEM-256 under qemu-aarch64)
# All hashing/XOF uses the official auxfunc; -fcommon lets the template's drng_algorithm coexist.
set -e
CC=${CC:-gcc}
CF="-std=c99 -O3 -fcommon -march=armv8.2-a -DDKM_AARCH64 -DDKE_USE_AARCH64 -DDKE_USE_AARCH64_NATIVE -DDKE_MODE=256 -DDKE_HASH=0 -DDKE_RANDOM=0 -I. -Wpedantic -Wall -Wextra -flto -fomit-frame-pointer"
ASMF="-march=armv8.2-a"
rm -rf build; mkdir -p build; i=0
for s in sm3.c dke_sm3.c dke_hash.c randombytes.c randombytes_sys.c reduce.c ntt.c  poly.c polyvec.c random_sampling.c dke_utils.c packing.c verify.c dkecpa.c dkecca.c KEM_AlgorithmInstance.c drng.c auxfunc.c KAT_KEM.c aarch64/ntt_neon.c aarch64/poly_neon.c aarch64/poly_pack_neon.c aarch64/polyvec_neon.c  aarch64/verify_neon.c aarch64/rejsample_neon.c aarch64-native/dke_aarch64_zetas.c aarch64-native/dke_rej_uniform_table.c ; do $CC $CF -c "$s" -o build/c$i.o; i=$((i+1)); done
for s in aarch64-native/ntt_native.S aarch64-native/intt_native.S aarch64-native/poly_reduce_native.S  aarch64-native/poly_tomont_native.S aarch64-native/poly_tobytes_native.S aarch64-native/mulcache_native.S  aarch64-native/basemul_k2_native.S aarch64-native/basemul_k4_native.S  aarch64-native/rej_uniform_native.S aarch64-native/ntt512_compiler.S   ;                  do $CC $ASMF -c "$s" -o build/a$i.o; i=$((i+1)); done
case "$(uname -s)" in Linux) LRT=-lrt;; *) LRT=;; esac
$CC -O3 -flto -fomit-frame-pointer build/*.o -o kat_DKEM-256 -lm $LRT
./kat_DKEM-256
echo "KAT written to output/KAT_KEM_DKEM-256.txt"
