/* Copyright (c) 2026 Hang Zhang. */

/* BiT-256 N=512 packed-domain AVX2 forward and inverse incomplete NTT. */

#define Q       %ymm0
#define FOUR    %ymm1
#define SCALE_L %ymm2
#define SCALE_H %ymm2

#define QDATA_16XQ_BYTES             0
#define QDATA_16XINV_SCALE_L_BYTES  64
#define QDATA_16XINV_SCALE_H_BYTES  96

.section .rodata
.p2align 5
packed_four:
    .short 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4

#include "ntt_roots.inc"

.text

.macro shuffle8 r0,r1,r2,r3
    vperm2i128 $0x20, %ymm\r1, %ymm\r0, %ymm\r2
    vperm2i128 $0x31, %ymm\r1, %ymm\r0, %ymm\r3
.endm

.macro shuffle4 r0,r1,r2,r3
    vpunpcklqdq %ymm\r1, %ymm\r0, %ymm\r2
    vpunpckhqdq %ymm\r1, %ymm\r0, %ymm\r3
.endm

/* Exchange two-coefficient (32-bit) groups; this macro is self-inverse. */
.macro shuffle2 r0,r1,r2,r3
    vmovsldup %ymm\r1, %ymm\r2
    vpblendd $0xAA, %ymm\r2, %ymm\r0, %ymm\r2
    vpsrlq $32, %ymm\r0, %ymm\r0
    vpblendd $0xAA, %ymm\r1, %ymm\r0, %ymm\r3
.endm

.macro REDUCE4 a,b,c,d
    vpmulhrsw FOUR, %ymm\a, %ymm12
    vpmulhrsw FOUR, %ymm\b, %ymm13
    vpmulhrsw FOUR, %ymm\c, %ymm14
    vpmulhrsw FOUR, %ymm\d, %ymm15
    vpmullw Q, %ymm12, %ymm12
    vpmullw Q, %ymm13, %ymm13
    vpmullw Q, %ymm14, %ymm14
    vpmullw Q, %ymm15, %ymm15
    vpsubw %ymm12, %ymm\a, %ymm\a
    vpsubw %ymm13, %ymm\b, %ymm\b
    vpsubw %ymm14, %ymm\c, %ymm\c
    vpsubw %ymm15, %ymm\d, %ymm\d
.endm

/* Four CT butterflies; rsi points to one generated 256-byte root set. */
.macro CT4 l0,l1,l2,l3,r0,r1,r2,r3,reduce=0
    vpmullw   0(%rsi), %ymm\r0, %ymm12
    vpmullw  32(%rsi), %ymm\r1, %ymm13
    vpmullw  64(%rsi), %ymm\r2, %ymm14
    vpmullw  96(%rsi), %ymm\r3, %ymm15
    vpmulhw 128(%rsi), %ymm\r0, %ymm\r0
    vpmulhw 160(%rsi), %ymm\r1, %ymm\r1
    vpmulhw 192(%rsi), %ymm\r2, %ymm\r2
    vpmulhw 224(%rsi), %ymm\r3, %ymm\r3
    vpmulhw Q, %ymm12, %ymm12
    vpmulhw Q, %ymm13, %ymm13
    vpmulhw Q, %ymm14, %ymm14
    vpmulhw Q, %ymm15, %ymm15
    vpsubw %ymm12, %ymm\r0, %ymm\r0
    vpsubw %ymm13, %ymm\r1, %ymm\r1
    vpsubw %ymm14, %ymm\r2, %ymm\r2
    vpsubw %ymm15, %ymm\r3, %ymm\r3

    vpsubw %ymm\r0, %ymm\l0, %ymm12
    vpsubw %ymm\r1, %ymm\l1, %ymm13
    vpsubw %ymm\r2, %ymm\l2, %ymm14
    vpsubw %ymm\r3, %ymm\l3, %ymm15
    vpaddw %ymm\r0, %ymm\l0, %ymm\l0
    vpaddw %ymm\r1, %ymm\l1, %ymm\l1
    vpaddw %ymm\r2, %ymm\l2, %ymm\l2
    vpaddw %ymm\r3, %ymm\l3, %ymm\l3
    vmovdqa %ymm12, %ymm\r0
    vmovdqa %ymm13, %ymm\r1
    vmovdqa %ymm14, %ymm\r2
    vmovdqa %ymm15, %ymm\r3
