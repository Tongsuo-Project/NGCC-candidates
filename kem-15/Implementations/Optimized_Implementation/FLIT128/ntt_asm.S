#include "params.h"
#include "consts.h"

#define QDATA_INV KEM_NAMESPACE(_qdata_inv)

#if KEM_MODE == 128
#define NTT_FIRST_LEN_BYTES 512
#define NTT_MIN_LEN_BYTES 8
#define POLY_BYTES 1024
#elif KEM_MODE == 256
#define NTT_FIRST_LEN_BYTES 1024
#define NTT_MIN_LEN_BYTES 16
#define POLY_BYTES 2048
#elif KEM_MODE == 512
#define NTT_FIRST_LEN_BYTES 2048
#define NTT_MIN_LEN_BYTES 32
#define POLY_BYTES 4096
#else
#error "Unsupported KEM_MODE"
#endif

.text
.p2align 5
.globl cdecl(ntt_avx)
.type cdecl(ntt_avx), @function
cdecl(ntt_avx):
    vmovdqa _16XQ*2(%rsi), %ymm0
    movl $1, %r9d
    movl $NTT_FIRST_LEN_BYTES, %r8d

.Lntt_len:
    xorl %r10d, %r10d

.Lntt_start:
    movq %r9, %rax
    shlq $4, %rax
    vpbroadcastq _ZETAS_EXP*2(%rsi,%rax), %ymm1
    vpbroadcastq _ZETAS_EXP*2+8(%rsi,%rax), %ymm2
    incl %r9d

    leaq (%rdi,%r10), %rdx
    leaq (%rdx,%r8), %rcx
    cmpl $32, %r8d
    jb .Lntt_small

.Lntt_ymm:
    vmovdqa (%rdx), %ymm3
    vmovdqa (%rdx,%r8), %ymm4
    vpmullw %ymm1, %ymm4, %ymm5
    vpmulhw %ymm2, %ymm4, %ymm4
    vpmulhw %ymm0, %ymm5, %ymm5
    vpsubw %ymm5, %ymm4, %ymm4
    vpaddw %ymm4, %ymm3, %ymm5
    vpsubw %ymm4, %ymm3, %ymm3
    vmovdqa %ymm5, (%rdx)
    vmovdqa %ymm3, (%rdx,%r8)
    addq $32, %rdx
    cmpq %rcx, %rdx
    jb .Lntt_ymm
    jmp .Lntt_block_done

.Lntt_small:
    cmpl $16, %r8d
    jb .Lntt_xmm8

.Lntt_xmm16:
    vmovdqa (%rdx), %xmm3
    vmovdqa (%rdx,%r8), %xmm4
    vpmullw %xmm1, %xmm4, %xmm5
    vpmulhw %xmm2, %xmm4, %xmm4
    vpmulhw %xmm0, %xmm5, %xmm5
    vpsubw %xmm5, %xmm4, %xmm4
    vpaddw %xmm4, %xmm3, %xmm5
    vpsubw %xmm4, %xmm3, %xmm3
    vmovdqa %xmm5, (%rdx)
    vmovdqa %xmm3, (%rdx,%r8)
    jmp .Lntt_block_done

.Lntt_xmm8:
    vmovq (%rdx), %xmm3
    vmovq (%rdx,%r8), %xmm4
    vpmullw %xmm1, %xmm4, %xmm5
    vpmulhw %xmm2, %xmm4, %xmm4
    vpmulhw %xmm0, %xmm5, %xmm5
    vpsubw %xmm5, %xmm4, %xmm4
    vpaddw %xmm4, %xmm3, %xmm5
    vpsubw %xmm4, %xmm3, %xmm3
    vmovq %xmm5, (%rdx)
    vmovq %xmm3, (%rdx,%r8)

.Lntt_block_done:
    leaq (%r10,%r8,2), %r10
    cmpl $POLY_BYTES, %r10d
    jb .Lntt_start
    shrl $1, %r8d
    cmpl $NTT_MIN_LEN_BYTES, %r8d
    jae .Lntt_len

    vzeroupper
    ret
.size cdecl(ntt_avx), .-cdecl(ntt_avx)

.p2align 5
.globl cdecl(invntt_avx)
.type cdecl(invntt_avx), @function
cdecl(invntt_avx):
    vmovdqa _16XQ*2(%rsi), %ymm0
    vmovdqa _16XV*2(%rsi), %ymm7
    leaq QDATA_INV(%rip), %rsi
    xorl %r9d, %r9d
    movl $NTT_MIN_LEN_BYTES, %r8d

/*
 * The first inverse layer accepts the wider range produced by the forward
 * transform and base multiplication. Its dedicated loop avoids the
 * small-length dispatch in the remaining layers.
 * There are exactly 64 blocks in this layer for all three parameter sets.
 */
.Lintt_first:
    movq %r9, %rax
    shlq $4, %rax
    vpbroadcastq (%rsi,%rax), %ymm1
    vpbroadcastq 8(%rsi,%rax), %ymm2

    movq %r9, %r10
    imulq $(NTT_MIN_LEN_BYTES * 2), %r10, %r10
    leaq (%rdi,%r10), %rdx

#if KEM_MODE == 128
    vmovq (%rdx), %xmm3
    vmovq NTT_MIN_LEN_BYTES(%rdx), %xmm4
#elif KEM_MODE == 256
    vmovdqa (%rdx), %xmm3
    vmovdqa NTT_MIN_LEN_BYTES(%rdx), %xmm4
#else
    vmovdqa (%rdx), %ymm3
    vmovdqa NTT_MIN_LEN_BYTES(%rdx), %ymm4
