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

        .text

        .global op_inc
        .global op_dec
        .global op_left
        .global op_right
        .global op_wait
        .global op_loop1
        .global op_loop2
        .global op_rep1
        .global op_rep1N
        .global op_rep2
        .global op_rep2N
        .global op_inner2
        .global op_inner2N
        .global op_irep2
        .global op_irep2N
        .global op_done

        .macro end op
        leaq end_\op(%rip), %r12
        jmpq *%r13
end_\op :
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
        .set tgt_op_loop1, .+3
        leaq 0x12345678(%rip), %r12
        cmpb $0, (%rsi)
        jz 1f
        leaq end_op_loop1(%rip), %r12
1:      jmpq *%r13
end_op_loop1:

op_loop2:
        .set tgt_op_loop2, .+3
        leaq 0x12345678(%rip), %r12
        cmpb $0, (%rsi)
        jnz 1f
        leaq end_op_loop2(%rip), %r12
1:      jmpq *%r13
end_op_loop2:

op_rep1:
        .set c_op_rep1, .+1
        movl $0x12345678, %ecx
end_op_rep1:
op_rep1N:
        movl %ecx, (%r8)
        addq $4, %r8
        .set c_op_rep1N, .+1
        movl $0x12345678, %ecx
end_op_rep1N:

op_rep2:
        decl %ecx /* could be loop */
        .set tgt_op_rep2, .+2
        jnz .+0x12345678
end_op_rep2:
op_rep2N:
        decl %ecx
        .set tgt_op_rep2N, .+2
        jnz .+0x12345678
        subq $4, %r8
        movl (%r8), %ecx
end_op_rep2N:

op_inner2:
        movl $1, %ecx
end_op_inner2:
op_inner2N:
        movl %ecx, (%r8)
        addq $4, %r8
        movl $1, %ecx
end_op_inner2N:

op_irep2:
        incl %ecx
        .set c_op_irep2, .+2
        cmpl $0x12345678, %ecx
        .set tgt_op_irep2, .+2
        jle .+0x12345678
end_op_irep2:
op_irep2N:
        incl %ecx
        .set c_op_irep2N, .+2
        cmpl $0x12345678, %ecx
        .set tgt_op_irep2N, .+2
        jle .+0x12345678
        subq $4, %r8
        movl (%r8), %ecx
end_op_irep2N:

op_done:
        leaq 0f(%rip), %r12
0:      jmpq *%r13
end_op_done:

        .section .rodata

        .global size_op_inc
        .global size_op_dec
        .global size_op_left
        .global size_op_right
        .global size_op_wait
        .global size_op_loop1
        .global size_op_loop2
        .global size_op_rep1
        .global size_op_rep1N
        .global size_op_rep2
        .global size_op_rep2N
        .global size_op_inner2
        .global size_op_inner2N
        .global size_op_irep2
        .global size_op_irep2N
        .global size_op_done

        .global rel_op_loop1
        .global rel_op_loop2
        .global rel_op_rep2
        .global rel_op_rep2N
        .global rel_op_irep2
        .global rel_op_irep2N

        .global count_op_rep1
        .global count_op_rep1N
        .global count_op_irep2
        .global count_op_irep2N

        .macro size op
size_\op : .int end_\op - \op
        .endm

        size op_inc
        size op_dec
        size op_left
        size op_right
        size op_wait
        size op_loop1
        size op_loop2
        size op_rep1
        size op_rep1N
        size op_rep2
        size op_rep2N
        size op_inner2
        size op_inner2N
        size op_irep2
        size op_irep2N
        size op_done

        .macro rel op
rel_\op : .int tgt_\op - \op
        .endm

        rel op_loop1
        rel op_loop2
        rel op_rep2
        rel op_rep2N
        rel op_irep2
        rel op_irep2N

        .macro count op
count_\op : .int c_\op - \op
        .endm

        count op_rep1
        count op_rep1N
        count op_irep2
        count op_irep2N
