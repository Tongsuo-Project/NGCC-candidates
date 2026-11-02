/* Copyright (c) 2026 Hang Zhang. */

/* BiT-128 natural-to-packed NTT-domain layout conversion. */

.text

.macro shuffle8 r0,r1,r2,r3
    vperm2i128 $0x20, %ymm\r1, %ymm\r0, %ymm\r2
    vperm2i128 $0x31, %ymm\r1, %ymm\r0, %ymm\r3
.endm

.macro shuffle4 r0,r1,r2,r3
    vpunpcklqdq %ymm\r1, %ymm\r0, %ymm\r2
    vpunpckhqdq %ymm\r1, %ymm\r0, %ymm\r3
.endm

.macro shuffle2 r0,r1,r2,r3
    vmovsldup %ymm\r1, %ymm\r2
    vpblendd $0xAA, %ymm\r2, %ymm\r0, %ymm\r2
    vpsrlq $32, %ymm\r0, %ymm\r0
    vpblendd $0xAA, %ymm\r1, %ymm\r0, %ymm\r3
.endm

.macro PACK_HALF base
    vmovdqu \base+  0(%rdi), %ymm4
    vmovdqu \base+ 32(%rdi), %ymm5
    vmovdqu \base+ 64(%rdi), %ymm6
    vmovdqu \base+ 96(%rdi), %ymm7
    vmovdqu \base+128(%rdi), %ymm8
    vmovdqu \base+160(%rdi), %ymm9
    vmovdqu \base+192(%rdi), %ymm10
    vmovdqu \base+224(%rdi), %ymm11
    shuffle8 4,8,3,8
    shuffle8 5,9,4,9
    shuffle8 6,10,5,10
    shuffle8 7,11,6,11
    shuffle4 3,5,7,5
    shuffle4 8,10,3,10
    shuffle4 4,6,8,6
    shuffle4 9,11,4,11
    shuffle2 7,8,9,8
    shuffle2 5,6,7,6
    shuffle2 3,4,5,4
    shuffle2 10,11,3,11
    vmovdqu %ymm9,  \base+  0(%rdi)
    vmovdqu %ymm8,  \base+ 32(%rdi)
    vmovdqu %ymm7,  \base+ 64(%rdi)
    vmovdqu %ymm6,  \base+ 96(%rdi)
    vmovdqu %ymm5,  \base+128(%rdi)
    vmovdqu %ymm4,  \base+160(%rdi)
    vmovdqu %ymm3,  \base+192(%rdi)
    vmovdqu %ymm11, \base+224(%rdi)
.endm

.p2align 5
.global ntt_pack_asm
.type ntt_pack_asm,@function
ntt_pack_asm:
    PACK_HALF 0
    PACK_HALF 256
    vzeroupper
    ret
.size ntt_pack_asm,.-ntt_pack_asm

.section .note.GNU-stack,"",@progbits
