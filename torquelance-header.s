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

        .set MAXCYCLES, 100000

        /*
         * int header(unsigned long len {%rdi}, char *tape {%rsi},
         *            char *pA {rdx}, char *pB {rcx},
         *            unsigned *repSA {r8}, unsigned *repSB {r9});
         */
header:
        subq $(6*8+16), %rsp
        movq %rbx, 16(%rsp)
        movq %rbp, 24(%rsp)
        movq %r12, 32(%rsp)
        movq %r13, 40(%rsp)
        movq %r14, 48(%rsp)
        movq %r15, 56(%rsp)

        leaq fallA(%rip), %rax          /* prepare [fallA, fallB] pointers */
        movq %rax, (%rsp)
        leaq fallB(%rip), %rax
        movq %rax, 8(%rsp)

        leaq -1(%rsi, %rdi), %rdi       /* compute pointer to end of tape */
        movq %rdx, %r12                 /* set initial IP of A */
        movq %rcx, %r13                 /* set initial IP of B */
        movq %rsi, %r14                 /* set tape start */
        movq %rdi, %r15                 /* set tape end */
        movb (%rdi), %bl                /* initialize cached (%rdi) */
        xorl %ecx, %ecx                 /* clear death counters */
        movl $MAXCYCLES, %edx           /* set cycle counter */
        leaq tick(%rip), %rbp           /* set tick pointer */

        jmp *%r12                       /* go */

tick:
        cmpb $1, (%r14)                 /* add a death if flag 0, otherwise reset if one death */
        jc 0f
        andb $0xfe, %cl
0:      adcb $0, %cl

        cmpb $1, (%r15)                 /* same for player B */
        jc 0f
        andb $0xfe, %ch
0:      adcb $0, %ch

        cmpb $2, %ch                    /* test for permadeaths */
        jae tick_deathB
        cmpb $2, %cl
        jae tick_deathA

        decl %edx                       /* no permadeaths but out of time -> tie */
        jz tie

        movb (%rdi), %bl                /* do next tick */
        jmpq *%r12

tie:    xorl %eax, %eax
        jmp fini

fallA:
        movb $2, %cl                    /* flag A for a definite loss */
        jmpq *%r13

fallB:
        cmpb $1, (%r14)                 /* count up possible pending death of A */
        adcb $0, %cl
        /* fall through: tick_deathB */

tick_deathB:
        xorl %eax, %eax                 /* tie if A also died, win for A otherwise */
        cmpb $2, %cl
        setbb %al
        jmp fini

tick_deathA:
        movl $-1, %eax                  /* win for B */
        /* fall through: fini */

fini:
        movq 16(%rsp), %rbx
        movq 24(%rsp), %rbp
        movq 32(%rsp), %r12
        movq 40(%rsp), %r13
        movq 48(%rsp), %r14
        movq 56(%rsp), %r15
        addq $(6*8+16), %rsp
        retq

header_size: .int .-header

        .global header
        .global header_size
