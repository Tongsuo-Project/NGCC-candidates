AUTO_COMPILERS:=$(shell for pair in gcc:g++ clang:clang++ cc:c++; do \
	cc=$${pair%:*}; cxx=$${pair#*:}; \
	command -v $$cc >/dev/null 2>&1 || continue; \
	command -v $$cxx >/dev/null 2>&1 || continue; \
	printf 'int main(void){return 0;}\n' | $$cc -x c -std=c23 -c -o /dev/null - >/dev/null 2>&1 || continue; \
	printf 'int main(){return 0;}\n' | $$cxx -x c++ -std=c++23 -c -o /dev/null - >/dev/null 2>&1 || continue; \
	echo "$$cc $$cxx"; exit; \
done)
AUTO_CC:=$(word 1,$(AUTO_COMPILERS))
AUTO_CXX:=$(word 2,$(AUTO_COMPILERS))

ifeq ($(origin CC),default)
ifeq ($(origin CXX),default)
ifneq ($(AUTO_COMPILERS),)
CC:=$(AUTO_CC)
CXX:=$(AUTO_CXX)
endif
endif
endif

ifeq ($(origin CC),default)
ifneq ($(origin CXX),default)
ifneq ($(findstring clang++,$(CXX)),)
CC:=clang
else ifneq ($(findstring g++,$(CXX)),)
CC:=gcc
endif
endif
endif

ifeq ($(origin CXX),default)
ifneq ($(origin CC),default)
ifneq ($(findstring clang,$(CC)),)
CXX:=clang++
else ifneq ($(findstring gcc,$(CC)),)
CXX:=g++
endif
endif
endif

ifeq ($(findstring clang,$(CXX)),clang)
LTO_FLAG=-flto
else
LTO_FLAG=-flto=auto
endif

CFLAGS+= -g -O3 -DNDEBUG -std=c23 -march=native -mtune=native $(LTO_FLAG)
CXXFLAGS+= -g -O3 -DNDEBUG -std=c++23 -march=native -mtune=native $(LTO_FLAG)
CPPFLAGS+= -I. -Isrc -Ilib -Ilib/blake2/sse
LDFLAGS+= $(LTO_FLAG)
SYDO_API_PARAM_TYPE?=sydo_256_f
ifneq ($(SYDO_API_PARAM_TYPE),)
CPPFLAGS+=-DSYDO_API_PARAM_TYPE=$(SYDO_API_PARAM_TYPE)
endif

COMMON_C_SOURCES=$(wildcard src/*.c) $(wildcard lib/*.c) $(wildcard lib/blake2/sse/*.c)
C_SOURCES=$(COMMON_C_SOURCES)
ASM_SOURCES=
CPP_SOURCES=$(wildcard src/*.cpp) $(wildcard src/avx2/*.cpp)
BENCH=bench
BENCH_SOURCE=test/bench.c
KAT=kat_sig
KAT_SOURCE=test/KAT_SIG.c

LIB_OBJECTS=$(C_SOURCES:.c=.o) $(CPP_SOURCES:.cpp=.o) $(ASM_SOURCES:.s=.o) SIG_AlgorithmInstance.o

.PHONY: all clean check-toolchain

all: check-toolchain $(BENCH) $(KAT)

check-toolchain:
	@if [ -z "$(AUTO_COMPILERS)" ] && [ "$(origin CC)" = "default" ] && [ "$(origin CXX)" = "default" ]; then \
		echo "error: no default compiler pair supports both -std=c23 and -std=c++23"; \
		echo "tried: gcc/g++, clang/clang++, cc/c++"; \
		echo "install a newer GCC or Clang toolchain, then run make again"; \
		exit 1; \
	fi
	@if ! command -v "$(CC)" >/dev/null 2>&1; then \
		echo "error: C compiler '$(CC)' was not found"; \
		exit 1; \
	fi
	@if ! command -v "$(CXX)" >/dev/null 2>&1; then \
		echo "error: C++ compiler '$(CXX)' was not found"; \
		exit 1; \
	fi
	@if ! printf 'int main(void){return 0;}\n' | "$(CC)" -x c -std=c23 -c -o /dev/null - >/dev/null 2>&1; then \
		echo "error: C compiler '$(CC)' does not support -std=c23"; \
		"$(CC)" --version 2>/dev/null | head -1 || true; \
		echo "install a newer GCC or Clang C compiler, then run make again"; \
		exit 1; \
	fi
	@if ! printf 'int main(){return 0;}\n' | "$(CXX)" -x c++ -std=c++23 -c -o /dev/null - >/dev/null 2>&1; then \
		echo "error: C++ compiler '$(CXX)' does not support -std=c++23"; \
		"$(CXX)" --version 2>/dev/null | head -1 || true; \
		echo "install a newer GCC or Clang C++ compiler, then run make again"; \
		exit 1; \
	fi
	@echo "toolchain: CC=$(CC) CXX=$(CXX)"

$(LIB_OBJECTS) $(BENCH_SOURCE:.c=.o) $(KAT_SOURCE:.c=.o): | check-toolchain

SIG_AlgorithmInstance.o: SIG_AlgorithmInstance.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BENCH): $(BENCH_SOURCE:.c=.o) $(LIB_OBJECTS)
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

$(KAT): $(KAT_SOURCE:.c=.o) $(LIB_OBJECTS)
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f \
		$(C_SOURCES:.c=.o) \
		$(wildcard lib/*.o) \
		$(wildcard lib/blake2/sse/*.o) \
		$(CPP_SOURCES:.cpp=.o) \
		$(ASM_SOURCES:.s=.o) \
		$(BENCH_SOURCE:.c=.o) \
		$(KAT_SOURCE:.c=.o) \
		SIG_AlgorithmInstance.o \
		$(BENCH) \
		$(KAT)
	rm -rf output
