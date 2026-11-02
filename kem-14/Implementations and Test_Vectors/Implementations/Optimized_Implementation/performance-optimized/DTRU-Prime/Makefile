CC ?= /usr/bin/cc
PK_PACK_OPT ?= 1
CFLAGS += -O3 -march=x86-64 -mavx2 -mtune=native -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra -I../../reference-implementation/DTRU-Prime
CFLAGS += -DPK_PACK_OPT=$(PK_PACK_OPT)
KAT_REF_DIR = ../../reference-implementation/DTRU-Prime

SOURCES = dtru.c poly.c reduce.c coding.c randombytes.c cbd.c\
  test/cpucycles.c test/speed.c pack.c kem.c fips202.c inverse.c\
  avx2_ntt.c avx2_cbd.c avx2_poly.c inv_dtru_prime1087_avx2.c

HEADERS = params.h dtru.h poly.h reduce.h coding.h randombytes.h cbd.h \
  test/cpucycles.h test/speed.h pack.h api.h fips202.h inverse.h crypto_hash_sha3256.h\
  avx2_ntt.h avx2_cbd.h avx2_poly.h inv_dtru_prime1087_avx2.h
  
SOURCES_PRINT = print_dtru.c print_kem.c poly.c reduce.c coding.c randombytes.c cbd.c\
  cpucycles.c speed.c pack.c fips202.c inverse.c 

SOURCES_N = $(SOURCES) radix_ntt_n1087.c
HEADERS_N = $(HEADERS) radix_ntt_n1087.h
KAT_PKE_SOURCES = dtru.c poly.c reduce.c coding.c cbd.c pack.c inverse.c\
  avx2_ntt.c avx2_cbd.c avx2_poly.c inv_dtru_prime1087_avx2.c radix_ntt_n1087.c
KAT_HEADERS = KEM_AlgorithmInstance.h $(KAT_REF_DIR)/auxfunc.h $(KAT_REF_DIR)/drng.h

all: test/test_pke test/test_kem test/test_speed test/test_ntt test/test_avx_ntt test/test_cbd test/test_avx_poly KAT_KEM

test/test_pke: test/test_pke.c $(SOURCES_N) $(HEADERS_N)
	$(CC) $(CFLAGS) $< $(SOURCES_N) -o $@ $(LDFLAGS)

test/test_kem: test/test_kem.c $(SOURCES_N) $(HEADERS_N)
	$(CC) $(CFLAGS) $< $(SOURCES_N) -o $@ $(LDFLAGS)

test/test_speed: test/test_speed.c $(SOURCES_N) $(HEADERS_N)
	$(CC) $(CFLAGS) $< $(SOURCES_N) -o $@ $(LDFLAGS)

test/test_ntt: test/test_ntt.c $(SOURCES_N) $(HEADERS_N)
	$(CC) $(CFLAGS) $< $(SOURCES_N) -o $@ $(LDFLAGS)

test/test_avx_ntt: test/test_avx_ntt.c $(SOURCES_N) $(HEADERS_N)
	$(CC) $(CFLAGS) $< $(SOURCES_N) -o $@ $(LDFLAGS)

test/test_cbd: test/test_cbd.c $(SOURCES_N) $(HEADERS_N)
	$(CC) $(CFLAGS) $< $(SOURCES_N) -o $@ $(LDFLAGS)

test/test_avx_poly: test/test_avx_poly.c $(SOURCES_N) $(HEADERS_N)
	$(CC) $(CFLAGS) $< $(SOURCES_N) -o $@ $(LDFLAGS)

KAT_KEM: KAT_KEM.c KEM_AlgorithmInstance.c $(KAT_PKE_SOURCES) $(HEADERS_N) $(KAT_HEADERS)
	$(CC) $(CFLAGS) KAT_KEM.c KEM_AlgorithmInstance.c $(KAT_REF_DIR)/auxfunc.c $(KAT_REF_DIR)/drng.c $(KAT_PKE_SOURCES) -o $@ $(LDFLAGS)

clean:
	rm -f test/test_pke
	rm -f test/test_kem
	rm -f test/test_speed
	rm -f test/test_ntt
	rm -f test/test_avx_ntt
	rm -f test/test_cbd
	rm -f test/test_avx_poly
	rm -f KAT_KEM
	rm -rf output
