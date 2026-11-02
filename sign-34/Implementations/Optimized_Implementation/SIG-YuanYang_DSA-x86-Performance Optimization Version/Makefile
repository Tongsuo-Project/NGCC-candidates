CC ?= gcc
CFLAGS ?= -std=c99 -Ofast -Wall -Wextra -Wpedantic -fopenmp \
	-march=native -flto -fomit-frame-pointer \
	-DYUANYANG_FORCE_SOFTWARE_RANDOMNESS
LDLIBS ?= -lm
ifeq ($(OS),Windows_NT)
LDLIBS += -lbcrypt
endif

PARAM ?= 512
BUILD_DIR ?= build/$(PARAM)
PARAM_DIR := yuanyang-$(PARAM)
BENCH_RUNS ?= 1000
BENCH_WARMUP_RUNS ?= 2
BATCH_BENCH_RUNS ?= $(BENCH_RUNS)
BATCH_PRESAMPLE_DEPTH ?= 256
BENCH_CPU ?= 1
BENCH_RUNNER ?= taskset -c $(BENCH_CPU)
GENERIC_CFLAGS ?= -std=c99 -Ofast -Wall -Wextra -Wpedantic -fopenmp \
	-march=x86-64 -flto -fomit-frame-pointer \
	-DYUANYANG_FORCE_SOFTWARE_RANDOMNESS

HOST_ARCH ?= $(shell uname -m 2>/dev/null || echo unknown)
ifneq ($(filter x86_64 amd64 i386 i486 i586 i686,$(HOST_ARCH)),)
YUANYANG_AVX2_DEFAULT := 1
else
YUANYANG_AVX2_DEFAULT := 0
endif
YUANYANG_ENABLE_AVX2 ?= $(YUANYANG_AVX2_DEFAULT)
ifeq ($(filter -DYUANYANG_HAVE_AVX2_IMPL=%,$(CFLAGS)),)
override CFLAGS += -DYUANYANG_HAVE_AVX2_IMPL=$(YUANYANG_ENABLE_AVX2)
endif

INCLUDES = -I. -I$(PARAM_DIR) -I$(PARAM_DIR)/keygen \
	-I$(PARAM_DIR)/keygen/ntrugen/src
COMMON_SRC = SIG_AlgorithmInstance.c codec.c common.c ntt.c poly_inv_ntt.c \
	keygen/keygen.c sign.c sign_ntt.c vrfy.c keygen/pairgen.c \
	keygen/perturbation.c sampler.c prng.c fpr.c fft.c keygen/fft.c \
	keygen/security_loss.c auxfunc.c drng.c

ifeq ($(PARAM),512)
NTRUGEN_TWIDDLES = keygen/ntrugen/src/twiddles9.c
else ifeq ($(PARAM),1024)
NTRUGEN_TWIDDLES = keygen/ntrugen/src/twiddles10.c
else ifeq ($(PARAM),2048)
NTRUGEN_TWIDDLES = keygen/ntrugen/src/twiddles11.c
else
$(error PARAM must be one of 512, 1024, or 2048)
endif

NTRUGEN_SRC = keygen/ntrugen/src/ntrugen.c \
	keygen/ntrugen/src/bigint.c keygen/ntrugen/src/bigintgcd.c \
	keygen/ntrugen/src/cfft.c keygen/ntrugen/src/complex.c \
	keygen/ntrugen/src/fft.c $(NTRUGEN_TWIDDLES)
ifeq ($(YUANYANG_ENABLE_AVX2),1)
ISA_SRC = ntt_avx2.c sign_ntt_avx2.c
else
ISA_SRC =
endif
CORE_SRC = $(COMMON_SRC) $(NTRUGEN_SRC)
CORE_OBJ = $(patsubst %.c,$(BUILD_DIR)/ref/%.o,$(CORE_SRC))
ISA_OBJ = $(patsubst %.c,$(BUILD_DIR)/isa/%.o,$(ISA_SRC))
LOCAL_CORE_SRC = sysrng.c cpu_features.c
LOCAL_CORE_OBJ = $(patsubst %.c,$(BUILD_DIR)/local-core/%.o,$(LOCAL_CORE_SRC))
DRNG_ALGORITHM_OBJ = $(BUILD_DIR)/ref/drng_algorithm.o
BENCH_EXE = SIG_bench-$(PARAM)
BATCH_BENCH_EXE = SIG_bench_batch-$(PARAM)
BENCH_OBJ = $(BUILD_DIR)/param/benchmark.o
BATCH_BENCH_OBJ = $(BUILD_DIR)/param/benchmark_batch.o
KAT_OBJ = $(BUILD_DIR)/kat/KAT_SIG.o
CHECK_KAT_OBJ = $(BUILD_DIR)/kat/check_kat.o
VERIFY_KAT_OBJ = $(BUILD_DIR)/kat/verify_kat.o
FUNCTIONAL_TEST_OBJ = $(BUILD_DIR)/local/functional_test.o
DEPS = $(CORE_OBJ:.o=.d) $(ISA_OBJ:.o=.d) $(LOCAL_CORE_OBJ:.o=.d) \
	$(DRNG_ALGORITHM_OBJ:.o=.d) $(BENCH_OBJ:.o=.d) \
	$(BATCH_BENCH_OBJ:.o=.d) $(KAT_OBJ:.o=.d) \
	$(CHECK_KAT_OBJ:.o=.d) $(VERIFY_KAT_OBJ:.o=.d) \
	$(FUNCTIONAL_TEST_OBJ:.o=.d)

