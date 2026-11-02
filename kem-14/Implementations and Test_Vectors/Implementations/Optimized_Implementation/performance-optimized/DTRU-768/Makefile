CC ?= /usr/bin/cc
PK_PACK_OPT ?= 1
 CFLAGS += -O3 -march=x86-64 -mavx2 -mtune=native -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra -I../../reference-implementation/DTRU-768
CFLAGS += -DPK_PACK_OPT=$(PK_PACK_OPT)
LDFLAGS += -lcrypto -ldl
KAT_REF_DIR = ../../reference-implementation/DTRU-768

SOURCES = dtru.c poly.c reduce.c coding.c randombytes.c cbd.c ntt.c inverse.c inverse_avx2.c\
  cpucycles.c speed.c pack.c kem.c fips202.c\
  ntt.S invntt.S basemul.S consts.c add.S reduce.S double.S
HEADERS = params.h dtru.h poly.h reduce.h coding.h ntt.h inverse.h\
  randombytes.h cpucycles.h speed.h pack.h api.h cbd.h fips202.h crypto_hash_sha3256.h symmetric_crypto.h\
  align.h consts.h inverse_avx2.h

KAT_PKE_SOURCES = $(filter-out kem.c,$(SOURCES))
KAT_HEADERS = KEM_AlgorithmInstance.h $(KAT_REF_DIR)/auxfunc.h $(KAT_REF_DIR)/drng.h

all: test_pke test_kem test_speed test_inv test_baseinv_speed KAT_KEM


test_pke: test_pke.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $< $(SOURCES) -o $@ $(LDFLAGS)

test_kem: test_kem.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $< $(SOURCES) -o $@ $(LDFLAGS)

test_speed: test_speed.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $< $(SOURCES) -o $@ $(LDFLAGS)

test_ntt: test_ntt.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $< $(SOURCES) -o $@ $(LDFLAGS)

test_inv: test_inv.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $< $(SOURCES) -o $@ $(LDFLAGS)

test_baseinv_speed: test_baseinv_speed.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $< $(SOURCES) -o $@ $(LDFLAGS)

KAT_KEM: KAT_KEM.c KEM_AlgorithmInstance.c $(KAT_PKE_SOURCES) $(HEADERS) $(KAT_HEADERS)
	$(CC) $(CFLAGS) KAT_KEM.c KEM_AlgorithmInstance.c $(KAT_REF_DIR)/auxfunc.c $(KAT_REF_DIR)/drng.c $(KAT_PKE_SOURCES) -o $@ $(LDFLAGS)

.PHONY: clean

clean:
	rm -f test_kem
	rm -f test_speed
	rm -f test_pke
	rm -f test_ntt
	rm -f test_inv
	rm -f test_baseinv_speed
	rm -f KAT_KEM
	rm -rf output
