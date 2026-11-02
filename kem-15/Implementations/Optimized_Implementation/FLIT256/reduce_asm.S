#include "params.h"

#define MONTGOMERY_REDUCE KEM_NAMESPACE(montgomery_reduce)
#define FREEZE KEM_NAMESPACE(freeze)
#define FREEZE_CENTERED KEM_NAMESPACE(freeze_centered)

#if KEM_MODE == 512
#define QINV_ASM 62209
#define BARRETT_V 5040
#else
#define QINV_ASM 64769
#define BARRETT_V 21817
#endif

.text
.p2align 4
.globl MONTGOMERY_REDUCE
.type MONTGOMERY_REDUCE, @function
MONTGOMERY_REDUCE:
    movl %edi, %eax
    imull $QINV_ASM, %eax, %ecx
    movswl %cx, %ecx
    imull $Q, %ecx, %ecx
    subl %ecx, %eax
    sarl $16, %eax
    movswl %ax, %eax
    ret
.size MONTGOMERY_REDUCE, .-MONTGOMERY_REDUCE

.p2align 4
.globl FREEZE
.type FREEZE, @function
FREEZE:
    movswl %di, %eax
    imull $BARRETT_V, %eax, %ecx
    sarl $24, %ecx
    imull $Q, %ecx, %ecx
    subl %ecx, %eax

    testl %eax, %eax
    leal Q(%eax), %ecx
    cmovsl %ecx, %eax
    cmpl $Q, %eax
    leal -Q(%eax), %ecx
    cmovgel %ecx, %eax
    movswl %ax, %eax
    ret
.size FREEZE, .-FREEZE

.p2align 4
.globl FREEZE_CENTERED
.type FREEZE_CENTERED, @function
FREEZE_CENTERED:
    movswl %di, %eax
    imull $BARRETT_V, %eax, %ecx
    sarl $24, %ecx
    imull $Q, %ecx, %ecx
    subl %ecx, %eax

    testl %eax, %eax
    leal Q(%eax), %ecx
    cmovsl %ecx, %eax
    cmpl $Q, %eax
    leal -Q(%eax), %ecx
    cmovgel %ecx, %eax

    cmpl $HALF_Q, %eax
    leal -Q(%eax), %ecx
    cmovgel %ecx, %eax
    movswl %ax, %eax
    ret
.size FREEZE_CENTERED, .-FREEZE_CENTERED

.section .note.GNU-stack,"",@progbits

