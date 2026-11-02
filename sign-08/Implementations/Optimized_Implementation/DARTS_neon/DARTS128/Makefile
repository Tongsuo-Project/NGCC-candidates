CC ?= /usr/bin/cc
ARCH ?= armv8
DARTS_MODE ?= 512
CFLAGS += -Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls \
  -Wshadow -Wpointer-arith -O3 -std=c99 -DDARTS_MODE=$(DARTS_MODE) \
  -fcommon

ifeq ($(ARCH),armv8)
  CFLAGS += -march=armv8-a+simd
endif
# ---------- source files ----------
SIGN_SRC = sign.c packing.c polyvec.c poly.c ntt.c consts.c \
           sampler.c encoding.c polyfix.c polymat.c fft.c fixpoint.c

SYM_SRC  = symmetric.c

ICCS_SRC = drng.c auxfunc.c

KAT_SRC  = SIG_AlgorithmInstance.c

SOURCES  = $(SIGN_SRC) $(SYM_SRC) $(ICCS_SRC) $(KAT_SRC)

# ---------- test directory ----------
TEST_DIR = test
HEADERS  = $(wildcard *.h) $(wildcard $(TEST_DIR)/*.h)

# ---------- targets ----------
.PHONY: all test KAT clean

BIN_DIR = bin/$(DARTS_MODE)

all: $(BIN_DIR)/KAT_SIG $(BIN_DIR)/test_mul $(BIN_DIR)/test_DARTS $(BIN_DIR)/test_speed $(BIN_DIR)/test_neon_reduce

# ===================== make KAT =====================
KAT: $(BIN_DIR)/KAT_SIG

$(BIN_DIR):
	mkdir -p $@

$(BIN_DIR)/KAT_SIG: KAT_SIG.c $(SOURCES) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ KAT_SIG.c $(SOURCES) -lm

# ===================== make test =====================
test: $(BIN_DIR)/test_mul $(BIN_DIR)/test_DARTS $(BIN_DIR)/test_speed $(BIN_DIR)/test_neon_reduce

$(BIN_DIR)/test_mul: $(TEST_DIR)/test_mul.c $(SOURCES) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_mul.c $(SOURCES) -lm

$(BIN_DIR)/test_DARTS: $(TEST_DIR)/test_DARTS.c $(SOURCES) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_DARTS.c $(SOURCES) -lm

$(BIN_DIR)/test_speed: $(TEST_DIR)/test_speed.c $(TEST_DIR)/cpucycles.c $(TEST_DIR)/speed_print.c $(SOURCES) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_speed.c $(TEST_DIR)/cpucycles.c $(TEST_DIR)/speed_print.c $(SOURCES) -lm

$(BIN_DIR)/test_neon_reduce: $(TEST_DIR)/test_neon_reduce.c consts.c $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_DIR)/test_neon_reduce.c consts.c -lm

# ===================== clean =====================
clean:
	-rm -rf bin
	-rm -rf output
