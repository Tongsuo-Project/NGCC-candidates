CC ?= gcc

PARAMS ?= flextree-256s
THASH ?= simple


# Keep this target AVX2-only; -march=native may enable wider host-specific SIMD.
OPTFLAGS ?= -O3 -march=native -mavx2 -fomit-frame-pointer -funroll-loops
WARNFLAGS ?= -std=c99 -Wpedantic -Wall -Wextra

FEATURE_DEFS = -DSPX_ENABLE_X8_WOTS -DSPX_ENABLE_X8_PORS

DEFS = -DPARAMS=$(PARAMS) $(FEATURE_DEFS)
CFLAGS = $(OPTFLAGS) $(WARNFLAGS) -flto $(DEFS) $(EXTRA_CFLAGS)
LIBS =

COMMON_SRCS = address.c \
              auxfunc.c \
              counters.c \
              drng.c \
              hash_sm3.c \
              hash_sm3x8.c \
              merkle.c \
              pors_fp.c \
              randombytes.c \
              SIG_AlgorithmInstance.c \
              sm3x8.c \
              thash_sm3_$(THASH).c \
              thash_sm3_$(THASH)x8.c \
              utils.c \
              utils_avx2.c \
              utilsx1.c \
              wots.c \
              wotsx1.c

COMMON_OBJS = $(COMMON_SRCS:.c=.o)

HEADERS = params.h \
          pors_precomputed.h \
          SIG_AlgorithmInstance.h \
          hashx8.h \
          sm3x8.h \
          thashx8.h \
          utils_avx2.h \
          wots_avx2.h \
          params/params-$(PARAMS).h \
          params/pors_precomputed-$(PARAMS).h

.PHONY: all clean show-config

all: KAT_SIG

show-config:
	@echo "PARAMS=$(PARAMS)"
	@echo "THASH=$(THASH)"
	@echo "ALGORITHM_INSTANCE is defined in SIG_AlgorithmInstance.h"
	@echo "SIMD_BATCH=x8"
	@echo "CFLAGS=$(CFLAGS)"

KAT_SIG: KAT_SIG.o sign.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -f *.o KAT_SIG