.if \reduce
    REDUCE4 \l0,\l1,\l2,\l3
    REDUCE4 \r0,\r1,\r2,\r3
.endif
.endm

/* Four GS butterflies using the explicit inverse roots Mn and U-V. */
.macro GS4 l0,l1,l2,l3,r0,r1,r2,r3,reduce=0
    vpaddw %ymm\r0, %ymm\l0, %ymm12
    vpaddw %ymm\r1, %ymm\l1, %ymm13
    vpaddw %ymm\r2, %ymm\l2, %ymm14
    vpaddw %ymm\r3, %ymm\l3, %ymm15
    vpsubw %ymm\r0, %ymm\l0, %ymm\l0
    vpsubw %ymm\r1, %ymm\l1, %ymm\l1
    vpsubw %ymm\r2, %ymm\l2, %ymm\l2
    vpsubw %ymm\r3, %ymm\l3, %ymm\l3

    vpmullw   0(%rsi), %ymm\l0, %ymm\r0
    vpmullw  32(%rsi), %ymm\l1, %ymm\r1
    vpmullw  64(%rsi), %ymm\l2, %ymm\r2
    vpmullw  96(%rsi), %ymm\l3, %ymm\r3
    vpmulhw 128(%rsi), %ymm\l0, %ymm\l0
    vpmulhw 160(%rsi), %ymm\l1, %ymm\l1
    vpmulhw 192(%rsi), %ymm\l2, %ymm\l2
    vpmulhw 224(%rsi), %ymm\l3, %ymm\l3
    vpmulhw Q, %ymm\r0, %ymm\r0
    vpmulhw Q, %ymm\r1, %ymm\r1
    vpmulhw Q, %ymm\r2, %ymm\r2
    vpmulhw Q, %ymm\r3, %ymm\r3
    vpsubw %ymm\r0, %ymm\l0, %ymm\r0
    vpsubw %ymm\r1, %ymm\l1, %ymm\r1
    vpsubw %ymm\r2, %ymm\l2, %ymm\r2
    vpsubw %ymm\r3, %ymm\l3, %ymm\r3

    vmovdqa %ymm12, %ymm\l0
    vmovdqa %ymm13, %ymm\l1
    vmovdqa %ymm14, %ymm\l2
    vmovdqa %ymm15, %ymm\l3
.if \reduce
    REDUCE4 \l0,\l1,\l2,\l3
.endif
.endm

.macro LOAD8 base
    vmovdqu \base+  0(%rdi), %ymm4
    vmovdqu \base+ 32(%rdi), %ymm5
    vmovdqu \base+ 64(%rdi), %ymm6
    vmovdqu \base+ 96(%rdi), %ymm7
    vmovdqu \base+128(%rdi), %ymm8
    vmovdqu \base+160(%rdi), %ymm9
    vmovdqu \base+192(%rdi), %ymm10
    vmovdqu \base+224(%rdi), %ymm11
.endm

