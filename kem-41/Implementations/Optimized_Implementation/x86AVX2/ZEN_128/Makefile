CC = gcc
CFLAGS += -O3 -march=x86-64 -mavx2 -mtune=native -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra

HEADERS = auxfunc.h drng.h sample.h ntt.h poly.h symmetric.h pke.h KEM_AlgorithmInstance.h
SOURCES = auxfunc.c drng.c sample.c ntt.S poly.c symmetric.c pke.c KEM_AlgorithmInstance.c
KAT_KEM_SOURCES = $(SOURCES) KAT_KEM.c
SPEED_KEM_SOURCES = $(SOURCES) cpucycles.c SPEED_KEM.c 
TEST_KEM_SOURCES = $(SOURCES) cpucycles.c TEST_KEM.c 

KAT_KEM: $(HEADERS) $(KAT_KEM_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(KAT_KEM_SOURCES)

SPEED_KEM: $(HEADERS) $(SPEED_KEM_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SPEED_KEM_SOURCES)

TEST_KEM: $(HEADERS) $(TEST_KEM_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_KEM_SOURCES)

.PHONY: clean
clean:
	-rm KAT_KEM SPEED_KEM TEST_KEM

