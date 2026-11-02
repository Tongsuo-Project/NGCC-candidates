# Rhyme-SM3 Optimized Implementation Makefile (AVX2)
# Instance: Rhyme-SM3-256

INST := Rhyme-SM3-256
MODE := 256

CC      ?= gcc
CFLAGS  ?= -O3 -march=native -mavx2 -std=c99 -Wall -Wextra -I. -Iinclude -Isrc/keygen \
           -DRHYME_NO_AES \
           -DRHYME_MODE=$(MODE) \
           -DOUTPUT_BLANK_TEST_VECTORS=0 \
           -DUSE_AVX2_SM3 \
           -DALGORITHM_INSTANCE='"$(INST)"'
LDLIBS   = -lm

CORE = src/poly.c \
       src/ntt.c \
       src/ntt_tables.c \
       src/sampler.c \
       src/encoding.c \
       src/packing.c \
       src/sign.c \
       src/zpntt.c \
       src/symmetric-shake.c \
       sm3.c sm3_xof.c simd/sm3_avx2.c \
       rhyme_xof.c \
       src/keygen/kg_main.c \
       src/keygen/kg_solver.c \
       src/keygen/kg_zint.c \
       src/keygen/kg_ntt.c \
       src/keygen/kg_primes.c

KAT_DEPS = randombytes.c auxfunc.c drng.c SIG_AlgorithmInstance.c KAT_SIG.c

SRCS = $(CORE) $(KAT_DEPS)

all: KAT_SIG

KAT_SIG: $(SRCS)
	$(CC) $(CFLAGS) -o KAT_SIG $(SRCS) $(LDLIBS)

clean:
	rm -f KAT_SIG *.o src/*.o src/keygen/*.o simd/*.o
