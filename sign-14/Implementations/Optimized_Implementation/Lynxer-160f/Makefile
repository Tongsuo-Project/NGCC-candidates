# Makefile for Lynxer-160f optimized implementation.
#
# XOF selection:
#   make            - SM3-based pseudoXOF (default, NGCC auxiliary function)
#   make XOF=SHAKE  - SHAKE/Keccak

CC ?= gcc
CXX ?= g++
CFLAGS += -O3 -std=c2x -march=native -mtune=native -Wall -Wextra
CXXFLAGS += -O3 -std=c++20 -march=native -mtune=native -Wall -Wextra
CPPFLAGS += -I. -Isha3

XOF ?= PSEUDO
ifeq ($(XOF),PSEUDO)
  CPPFLAGS += -DXOF_PSEUDO
endif

CPP_SOURCES = $(wildcard *.cpp) $(wildcard avx2/*.cpp)
C_SOURCES = $(wildcard *.c) $(wildcard sha3/*.c)
ASM_SOURCES = $(wildcard sha3/*.s)
OBJS = $(CPP_SOURCES:.cpp=.o) $(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o)

TARGET = KAT_SIG

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

sha3/%.o: sha3/%.c
	$(CC) $(CFLAGS) -Wno-unused-variable $(CPPFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o: %.s
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

kat: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf output

.PHONY: all kat clean
