BUILD ?= performance

# Default to Clang performance tuning; select BUILD=ngcc for GCC.
ifeq ($(BUILD),ngcc)
CC = gcc
CFLAGS += -std=c99 -Wpedantic -Wall -Wextra -O2
else ifeq ($(BUILD),performance)
CC = clang
CFLAGS += -O3 -march=native -fomit-frame-pointer
else
$(error Unsupported BUILD "$(BUILD)"; use ngcc or performance)
endif

HEADERS = auxfunc.h drng.h sample.h ntt.h poly.h symmetric.h pke.h KEM_AlgorithmInstance.h
SOURCES = auxfunc.c drng.c sample.c ntt.c poly.c symmetric.c pke.c KEM_AlgorithmInstance.c
KAT_KEM_SOURCES = $(SOURCES) KAT_KEM.c
SPEED_KEM_SOURCES = $(SOURCES) cpucycles.c SPEED_KEM.c 


KAT_KEM: Makefile $(HEADERS) $(KAT_KEM_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(KAT_KEM_SOURCES)
	@rm -f .build-*-$@
	@touch .build-$(BUILD)-$@

SPEED_KEM: Makefile $(HEADERS) $(SPEED_KEM_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SPEED_KEM_SOURCES)
	@rm -f .build-*-$@
	@touch .build-$(BUILD)-$@

# Track each executable separately, including switches within one second.
ifeq ($(wildcard .build-$(BUILD)-KAT_KEM),)
KAT_KEM: FORCE
endif
ifeq ($(wildcard .build-$(BUILD)-SPEED_KEM),)
SPEED_KEM: FORCE
endif

FORCE:

.PHONY: clean FORCE
clean:
	-rm KAT_KEM SPEED_KEM
	-rm -f .build-*-KAT_KEM .build-*-SPEED_KEM

