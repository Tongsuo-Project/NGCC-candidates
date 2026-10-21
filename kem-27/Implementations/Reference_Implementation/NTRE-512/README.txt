NTRE-512  -  Reference Implementation
=====================================

Algorithm : NTRE  (NTRU-based KEM)
Instance  : NTRE-512  (n = 2304, q = 3457, D = 4)

Layout
------
  ./                            NGCC API_PKC files
    auxfunc.{c,h}               NGCC auxiliary hash/XOF functions
    drng.{c,h}                  NGCC deterministic RNG for KAT
    KAT_KEM.c                   NGCC KAT generator
    KEM_AlgorithmInstance.{c,h} kem_keygen / kem_enc / kem_dec
    Makefile, README.txt

  src/                          NTRE-specific support source
    params.h                    NTRE_N / NTRE_Q / NTRE_D, byte lengths
    poly.{c,h}                  polynomial arithmetic, CBD'_1, mod-2 recovery
    ntt.{c,h}                   NTT and base multiplication
    symmetric.{c,h}             G / H / F / XOF wrappers using auxfunc

Build
-----
    make
    ./bin/KAT_KEM
    # writes output/KAT_KEM_NTRE-512.txt

Byte lengths
------------
    NTRE_PUBLICKEYBYTES   = 3456
    NTRE_SECRETKEYBYTES   = 6944
    NTRE_CIPHERTEXTBYTES  = 3456
    NTRE_SSBYTES          = 64

Reference implementation: portable ISO C99, no platform-specific intrinsics.
