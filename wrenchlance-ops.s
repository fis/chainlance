        /*
         * Register usage:
         *
         * al: cached value of (%rdi) for []
         * bl, bh: death counters for A, B
         * ecx, edx: current repeat counts for A, B
         * rsi, rdi: data pointers A, B
         * ebp: cycle counter
         * [rsp]: pointer at fallA
         * r8, r9: repeat stack pointers for A, B
         * r10, r11: first and last element of the tape
         * r12, r13: saved IP for A, B
         */

        .macro size sym
        .set size_\sym, .-\sym
        .endm

        .macro end op
        leaq 9f(%rip), %r12
        jmpq *%r13
9:      size \op
        .endm

op_inc:
        incb (%rsi)
        end op_inc

op_dec:
        decb (%rsi)
        end op_dec

op_left:
        decq %rsi
        cmpq %r10, %rsi
        jae 0f
        jmpq *(%rsp)
0:      end op_left

op_right:
        incq %rsi
        cmpq %r11, %rsi
        jbe 0f
        jmpq *(%rsp)
0:      end op_right

op_wait:
        end op_wait

op_loop1:
        .set op_loop1_rel, 3
        leaq 0x12345678(%rip), %r12
        cmpb $0, (%rsi)
        jz 1f
        leaq 0f(%rip), %r12
1:      jmpq *%r13
0:      size op_loop1

op_loop2:
        .set op_loop2_rel, 3
        leaq 0x12345678(%rip), %r12
        cmpb $0, (%rsi)
        jnz 1f
        leaq 0f(%rip), %r12
1:      jmpq *%r13
0:      size op_loop2

op_rep1:
        .set op_rep1_count, .-op_rep1+1
        movl $0x12345678, %ecx
        size op_rep1
op_rep1N:
        movl %ecx, (%r8)
        addq $4, %r8
        .set op_rep1N_count, .-op_rep1N+1
        movl $0x12345678, %ecx
        size op_rep1N

op_rep2:
        decl %ecx /* could be loop */
        .set op_rep2_rel, .-op_rep2+2
        jnz .+0x12345678
        size op_rep2
op_rep2N:
        decl %ecx
        .set op_rep2N_rel, .-op_rep2N+2
        jnz .+0x12345678
        subq $4, %r8
        movl (%r8), %ecx
        size op_rep2N

op_inner2:
        movl $1, %ecx
        size op_inner2
op_inner2N:
        movl %ecx, (%r8)
        addq $4, %r8
        movl $1, %ecx
        size op_inner2N

op_irep2:
        incl %ecx
        .set op_irep2_count, .-op_irep2+2
        cmpl $0x12345678, %ecx
        .set op_irep2_rel, .-op_irep2+2
        jle .+0x12345678
        size op_irep2
op_irep2N:
        incl %ecx
        .set op_irep2N_count, .-op_irep2N+2
        cmpl $0x12345678, %ecx
        .set op_irep2N_rel, .-op_irep2N+2
        jle .+0x12345678
        subq $4, %r8
        movl (%r8), %ecx
        size op_irep2N

op_done:
        leaq 0f(%rip), %r12
0:      jmpq *%r13
        size op_done

        .global op_inc
        .global size_op_inc
        .global op_dec
        .global size_op_dec
        .global op_left
        .global size_op_left
        .global op_right
        .global size_op_right
        .global op_wait
        .global size_op_wait
        .global op_loop1
        .global op_loop1_rel
        .global size_op_loop1
        .global op_loop2
        .global op_loop2_rel
        .global size_op_loop2
        .global op_rep1
        .global op_rep1_count
        .global size_op_rep1
        .global op_rep1N
        .global op_rep1N_count
        .global size_op_rep1N
        .global op_rep2
        .global op_rep2_rel
        .global size_op_rep2
        .global op_rep2N
        .global op_rep2N_rel
        .global size_op_rep2N
        .global op_inner2
        .global size_op_inner2
        .global op_inner2N
        .global size_op_inner2N
        .global op_irep2
        .global op_irep2_rel
        .global op_irep2_count
        .global size_op_irep2
        .global op_irep2N
        .global op_irep2N_rel
        .global op_irep2N_count
        .global size_op_irep2N
        .global op_done
        .global size_op_done