.macro FWD_HALF base
    LOAD8 \base

    CT4 4,5,6,7,8,9,10,11,1
    addq $256, %rsi
    shuffle8 4,8,3,8
    shuffle8 5,9,4,9
    shuffle8 6,10,5,10
    shuffle8 7,11,6,11

    CT4 3,8,4,9,5,10,6,11,0
    addq $256, %rsi
    shuffle4 3,5,7,5
    shuffle4 8,10,3,10
    shuffle4 4,6,8,6
    shuffle4 9,11,4,11

    CT4 7,5,3,10,8,6,4,11,0
    addq $256, %rsi
    shuffle2 7,8,9,8
    shuffle2 5,6,7,6
    shuffle2 3,4,5,4
    shuffle2 10,11,3,11

    CT4 9,8,7,6,5,4,3,11,1
    addq $256, %rsi
    CT4 9,8,5,4,7,6,3,11,0
    addq $256, %rsi
    CT4 9,7,5,3,8,6,4,11,1
    addq $256, %rsi

    vmovdqu %ymm9,  \base+  0(%rdi)
    vmovdqu %ymm8,  \base+ 32(%rdi)
    vmovdqu %ymm7,  \base+ 64(%rdi)
    vmovdqu %ymm6,  \base+ 96(%rdi)
    vmovdqu %ymm5,  \base+128(%rdi)
    vmovdqu %ymm4,  \base+160(%rdi)
    vmovdqu %ymm3,  \base+192(%rdi)
    vmovdqu %ymm11, \base+224(%rdi)
.endm

/* Fuse t=256 and both t=128 branches for two vectors from every quarter. */
.macro FWD_TOP2 off
    vmovdqu \off+  0(%rdi), %ymm4
    vmovdqu \off+ 32(%rdi), %ymm5
    vmovdqu \off+256(%rdi), %ymm6
    vmovdqu \off+288(%rdi), %ymm7
    vmovdqu \off+512(%rdi), %ymm8
    vmovdqu \off+544(%rdi), %ymm9
    vmovdqu \off+768(%rdi), %ymm10
    vmovdqu \off+800(%rdi), %ymm11

    vmovdqa packed_root_sets+256*0+0(%rip), %ymm2
    vmovdqa packed_root_sets+256*0+128(%rip), %ymm3
    vpmullw %ymm2, %ymm8, %ymm12
    vpmullw %ymm2, %ymm9, %ymm13
    vpmullw %ymm2, %ymm10, %ymm14
    vpmullw %ymm2, %ymm11, %ymm15
    vpmulhw %ymm3, %ymm8, %ymm8
    vpmulhw %ymm3, %ymm9, %ymm9
    vpmulhw %ymm3, %ymm10, %ymm10
    vpmulhw %ymm3, %ymm11, %ymm11
    vpmulhw Q, %ymm12, %ymm12
    vpmulhw Q, %ymm13, %ymm13
    vpmulhw Q, %ymm14, %ymm14
    vpmulhw Q, %ymm15, %ymm15
    vpsubw %ymm12, %ymm8, %ymm8
    vpsubw %ymm13, %ymm9, %ymm9
    vpsubw %ymm14, %ymm10, %ymm10
    vpsubw %ymm15, %ymm11, %ymm11
    vpsubw %ymm8, %ymm4, %ymm12
    vpsubw %ymm9, %ymm5, %ymm13
    vpsubw %ymm10, %ymm6, %ymm14
    vpsubw %ymm11, %ymm7, %ymm15
    vpaddw %ymm8, %ymm4, %ymm4
    vpaddw %ymm9, %ymm5, %ymm5
    vpaddw %ymm10, %ymm6, %ymm6
    vpaddw %ymm11, %ymm7, %ymm7
    vmovdqa %ymm12, %ymm8
    vmovdqa %ymm13, %ymm9
    vmovdqa %ymm14, %ymm10
    vmovdqa %ymm15, %ymm11

    vmovdqa packed_root_sets+256*1+0(%rip), %ymm2
    vpmullw %ymm2, %ymm6, %ymm12
    vpmullw %ymm2, %ymm7, %ymm13
    vmovdqa packed_root_sets+256*14+0(%rip), %ymm2
    vpmullw %ymm2, %ymm10, %ymm14
    vpmullw %ymm2, %ymm11, %ymm15
    vmovdqa packed_root_sets+256*1+128(%rip), %ymm3
    vpmulhw %ymm3, %ymm6, %ymm6
    vpmulhw %ymm3, %ymm7, %ymm7
    vmovdqa packed_root_sets+256*14+128(%rip), %ymm3
    vpmulhw %ymm3, %ymm10, %ymm10
    vpmulhw %ymm3, %ymm11, %ymm11
    vpmulhw Q, %ymm12, %ymm12
    vpmulhw Q, %ymm13, %ymm13
    vpmulhw Q, %ymm14, %ymm14
    vpmulhw Q, %ymm15, %ymm15
    vpsubw %ymm12, %ymm6, %ymm6
    vpsubw %ymm13, %ymm7, %ymm7
    vpsubw %ymm14, %ymm10, %ymm10
    vpsubw %ymm15, %ymm11, %ymm11
    vpsubw %ymm6, %ymm4, %ymm12
    vpsubw %ymm7, %ymm5, %ymm13
    vpsubw %ymm10, %ymm8, %ymm14
    vpsubw %ymm11, %ymm9, %ymm15
    vpaddw %ymm6, %ymm4, %ymm4
    vpaddw %ymm7, %ymm5, %ymm5
    vpaddw %ymm10, %ymm8, %ymm8
    vpaddw %ymm11, %ymm9, %ymm9
    vmovdqa %ymm12, %ymm6
    vmovdqa %ymm13, %ymm7
    vmovdqa %ymm14, %ymm10
    vmovdqa %ymm15, %ymm11

    vmovdqu %ymm4,  \off+  0(%rdi)
    vmovdqu %ymm5,  \off+ 32(%rdi)
    vmovdqu %ymm6,  \off+256(%rdi)
    vmovdqu %ymm7,  \off+288(%rdi)
    vmovdqu %ymm8,  \off+512(%rdi)
    vmovdqu %ymm9,  \off+544(%rdi)
    vmovdqu %ymm10, \off+768(%rdi)
    vmovdqu %ymm11, \off+800(%rdi)
