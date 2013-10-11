/*
 * wrenchlance bfjoust interpreter; based on gearlance and so forth.
 * precompiler of a left-side bfjoust warrior into PIC x86-64 machine code.
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

#define PARSER_EXTRAFIELDS unsigned code;
#include "parser.h"

/* compilation */

static char *compile(struct oplist *ops, unsigned *outsize);

/* main application */

int main(int argc, char *argv[])
{
	/* check args */

	if (argc != 3)
	{
		fprintf(stderr, "usage: %s left.bfjoust left.bin\n", argv[0]);
		return 1;
	}

	/* parse competitor */

	int fd_in = sopen(argv[1]), fd_out = screat(argv[2]);

	if (setjmp(fail_buf))
	{
		printf("parse error: %s\n", fail_msg);
		return 1;
	}

	struct oplist *ops = parse(fd_in);

	/* compile it */

	unsigned size;
	char *code = compile(ops, &size);

	if (write(fd_out, code, size) != size)
		die("write failed: %s", argv[2]);

	close(fd_out);
	return 0;
}

/* compilation, impl */

extern const char
	op_inc, size_op_inc, op_dec, size_op_dec,
	op_left, size_op_left, op_right, size_op_right,
	op_wait, size_op_wait,
	op_loop1, op_loop1_rel, size_op_loop1,
	op_loop2, op_loop2_rel, size_op_loop2,
	op_rep1, op_rep1_count, size_op_rep1,
	op_rep1N, op_rep1N_count, size_op_rep1N,
	op_rep2, op_rep2_rel, size_op_rep2,
	op_rep2N, op_rep2N_rel, size_op_rep2N,
	op_inner2, size_op_inner2,
	op_inner2N, size_op_inner2N,
	op_irep2, op_irep2_rel, op_irep2_count, size_op_irep2,
	op_irep2N, op_irep2N_rel, op_irep2N_count, size_op_irep2N,
	op_done, size_op_done;

#define AV(s) ((unsigned)(unsigned long)(s))

struct jitnfo
{
	const char *code, *size, *rel, *count;
	struct jitnfo *nest;
};

static struct jitnfo optable[OP_MAX] = {
	[OP_INC]    = { &op_inc, &size_op_inc, 0, 0, 0, },
	[OP_DEC]    = { &op_dec, &size_op_dec, 0, 0, 0, },
	[OP_LEFT]   = { &op_left, &size_op_left, 0, 0, 0, },
	[OP_RIGHT]  = { &op_right, &size_op_right, 0, 0, 0, },
	[OP_WAIT]   = { &op_wait, &size_op_wait, 0, 0, 0, },
	[OP_LOOP1]  = { &op_loop1, &size_op_loop1, &op_loop1_rel, 0, 0, },
	[OP_LOOP2]  = { &op_loop2, &size_op_loop2, &op_loop2_rel, 0, 0, },
	[OP_REP1]   = { &op_rep1, &size_op_rep1, 0, &op_rep1_count,
	                &(struct jitnfo){ &op_rep1N, &size_op_rep1N, 0, &op_rep1N_count, 0 } },
	[OP_REP2]   = { &op_rep2, &size_op_rep2, &op_rep2_rel, 0,
	                &(struct jitnfo){ &op_rep2N, &size_op_rep2N, &op_rep2N_rel, 0, 0 } },
	[OP_IREP1]  = { &op_rep1, &size_op_rep1, 0, &op_rep1_count,
	                &(struct jitnfo){ &op_rep1N, &size_op_rep1N, 0, &op_rep1N_count, 0 } },
	[OP_INNER1] = { &op_rep2, &size_op_rep2, &op_rep2_rel, 0,
	                &(struct jitnfo){ &op_rep2N, &size_op_rep2N, &op_rep2N_rel, 0, 0 } },
	[OP_INNER2] = { &op_inner2, &size_op_inner2, 0, 0,
	                &(struct jitnfo){ &op_inner2N, &size_op_inner2N, 0, 0, 0 } },
	[OP_IREP2]  = { &op_irep2, &size_op_irep2, &op_irep2_rel, &op_irep2_count,
	                &(struct jitnfo){ &op_irep2N, &size_op_irep2N, &op_irep2N_rel, &op_irep2N_count, 0 } },
	[OP_DONE]   = { &op_done, &size_op_done, 0, 0, 0 },
};

int opnest[OP_MAX] = {
	[OP_REP1] = 1,  [OP_IREP1] = 1,   [OP_INNER2] = 1,
	[OP_REP2] = -1, [OP_INNER1] = -1, [OP_IREP2] = -1
};

static char *compile(struct oplist *ops, unsigned *outsize)
{
	struct op *opl = ops->ops;
	int depth;

	/* compute program size and offsets */

	unsigned size = 0;

	depth = 0;
	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &opl[at];
		struct jitnfo *nfo = &optable[op->type];

		if (opnest[op->type] < 0)
			depth--;
		if (nfo->nest && depth > 0)
			nfo = nfo->nest;
		if (opnest[op->type] > 0)
			depth++;

		op->code = size;
		size += AV(nfo->size);
	}

	*outsize = size;

	/* allocate memory for compiled program */

	char *base = smalloc(size);

	/* generate machine code */

	depth = 0;
	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &opl[at];
		struct jitnfo *nfo = &optable[op->type];

		if (opnest[op->type] < 0)
			depth--;
		if (nfo->nest && depth > 0)
			nfo = nfo->nest;
		if (opnest[op->type] > 0)
			depth++;

		memcpy(base + op->code, nfo->code, AV(nfo->size));

		if (AV(nfo->rel))
		{
			char *target = base + opl[op->match+1].code;
			int disp = target - (base + op->code + AV(nfo->rel) + 4);
			*(int *)(base + op->code + AV(nfo->rel)) = disp;
		}

		if (AV(nfo->count))
			*(unsigned *)(base + op->code + AV(nfo->count)) = op->count;
	}

	return base;
}

#include "parser.c"
#include "common.c"
