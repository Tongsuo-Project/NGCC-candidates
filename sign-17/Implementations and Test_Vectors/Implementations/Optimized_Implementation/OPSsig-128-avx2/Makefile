CC ?= /usr/bin/cc

CFLAGS ?= -O3 -march=x86-64 -mavx2 -mtune=native -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra

LDFLAGS ?=
LDLIBS ?=

INSTANCE_DIR ?= $(notdir $(CURDIR))
INSTANCE_NAME ?= $(patsubst %-avx2,%,$(INSTANCE_DIR))
RESULT_VARIANT ?= avx2
SUBMISSION_NAME ?= 数字签名-$(INSTANCE_NAME)-x86-性能优化版
PACKAGE_OUTPUT_DIR ?= $(CURDIR)/../../../Submission_Packages
SELF_EVAL_SCRIPT := ../../../Self_Evaluation/scripts/run_self_eval.sh
PACKAGE_SCRIPT := ../../../Self_Evaluation/scripts/package_submission.sh

ENABLE_POPCNT ?= 0
ifneq ($(ENABLE_POPCNT),0)
CFLAGS += -mpopcnt
endif

SOURCES = \
  sign.c \
  packing.c \
  polyvec.c \
  poly.c \
  rejsample.c \
  ntt.c \
  pointwise.S \
  consts.c \
  reduce.c \
  rounding.c \
  sm3x4.c \
  auxfunc.c \
  drng.c

HEADERS = \
  align.h \
  params.h \
  sign.h \
  packing.h \
  polyvec.h \
  poly.h \
  rejsample.h \
  ntt.h \
  consts.h \
  reduce.h \
  rounding.h \
  sm3x4.h \
  auxfunc.h \
  drng.h

.PHONY: all test speed mem kat ntt_test vectors_test selfeval package clean

all: test speed mem ntt_test vectors_test

test: test_OPSsig

speed: test_speed_OPSsig

mem: test_mem_OPSsig test_static_mem_OPSsig

kat: kat_sig

ntt_test: test_ntt_260602

vectors_test: test_vectors-260602

selfeval:
	$(SELF_EVAL_SCRIPT) "$(CURDIR)" "$(RESULT_VARIANT)" "$(INSTANCE_NAME)" "$(SUBMISSION_NAME)"

package:
	$(PACKAGE_SCRIPT) "$(CURDIR)" "$(SUBMISSION_NAME)" "$(RESULT_VARIANT)" "$(PACKAGE_OUTPUT_DIR)"

test_OPSsig: test_OPSsig.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(SOURCES) $(LDFLAGS) $(LDLIBS)

test_speed_OPSsig: test_speed_OPSsig.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(SOURCES) $(LDFLAGS) -lrt $(LDLIBS)

test_mem_OPSsig: test_mem_OPSsig.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(SOURCES) $(LDFLAGS) $(LDLIBS)

test_static_mem_OPSsig: test_static_mem_OPSsig.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(SOURCES) $(LDFLAGS) $(LDLIBS)

test_ntt: test_ntt.c ntt.c reduce.c params.h ntt.h reduce.h
	$(CC) $(CFLAGS) -o $@ $< ntt.c reduce.c $(LDFLAGS) $(LDLIBS)

test_ntt_260602: test_ntt_260602.c poly.c rejsample.c ntt.c pointwise.S consts.c reduce.c rounding.c sm3x4.c auxfunc.c params.h poly.h rejsample.h ntt.h consts.h reduce.h rounding.h sm3x4.h auxfunc.h
	$(CC) $(CFLAGS) -o $@ $< poly.c rejsample.c ntt.c pointwise.S consts.c reduce.c rounding.c sm3x4.c auxfunc.c $(LDFLAGS) $(LDLIBS)

test_vectors-260602: test_vectors-260602.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(SOURCES) $(LDFLAGS) $(LDLIBS)

kat_sig: KAT_SIG.c SIG_AlgorithmInstance.c $(SOURCES) $(HEADERS) SIG_AlgorithmInstance.h
	$(CC) $(CFLAGS) -o $@ KAT_SIG.c SIG_AlgorithmInstance.c $(SOURCES) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f test_OPSsig test_speed_OPSsig test_mem_OPSsig test_static_mem_OPSsig test_ntt test_ntt_260602 test_vectors-260602 kat_sig
