CC      ?= gcc
CFLAGS  ?= -O3 -march=x86-64 -mavx2 -mtune=native -flto \
           -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra \
           -Wno-unused-parameter -Wno-unused-result -I . -I src

NTRE_SRC = src/poly.c src/ntt.c src/symmetric.c KEM_AlgorithmInstance.c
NGCC_SRC = auxfunc.c drng.c
ASM_SRC  = asm/add.s
KAT_SRC  = KAT_KEM.c

.PHONY: all clean test kat

all: bin/KAT_KEM

bin/KAT_KEM: $(NTRE_SRC) $(NGCC_SRC) $(ASM_SRC) $(KAT_SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(NTRE_SRC) $(NGCC_SRC) $(ASM_SRC) $(KAT_SRC)

test:
	$(MAKE) -C test

kat: bin/KAT_KEM
	./bin/KAT_KEM

clean:
	rm -rf bin output
	$(MAKE) -C test clean
