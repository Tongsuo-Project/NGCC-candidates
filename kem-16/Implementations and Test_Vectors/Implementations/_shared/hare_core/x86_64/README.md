# x86_64 optimized core

The x86_64 core uses AVX2 and PCLMULQDQ where appropriate.

```text
vector.c          AVX2 add/compare/truncate and support expansion via public scan
gf.c              PCLMUL GF(2^8) multiplication
gf2x.c            selected KR GF2X: PCLMUL + Toom-3/Karatsuba + AVX2 reduction
reed_muller.c     AVX2 Hadamard-style RM path with HARE erasure semantics
reed_solomon.c    erasure-aware RS with static generator constants
```

The KR GF2X plan is selected entirely from public compile-time parameters.
