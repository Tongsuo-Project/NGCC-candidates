CC =gcc
CFLAGS += -Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls \
  -Wshadow -Wpointer-arith -O3 -fomit-frame-pointer -mavx2 -march=native \
  -mtune=native -funroll-loops -flto
NISTFLAGS += -Wno-unused-result -O3 -fomit-frame-pointer -mavx2 -march=native \
  -mtune=native -funroll-loops -flto
RM = /bin/rm

SOURCES= auxfunc.c drng.c  poly.c symmetric.c avx2.c ntt_asm_consts.c asm/ntt_full.s asm/invntt_full.s asm/baseinv_full.s asm/basemul_full.s asm/baseadd_full.s
HEADERS= auxfunc.h drng.h  params.h poly.h ntt.h symmetric.h reduce.h avx2.h
 
SOURCES_KAT = $(SOURCES) KEM_AlgorithmInstance.c
HEADERS_KAT = $(HEADERS) KEM_AlgorithmInstance.h

.PHONY: all KAT clean

all: KAT

KAT: $(HEADERS_KAT) $(SOURCES_KAT) KAT_KEM.c
	$(CC) $(NISTFLAGS) -o $@ KAT_KEM.c $(SOURCES_KAT) $(LDFLAGS)

clean:
	$(RM) -f KAT
