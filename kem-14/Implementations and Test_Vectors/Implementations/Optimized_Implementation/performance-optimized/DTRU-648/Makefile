CC ?= /usr/bin/cc
CFLAGS += -O3 -march=x86-64 -mavx2 -mtune=native -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra
# Default to the 972-byte 12-bit public-key format; use PK_PACK_OPT=1 for the 954-byte compressed format.
PK_PACK_OPT ?= 0
PK_PACK_CFLAGS = -DPK_PACK_OPT=$(PK_PACK_OPT)

KAT_PKE_SOURCES = dtru.c poly.c reduce.c coding.c cbd.c ntt.c inverse.c inverse_avx2.c pack.c pack_avx2.c poly_avx2.c
KAT_HEADERS = params.h dtru.h poly.h reduce.h coding.h ntt.h inverse.h inverse_avx2.h \
  pack.h cbd.h poly_avx2.h KEM_AlgorithmInstance.h auxfunc.h drng.h
SPEED_SOURCES = $(KAT_PKE_SOURCES) randombytes.c cpucycles.c speed.c kem.c auxfunc.c
SPEED_HEADERS = $(KAT_HEADERS) randombytes.h cpucycles.h speed.h api.h auxfunc.h
NTESTS ?= 100000

all: KAT_KEM

KAT_KEM: KAT_KEM.c KEM_AlgorithmInstance.c $(KAT_PKE_SOURCES) $(KAT_HEADERS)
	$(CC) $(CFLAGS) $(PK_PACK_CFLAGS) KAT_KEM.c KEM_AlgorithmInstance.c auxfunc.c drng.c $(KAT_PKE_SOURCES) -o $@ $(LDFLAGS)

test_speed: test_speed.c $(SPEED_SOURCES) $(SPEED_HEADERS)
	$(CC) $(CFLAGS) $(PK_PACK_CFLAGS) -DNTESTS=$(NTESTS) $< $(SPEED_SOURCES) -o $@ $(LDFLAGS)

.PHONY: all clean

clean:
	rm -f test_kem test_kem_pk0 test_kem_pk1
	rm -f test_pke test_pke_pk0 test_pke_pk1
	rm -f test_speed
	rm -f test_inv
	rm -f test_pack test_pack_pk0 test_pack_sanitize test_pack_pk0_sanitize
	rm -f bench_pack bench_pack_pk0 bench_pack_pk1
	rm -f KAT_KEM