all: KAT_SIG check_kat verify_kat $(BENCH_EXE) $(BATCH_BENCH_EXE) functional_test

$(BENCH_EXE): $(BENCH_OBJ) $(CORE_OBJ) $(ISA_OBJ) $(LOCAL_CORE_OBJ) \
	$(DRNG_ALGORITHM_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BATCH_BENCH_EXE): $(BATCH_BENCH_OBJ) $(CORE_OBJ) $(ISA_OBJ) \
	$(LOCAL_CORE_OBJ) $(DRNG_ALGORITHM_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

KAT_SIG: $(KAT_OBJ) $(CORE_OBJ) $(ISA_OBJ) $(LOCAL_CORE_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

check_kat: $(CHECK_KAT_OBJ) $(CORE_OBJ) $(ISA_OBJ) $(LOCAL_CORE_OBJ) \
	$(DRNG_ALGORITHM_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

verify_kat: $(VERIFY_KAT_OBJ) $(CORE_OBJ) $(ISA_OBJ) $(LOCAL_CORE_OBJ) \
	$(DRNG_ALGORITHM_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

functional_test: $(FUNCTIONAL_TEST_OBJ) $(CORE_OBJ) $(ISA_OBJ) \
	$(LOCAL_CORE_OBJ) $(DRNG_ALGORITHM_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

benchmark:
	@$(MAKE) -s -B PARAM=$(PARAM) BUILD_DIR="$(BUILD_DIR)" \
		YUANYANG_ENABLE_AVX2=1 $(BENCH_EXE)
	@$(BENCH_RUNNER) ./$(BENCH_EXE) $(BENCH_WARMUP_RUNS) >/dev/null 2>&1
	@$(BENCH_RUNNER) ./$(BENCH_EXE) $(BENCH_RUNS)

benchmark_batch:
	@$(MAKE) -s -B PARAM=$(PARAM) BUILD_DIR="$(BUILD_DIR)" \
		YUANYANG_ENABLE_AVX2=1 $(BATCH_BENCH_EXE)
	@$(BENCH_RUNNER) ./$(BATCH_BENCH_EXE) $(BENCH_WARMUP_RUNS) \
		$(BATCH_PRESAMPLE_DEPTH) >/dev/null 2>&1
	@$(BENCH_RUNNER) ./$(BATCH_BENCH_EXE) $(BATCH_BENCH_RUNS) \
		$(BATCH_PRESAMPLE_DEPTH)

benchmark-generic:
	$(MAKE) PARAM=$(PARAM) BUILD_DIR="$(BUILD_DIR)-generic" \
		BENCH_EXE="$(BENCH_EXE)-generic" YUANYANG_ENABLE_AVX2=0 \
		CFLAGS="$(GENERIC_CFLAGS)" $(BENCH_EXE)-generic
	YUANYANG_CPU=generic ./$(BENCH_EXE)-generic $(BENCH_WARMUP_RUNS) >/dev/null
	YUANYANG_CPU=generic ./$(BENCH_EXE)-generic $(BENCH_RUNS)

kat_ngcc:
	$(MAKE) PARAM=$(PARAM) BUILD_DIR="$(BUILD_DIR)-ngcc-kats" \
		CFLAGS="$(CFLAGS) -DNGCC_KATS" KAT_SIG check_kat verify_kat
	./KAT_SIG
	./check_kat output/KAT_SIG_yuanyang-$(PARAM).txt
	./verify_kat output/KAT_SIG_yuanyang-$(PARAM).txt

benchmark-all:
	$(MAKE) PARAM=512 benchmark
	@echo ""
	$(MAKE) PARAM=1024 benchmark
	@echo ""
	$(MAKE) PARAM=2048 benchmark

benchmark-batch-all:
	$(MAKE) PARAM=512 benchmark_batch
	@echo ""
	$(MAKE) PARAM=1024 benchmark_batch
	@echo ""
	$(MAKE) PARAM=2048 benchmark_batch

kat-all:
	$(MAKE) PARAM=512 kat_ngcc
	@echo ""
	$(MAKE) PARAM=1024 kat_ngcc
	@echo ""
	$(MAKE) PARAM=2048 kat_ngcc

$(BUILD_DIR)/local/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/local-core/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/param/benchmark.o: $(PARAM_DIR)/benchmark.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) \
		-DYUANYANG_BENCH_BUILD_FLAGS='"$(CFLAGS)"' \
		-DYUANYANG_BENCH_BUILD_TOOL='"GNU Make $(MAKE_VERSION)"' \
		-MMD -MP -c -o $@ $<

$(BUILD_DIR)/param/benchmark_batch.o: $(PARAM_DIR)/benchmark_batch.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/ref/%.o: $(PARAM_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/isa/%.o: $(PARAM_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -mavx2 $(INCLUDES) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/kat/KAT_SIG.o: $(PARAM_DIR)/KAT_SIG.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/kat/check_kat.o: $(PARAM_DIR)/check_kat.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/kat/verify_kat.o: $(PARAM_DIR)/verify_kat.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $<

clean:
	rm -rf build build-benchmark benchmark-results output KAT_SIG check_kat \
		verify_kat functional_test SIG_bench-* SIG_bench_batch-*

-include $(DEPS)

.PHONY: all benchmark benchmark_batch benchmark-generic benchmark-all \
	benchmark-batch-all kat_ngcc kat-all clean
