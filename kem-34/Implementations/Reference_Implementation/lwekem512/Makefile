CC=gcc
CFLAGS += -O3 -march=native -fomit-frame-pointer
LDFLAGS=-lcrypto

SOURCES= auxfunc.c drng.c KAT_KEM.c KEM_lwekem512.c
HEADERS= auxfunc.h drng.h KEM_lwekem512.h

genKAT: $(HEADERS) $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

.PHONY: clean

clean:
	-rm genKAT -rf output
