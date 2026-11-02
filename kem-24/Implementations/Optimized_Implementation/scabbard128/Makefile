CC=/usr/bin/gcc

CFLAGS:= -std=c99 -Wpedantic -Wall -Wextra -O2
#CFLAGS := -O3 -march=x86-64 -mavx2 -mtune=native -flto -fomit-frame-pointer -std=c99 -Wpedantic -Wall -Wextra#-O3 -march=native -fomit-frame-pointer 
#OPTFLAGS=-D__AVX2 
#LDFLAGS=-lcrypto -mavx2

SOURCES= auxfunc.c cbd.c drng.c indcpa.c KEM_scabbard128.c packing.c poly.c polyvec.c verify.c round.S minal.c
HEADERS= auxfunc.h cbd.h drng.h indcpa.h KEM_scabbard128.h packing.h params.h poly.h polyvec.h verify.h minal.h

testavx: $(HEADERS) $(SOURCES) test_avx.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -o $@ $(SOURCES) test_avx.c $(LDFLAGS)

test/test_kex: $(HEADERS) $(SOURCES) test/test_kex.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -o $@ $(SOURCES) test/test_kex.c $(LDFLAGS)
	
genKAT: $(HEADERS) $(SOURCES) test/test_kex.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -o $@ $(SOURCES) test/test_kex.c $(LDFLAGS)

.PHONY: clean bench_speed

bench_speed: $(HEADERS) $(SOURCES) test/cpucycles.c test/cpucycles.h test/speed_print.c test/speed_print.h test/test_speed.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -o $@ $(SOURCES) test/cpucycles.c test/speed_print.c test/test_speed.c $(LDFLAGS)

clean:
	-rm genKAT bench_speed -rf output result_bench_speed.txt testavx
