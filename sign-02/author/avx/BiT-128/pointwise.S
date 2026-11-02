/*
 * Copyright (c) 2026 Hang Zhang.
 * State Key Laboratory of Cyberspace Security Defense,
 * Institute of Information Engineering, CAS
 * School of Cyber Security, University of Chinese Academy of Sciences
 */
/* BiT-128 AVX2 pointwise multiplication, dot3, and Montgomery lift. */

#define Q       %ymm15
#define QINV    %ymm14
#define CONST_H %ymm14
#define CONST_L %ymm13

#define QDATA_16XQ_BYTES      0
#define QDATA_16XQINV_BYTES  32
#define QDATA_16XR2_L_BYTES 128
#define QDATA_16XR2_H_BYTES 160

.text

/* Four independent coefficient-wise Montgomery products. */
.macro POINTWISE4
    vmovdqu  0(%rsi), %ymm0
    vmovdqu 32(%rsi), %ymm1
    vmovdqu 64(%rsi), %ymm2
    vmovdqu 96(%rsi), %ymm3
    vmovdqu  0(%rdx), %ymm4
    vmovdqu 32(%rdx), %ymm5
    vmovdqu 64(%rdx), %ymm6
    vmovdqu 96(%rdx), %ymm7

    vpmullw %ymm4, %ymm0, %ymm8
    vpmullw %ymm5, %ymm1, %ymm9
    vpmullw %ymm6, %ymm2, %ymm10
    vpmullw %ymm7, %ymm3, %ymm11
    vpmulhw %ymm4, %ymm0, %ymm0
    vpmulhw %ymm5, %ymm1, %ymm1
    vpmulhw %ymm6, %ymm2, %ymm2
    vpmulhw %ymm7, %ymm3, %ymm3
    vpmullw QINV, %ymm8, %ymm8
    vpmullw QINV, %ymm9, %ymm9
    vpmullw QINV, %ymm10, %ymm10
    vpmullw QINV, %ymm11, %ymm11
    vpmulhw Q, %ymm8, %ymm8
    vpmulhw Q, %ymm9, %ymm9
    vpmulhw Q, %ymm10, %ymm10
    vpmulhw Q, %ymm11, %ymm11
    vpsubw %ymm8, %ymm0, %ymm0
    vpsubw %ymm9, %ymm1, %ymm1
    vpsubw %ymm10, %ymm2, %ymm2
    vpsubw %ymm11, %ymm3, %ymm3
    vmovdqu %ymm0,  0(%rdi)
    vmovdqu %ymm1, 32(%rdi)
    vmovdqu %ymm2, 64(%rdi)
    vmovdqu %ymm3, 96(%rdi)
.endm

.p2align 5
.global ntt_basemul_asm
.type ntt_basemul_asm,@function
ntt_basemul_asm:
    leaq qdata(%rip), %rcx
    vmovdqa QDATA_16XQ_BYTES(%rcx), Q
    vmovdqa QDATA_16XQINV_BYTES(%rcx), QINV
    movl $4, %r8d
.p2align 4
.Lbasemul_loop:
    POINTWISE4
    addq $128, %rdi
    addq $128, %rsi
    addq $128, %rdx
    decl %r8d
    jne .Lbasemul_loop
    vzeroupper
    ret
.size ntt_basemul_asm,.-ntt_basemul_asm

/* Three-term dot product with one load/store per accumulator block. */
.macro DOT3_BLOCK off
    vmovdqu \off(%rsi),      %ymm0
    vmovdqu \off(%rdx),      %ymm1
    vmovdqu 512+\off(%rsi),  %ymm3
    vmovdqu 512+\off(%rdx),  %ymm4
    vmovdqu 1024+\off(%rsi), %ymm6
    vmovdqu 1024+\off(%rdx), %ymm7

    vpmullw %ymm1, %ymm0, %ymm2
    vpmullw %ymm4, %ymm3, %ymm5
    vpmullw %ymm7, %ymm6, %ymm8
    vpmulhw %ymm1, %ymm0, %ymm0
    vpmulhw %ymm4, %ymm3, %ymm3
    vpmulhw %ymm7, %ymm6, %ymm6
    vpmullw QINV, %ymm2, %ymm2
    vpmullw QINV, %ymm5, %ymm5
    vpmullw QINV, %ymm8, %ymm8
    vpmulhw Q, %ymm2, %ymm2
    vpmulhw Q, %ymm5, %ymm5
    vpmulhw Q, %ymm8, %ymm8
    vpsubw %ymm2, %ymm0, %ymm0
    vpsubw %ymm5, %ymm3, %ymm3
    vpsubw %ymm8, %ymm6, %ymm6
    vpaddw %ymm3, %ymm0, %ymm0
    vpaddw %ymm6, %ymm0, %ymm0
    vmovdqu %ymm0, \off(%rdi)
