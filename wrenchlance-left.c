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
	op_inc, op_dec, op_left, op_right,
	op_wait,
	op_loop1, op_loop2,
	op_rep1, op_rep1N,
	op_rep2, op_rep2N,
	op_inner2, op_inner2N,
	op_irep2, op_irep2N,
	op_done;

extern const unsigned
	size_op_inc, size_op_dec, size_op_left, size_op_right,
	size_op_wait,
	size_op_loop1, size_op_loop2,
	size_op_rep1, size_op_rep1N,
	size_op_rep2, size_op_rep2N,
	size_op_inner2, size_op_inner2N,
	size_op_irep2, size_op_irep2N,
	size_op_done;

extern const unsigned
	rel_op_loop1, rel_op_loop2,
	rel_op_rep2, rel_op_rep2N,
	rel_op_irep2, rel_op_irep2N;

extern const unsigned
	count_op_rep1, count_op_rep1N,
	count_op_irep2, count_op_irep2N;

struct jitnfo
{
	const char *code;
	const unsigned *size, *rel, *count;
	struct jitnfo *nest;
};

static struct jitnfo optable[OP_MAX] = {
	[OP_INC]    = { .code = &op_inc, .size = &size_op_inc },
	[OP_DEC]    = { .code = &op_dec, .size = &size_op_dec },
	[OP_LEFT]   = { .code = &op_left, .size = &size_op_left },
	[OP_RIGHT]  = { .code = &op_right, .size = &size_op_right },
	[OP_WAIT]   = { .code = &op_wait, .size = &size_op_wait },
	[OP_LOOP1]  = { .code = &op_loop1, .size = &size_op_loop1, .rel = &rel_op_loop1 },
	[OP_LOOP2]  = { .code = &op_loop2, .size = &size_op_loop2, .rel = &rel_op_loop2 },
	[OP_REP1]   = { .code = &op_rep1, .size = &size_op_rep1, .count = &count_op_rep1,
	                .nest = &(struct jitnfo){ .code = &op_rep1N, .size = &size_op_rep1N, .count = &count_op_rep1N } },
	[OP_REP2]   = { .code = &op_rep2, .size = &size_op_rep2, .rel = &rel_op_rep2,
	                .nest = &(struct jitnfo){ .code = &op_rep2N, .size = &size_op_rep2N, .rel = &rel_op_rep2N } },
	[OP_IREP1]  = { .code = &op_rep1, .size = &size_op_rep1, .count = &count_op_rep1,
	                .nest = &(struct jitnfo){ .code = &op_rep1N, .size = &size_op_rep1N, .count = &count_op_rep1N } },
	[OP_INNER1] = { .code = &op_rep2, .size = &size_op_rep2, .rel = &rel_op_rep2,
	                .nest = &(struct jitnfo){ .code = &op_rep2N, .size = &size_op_rep2N, .rel = &rel_op_rep2N } },
	[OP_INNER2] = { .code = &op_inner2, .size = &size_op_inner2,
	                .nest = &(struct jitnfo){ .code = &op_inner2N, .size = &size_op_inner2N } },
	[OP_IREP2]  = { .code = &op_irep2, .size = &size_op_irep2, .rel = &rel_op_irep2, .count = &count_op_irep2,
	                .nest = &(struct jitnfo){ .code = &op_irep2N, .size = &size_op_irep2N, .rel = &rel_op_irep2N, .count = &count_op_irep2N } },
	[OP_DONE]   = { .code = &op_done, .size = &size_op_done },
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
	for (unsigned at = 0; at < ops->len; at++)
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
		size += *nfo->size;
	}

	*outsize = size;

	/* allocate memory for compiled program */

	char *base = smalloc(size);

	/* generate machine code */

	depth = 0;
	for (unsigned at = 0; at < ops->len; at++)
	{
		struct op *op = &opl[at];
		struct jitnfo *nfo = &optable[op->type];

		if (opnest[op->type] < 0)
			depth--;
		if (nfo->nest && depth > 0)
			nfo = nfo->nest;
		if (opnest[op->type] > 0)
			depth++;

		memcpy(base + op->code, nfo->code, *nfo->size);

		if (nfo->rel)
		{
			char *target = base + opl[op->match+1].code;
			int disp = target - (base + op->code + *nfo->rel + 4);
			*(int *)(base + op->code + *nfo->rel) = disp;
		}

		if (nfo->count)
			*(unsigned *)(base + op->code + *nfo->count) = op->count;
	}

	return base;
}
