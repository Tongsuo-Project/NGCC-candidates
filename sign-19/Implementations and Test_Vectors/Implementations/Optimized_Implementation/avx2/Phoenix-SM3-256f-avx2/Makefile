ALGORITHM_INSTANCE = Phoenix-SM3-256f-avx2
PARAMS = phoenix-sm3-256f

CC ?= gcc
CFLAGS ?= -O3 -std=c99 -march=native
CFLAGS += -DPARAMS=$(PARAMS) -I. -DALLOW_DEEP_TREES -DSUBMISSION_DRNG -DAVX2_OPTIMIZED

CORE_SOURCES = \
	address.c \
	auxfunc.c \
	counter.c \
	hash_sm3.c sm3_drng.c hash_sm3x4.c hash_sm3x8.c \
	merkle.c \
	octopus.c \
	pseudoXOF_avx2.c \
	randombytes.c \
	sign.c \
	tfors.c \
	thash_sm3_simple.c thash_sm3_simplex8.c thash_sm3_simplex4.c \
	utils.c \
	utilsx1.c \
	utilsx4.c \
	utilsx8.c \
	gwots.c \
	gwotsx1.c

AVX2_SOURCES = \
	sm3_avx2.c \
	sm3_x4.c \
	sm3_x8.c

SUBMISSION_SOURCES = \
	SIG_AlgorithmInstance.c \
	drng.c \
	KAT_SIG.c

.PHONY: all clean kat benchmark test-sm3-xofx4 test-pseudoXOF-avx2

all: KAT_SIG

kat: KAT_SIG
	./KAT_SIG

KAT_SIG: $(CORE_SOURCES) $(AVX2_SOURCES) $(SUBMISSION_SOURCES) SIG_AlgorithmInstance.h drng.h
	$(CC) $(CFLAGS) -DPARAMNAME=\"$(ALGORITHM_INSTANCE)\" -o $@ $(CORE_SOURCES) $(AVX2_SOURCES) $(SUBMISSION_SOURCES)

benchmark: $(CORE_SOURCES) $(AVX2_SOURCES) test/benchmark.c test/cycles.c
	$(CC) $(CFLAGS) -U SUBMISSION_DRNG -DPARAMNAME=\"$(ALGORITHM_INSTANCE)\" -o $@ $(CORE_SOURCES) $(AVX2_SOURCES) test/benchmark.c test/cycles.c


test-sm3-xofx4: $(CORE_SOURCES) $(AVX2_SOURCES) test/sm3_xofx4.c
	$(CC) $(CFLAGS) -U SUBMISSION_DRNG -DPARAMNAME=\"$(ALGORITHM_INSTANCE)\" -o $@ $(CORE_SOURCES) $(AVX2_SOURCES) test/sm3_xofx4.c

test-pseudoXOF-avx2: $(CORE_SOURCES) $(AVX2_SOURCES) pseudoXOF_avx2.c test_pseudoXOF_avx2.c
	$(CC) $(CFLAGS) -U SUBMISSION_DRNG -DPARAMNAME=\"$(ALGORITHM_INSTANCE)\" -o $@ $(CORE_SOURCES) $(AVX2_SOURCES) pseudoXOF_avx2.c test_pseudoXOF_avx2.c

clean:
	$(RM) KAT_SIG
	$(RM) benchmark
	$(RM) test-sm3-xofx4
	$(RM) test-pseudoXOF-avx2
	$(RM) -r output

