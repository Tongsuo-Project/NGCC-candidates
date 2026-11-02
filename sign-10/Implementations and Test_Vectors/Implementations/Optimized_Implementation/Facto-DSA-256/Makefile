CC ?= gcc
BASE_CFLAGS = -std=c99 -O3 -flto -fomit-frame-pointer -Wpedantic -Wall -Wextra
LDFLAGS_SO = -shared
TARGET = kat_sig
KYX1_MARCH = rv64imafdcv_zba_zbb_zbc_zbs_zicond_zfh_zfhmin_zve32f_zve32x_zve64d_zve64f_zve64x_zvfh_zvfhmin

FACTODSA_SRC = \
	auxfunc.c \
	drng.c \
	SIG_AlgorithmInstance.c

FACTODSA_OBJ = $(FACTODSA_SRC:.c=.o)

ifeq ($(shell uname -m),x86_64)
	CFLAGS = $(BASE_CFLAGS) -DFACTO_SIMD_AVX2=1 -march=x86-64 -mavx2 -mtune=native
else ifeq ($(shell uname -m),aarch64)
	CFLAGS = $(BASE_CFLAGS) -DFACTO_SIMD_NEON=1 -march=armv8.2-a+sve
else ifeq ($(shell uname -m),arm64)
	CFLAGS = $(BASE_CFLAGS) -DFACTO_SIMD_NEON=1 -march=armv8.2-a+sve
else ifeq ($(shell uname -m),riscv64)
	CFLAGS = $(BASE_CFLAGS) -march=$(KYX1_MARCH)
else
	CFLAGS += $(BASE_CFLAGS)
endif

all: $(TARGET)

tests: run_benchmarks.c
	$(CC) $(CFLAGS) -DNDEBUG run_benchmarks.c -o run_benchmarks

bench: test.c SIG_AlgorithmInstance.c SIG_AlgorithmInstance.h auxfunc.c auxfunc.h drng.c drng.h
	$(CC) $(CFLAGS) -DNDEBUG -o bench_sig test.c SIG_AlgorithmInstance.c auxfunc.c drng.c

$(TARGET): KAT_SIG.o SIG_AlgorithmInstance.o auxfunc.o drng.o
	$(CC) $(CFLAGS) -o $(TARGET) KAT_SIG.o SIG_AlgorithmInstance.o auxfunc.o drng.o

KAT_SIG.o: KAT_SIG.c SIG_AlgorithmInstance.h drng.h
	$(CC) $(CFLAGS) -c KAT_SIG.c -o KAT_SIG.o

SIG_AlgorithmInstance.o: SIG_AlgorithmInstance.c SIG_AlgorithmInstance.h drng.h auxfunc.h
	$(CC) $(CFLAGS) -c SIG_AlgorithmInstance.c -o SIG_AlgorithmInstance.o

auxfunc.o: auxfunc.c auxfunc.h
	$(CC) $(CFLAGS) -c auxfunc.c -o auxfunc.o

libfactodsa256-opti.so: $(FACTODSA_OBJ)
	$(CC) $(CFLAGS) -fPIC -DFACTODSA_SO_BUILD -c SIG_AlgorithmInstance.c -o SIG_AlgorithmInstance.o
	$(CC) $(CFLAGS) -fPIC -c auxfunc.c -o auxfunc.o
	$(CC) $(CFLAGS) -fPIC -c drng.c -o drng.o
	$(CC) $(CFLAGS) -fPIC -o $@ $(FACTODSA_OBJ) $(LDFLAGS_SO)


drng.o: drng.c drng.h
	$(CC) $(CFLAGS) -c drng.c -o drng.o

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) run_benchmarks bench_sig *.o
	rm -f *.so
	rm -rf output

.PHONY: all run clean
