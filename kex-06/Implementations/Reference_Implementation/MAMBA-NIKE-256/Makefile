# Makefile for MAMBA-NIKE KAT KEX test
CC = /usr/bin/gcc
CFLAGS = -std=c99 -Wall -Wextra -O3 -fomit-frame-pointer -DKAT_BUILD

ALGO_SRC = KEX_AlgorithmInstance.c
ALGO_OBJ = crypto_stream_chacha20.o poly.o toom.o error_correction.o \
           nike.o reduce.o fips202.o

all: KAT_KEX

KAT_KEX: $(ALGO_OBJ) $(ALGO_SRC) KAT_KEX.c auxfunc.c drng.c
	$(CC) $(CFLAGS) -o KAT_KEX $(ALGO_SRC) $(ALGO_OBJ) KAT_KEX.c auxfunc.c drng.c -lm

crypto_stream_chacha20.o: crypto_stream_chacha20.c crypto_stream_chacha20.h
	$(CC) $(CFLAGS) -c crypto_stream_chacha20.c

poly.o: poly.c poly.h params.h reduce.h toom.h
	$(CC) $(CFLAGS) -c poly.c

toom.o: toom.c toom.h params.h
	$(CC) $(CFLAGS) -c toom.c

error_correction.o: error_correction.c error_correction.h params.h poly.h
	$(CC) $(CFLAGS) -c error_correction.c

nike.o: nike.c nike.h poly.h error_correction.h fips202.h
	$(CC) $(CFLAGS) -c nike.c

reduce.o: reduce.c reduce.h params.h
	$(CC) $(CFLAGS) -c reduce.c

fips202.o: fips202.c fips202.h
	$(CC) $(CFLAGS) -c fips202.c

run: KAT_KEX
	./KAT_KEX

clean:
	rm -rf KAT_KEX *.o output

.PHONY: all run clean
