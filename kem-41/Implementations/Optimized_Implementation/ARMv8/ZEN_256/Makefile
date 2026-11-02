CC ?= gcc
CFLAGS ?= -O3 -march=armv8.2-a+crypto+sha3+dotprod+sve -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra

HEADERS = auxfunc.h drng.h sample.h ntt.h poly.h symmetric.h pke.h KEM_AlgorithmInstance.h
POLY_FASTINV_ASM = fastinversion.S
POLY_ASM_PARTS = $(POLY_FASTINV_ASM)
SOURCES = auxfunc.c drng.c sample.S ntt.S poly.c $(POLY_FASTINV_ASM) symmetric.c pke.S KEM_AlgorithmInstance.c
KAT_KEM_SOURCES = $(SOURCES) KAT_KEM.c
SPEED_KEM_SOURCES = $(SOURCES) cpucycles.c SPEED_KEM.c 
BENCHMARK_SOURCES = $(SOURCES) benchmark.c


KAT_KEM: $(HEADERS) $(KAT_KEM_SOURCES) $(POLY_ASM_PARTS)
	$(CC) $(CFLAGS) -o $@ $(KAT_KEM_SOURCES)

SPEED_KEM: $(HEADERS) $(SPEED_KEM_SOURCES) $(POLY_ASM_PARTS)
	$(CC) $(CFLAGS) -o $@ $(SPEED_KEM_SOURCES)

benchmark: $(HEADERS) $(BENCHMARK_SOURCES) $(POLY_ASM_PARTS)
	$(CC) $(CFLAGS) -o $@ $(BENCHMARK_SOURCES)

.PHONY: clean
clean:
	-rm KAT_KEM SPEED_KEM benchmark
