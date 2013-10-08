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

        .macro size sym
        .set size_\sym, .-\sym
        .endm

        .macro endA op
        leaq 9f(%rip), %r12
        jmpq *%r13
9:      size \op
        .endm

        .macro endB op
        leaq 9f(%rip), %r13
        jmpq *%rbp
9:      size \op
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
        jge 0f
        jmpq *(%rsp)
0:      endA op_leftA
op_leftB:
        incq %rdi
        cmpq %r15, %rdi
        jle 0f
        jmpq *8(%rsp)
0:      endB op_leftB

op_rightA:
        incq %rsi
        cmpq %r15, %rsi
        jle 0f
        jmpq *(%rsp)
0:      endA op_rightA
op_rightB:
        decq %rdi
        cmpq %r14, %rdi
        jge 0f
        jmpq *8(%rsp)
0:      endB op_rightB

op_waitA:
        endA op_waitA
op_waitB:
        endB op_waitB

op_loop1A:
        .set op_loop1A_rel, 3
        leaq 0x12345678(%rip), %rax
        leaq 0f(%rip), %r12
        cmpb $0, (%rsi)
        cmovzq %rax, %r12
        jmpq *%r13
0:      size op_loop1A
op_loop1B:
        .set op_loop1B_rel, 3
        leaq 0x12345678(%rip), %rax
        leaq 0f(%rip), %r13
        testb %bl, %bl
        cmovzq %rax, %r13
        jmpq *%rbp
0:      size op_loop1B

op_loop2A:
        .set op_loop2A_rel, 3
        leaq 0x12345678(%rip), %rax
        leaq 0f(%rip), %r12
        cmpb $0, (%rsi)
        cmovnzq %rax, %r12
        jmpq *%r13
0:      size op_loop2A
op_loop2B:
        .set op_loop2B_rel, 3
        leaq 0x12345678(%rip), %rax
        leaq 0f(%rip), %r13
        testb %bl, %bl
        cmovnzq %rax, %r13
        jmpq *%rbp
0:      size op_loop2B

op_rep1A:
        movl %r10d, (%r8)
        addq $4, %r8
        .set op_rep1A_count, .-op_rep1A+2
        movl $0x12345678, %r10d
        size op_rep1A
op_rep1B:
        movl %r11d, (%r9)
        addq $4, %r9
        .set op_rep1B_count, .-op_rep1B+2
        movl $0x12345678, %r11d
        size op_rep1B

op_rep2A:
        decl %r10d
        .set op_rep2A_rel, .-op_rep2A+2
        jnz .+0x12345678
        subq $4, %r8
        movl (%r8), %r10d
        size op_rep2A
op_rep2B:
        decl %r11d
        .set op_rep2B_rel, .-op_rep2B+2
        jnz .+0x12345678
        subq $4, %r9
        movl (%r9), %r11d
        size op_rep2B

op_inner2A:
        movl %r10d, (%r8)
        addq $4, %r8
        movl $1, %r10d
        size op_inner2A
op_inner2B:
        movl %r11d, (%r9)
        addq $4, %r9
        movl $1, %r11d
        size op_inner2B

op_irep2A:
        incl %r10d
        .set op_irep2A_count, .-op_irep2A+3
        cmpl $0x12345678, %r10d
        .set op_irep2A_rel, .-op_irep2A+2
        jle .+0x12345678
        subq $4, %r8
        movl (%r8), %r10d
        size op_irep2A
op_irep2B:
        incl %r11d
        .set op_irep2B_count, .-op_irep2B+3
        cmpl $0x12345678, %r11d
        .set op_irep2B_rel, .-op_irep2B+2
        jle .+0x12345678
        subq $4, %r9
        movl (%r9), %r11d
        size op_irep2B

op_doneA:
        leaq 0f(%rip), %r12
0:      jmpq *%r13
        size op_doneA
op_doneB:
        movq %rbp, %r13
        jmpq *%rbp
        size op_doneB

        .global op_incA
        .global size_op_incA
        .global op_incB
        .global size_op_incB
        .global op_decA
        .global size_op_decA
        .global op_decB
        .global size_op_decB
        .global op_leftA
        .global size_op_leftA
        .global op_leftB
        .global size_op_leftB
        .global op_rightA
        .global size_op_rightA
        .global op_rightB
        .global size_op_rightB
        .global op_waitA
        .global size_op_waitA
        .global op_waitB
        .global size_op_waitB
        .global op_loop1A
        .global op_loop1A_rel
        .global size_op_loop1A
        .global op_loop1B
        .global op_loop1B_rel
        .global size_op_loop1B
        .global op_loop2A
        .global op_loop2A_rel
        .global size_op_loop2A
        .global op_loop2B
        .global op_loop2B_rel
        .global size_op_loop2B
        .global op_rep1A
        .global op_rep1A_count
        .global size_op_rep1A
        .global op_rep1B
        .global op_rep1B_count
        .global size_op_rep1B
        .global op_rep2A
        .global op_rep2A_rel
        .global size_op_rep2A
        .global op_rep2B
        .global op_rep2B_rel
        .global size_op_rep2B
        .global op_inner2A
        .global size_op_inner2A
        .global op_inner2B
        .global size_op_inner2B
        .global op_irep2A
        .global op_irep2A_rel
        .global op_irep2A_count
        .global size_op_irep2A
        .global op_irep2B
        .global op_irep2B_rel
        .global op_irep2B_count
        .global size_op_irep2B
        .global op_doneA
        .global size_op_doneA
        .global op_doneB
        .global size_op_doneB
