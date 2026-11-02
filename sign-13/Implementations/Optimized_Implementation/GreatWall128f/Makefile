CC ?= gcc

CFLAGS ?= -O3 -march=x86-64 -mavx2 -maes -mpclmul -mtune=native -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra
CPPFLAGS += -I.
LDFLAGS ?= -O3 -march=x86-64 -mavx2 -maes -mpclmul -mtune=native -flto -fomit-frame-pointer
LDLIBS += -lcrypto

ENC_CONSTRAINTS_ENABLE_TIMING ?= 0
override CPPFLAGS += -DENC_CONSTRAINTS_ENABLE_TIMING=$(ENC_CONSTRAINTS_ENABLE_TIMING)
GF137_POW_BACKEND ?= square
ifeq ($(GF137_POW_BACKEND),square)
override CPPFLAGS += -DGREATWALL_GF137_POW_SQUARE=1
else ifeq ($(GF137_POW_BACKEND),matrix)
override CPPFLAGS += -DGREATWALL_GF137_POW_SQUARE=0
else
$(error GF137_POW_BACKEND must be square or matrix)
endif
GF137_KEYGEN_BACKEND ?= isolated
ifeq ($(GF137_KEYGEN_BACKEND),isolated)
override CPPFLAGS += -DGREATWALL_KEYGEN_GF137_ISOLATED=1
else ifeq ($(GF137_KEYGEN_BACKEND),legacy)
override CPPFLAGS += -DGREATWALL_KEYGEN_GF137_ISOLATED=0
else
$(error GF137_KEYGEN_BACKEND must be isolated or legacy)
endif

HASH_BACKEND ?= sm3
ifeq ($(HASH_BACKEND),sm3)
override CPPFLAGS += -DGREATWALL_HASH_BACKEND_SM3
EXTRA_OBJS := auxfunc.o
endif

TARGET := KAT_SIG

SRCS := \
	KAT_SIG.c \
	SIG_AlgorithmInstance.c \
	drng.c \
	api.c \
	aes.c \
	aes_impl.c \
	block.c \
	block_impl.c \
	faest.c \
	keygen_gf137.c \
	gf137_proof.c \
	greatwall.c \
	greatwall-128-matrix.c \
	greatwall_impl.c \
	hash.c \
	KeccakHash.c \
	KeccakHashtimes4.c \
	KeccakP-1600-times2-SIMD128.c \
	KeccakP-1600-times4-SIMD256.c \
	KeccakP-1600-times8-on4.c \
	KeccakSponge.c \
	KeccakSpongetimes4.c \
	owf_proof.c \
	polynomials.c \
	polynomials_impl.c \
	prgs.c \
	quicksilver.c \
	randomness_randombytes.c \
	small_vole.c \
	transpose.c \
	universal_hash.c \
	util.c \
	vector_com.c \
	vole_check.c \
	vole_commit.c

ASM_SRCS := \
	KeccakP-1600-AVX2.s

OBJS := $(SRCS:.c=.o) $(ASM_SRCS:.s=.o)
DEPS := $(SRCS:.c=.d) $(EXTRA_OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(filter-out randomness_randombytes.o,$(OBJS)) randomness_kat_fixed.o $(EXTRA_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(OBJS) $(EXTRA_OBJS): Makefile

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

auxfunc.o: auxfunc.c auxfunc.h Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

%.o: %.s
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

-include $(DEPS)

CLEAN_PROGRAMS := KAT_SIG bench_sig KAT_SIG_sm3_fixed KAT_SIG_deterministic \
	bench_sig_deterministic api_test test_arithmetic test_rng_failure sync_vector

clean:
	rm -f -- .build-config *.o *.d *.a *.so *.so.* \
		$(CLEAN_PROGRAMS) $(addsuffix .exe,$(CLEAN_PROGRAMS))

.PHONY: all clean

# Rebuild objects when the backend or compiler flags change.
.PHONY: FORCE
FORCE:

.build-config: FORCE
	@printf '%s\n' '$(CC)' '$(CPPFLAGS)' '$(CFLAGS)' '$(LDFLAGS)' '$(LDLIBS)' '$(HASH_BACKEND)' | cmp -s - $@ || \
	  printf '%s\n' '$(CC)' '$(CPPFLAGS)' '$(CFLAGS)' '$(LDFLAGS)' '$(LDLIBS)' '$(HASH_BACKEND)' > $@

$(OBJS) $(EXTRA_OBJS) KAT_SIG.o SIG_AlgorithmInstance.o randomness_kat_fixed.o: .build-config
-include randomness_kat_fixed.d

# Benchmarks use the official DRNG for SM3 and OS randomness for SHAKE.
BENCH_RNG_OBJ := randomness_os.o
ifeq ($(HASH_BACKEND),sm3)
BENCH_RNG_OBJ := randomness_kat_fixed.o
endif
BENCH_OBJS := $(sort $(filter-out KAT_SIG.o randomness_kat_fixed.o randomness_randombytes.o randomness_os.o,$(OBJS)) $(BENCH_RNG_OBJ) SIG_AlgorithmInstance.o $(EXTRA_OBJS))

bench_sig: bench_sig.o $(BENCH_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

bench_sig.o: ../bench_sig.c Makefile .build-config
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

randomness_os.o: .build-config

-include bench_sig.d randomness_os.d
