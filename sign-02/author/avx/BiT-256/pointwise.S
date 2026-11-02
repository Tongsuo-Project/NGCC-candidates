/*
 * Copyright (c) 2026 Hang Zhang.
 * State Key Laboratory of Cyberspace Security Defense,
 * Institute of Information Engineering, CAS
 * School of Cyber Security, University of Chinese Academy of Sciences
 */
/* N=512 incomplete-NTT basemul, dot3, and Montgomery lift. */

#define Q       %ymm15
#define QINV    %ymm14
#define SWAP    %ymm13
#define CONST_H %ymm14
#define CONST_L %ymm13

#define QDATA_16XQ_BYTES      0
#define QDATA_16XQINV_BYTES  32
#define QDATA_16XR2_L_BYTES 128
#define QDATA_16XR2_H_BYTES 160

.section .rodata
.p2align 5
packed_pair_swap:
    .byte 2,3,0,1,6,7,4,5,10,11,8,9,14,15,12,13
    .byte 2,3,0,1,6,7,4,5,10,11,8,9,14,15,12,13

.extern packed_basemul_roots

.text

/*
 * Two vectors, i.e. sixteen quadratic products, in parallel.  For each
 * adjacent pair this computes
 *   c0 = a0*b0 + zeta*a1*b1,  c1 = a0*b1 + a1*b0.
 * The table stores zeta*q^-1 followed by zeta for each packed vector.
 */
.macro BASEMUL2
    vmovdqu   0(%rsi), %ymm0
    vmovdqu   0(%rdx), %ymm1
    vmovdqa   0(%r10), %ymm2
    vmovdqa  32(%r10), %ymm3
    vmovdqu  32(%rsi), %ymm4
    vmovdqu  32(%rdx), %ymm5
    vmovdqa  64(%r10), %ymm6
    vmovdqa  96(%r10), %ymm7

    vpmullw %ymm1, %ymm2, %ymm2
    vpmullw %ymm5, %ymm6, %ymm6
    vpmulhw %ymm1, %ymm3, %ymm3
    vpmulhw %ymm5, %ymm7, %ymm7
    vpmulhw Q, %ymm2, %ymm2
    vpmulhw Q, %ymm6, %ymm6
    vpsubw %ymm2, %ymm3, %ymm3
    vpsubw %ymm6, %ymm7, %ymm7

    vpblendw $0xAA, %ymm3, %ymm1, %ymm2
    vpblendw $0xAA, %ymm7, %ymm5, %ymm6
    vpshufb SWAP, %ymm1, %ymm3
    vpshufb SWAP, %ymm5, %ymm7
    vpmaddwd %ymm2, %ymm0, %ymm2
    vpmaddwd %ymm3, %ymm0, %ymm0
    vpmaddwd %ymm6, %ymm4, %ymm6
    vpmaddwd %ymm7, %ymm4, %ymm4

    vpmullw QINV, %ymm2, %ymm1
    vpmullw QINV, %ymm0, %ymm3
    vpmullw QINV, %ymm6, %ymm5
    vpmullw QINV, %ymm4, %ymm7
    vpsrad $16, %ymm2, %ymm2
    vpsrad $16, %ymm0, %ymm0
    vpsrad $16, %ymm6, %ymm6
    vpsrad $16, %ymm4, %ymm4
    vpmulhw Q, %ymm1, %ymm1
    vpmulhw Q, %ymm3, %ymm3
    vpmulhw Q, %ymm5, %ymm5
    vpmulhw Q, %ymm7, %ymm7
    vpsubw %ymm1, %ymm2, %ymm2
    vpsubw %ymm3, %ymm0, %ymm0
    vpsubw %ymm5, %ymm6, %ymm6
    vpsubw %ymm7, %ymm4, %ymm4
    vpslld $16, %ymm0, %ymm0
    vpslld $16, %ymm4, %ymm4
    vpblendw $0xAA, %ymm0, %ymm2, %ymm2
    vpblendw $0xAA, %ymm4, %ymm6, %ymm6
    vmovdqu %ymm2,  0(%rdi)
    vmovdqu %ymm6, 32(%rdi)
.endm

