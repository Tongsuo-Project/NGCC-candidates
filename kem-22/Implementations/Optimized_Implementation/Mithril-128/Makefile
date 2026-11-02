CC = gcc
CFLAGS += -O3
CFLAGS += -Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls \
  -Wshadow -Wpointer-arith -fomit-frame-pointer -mavx2 -mbmi2 -mpopcnt \
  -march=native -mtune=native -z noexecstack

RM = /bin/rm

BASE_FOLDER = .
ARITH_FOLDER = ./arith
UTILS_FOLDER = ./utils

INCLUDES = -I$(BASE_FOLDER) -I$(UTILS_FOLDER) -I$(ARITH_FOLDER)

override SECURITY_LEVEL = 128

SOURCES_RRLWR = $(ARITH_FOLDER)/fprime.c $(ARITH_FOLDER)/poly.c $(ARITH_FOLDER)/poly.S $(ARITH_FOLDER)/ring.c $(ARITH_FOLDER)/packing.c $(ARITH_FOLDER)/packing.S $(ARITH_FOLDER)/uniform.c $(ARITH_FOLDER)/ntt.S $(ARITH_FOLDER)/intt.S $(ARITH_FOLDER)/pointwise.S pke.c kem.c
SOURCES_UTILS = $(UTILS_FOLDER)/auxfunc.c $(UTILS_FOLDER)/drng.c
SOURCES = $(SOURCES_RRLWR) $(SOURCES_UTILS)

HEADERS_RRLWR = $(ARITH_FOLDER)/fprime.h $(ARITH_FOLDER)/poly.h $(ARITH_FOLDER)/ring.h $(ARITH_FOLDER)/packing.h $(ARITH_FOLDER)/uniform.h $(ARITH_FOLDER)/ntt.h $(ARITH_FOLDER)/ntt_param.h $(ARITH_FOLDER)/intt_param.h $(ARITH_FOLDER)/ntt_avx_macros.inc parameters.h hash_domain.h pke.h kem.h
HEADERS_UTILS = $(UTILS_FOLDER)/auxfunc.h $(UTILS_FOLDER)/drng.h
HEADERS = $(HEADERS_RRLWR) $(HEADERS_UTILS)

TEST_TARGET = test/test_KEM$(SECURITY_LEVEL)
UNIT_TEST_TARGET = test/unit_tests_KEM$(SECURITY_LEVEL)
SPEED_TARGET = test/test_speed_KEM$(SECURITY_LEVEL)
KAT_TARGET = KAT/KAT_KEM$(SECURITY_LEVEL)
BREAKDOWN_TARGET = test/test_breakdown_KEM$(SECURITY_LEVEL)
TARGETS = $(TEST_TARGET) $(UNIT_TEST_TARGET) $(SPEED_TARGET) $(KAT_TARGET) $(BREAKDOWN_TARGET)

.PHONY: all test clean speed KAT breakdown

all: test speed KAT breakdown

test: $(TEST_TARGET) $(UNIT_TEST_TARGET)
	@if [ "$(run)" = "1" ]; then \
		./$(TEST_TARGET) && \
		./$(UNIT_TEST_TARGET); \
	fi

speed: $(SPEED_TARGET)
	@if [ "$(run)" = "1" ]; then \
		./$(SPEED_TARGET); \
	fi

KAT: $(KAT_TARGET)
	@if [ "$(run)" = "1" ]; then \
		./$(KAT_TARGET); \
	fi

breakdown: $(BREAKDOWN_TARGET)
	@if [ "$(run)" = "1" ]; then \
		./$(BREAKDOWN_TARGET); \
	fi

$(TEST_TARGET): test/test_KEM%: $(SOURCES) $(HEADERS) test/test_KEM.c
	$(CC) $(INCLUDES) -DRRLWR_SECURITY_LEVEL=$* -DMEASURE_HEURISTICS $(CFLAGS) $(SOURCES) test/test_KEM.c -o $@

$(UNIT_TEST_TARGET): test/unit_tests_KEM%: $(SOURCES) $(HEADERS) test/unit_tests_KEM.c
	$(CC) $(INCLUDES) -DRRLWR_SECURITY_LEVEL=$* $(CFLAGS) $(SOURCES) test/unit_tests_KEM.c -o $@

$(SPEED_TARGET): test/test_speed_KEM%: $(SOURCES) $(HEADERS) test/cpucycles.h test/cpucycles.c test/speed_print.c test/speed_print.h test/test_speed_KEM.c
	$(CC) $(INCLUDES) -DRRLWR_SECURITY_LEVEL=$* $(CFLAGS) $(SOURCES) test/cpucycles.c test/speed_print.c test/test_speed_KEM.c -o $@

$(KAT_TARGET): KAT/KAT_KEM%: $(SOURCES) $(HEADERS) KAT/KAT_KEM.c
	$(CC) $(INCLUDES) -DRRLWR_SECURITY_LEVEL=$* -DOUTPUT_BLANK_TEST_VECTORS=0 -DALGORITHM_INSTANCE=\"KEM_RRLWR$*\" $(CFLAGS) $(SOURCES) KAT/KAT_KEM.c -o $@

$(BREAKDOWN_TARGET): test/test_breakdown_KEM%: $(SOURCES) $(HEADERS) pke_test.c pke_test.h kem_test.c kem_test.h test/cpucycles.h test/cpucycles.c test/test_breakdown_KEM.c
	$(CC) $(INCLUDES) -DRRLWR_SECURITY_LEVEL=$* $(CFLAGS) $(SOURCES) pke_test.c kem_test.c test/cpucycles.c test/test_breakdown_KEM.c -o $@

clean:
	-$(RM) -f $(TARGETS)
	-$(RM) -Rf output
