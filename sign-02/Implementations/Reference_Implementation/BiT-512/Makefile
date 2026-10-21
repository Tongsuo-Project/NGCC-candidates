# BiT reference implementation — x86 evaluation guide §3.2 (Table 1)
CC       = gcc
CFLAGS  ?= -std=c99 -Wpedantic -Wall -Wextra -O2
CPPFLAGS ?= -I.

TARGET   = KAT_SIG

# All .c files except KAT_SIG.c (which owns main()); packing.c ordered last
# so its overrides take precedence.
SRCS = $(filter-out KAT_SIG.c, $(wildcard *.c))
SRCS := $(filter-out packing.c, $(SRCS)) packing.c
OBJS = $(SRCS:.c=.o)

.PHONY: all correctness speed clean

all: $(TARGET)

$(TARGET): KAT_SIG.c $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@

correctness: $(OBJS) test/test_correctness.c test/test_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) test/test_correctness.c $(OBJS) -o $@
	./$@

# Unity build: all sources in one gcc invocation → aggressive cross-file inlining
# for the tiny helpers (NTT butterflies, Barrett reductions) that dominate cycles.
speed: test/test_speed.c test/cpucycles.c test/speed_print.c test/test_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -DNTESTS=1000 \
	  test/test_speed.c test/cpucycles.c test/speed_print.c \
	  $(SRCS) -o $@ -lm
	./$@

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) correctness speed