.endm

.p2align 5
.global ntt_fwd_asm
.type ntt_fwd_asm,@function
ntt_fwd_asm:
    leaq qdata(%rip), %rcx
    vmovdqa QDATA_16XQ_BYTES(%rcx), Q
    vmovdqa packed_four(%rip), FOUR
    FWD_TOP2 0
    FWD_TOP2 64
    FWD_TOP2 128
    FWD_TOP2 192

    /* Finish each independent 128-coefficient subtree. */
    leaq packed_root_sets+256*2(%rip), %rsi
    FWD_HALF 0
    FWD_HALF 256
    leaq packed_root_sets+256*15(%rip), %rsi
    FWD_HALF 512
    FWD_HALF 768
    vzeroupper
    ret
.size ntt_fwd_asm,.-ntt_fwd_asm

.macro INV_HALF base,set
    vmovdqu \base+  0(%rdi), %ymm9
    vmovdqu \base+ 32(%rdi), %ymm8
    vmovdqu \base+ 64(%rdi), %ymm7
    vmovdqu \base+ 96(%rdi), %ymm6
    vmovdqu \base+128(%rdi), %ymm5
    vmovdqu \base+160(%rdi), %ymm4
    vmovdqu \base+192(%rdi), %ymm3
    vmovdqu \base+224(%rdi), %ymm11

    leaq packed_inv_root_sets+256*\set(%rip), %rsi
    GS4 9,7,5,3,8,6,4,11,0
    subq $256, %rsi
    GS4 9,8,5,4,7,6,3,11,1
    subq $256, %rsi
    GS4 9,8,7,6,5,4,3,11,0

    shuffle2 3,11,10,11
    shuffle2 5,4,3,4
    shuffle2 7,6,5,6
    shuffle2 9,8,7,8
    subq $256, %rsi
    GS4 7,5,3,10,8,6,4,11,1

    shuffle4 4,11,9,11
    shuffle4 8,6,4,6
    shuffle4 3,10,8,10
    shuffle4 7,5,3,5
    subq $256, %rsi
    GS4 3,8,4,9,5,10,6,11,0

    shuffle8 6,11,7,11
    shuffle8 5,10,6,10
    shuffle8 4,9,5,9
    shuffle8 3,8,4,8
    subq $256, %rsi
    GS4 4,5,6,7,8,9,10,11,1

    vmovdqu %ymm4,  \base+  0(%rdi)
    vmovdqu %ymm5,  \base+ 32(%rdi)
    vmovdqu %ymm6,  \base+ 64(%rdi)
    vmovdqu %ymm7,  \base+ 96(%rdi)
    vmovdqu %ymm8,  \base+128(%rdi)
    vmovdqu %ymm9,  \base+160(%rdi)
    vmovdqu %ymm10, \base+192(%rdi)
    vmovdqu %ymm11, \base+224(%rdi)