.p2align 5
.global ntt_basemul_asm
.type ntt_basemul_asm,@function
ntt_basemul_asm:
    leaq qdata(%rip), %rcx
    vmovdqa QDATA_16XQ_BYTES(%rcx), Q
    vmovdqa QDATA_16XQINV_BYTES(%rcx), QINV
    vmovdqa packed_pair_swap(%rip), SWAP
    leaq packed_basemul_roots(%rip), %r10
    movl $16, %r8d
.p2align 4
.Lbasemul_loop:
    BASEMUL2
    addq $64, %rdi
    addq $64, %rsi
    addq $64, %rdx
    addq $128, %r10
    decl %r8d
    jne .Lbasemul_loop
    vzeroupper
    ret
.size ntt_basemul_asm,.-ntt_basemul_asm

/* Three quadratic products share roots and one final Montgomery reduction. */
.macro DOT3_BLOCK
    vmovdqu    0(%rdx), %ymm0
    vmovdqu 1024(%rdx), %ymm1
    vmovdqu 2048(%rdx), %ymm2
    vmovdqa    0(%r10), %ymm10
    vmovdqa   32(%r10), %ymm11

    vpmullw %ymm10, %ymm0, %ymm3
    vpmullw %ymm10, %ymm1, %ymm4
    vpmullw %ymm10, %ymm2, %ymm5
    vpmulhw %ymm11, %ymm0, %ymm6
    vpmulhw %ymm11, %ymm1, %ymm7
    vpmulhw %ymm11, %ymm2, %ymm8
    vpmulhw Q, %ymm3, %ymm3
    vpmulhw Q, %ymm4, %ymm4
    vpmulhw Q, %ymm5, %ymm5
    vpsubw %ymm3, %ymm6, %ymm6
    vpsubw %ymm4, %ymm7, %ymm7
    vpsubw %ymm5, %ymm8, %ymm8
    vpblendw $0xAA, %ymm6, %ymm0, %ymm3
    vpblendw $0xAA, %ymm7, %ymm1, %ymm4
    vpblendw $0xAA, %ymm8, %ymm2, %ymm5
    vpshufb SWAP, %ymm0, %ymm6
    vpshufb SWAP, %ymm1, %ymm7
    vpshufb SWAP, %ymm2, %ymm8

    vmovdqu    0(%rsi), %ymm9
    vpmaddwd %ymm9, %ymm3, %ymm3
    vpmaddwd %ymm9, %ymm6, %ymm6
    vmovdqu 1024(%rsi), %ymm9
    vpmaddwd %ymm9, %ymm4, %ymm4
    vpmaddwd %ymm9, %ymm7, %ymm7
    vmovdqu 2048(%rsi), %ymm9
    vpmaddwd %ymm9, %ymm5, %ymm5
    vpmaddwd %ymm9, %ymm8, %ymm8
    vpaddd %ymm4, %ymm3, %ymm3
    vpaddd %ymm5, %ymm3, %ymm3
    vpaddd %ymm7, %ymm6, %ymm6
    vpaddd %ymm8, %ymm6, %ymm6

    vpmullw QINV, %ymm3, %ymm0
    vpmullw QINV, %ymm6, %ymm1
    vpsrad $16, %ymm3, %ymm3
    vpsrad $16, %ymm6, %ymm6
    vpmulhw Q, %ymm0, %ymm0
    vpmulhw Q, %ymm1, %ymm1
    vpsubw %ymm0, %ymm3, %ymm3
    vpsubw %ymm1, %ymm6, %ymm6
    vpslld $16, %ymm6, %ymm6
    vpblendw $0xAA, %ymm6, %ymm3, %ymm3
    vmovdqu %ymm3, (%rdi)
.endm

.p2align 5
.global ntt_dot3_asm
.type ntt_dot3_asm,@function
ntt_dot3_asm:
    leaq qdata(%rip), %rcx
    vmovdqa QDATA_16XQ_BYTES(%rcx), Q
    vmovdqa QDATA_16XQINV_BYTES(%rcx), QINV
    vmovdqa packed_pair_swap(%rip), SWAP
    leaq packed_basemul_roots(%rip), %r10
    movl $32, %r8d
.p2align 4
.Ldot3_loop:
    DOT3_BLOCK
    addq $32, %rdi
    addq $32, %rsi
    addq $32, %rdx
    addq $64, %r10
    decl %r8d
    jne .Ldot3_loop
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
    movl $8, %r8d
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
