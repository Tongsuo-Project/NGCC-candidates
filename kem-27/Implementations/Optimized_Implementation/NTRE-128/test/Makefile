CC      ?= gcc
CFLAGS  ?= -O3 -march=x86-64 -mavx2 -mtune=native -flto \
           -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra \
           -Wno-unused-parameter -Wno-unused-result -I .. -I ../src

NTRE_SRC = ../src/poly.c ../src/ntt.c ../src/symmetric.c ../KEM_AlgorithmInstance.c
NGCC_SRC = ../auxfunc.c ../drng.c
ASM_SRC  = ../asm/add.s
TEST_SRC = test.c

.PHONY: all clean

all: test_ntre

test_ntre: $(NTRE_SRC) $(NGCC_SRC) $(ASM_SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $(NTRE_SRC) $(NGCC_SRC) $(ASM_SRC) $(TEST_SRC)

clean:
	rm -f test_ntre
