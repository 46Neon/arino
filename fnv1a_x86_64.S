/* System V AMD64: RDI=bytes, RSI=length, EAX=32-bit FNV-1a result. */
    .text
    .globl arino_fnv1a32_x86_64
    .type arino_fnv1a32_x86_64, @function
arino_fnv1a32_x86_64:
    pushq   %rbx                    /* preserve callee-saved register */
    movq    %rdi, %rbx              /* RBX walks the zero-copy input */
    movq    %rsi, %rcx              /* RCX is the remaining byte count */
    movl    $0x811c9dc5, %eax       /* FNV offset basis in RAX/EAX */
.Lfnv_loop:
    testq   %rcx, %rcx
    jz      .Lfnv_done
    movzbl  (%rbx), %edx
    xorl    %edx, %eax
    imull   $0x01000193, %eax, %eax /* FNV prime */
    incq    %rbx
    decq    %rcx
    jmp     .Lfnv_loop
.Lfnv_done:
    popq    %rbx
    ret
    .size arino_fnv1a32_x86_64, .-arino_fnv1a32_x86_64

    .section .note.GNU-stack,"",@progbits