.endm

.macro SCALE4 a,b,c,d
    vpmullw SCALE_L, %ymm\a, %ymm12
    vpmullw SCALE_L, %ymm\b, %ymm13
    vpmullw SCALE_L, %ymm\c, %ymm14
    vpmullw SCALE_L, %ymm\d, %ymm15
    vpmulhw SCALE_H, %ymm\a, %ymm\a
    vpmulhw SCALE_H, %ymm\b, %ymm\b
    vpmulhw SCALE_H, %ymm\c, %ymm\c
    vpmulhw SCALE_H, %ymm\d, %ymm\d
    vpmulhw Q, %ymm12, %ymm12
    vpmulhw Q, %ymm13, %ymm13
    vpmulhw Q, %ymm14, %ymm14
    vpmulhw Q, %ymm15, %ymm15
    vpsubw %ymm12, %ymm\a, %ymm\a
    vpsubw %ymm13, %ymm\b, %ymm\b
    vpsubw %ymm14, %ymm\c, %ymm\c
    vpsubw %ymm15, %ymm\d, %ymm\d
    vpaddw Q, %ymm\a, %ymm12
    vpaddw Q, %ymm\b, %ymm13
    vpaddw Q, %ymm\c, %ymm14
    vpaddw Q, %ymm\d, %ymm15
    vpminuw %ymm12, %ymm\a, %ymm\a
    vpminuw %ymm13, %ymm\b, %ymm\b
    vpminuw %ymm14, %ymm\c, %ymm\c
    vpminuw %ymm15, %ymm\d, %ymm\d
.endm

