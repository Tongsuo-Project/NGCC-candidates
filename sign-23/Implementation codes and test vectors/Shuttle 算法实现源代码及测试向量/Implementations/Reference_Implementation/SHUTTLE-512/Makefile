# Self-contained NGCC build for Reference Implementation of SHUTTLE-512.
#
#   make            build the KAT generator (kat_sig) -> ./kat_sig
#   make vectors    build + run it; writes output/KAT_SIG_SHUTTLE-512.txt
#   make clean
#
# This folder is self-contained: no symlinks, no include paths outside it.
# The compile flags are the NGCC-mandated set for this implementation tier.

CC      ?= gcc
MODE     = 512
QSET     = q59393n1024
INSTANCE = SHUTTLE-512

CFLAGS  = -std=c99 -Wpedantic -Wall -Wextra -O2
CPPFLAGS = -I. -Intt/$(QSET) -DSHUTTLE_MODE=$(MODE)

# Scheme + ICCS-infra scalar sources (the SM3 Hash-DRBG XOF backend).
SCHEME_SRCS = \
  SIG_AlgorithmInstance.c sign.c polyvec.c sampler.c sampler_u.c irs.c \
  rounding.c packing.c poly.c poly_ntt.c reduce.c rans.c approx_exp.c \
  approx_log.c symmetric.c drng.c auxfunc.c \
  ntt/$(QSET)/ntt_ref.c

SIMD_SRCS =

ALL_SRCS = $(SCHEME_SRCS) $(SIMD_SRCS)

.PHONY: all vectors clean

all: kat_sig

kat_sig: KAT_SIG.c $(ALL_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) KAT_SIG.c $(ALL_SRCS) -o $@

vectors: kat_sig
	./kat_sig
	@echo "wrote output/KAT_SIG_$(INSTANCE).txt"

clean:
	rm -f kat_sig
	rm -rf output
