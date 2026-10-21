CC ?= gcc

CFLAGS ?= -O2 -march=x86-64 -std=c11 -Wpedantic -Wall -Wextra -Wno-missing-braces -MMD -MP -I.
LDFLAGS ?= -O2 -march=x86-64 -lcrypto

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
	greatwall.c \
	greatwall-512-matrix.c \
	greatwall_impl.c \
	hash.c \
	KeccakHash.c \
	KeccakP-1600-reference.c \
	KeccakSponge.c \
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

OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.d $(TARGET) bench_sig
	rm -rf output

-include $(DEPS)

.PHONY: all clean