/* Reverse the fused top: t=128 branches, t=256, then the final scale. */
.macro INV_TOP2 off
    vmovdqu \off+  0(%rdi), %ymm4
    vmovdqu \off+ 32(%rdi), %ymm5
    vmovdqu \off+256(%rdi), %ymm6
    vmovdqu \off+288(%rdi), %ymm7
    vmovdqu \off+512(%rdi), %ymm8
    vmovdqu \off+544(%rdi), %ymm9
    vmovdqu \off+768(%rdi), %ymm10
    vmovdqu \off+800(%rdi), %ymm11

    vpaddw %ymm6, %ymm4, %ymm12
    vpaddw %ymm7, %ymm5, %ymm13
    vpaddw %ymm10, %ymm8, %ymm14
    vpaddw %ymm11, %ymm9, %ymm15
    vpsubw %ymm6, %ymm4, %ymm4
    vpsubw %ymm7, %ymm5, %ymm5
    vpsubw %ymm10, %ymm8, %ymm8
    vpsubw %ymm11, %ymm9, %ymm9
    vmovdqa packed_inv_root_sets+256*1+0(%rip), %ymm2
    vpmullw %ymm2, %ymm4, %ymm6
    vpmullw %ymm2, %ymm5, %ymm7
    vmovdqa packed_inv_root_sets+256*14+0(%rip), %ymm2
    vpmullw %ymm2, %ymm8, %ymm10
    vpmullw %ymm2, %ymm9, %ymm11
    vmovdqa packed_inv_root_sets+256*1+128(%rip), %ymm3
    vpmulhw %ymm3, %ymm4, %ymm4
    vpmulhw %ymm3, %ymm5, %ymm5
    vmovdqa packed_inv_root_sets+256*14+128(%rip), %ymm3
    vpmulhw %ymm3, %ymm8, %ymm8
    vpmulhw %ymm3, %ymm9, %ymm9
    vpmulhw Q, %ymm6, %ymm6
    vpmulhw Q, %ymm7, %ymm7
    vpmulhw Q, %ymm10, %ymm10
    vpmulhw Q, %ymm11, %ymm11
    vpsubw %ymm6, %ymm4, %ymm6
    vpsubw %ymm7, %ymm5, %ymm7
    vpsubw %ymm10, %ymm8, %ymm10
    vpsubw %ymm11, %ymm9, %ymm11
    vmovdqa %ymm12, %ymm4
    vmovdqa %ymm13, %ymm5
    vmovdqa %ymm14, %ymm8
    vmovdqa %ymm15, %ymm9

    vpaddw %ymm8, %ymm4, %ymm12
    vpaddw %ymm9, %ymm5, %ymm13
    vpaddw %ymm10, %ymm6, %ymm14
    vpaddw %ymm11, %ymm7, %ymm15
    vpsubw %ymm8, %ymm4, %ymm4
    vpsubw %ymm9, %ymm5, %ymm5
    vpsubw %ymm10, %ymm6, %ymm6
    vpsubw %ymm11, %ymm7, %ymm7
    vmovdqa packed_inv_root_sets+256*0+0(%rip), %ymm2
    vpmullw %ymm2, %ymm4, %ymm8
    vpmullw %ymm2, %ymm5, %ymm9
    vpmullw %ymm2, %ymm6, %ymm10
    vpmullw %ymm2, %ymm7, %ymm11
    vmovdqa packed_inv_root_sets+256*0+128(%rip), %ymm3
    vpmulhw %ymm3, %ymm4, %ymm4
    vpmulhw %ymm3, %ymm5, %ymm5
    vpmulhw %ymm3, %ymm6, %ymm6
    vpmulhw %ymm3, %ymm7, %ymm7
    vpmulhw Q, %ymm8, %ymm8
    vpmulhw Q, %ymm9, %ymm9
    vpmulhw Q, %ymm10, %ymm10
    vpmulhw Q, %ymm11, %ymm11
    vpsubw %ymm8, %ymm4, %ymm8
    vpsubw %ymm9, %ymm5, %ymm9
    vpsubw %ymm10, %ymm6, %ymm10
    vpsubw %ymm11, %ymm7, %ymm11
    vmovdqa %ymm12, %ymm4
    vmovdqa %ymm13, %ymm5
    vmovdqa %ymm14, %ymm6
    vmovdqa %ymm15, %ymm7

    vmovdqa QDATA_16XINV_SCALE_L_BYTES(%rcx), SCALE_L
    SCALE4 4,5,6,7
    SCALE4 8,9,10,11
    vmovdqu %ymm4,  \off+  0(%rdi)
    vmovdqu %ymm5,  \off+ 32(%rdi)
    vmovdqu %ymm6,  \off+256(%rdi)
    vmovdqu %ymm7,  \off+288(%rdi)
    vmovdqu %ymm8,  \off+512(%rdi)
    vmovdqu %ymm9,  \off+544(%rdi)
    vmovdqu %ymm10, \off+768(%rdi)
    vmovdqu %ymm11, \off+800(%rdi)
.endm

.p2align 5
.global ntt_inv_asm
.type ntt_inv_asm,@function
ntt_inv_asm:
    leaq qdata(%rip), %rcx
    vmovdqa QDATA_16XQ_BYTES(%rcx), Q
    vmovdqa packed_four(%rip), FOUR

    INV_HALF   0,7
    INV_HALF 256,13
    INV_HALF 512,20
    INV_HALF 768,26

    INV_TOP2 0
    INV_TOP2 64
    INV_TOP2 128
    INV_TOP2 192
    vzeroupper
    ret
.size ntt_inv_asm,.-ntt_inv_asm

.section .note.GNU-stack,"",@progbits
