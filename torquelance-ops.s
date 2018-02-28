        /*
         * Register usage:
         *
         * bl: cached value of (%rdi) for []
         * cl, ch: death counters for A, B
         * edx: cycle counter
         * rsi, rdi: data pointers A, B
         * rsp: points at [fallA, fallB] pointers
         * rbp: points at tick
         * r8, r9: repeat stack pointers for A, B
         * r10d, r11d: current repeat count for A, B
         * r12, r13: saved IP for A, B
         * r14, r15: first and last element of the tape
         */

        .text

        .global op_incA
        .global op_incB
        .global op_decA
        .global op_decB
        .global op_leftA
        .global op_leftB
        .global op_rightA
        .global op_rightB
        .global op_waitA
        .global op_waitB
        .global op_loop1A
        .global op_loop1B
        .global op_loop2A
        .global op_loop2B
        .global op_rep1A
        .global op_rep1B
        .global op_rep2A
        .global op_rep2B
        .global op_inner2A
        .global op_inner2B
        .global op_irep2A
        .global op_irep2B
        .global op_doneA
        .global op_doneB

        .macro endA op
        leaq end_\op(%rip), %r12
        jmpq *%r13
end_\op :
        .endm

        .macro endB op
        leaq end_\op(%rip), %r13
        jmpq *%rbp
end_\op:
        .endm

op_incA:
        incb (%rsi)
        endA op_incA
op_incB:
        incb (%rdi)
        endB op_incB

op_decA:
        decb (%rsi)
        endA op_decA
op_decB:
        decb (%rdi)
        endB op_decB

op_leftA:
        decq %rsi
        cmpq %r14, %rsi
        jae 0f
        jmpq *(%rsp)
0:      endA op_leftA
op_leftB:
        incq %rdi
        cmpq %r15, %rdi
        jbe 0f
        jmpq *8(%rsp)
0:      endB op_leftB

op_rightA:
        incq %rsi
        cmpq %r15, %rsi
        jbe 0f
        jmpq *(%rsp)
0:      endA op_rightA
op_rightB:
        decq %rdi
        cmpq %r14, %rdi
        jae 0f
        jmpq *8(%rsp)
0:      endB op_rightB

op_waitA:
        endA op_waitA
op_waitB:
        endB op_waitB

op_loop1A:
        .set tgt_op_loop1A, .+3
        leaq 0x12345678(%rip), %rax
        leaq end_op_loop1A(%rip), %r12
        cmpb $0, (%rsi)
        cmovzq %rax, %r12
        jmpq *%r13
end_op_loop1A:
op_loop1B:
        .set tgt_op_loop1B, .+3
        leaq 0x12345678(%rip), %rax
        leaq end_op_loop1B(%rip), %r13
        testb %bl, %bl
        cmovzq %rax, %r13
        jmpq *%rbp
end_op_loop1B:

op_loop2A:
        .set tgt_op_loop2A, .+3
        leaq 0x12345678(%rip), %rax
        leaq end_op_loop2A(%rip), %r12
        cmpb $0, (%rsi)
        cmovnzq %rax, %r12
        jmpq *%r13
end_op_loop2A:
op_loop2B:
        .set tgt_op_loop2B, .+3
        leaq 0x12345678(%rip), %rax
        leaq end_op_loop2B(%rip), %r13
        testb %bl, %bl
        cmovnzq %rax, %r13
        jmpq *%rbp
end_op_loop2B:

op_rep1A:
        movl %r10d, (%r8)
        addq $4, %r8
        .set c_op_rep1A, .+2
        movl $0x12345678, %r10d
end_op_rep1A:
op_rep1B:
        movl %r11d, (%r9)
        addq $4, %r9
        .set c_op_rep1B, .+2
        movl $0x12345678, %r11d
end_op_rep1B:

op_rep2A:
        decl %r10d
        .set tgt_op_rep2A, .+2
        jnz .+0x12345678
        subq $4, %r8
        movl (%r8), %r10d
end_op_rep2A:
op_rep2B:
        decl %r11d
        .set tgt_op_rep2B, .+2
        jnz .+0x12345678
        subq $4, %r9
        movl (%r9), %r11d
end_op_rep2B:

op_inner2A:
        movl %r10d, (%r8)
        addq $4, %r8
        movl $1, %r10d
end_op_inner2A:
op_inner2B:
        movl %r11d, (%r9)
        addq $4, %r9
        movl $1, %r11d
end_op_inner2B:

op_irep2A:
        incl %r10d
        .set c_op_irep2A, .+3
        cmpl $0x12345678, %r10d
        .set tgt_op_irep2A, .+2
        jle .+0x12345678
        subq $4, %r8
        movl (%r8), %r10d
end_op_irep2A:
op_irep2B:
        incl %r11d
        .set c_op_irep2B, .+3
        cmpl $0x12345678, %r11d
        .set tgt_op_irep2B, .+2
        jle .+0x12345678
        subq $4, %r9
        movl (%r9), %r11d
end_op_irep2B:

op_doneA:
        leaq 0f(%rip), %r12
0:      jmpq *%r13
end_op_doneA:
op_doneB:
        movq %rbp, %r13
        jmpq *%rbp
end_op_doneB:

        .section .rodata

        .global size_op_incA
        .global size_op_incB
        .global size_op_decA
        .global size_op_decB
        .global size_op_leftA
        .global size_op_leftB
        .global size_op_rightA
        .global size_op_rightB
        .global size_op_waitA
        .global size_op_waitB
        .global size_op_loop1A
        .global size_op_loop1B
        .global size_op_loop2A
        .global size_op_loop2B
        .global size_op_rep1A
        .global size_op_rep1B
        .global size_op_rep2A
        .global size_op_rep2B
        .global size_op_inner2A
        .global size_op_inner2B
        .global size_op_irep2A
        .global size_op_irep2B
        .global size_op_doneA
        .global size_op_doneB

        .global rel_op_loop1A
        .global rel_op_loop1B
        .global rel_op_loop2A
        .global rel_op_loop2B
        .global rel_op_rep2A
        .global rel_op_rep2B
        .global rel_op_irep2A
        .global rel_op_irep2B

        .global count_op_rep1A
        .global count_op_rep1B
        .global count_op_irep2A
        .global count_op_irep2B

        .macro size op
size_\op : .int end_\op - \op
        .endm

        size op_incA
        size op_incB
        size op_decA
        size op_decB
        size op_leftA
        size op_leftB
        size op_rightA
        size op_rightB
        size op_waitA
        size op_waitB
        size op_loop1A
        size op_loop1B
        size op_loop2A
        size op_loop2B
        size op_rep1A
        size op_rep1B
        size op_rep2A
        size op_rep2B
        size op_inner2A
        size op_inner2B
        size op_irep2A
        size op_irep2B
        size op_doneA
        size op_doneB

        .macro rel op
rel_\op : .int tgt_\op - \op
        .endm

        rel op_loop1A
        rel op_loop1B
        rel op_loop2A
        rel op_loop2B
        rel op_rep2A
        rel op_rep2B
        rel op_irep2A
        rel op_irep2B

        .macro count op
count_\op : .int c_\op - \op
        .endm

        count op_rep1A
        count op_rep1B
        count op_irep2A
        count op_irep2B
