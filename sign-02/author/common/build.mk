# Copyright (c) 2026 Hang Zhang.

.DEFAULT_GOAL := all
CC = gcc
ifeq ($(IMPLEMENTATION),ref)
# Reference benchmark flags from the specification, section 6.2.1.
CFLAGS ?= -std=c99 -Wpedantic -Wall -Wextra -O2
else
CFLAGS ?= -std=c99 -Wpedantic -Wall -Wextra -O3 -fomit-frame-pointer
endif
CPPFLAGS ?=
LDLIBS ?= -lm
BACKEND ?= shake
NTESTS ?= 1000
RUNNER ?=

ifeq ($(BACKEND),sm3)
override CPPFLAGS += -DBIT_USE_SHAKE=0
else ifneq ($(BACKEND),shake)
$(error BACKEND must be shake or sm3)
endif
override CPPFLAGS += -I. -Itest

SRCS := $(filter-out KAT_SIG.c,$(wildcard *.c))
OBJS := $(SRCS:.c=.o)
ifeq ($(IMPLEMENTATION),avx)
override CFLAGS += -mavx2
ASM_SRCS := $(wildcard *.S)
OBJS += $(ASM_SRCS:.S=_avx.o) keccakx4_lanes.o
else ifneq ($(IMPLEMENTATION),ref)
$(error IMPLEMENTATION must be ref or avx)
endif

HEADERS := $(wildcard *.h *.inc ../../common/*.h ../../common/*.inc)
TEST_HEADERS := $(wildcard test/*.h)
$(OBJS): $(HEADERS)
-include $(OBJS:.o=.d)

.PHONY: all check correctness speed clean message-binding
all: KAT_SIG
check: correctness
ifneq ($(wildcard test/test_message_binding.c),)
check: message-binding

message-binding: test/test_message_binding.c $(TEST_HEADERS) $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(OBJS) -Wl,--wrap=bit_hash_message $(LDLIBS) -o $@
	$(RUNNER) ./$@
endif

KAT_SIG: KAT_SIG.c $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ $(LDLIBS) -o $@

correctness: test/test_correctness.c $(TEST_HEADERS) $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(OBJS) $(LDLIBS) -o $@
	$(RUNNER) ./$@

speed: test/test_speed.c test/cpucycles.c test/speed_print.c $(TEST_HEADERS) $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DNTESTS=$(NTESTS) \
	  test/test_speed.c test/cpucycles.c test/speed_print.c $(OBJS) $(LDLIBS) -o $@
	$(RUNNER) ./$@

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

ifeq ($(IMPLEMENTATION),avx)
%_avx.o: %.S
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

keccakx4_lanes.o: ../common/keccakx4_lanes.c ../common/keccakx4_lanes.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@
endif

clean:
	rm -f KAT_SIG correctness speed message-binding $(OBJS) $(OBJS:.o=.d)