.endm

.p2align 5
.global ntt_dot3_asm
.type ntt_dot3_asm,@function
ntt_dot3_asm:
    leaq qdata(%rip), %rcx
    vmovdqa QDATA_16XQ_BYTES(%rcx), Q
    vmovdqa QDATA_16XQINV_BYTES(%rcx), QINV
    DOT3_BLOCK   0
    DOT3_BLOCK  32
    DOT3_BLOCK  64
    DOT3_BLOCK  96
    DOT3_BLOCK 128
    DOT3_BLOCK 160
    DOT3_BLOCK 192
    DOT3_BLOCK 224
    DOT3_BLOCK 256
    DOT3_BLOCK 288
    DOT3_BLOCK 320
    DOT3_BLOCK 352
    DOT3_BLOCK 384
    DOT3_BLOCK 416
    DOT3_BLOCK 448
    DOT3_BLOCK 480
    vzeroupper
    ret
.size ntt_dot3_asm,.-ntt_dot3_asm

/* Four-vector Montgomery multiplication by the fixed R^2 lift constant. */
.macro LIFT4
    vmovdqu   0(%rdi), %ymm0
    vmovdqu  32(%rdi), %ymm1
    vmovdqu  64(%rdi), %ymm2
    vmovdqu  96(%rdi), %ymm3
    vpmullw CONST_L, %ymm0, %ymm4
    vpmullw CONST_L, %ymm1, %ymm5
    vpmullw CONST_L, %ymm2, %ymm6
    vpmullw CONST_L, %ymm3, %ymm7
    vpmulhw CONST_H, %ymm0, %ymm0
    vpmulhw CONST_H, %ymm1, %ymm1
    vpmulhw CONST_H, %ymm2, %ymm2
    vpmulhw CONST_H, %ymm3, %ymm3
    vpmulhw Q, %ymm4, %ymm4
    vpmulhw Q, %ymm5, %ymm5
    vpmulhw Q, %ymm6, %ymm6
    vpmulhw Q, %ymm7, %ymm7
    vpsubw %ymm4, %ymm0, %ymm0
    vpsubw %ymm5, %ymm1, %ymm1
    vpsubw %ymm6, %ymm2, %ymm2
    vpsubw %ymm7, %ymm3, %ymm3
    vpaddw Q, %ymm0, %ymm4
    vpaddw Q, %ymm1, %ymm5
    vpaddw Q, %ymm2, %ymm6
    vpaddw Q, %ymm3, %ymm7
    vpminuw %ymm4, %ymm0, %ymm0
    vpminuw %ymm5, %ymm1, %ymm1
    vpminuw %ymm6, %ymm2, %ymm2
    vpminuw %ymm7, %ymm3, %ymm3
    vmovdqu %ymm0,   0(%rdi)
    vmovdqu %ymm1,  32(%rdi)
    vmovdqu %ymm2,  64(%rdi)
    vmovdqu %ymm3,  96(%rdi)
.endm

.p2align 5
.global ntt_montgomery_lift_asm
.type ntt_montgomery_lift_asm,@function
ntt_montgomery_lift_asm:
    leaq qdata(%rip), %rcx
    vmovdqa QDATA_16XQ_BYTES(%rcx), Q
    vmovdqa QDATA_16XR2_L_BYTES(%rcx), CONST_L
    vmovdqa QDATA_16XR2_H_BYTES(%rcx), CONST_H
    movl $4, %r8d
.p2align 4
.Llift_loop:
    LIFT4
    addq $128, %rdi
    decl %r8d
    jne .Llift_loop
    vzeroupper
    ret
.size ntt_montgomery_lift_asm,.-ntt_montgomery_lift_asm

.section .note.GNU-stack,"",@progbits
