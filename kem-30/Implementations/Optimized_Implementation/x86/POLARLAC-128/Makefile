# Performance-optimized x86 build configuration
# Required baseline: x86-64 + AVX2, C99, high optimization and LTO.

CC ?= gcc

OPTFLAGS ?= -O3 -march=x86-64 -mavx2 -mtune=native -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra
EXTRA_CFLAGS ?=
CFLAGS += $(OPTFLAGS) $(EXTRA_CFLAGS)

LDFLAGS += -flto

# Kyber-aligned AVX2 build configuration for PolarLAC MLWE
# CC ?= clang
# CFLAGS = -O3 -mavx2 -march=native -fomit-frame-pointer

# # Kyber official Makefile does not enable LTO; keep this empty for fair comparison.
# LDFLAGS ?=

# Hash / XOF backend selection
# USE_SHAKE=1 enables the optimized SHAKE/SHA3 backend and SHAKE128x4 path.
# USE_SHAKE=0 uses the API_PKC auxiliary SM3/pseudoXOF backend.
USE_SHAKE ?= 0
CFLAGS += -DBIT_USE_SHAKE=$(USE_SHAKE)

COMMON_SOURCES_BASE = auxfunc.c drng.c kem_random.c symmetric.c sample.c poly.c pke.c polar.c ntt.c ntt_tables.c ntt_avx2.c fft.c KEM_AlgorithmInstance.c

ifeq ($(USE_SHAKE),1)
COMMON_SOURCES = $(COMMON_SOURCES_BASE) fips202.c fips202x4.c f1600x4.S
HEADERS_EXTRA = fips202.h fips202x4.h
else
COMMON_SOURCES = $(COMMON_SOURCES_BASE)
HEADERS_EXTRA =
endif

SOURCES = $(COMMON_SOURCES) test.c
KAT_KEM_SOURCES = $(COMMON_SOURCES) KAT_KEM.c
CYCLES_SOURCES = $(COMMON_SOURCES) cycles_test.c

HEADERS = auxfunc.h drng.h kem_random.h symmetric.h sample.h params.h poly.h pke.h polar.h ntt.h ntt_avx2.h ntt_avx2_params.h fft.h KEM_AlgorithmInstance.h $(HEADERS_EXTRA)

BENCH_SOURCE := $(wildcard bench100k.c)

test: $(HEADERS) $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

kat_kem: $(HEADERS) $(KAT_KEM_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(KAT_KEM_SOURCES) $(LDFLAGS)

cycles_test: $(HEADERS) $(CYCLES_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(CYCLES_SOURCES) $(LDFLAGS)

ifneq ($(BENCH_SOURCE),)
BENCH_SOURCES = $(COMMON_SOURCES) bench100k.c

bench_byte: $(HEADERS) $(BENCH_SOURCES)
	$(CC) $(CFLAGS) -DBYTE_PK=1 -o $@ $(BENCH_SOURCES) $(LDFLAGS)
else
bench_byte:
	@echo "bench100k.c not found; skip bench_byte target."
endif

.PHONY: clean
clean:
	-rm -f test kat_kem cycles_test bench_byte *.o *.s *.bc *.lto.o output.*
