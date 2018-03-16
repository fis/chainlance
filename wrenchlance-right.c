/*
 * wrenchlance bfjoust interpreter; based on gearlance and so forth.
 * precompiler of a right-side bfjoust warrior into x86-64 asm source
 * of an executable to run a tournament against wrenchlance-left binary.
 *
 * Copyright (c) 2011-2013 Heikki Kallasjoki
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "parser.h"

/* compilation */

static void compile(struct oplist *ops, int kettle);

/* main application */

int main(int argc, char *argv[])
{
	/* check args */

	if (argc != 2)
	{
		fprintf(stderr, "usage: %s right.bfjoust | gcc -std=gnu99 wrenchlance-stub.c wrenchlance-header.s -x assembler - -o right\n", argv[0]);
		return 1;
	}

	/* parse competitor */

	int fd_in = sopen(argv[1]);

	if (setjmp(fail_buf))
	{
		printf("parse error: %s\n", fail_msg);
		return 1;
	}

	struct oplist *ops = parse(fd_in);

	/* compile the sieve version */

	compile(ops, 0);

	/* flip polarity */

	struct op *opl = ops->ops;

	for (unsigned at = 0; at < ops->len; at++)
	{
		struct op *op = &opl[at];
		if (op->type == OP_INC)
			op->type = OP_DEC;
		else if (op->type == OP_DEC)
			op->type = OP_INC;
	}

	/* compile the kettle version */

	compile(ops, 1);

	return 0;
}

/* compilation, impl */

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

const char *header =
	".macro next\n"
	"leaq 9f(%rip), %r13\n"
	"jmp tick\n"
	"9:\n"
	".endm\n";

#define op_inc	  \
	"incb (%rdi)\n" \
	"next\n"
#define op_dec	  \
	"decb (%rdi)\n" \
	"next\n"
#define op_left	  \
	"incq %rdi\n" \
	"cmpq %r11, %rdi\n" \
	"ja fallB\n" \
	"next\n"
#define op_right	  \
	"decq %rdi\n" \
	"cmpq %r10, %rdi\n" \
	"jb fallB\n" \
	"next\n"
#define op_wait	  \
	"next\n"

#define op_loop1	  \
	"leaq op%c_%d(%%rip), %%r13\n" \
	"testb %%al, %%al\n" \
	"jz tick\n" \
	"leaq 0f(%%rip), %%r13\n" \
	"jmp tick\n" \
	"0:\n"
#define op_loop2	  \
	"leaq op%c_%d(%%rip), %%r13\n" \
	"testb %%al, %%al\n" \
	"jnz tick\n" \
	"leaq 0f(%%rip), %%r13\n" \
	"jmp tick\n" \
	"0:\n"

#define op_rep1	  \
	"movl $%d, %%edx\n"
#define op_rep1N	  \
	"movl %%edx, (%%r9)\n" \
	"addq $4, %%r9\n" \
	"movl $%d, %%edx\n"

#define op_rep2	  \
	"decl %%edx\n" \
	"jnz op%c_%d\n"
#define op_rep2N	  \
	"decl %%edx\n" \
	"jnz op%c_%d\n" \
	"subq $4, %%r9\n" \
	"movl (%%r9), %%edx\n"

#define op_inner2	  \
	"movl $1, %edx\n"
#define op_inner2N	  \
	"movl %edx, (%r9)\n" \
	"addq $4, %r9\n" \
	"movl $1, %edx\n"

#define op_irep2	  \
	"incl %%edx\n" \
	"cmpl $%d, %%edx\n" \
	"jle op%c_%d\n"
#define op_irep2N	  \
	"incl %%edx\n" \
	"cmpl $%d, %%edx\n" \
	"jle op%c_%d\n" \
	"subq $4, %%r9\n" \
	"movl (%%r9), %%edx\n"

#define op_done	  \
	"lea tick(%rip), %r13\n" \
	"jmp tick\n"

#define OP_REL 1
#define OP_COUNT 2
#define OP_COUNT_REL 3

struct opnfo
{
	const char *code;
	const char *nest;
	int mode; /* 0, OP_REL, OP_COUNT or OP_COUNT_REL */
};

static struct opnfo optable[OP_MAX] = {
	[OP_INC]    = { op_inc, 0, 0 },
	[OP_DEC]    = { op_dec, 0, 0 },
	[OP_LEFT]   = { op_left, 0, 0 },
	[OP_RIGHT]  = { op_right, 0, 0 },
	[OP_WAIT]   = { op_wait, 0, 0 },
	[OP_LOOP1]  = { op_loop1, 0, OP_REL },
	[OP_LOOP2]  = { op_loop2, 0, OP_REL },
	[OP_REP1]   = { op_rep1, op_rep1N, OP_COUNT },
	[OP_REP2]   = { op_rep2, op_rep2N, OP_REL },
	[OP_IREP1]  = { op_rep1, op_rep1N, OP_COUNT },
	[OP_INNER1] = { op_rep2, op_rep2N, OP_REL },
	[OP_INNER2] = { op_inner2, op_inner2N, 0 },
	[OP_IREP2]  = { op_irep2, op_irep2N, OP_COUNT_REL },
	[OP_DONE]   = { op_done, 0, 0 },
};

int opnest[OP_MAX] = {
	[OP_REP1] = 1,  [OP_IREP1] = 1,   [OP_INNER2] = 1,
	[OP_REP2] = -1, [OP_INNER1] = -1, [OP_IREP2] = -1
};

static void compile(struct oplist *ops, int kettle)
{
	if (!kettle)
		printf("%s", header);

	char pfx = kettle ? 'K' : 'S';
	printf("prog%c:\n", pfx);
	printf(".global prog%c\n", pfx);

	/* write out labels and opcodes */

	struct op *opl = ops->ops;
	int depth = 0;

	for (unsigned at = 0; at < ops->len; at++)
	{
		struct op *op = &opl[at];
		struct opnfo *nfo = &optable[op->type];

		const char *code = nfo->code;

		if (opnest[op->type] < 0)
			depth--;
		if (nfo->nest && depth > 0)
			code = nfo->nest;
		if (opnest[op->type] > 0)
			depth++;

		printf("op%c_%d:\n", pfx, at);

		switch (nfo->mode)
		{
		case 0:
			printf("%s", code);
			break;
		case OP_REL:
			printf(code, pfx, op->match+1);
			break;
		case OP_COUNT:
			printf(code, op->count);
			break;
		case OP_COUNT_REL:
			printf(code, op->count, pfx, op->match+1);
			break;
		}
	}
}
