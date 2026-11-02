CC ?= /usr/bin/cc
CFLAGS += -O3 -march=x86-64 -mavx2 -mtune=native -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra
LDFLAGS += -lcrypto -ldl

HEADERS = params.h dtru.h poly.h reduce.h coding.h ntt.h ntt_avx2.h inverse.h inverse_avx2.h\
  randombytes.h cpucycles.h speed.h pack.h api.h cbd.h auxfunc.h crypto_hash_sha3256.h symmetric_crypto.h\
  align.h consts.h shuffle.h
KAT_PKE_SOURCES = dtru.c poly.c reduce.c coding.c cbd.c ntt.c ntt_avx2.c invntt_avx2.c inverse.c inverse_avx2.c\
  pack.c consts.c
KAT_HEADERS = KEM_AlgorithmInstance.h auxfunc.h drng.h
SELFTEST_SOURCES = KEM_AlgorithmInstance.c auxfunc.c drng.c $(KAT_PKE_SOURCES)
SELFTEST_HEADERS = $(HEADERS) $(KAT_HEADERS)
SPEED_SOURCES = dtru.c poly.c reduce.c coding.c randombytes.c cbd.c ntt.c ntt_avx2.c invntt_avx2.c inverse.c inverse_avx2.c\
  cpucycles.c speed.c pack.c kem.c auxfunc.c \
  consts.c
NTESTS ?= 100000

all: KAT_KEM test_speed

KAT_KEM: KAT_KEM.c KEM_AlgorithmInstance.c $(KAT_PKE_SOURCES) $(HEADERS) $(KAT_HEADERS)
	$(CC) $(CFLAGS) KAT_KEM.c KEM_AlgorithmInstance.c auxfunc.c drng.c $(KAT_PKE_SOURCES) -o $@ $(LDFLAGS)

test_speed: test_speed.c $(SPEED_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -DNTESTS=$(NTESTS) $< $(SPEED_SOURCES) -o $@ $(LDFLAGS)

test_resource:
	python3 ../../tools/compare_dtru_resources.py --parameter 512

.PHONY: all clean test_resource

clean:
	rm -f test_kem
	rm -f test_speed
	rm -f test_pke
	rm -f test_resource
	rm -f test_inv
	rm -f test_ntt
	rm -f KAT_KEM
