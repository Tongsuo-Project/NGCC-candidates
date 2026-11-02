CC ?= gcc
LDFLAGS ?=
BIT_USE_SHAKE ?= 0
RL_KEM_USE_CONJ_NTT_REJECTION ?= 0
POLARLAC_COMPONENT_B_FFT_SELECTED ?= 0
POLARLAC_COMPONENT_B_TERNARY_SELECTED ?= 0
POLARLAC_COMPONENT_B_UNIFORM_SELECTED ?= 0
EXTRA_CFLAGS ?=

COMMON_CFLAGS := -O3 -march=armv8.2-a+sve -flto=1 -flto-partition=none -fomit-frame-pointer \
                 -std=c99 -Wpedantic -Wall -Wextra \
                 -DPOLARLAC_USE_ARM_MLWE_ARITH=1 \
                 -DPOLARLAC_COMPONENT_B_FFT_SELECTED=$(POLARLAC_COMPONENT_B_FFT_SELECTED) \
                 -DPOLARLAC_COMPONENT_B_TERNARY_SELECTED=$(POLARLAC_COMPONENT_B_TERNARY_SELECTED) \
                 -DPOLARLAC_COMPONENT_B_UNIFORM_SELECTED=$(POLARLAC_COMPONENT_B_UNIFORM_SELECTED) \
                 -DBIT_USE_SHAKE=$(BIT_USE_SHAKE) \
                 -DRL_KEM_USE_CONJ_NTT_REJECTION=$(RL_KEM_USE_CONJ_NTT_REJECTION)
ifeq ($(BIT_USE_SHAKE),1)
COMMON_CFLAGS += -D_DEFAULT_SOURCE
endif
CFLAGS ?= $(COMMON_CFLAGS)
CFLAGS += $(EXTRA_CFLAGS)

COMMON_SOURCES= auxfunc.c drng.c fips202.c symmetric.c sample.c poly.c pke.c polar.c ntt.c fft.c KEM_AlgorithmInstance.c
SOURCES= $(COMMON_SOURCES) test.c
KAT_KEM_SOURCES= $(COMMON_SOURCES) KAT_KEM.c
CYCLES_SOURCES= $(COMMON_SOURCES) cycles_test.c
HEADERS= auxfunc.h drng.h fips202.h symmetric.h sample.h params.h poly.h pke.h polar.h ntt.h arm_mlwe_arith.h fft.h arm_mlwe_sampler_fft.h random_fips.h KEM_AlgorithmInstance.h

test: $(HEADERS) $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

kat_kem: $(HEADERS) $(KAT_KEM_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(KAT_KEM_SOURCES) $(LDFLAGS)

cycles_test: $(HEADERS) $(CYCLES_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(CYCLES_SOURCES) $(LDFLAGS)

.PHONY: clean
clean:
	-rm -f test kat_kem cycles_test *.o *.su
	-rm -rf output
