CC ?= /usr/bin/cc
CFLAGS += -Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls \
  -Wshadow -Wpointer-arith -mavx2 -mbmi2 -mpopcnt \
  -march=native -mtune=native -O3 -fomit-frame-pointer -z noexecstack \
  -DSECURITY_LEVEL=128
RM = /bin/rm

SOURCES = kem.c indcpa.c polyvec.c poly.c ntt.c cbd.c reduce.c verify.c \
          consts.c rejsample.c drng.c auxfunc.c symmetric-shake.c fq.S
HEADERS = params.h kem.h indcpa.h polyvec.h poly.h ntt.h cbd.h reduce.h \
          verify.h consts.h rejsample.h symmetric.h drng.h auxfunc.h \
          KEM_AlgorithmInstance.h zetas_7681_avx.h align.h
KAT_SOURCES = KAT_KEM.c KEM_AlgorithmInstance.c

.PHONY: all kat clean

all: kat

kat: $(SOURCES) $(HEADERS) $(KAT_SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) $(KAT_SOURCES) -o kem_kat $(LDFLAGS)

clean:
	-$(RM) -f *.gcno *.gcda *.lcov *.o
	-$(RM) -f kem_kat
	-$(RM) -rf output/
