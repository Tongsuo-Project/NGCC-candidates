CC = gcc

PARAMS ?= cedrusc-sm3-160s
THASH ?= simple


OPTFLAGS ?= -O3 -march=native -mavx2 -fomit-frame-pointer -funroll-loops
WARNFLAGS ?= -Wall -Wextra
FEATURE_DEFS = -DSPX_ENABLE_X8_WOTS
CFLAGS = $(OPTFLAGS) $(WARNFLAGS) -flto -DPARAMS=$(PARAMS)

COMMON_SRCS = address.c \
              auxfunc.c \
              counters.c \
              drng.c \
              hash_sm3.c \
              hash_sm3x8.c \
              merkle.c \
              fors.c \
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
          SIG_AlgorithmInstance.h \
          hashx8.h \
          sm3x8.h \
          thashx8.h \
          utils_avx2.h \
          wots_avx2.h \
          params/params-$(PARAMS).h

.PHONY: all clean show-config

all: KAT_SIG

show-config:
	@echo "PARAMS=$(PARAMS)"
	@echo "THASH=$(THASH)"
	@echo "ALGORITHM_INSTANCE=$(ALGORITHM_INSTANCE)"
	@echo "HASH=SM3"
	@echo "SIMD_BATCH=x8 WOTS/Merkle"

KAT_SIG: KAT_SIG.o sign.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o KAT_SIG