#endif
    vpsubw %ymm4, %ymm3, %ymm5
    vpaddw %ymm4, %ymm3, %ymm3

    /* Signed Barrett reduction keeps the butterfly sum in a narrow range. */
    vpmulhw %ymm7, %ymm3, %ymm8
    vpsraw $8, %ymm8, %ymm8
    vpmullw %ymm0, %ymm8, %ymm8
    vpsubw %ymm8, %ymm3, %ymm3

    vpmullw %ymm1, %ymm5, %ymm8
    vpmulhw %ymm2, %ymm5, %ymm4
    vpmulhw %ymm0, %ymm8, %ymm8
    vpsubw %ymm8, %ymm4, %ymm4

#if KEM_MODE == 128
    vmovq %xmm3, (%rdx)
    vmovq %xmm4, NTT_MIN_LEN_BYTES(%rdx)
#elif KEM_MODE == 256
    vmovdqa %xmm3, (%rdx)
    vmovdqa %xmm4, NTT_MIN_LEN_BYTES(%rdx)
#else
    vmovdqa %ymm3, (%rdx)
    vmovdqa %ymm4, NTT_MIN_LEN_BYTES(%rdx)
#endif
    incl %r9d
    cmpl $64, %r9d
    jb .Lintt_first

    shll $1, %r8d

.Lintt_len:
    xorl %r10d, %r10d

.Lintt_start:
    movq %r9, %rax
    shlq $4, %rax
    vpbroadcastq (%rsi,%rax), %ymm1
    vpbroadcastq 8(%rsi,%rax), %ymm2
    incl %r9d

    leaq (%rdi,%r10), %rdx
    leaq (%rdx,%r8), %rcx
    cmpl $32, %r8d
    jb .Lintt_small

.Lintt_ymm:
    vmovdqa (%rdx), %ymm3
    vmovdqa (%rdx,%r8), %ymm4
    vpsubw %ymm4, %ymm3, %ymm5
    vpaddw %ymm4, %ymm3, %ymm3

    vpmulhw %ymm7, %ymm3, %ymm8
    vpsraw $8, %ymm8, %ymm8
    vpmullw %ymm0, %ymm8, %ymm8
    vpsubw %ymm8, %ymm3, %ymm3

    vpmullw %ymm1, %ymm5, %ymm10
    vpmulhw %ymm2, %ymm5, %ymm11
    vpmulhw %ymm0, %ymm10, %ymm10
    vpsubw %ymm10, %ymm11, %ymm11
    vmovdqa %ymm3, (%rdx)
    vmovdqa %ymm11, (%rdx,%r8)
    addq $32, %rdx
    cmpq %rcx, %rdx
    jb .Lintt_ymm
    jmp .Lintt_block_done

.Lintt_small:
    cmpl $16, %r8d
    jb .Lintt_xmm8

.Lintt_xmm16:
    vmovdqa (%rdx), %xmm3
    vmovdqa (%rdx,%r8), %xmm4
    vpsubw %xmm4, %xmm3, %xmm5
    vpaddw %xmm4, %xmm3, %xmm3

    vpmulhw %xmm7, %xmm3, %xmm8
    vpsraw $8, %xmm8, %xmm8
    vpmullw %xmm0, %xmm8, %xmm8
    vpsubw %xmm8, %xmm3, %xmm3

    vpmullw %xmm1, %xmm5, %xmm10
    vpmulhw %xmm2, %xmm5, %xmm11
    vpmulhw %xmm0, %xmm10, %xmm10
    vpsubw %xmm10, %xmm11, %xmm11
    vmovdqa %xmm3, (%rdx)
    vmovdqa %xmm11, (%rdx,%r8)
    jmp .Lintt_block_done

.Lintt_xmm8:
    vmovq (%rdx), %xmm3
    vmovq (%rdx,%r8), %xmm4
    vpsubw %xmm4, %xmm3, %xmm5
    vpaddw %xmm4, %xmm3, %xmm3

    vpmulhw %xmm7, %xmm3, %xmm8
    vpsraw $8, %xmm8, %xmm8
    vpmullw %xmm0, %xmm8, %xmm8
    vpsubw %xmm8, %xmm3, %xmm3

    vpmullw %xmm1, %xmm5, %xmm10
    vpmulhw %xmm2, %xmm5, %xmm11
    vpmulhw %xmm0, %xmm10, %xmm10
    vpsubw %xmm10, %xmm11, %xmm11
    vmovq %xmm3, (%rdx)
    vmovq %xmm11, (%rdx,%r8)

.Lintt_block_done:
    leaq (%r10,%r8,2), %r10
    cmpl $POLY_BYTES, %r10d
    jb .Lintt_start
    shll $1, %r8d
    cmpl $NTT_FIRST_LEN_BYTES, %r8d
    jbe .Lintt_len

    vpbroadcastq 2032(%rsi), %ymm1
    vpbroadcastq 2040(%rsi), %ymm2
    movq %rdi, %rdx
    leaq POLY_BYTES(%rdi), %rcx

.Lintt_scale:
    vmovdqa (%rdx), %ymm3
    vpmullw %ymm1, %ymm3, %ymm4
    vpmulhw %ymm2, %ymm3, %ymm3
    vpmulhw %ymm0, %ymm4, %ymm4
    vpsubw %ymm4, %ymm3, %ymm3
    vmovdqa %ymm3, (%rdx)
    addq $32, %rdx
    cmpq %rcx, %rdx
    jb .Lintt_scale

    vzeroupper
    ret
.size cdecl(invntt_avx), .-cdecl(invntt_avx)

.section .note.GNU-stack,"",@progbits

