CC ?= gcc

# Optimized implementation: maximum performance on x86-64 with SM4 and non-hash ISA acceleration.
# All source files are self-contained in this directory.
CFLAGS = -O3 -std=c11 -pedantic -Wall -Wextra \
         -I. -I./utils_sm4 \
         -include fallbacks.h
LDFLAGS = -lm

NDEBUG ?= 1
LTO_FLAG := -flto
ifeq (,$(findstring clang,$(shell $(CC) --version 2>/dev/null)))
LTO_FLAG := -flto=auto
endif
ifeq ($(NDEBUG),1)
CFLAGS += -DNDEBUG -march=native $(LTO_FLAG)
endif

TIMING ?= 0
COMPONENT_TIMING ?= $(TIMING)
COMPONENT_TIMING_VERBOSE ?= 0
ifeq ($(COMPONENT_TIMING),1)
CFLAGS += -D_POSIX_C_SOURCE=200809L -DSM4TH_COMPONENT_TIMING=1
endif
ifeq ($(COMPONENT_TIMING_VERBOSE),1)
CFLAGS += -DSM4TH_COMPONENT_TIMING_VERBOSE=1
endif

# PCLMUL: GF(2^128) carryless multiply acceleration.
# Default on for x86/x86_64, off otherwise.
ifneq (,$(filter x86_64 i386,$(shell uname -m)))
FIELD_PCLMUL ?= 1
else
FIELD_PCLMUL ?= 0
endif
ifeq ($(FIELD_PCLMUL),1)
CFLAGS += -mpclmul -DSM4TH_FIELD_PCLMUL
endif

# S-box table: precomputed lookup. This is a portable C optimization, not
# instruction-set acceleration, so keep it enabled by default.
ifneq (,$(filter x86_64 i386,$(shell uname -m)))
USE_SBOX_TABLE ?= 1
else
USE_SBOX_TABLE ?= 1
endif
ifeq ($(USE_SBOX_TABLE),1)
CFLAGS += -DSBOX_TABLE -DSM4_SBOX_TABLE
endif

# SM4 assembly acceleration (sources under utils_sm4/).
# Default on for x86/x86_64, off otherwise.
ifneq (,$(filter x86_64 i386,$(shell uname -m)))
PRG_ACCE ?= 1
else
PRG_ACCE ?= 0
endif
CFLAGS += -DPRG_ACCE=$(PRG_ACCE) -DSM4TH_ENABLE_SM4_ACCE=$(PRG_ACCE)

# Hygon CIS SM4 instruction backend. Keep this separate from PRG_ACCE so the
# Hygon path can be tested without enabling the existing AES-NI based PRG path.
HYGON_SM4_ACCE ?= 0
HYGON_CIS_SRCS =
HYGON_CIS_ASM_SOURCE =
HYGON_CIS_ASM_OBJECT =
ifeq ($(HYGON_SM4_ACCE),1)
ifneq (,$(filter x86_64,$(shell uname -m)))
CFLAGS += -D_POSIX_C_SOURCE=200809L -DSM4TH_ENABLE_HYGON_CIS=1
HYGON_CIS_SRCS = utils_sm4/hygon_cis_sm4.c
HYGON_CIS_ASM_SOURCE = utils_sm4/hygon_cis_sm4_asm.S
HYGON_CIS_ASM_OBJECT = hygon_cis_sm4_asm.o
else
$(error HYGON_SM4_ACCE=1 requires x86_64)
endif
else
CFLAGS += -DSM4TH_ENABLE_HYGON_CIS=0
endif


# SM4 ASM object: compile utils_sm4/sm4_asm.S on x86-64/i386
X86_ASM_SOURCE =
X86_ASM_OBJECT =
ifneq (,$(filter x86_64 i386,$(shell uname -m)))
ifeq ($(PRG_ACCE),1)
X86_ASM_SOURCE = utils_sm4/sm4_asm.S
X86_ASM_OBJECT = sm4_asm.o
endif
endif

ifeq ($(HYGON_SM4_ACCE),1)
$(HYGON_CIS_ASM_OBJECT): $(HYGON_CIS_ASM_SOURCE)
	$(CC) -c -x assembler-with-cpp -o $@ $<
endif

COMMON_SRCS = \
  auxfunc.c bavc.c compat.c drng.c fields.c owf.c params.c prg.c \
  random_oracle.c sig_impl.c sig_timing.c universal_hashing.c utils.c vole.c

UTILS_SM4_SRCS = \
  utils_sm4/sm4_affine.c utils_sm4/sm4_core.c utils_sm4/sm4_sbox.c \
  sm4_witness.c utils_sm4/sm4_ecb.c \
  sm4th_sm4_128.c \
  $(HYGON_CIS_SRCS)

INSTANCE_SRCS = \
  sm4th_em_d2_128s_loose.c sm4th_em_d2_128s_loose_AlgorithmInstance.c

SOURCES = $(COMMON_SRCS) $(UTILS_SM4_SRCS) \
          $(INSTANCE_SRCS)
OBJECTS = $(SOURCES:.c=.o)

all: sm4th_em_d2_128s_loose_bench

$(X86_ASM_OBJECT): $(X86_ASM_SOURCE)
	$(CC) -c -x assembler-with-cpp -o $@ $<

KAT_SIG: KAT_SIG.c $(SOURCES) $(X86_ASM_OBJECT) $(HYGON_CIS_ASM_OBJECT)
	$(CC) $(CFLAGS) -o $@ KAT_SIG.c $(SOURCES) $(X86_ASM_OBJECT) $(HYGON_CIS_ASM_OBJECT) $(LDFLAGS)

sm4th_em_d2_128s_loose_bench: sm4th_em_d2_128s_loose_bench.c $(SOURCES) $(X86_ASM_OBJECT) $(HYGON_CIS_ASM_OBJECT)
	$(CC) $(CFLAGS) -o $@ sm4th_em_d2_128s_loose_bench.c $(SOURCES) $(X86_ASM_OBJECT) $(HYGON_CIS_ASM_OBJECT) $(LDFLAGS)

ifeq ($(HYGON_SM4_ACCE),1)
hygon_cis_sm4_probe: hygon_cis_sm4_probe.c $(HYGON_CIS_SRCS) $(HYGON_CIS_ASM_OBJECT)
	$(CC) $(CFLAGS) -o $@ hygon_cis_sm4_probe.c $(HYGON_CIS_SRCS) $(HYGON_CIS_ASM_OBJECT) $(LDFLAGS)
else
hygon_cis_sm4_probe:
	@echo "Use: make PRG_ACCE=0 HYGON_SM4_ACCE=1 hygon_cis_sm4_probe"
	@exit 1
endif

clean:
	rm -f sm4th_em_d2_128s_loose_bench sm4th_em_d2_128s_loose_test KAT_SIG \
	      hygon_cis_sm4_probe $(X86_ASM_OBJECT) $(HYGON_CIS_ASM_OBJECT) $(OBJECTS)
	rm -f *.o utils_sm4/*.o utils_ballet/*.o utils_ublock/*.o utils_vistrutah/*.o
	rm -rf output

.PHONY: all clean KAT_SIG sm4th_em_d2_128s_loose_bench hygon_cis_sm4_probe
