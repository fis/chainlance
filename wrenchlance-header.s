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

        /* OLD
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

        .set MAXCYCLES, 100000

        /*
         * int header(unsigned long len {rdi}, char *tape {rsi},
         *            char *pA {rdx}, char *pB {rcx},
         *            unsigned *repSA {r8}, unsigned *repSB {r9});
         */
header:
        subq $(4*8+8), %rsp
        movq %rbx, 8(%rsp)
        movq %rbp, 16(%rsp)
        movq %r12, 24(%rsp)
        movq %r13, 32(%rsp)

        leaq fallA(%rip), %rax          /* save fallA pointer for program A */
        movq %rax, (%rsp)

        leaq -1(%rsi, %rdi), %rdi       /* compute pointer to end of tape */
        movq %rdx, %r12                 /* set initial IP of A */
        movq %rcx, %r13                 /* set initial IP of B */
        movq %rsi, %r10                 /* set tape start */
        movq %rdi, %r11                 /* set tape end */
        movb (%rdi), %al                /* initialize cached (%rdi) */
        xorl %ebx, %ebx                 /* clear death flags */
        movl $MAXCYCLES, %ebp           /* set cycle counter */

        jmp *%r12                       /* go */

tick:
        cmpb $1, (%r10)                 /* add a death if flag 0, otherwise reset if one death */
        jc 0f
        andb $0xfe, %bl
0:      adcb $0, %bl

        cmpb $1, (%r11)                 /* same for player B */
        jc 0f
        andb $0xfe, %bh
0:      adcb $0, %bh

        cmpb $2, %bh                    /* test for permadeaths */
        jae tick_deathB
        cmpb $2, %bl
        jae tick_deathA

        decl %ebp                       /* no permadeaths but out of time -> tie */
        jz tie

        movb (%rdi), %al                /* do next tick */
        jmpq *%r12

tie:    xorl %eax, %eax
        jmp fini

fallA:
        movb $2, %bl                    /* flag A for a definite loss */
        jmpq *%r13

fallB:
        cmpb $1, (%r10)                 /* count up possible pending death of A */
        adcb $0, %bl
        /* fall through: tick_deathB */

tick_deathB:
        xorl %eax, %eax                 /* tie if A also died, win for A otherwise */
        cmpb $2, %bl
        setbb %al
        jmp fini

tick_deathA:
        movl $-1, %eax                  /* win for B */
        /* fall through: fini */

fini:
        movq 8(%rsp), %rbx
        movq 16(%rsp), %rbp
        movq 24(%rsp), %r12
        movq 32(%rsp), %r13
        addq $(4*8+8), %rsp
        retq

        .global header
        .global tick
        .global fallA
        .global fallB
