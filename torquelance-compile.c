/*
 * torquelance bfjoust interpreter; based on gearlance and so forth.
 * precompiler of bfjoust into PIC x86-64 machine code.
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

static char *compile(struct oplist *ops, unsigned *outsize, unsigned mode_B);

/* main application */

int main(int argc, char *argv[])
{
	/* check args */

	if (argc != 4 || (strcmp(argv[3], "A") != 0
	                  && strcmp(argv[3], "B") != 0
	                  && strcmp(argv[3], "B2") != 0))
	{
		fprintf(stderr, "usage: %s prog.bfjoust prog.bin A|B|B2\n", argv[0]);
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

	/* flip polarity if required */

	if (strcmp(argv[3], "B2") == 0)
	{
		struct op *opl = ops->ops;

		for (unsigned at = 0; at < ops->len; at++)
		{
			struct op *op = &opl[at];
			if (op->type == OP_INC)
				op->type = OP_DEC;
			else if (op->type == OP_DEC)
				op->type = OP_INC;
		}
	}

	/* for debugging purposes, dump out the parse */

#if 0
	unsigned char opchars[] = {
		[OP_INC] = '+', [OP_DEC] = '-', [OP_LEFT] = '<', [OP_RIGHT] = '>',
		[OP_WAIT] = '.', [OP_LOOP1] = '[', [OP_LOOP2] = ']',
		[OP_REP1] = '(', [OP_REP2] = ')',
		[OP_IREP1] = '(', [OP_INNER1] = '{', [OP_INNER2] = '}', [OP_IREP2] = ')',
  };
	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &ops->ops[at];
		printf("%3d:  %c  ", at, opchars[op->type]);
		if (op->match != -1) printf("m%-2d  ", op->match); else printf("     ");
		if (op->inner != -1) printf("i%-2d  ", op->inner); else printf("     ");
		if (op->count != -1) printf("*%d", op->count);
		printf("\n");
	}
	return 0;
#endif

	/* compile it */

	unsigned size;
	char *code = compile(ops, &size, strcmp(argv[3], "A") != 0);

	if (write(fd_out, code, size) != size)
		die("write failed: %s", argv[2]);

	close(fd_out);
	return 0;
}

/* compilation, impl */

extern const char
	op_incA, op_incB, op_decA, op_decB,
	op_leftA, op_leftB, op_rightA, op_rightB,
	op_waitA, op_waitB,
	op_loop1A, op_loop1B, op_loop2A, op_loop2B,
	op_rep1A, op_rep1B, op_rep2A, op_rep2B,
	op_inner2A, op_inner2B, op_irep2A, op_irep2B,
	op_doneA, op_doneB;

extern const unsigned
	size_op_incA, size_op_incB, size_op_decA, size_op_decB,
	size_op_leftA, size_op_leftB, size_op_rightA, size_op_rightB,
	size_op_waitA, size_op_waitB,
	size_op_loop1A, size_op_loop1B, size_op_loop2A, size_op_loop2B,
	size_op_rep1A, size_op_rep1B, size_op_rep2A, size_op_rep2B,
	size_op_inner2A, size_op_inner2B, size_op_irep2A, size_op_irep2B,
	size_op_doneA, size_op_doneB;

extern const unsigned
	rel_op_loop1A, rel_op_loop1B, rel_op_loop2A, rel_op_loop2B,
	rel_op_rep2A, rel_op_rep2B, rel_op_irep2A, rel_op_irep2B;

extern const unsigned
	count_op_rep1A, count_op_rep1B, count_op_irep2A, count_op_irep2B;

struct jitnfo
{
	const char *code;
	const unsigned *size, *rel, *count;
};

#define OPTABLE(p) {	  \
		[OP_INC]    = { .code = &op_inc##p, .size = &size_op_inc##p }, \
		[OP_DEC]    = { .code = &op_dec##p, .size = &size_op_dec##p }, \
		[OP_LEFT]   = { .code = &op_left##p, .size = &size_op_left##p }, \
		[OP_RIGHT]  = { .code = &op_right##p, .size = &size_op_right##p }, \
		[OP_WAIT]   = { .code = &op_wait##p, .size = &size_op_wait##p }, \
		[OP_LOOP1]  = { .code = &op_loop1##p, .size = &size_op_loop1##p, .rel = &rel_op_loop1##p }, \
		[OP_LOOP2]  = { .code = &op_loop2##p, .size = &size_op_loop2##p, .rel = &rel_op_loop2##p }, \
		[OP_REP1]   = { .code = &op_rep1##p, .size = &size_op_rep1##p, .count = &count_op_rep1##p }, \
		[OP_REP2]   = { .code = &op_rep2##p, .size = &size_op_rep2##p, .rel = &rel_op_rep2##p }, \
		[OP_IREP1]  = { .code = &op_rep1##p, .size = &size_op_rep1##p, .count = &count_op_rep1##p }, \
		[OP_INNER1] = { .code = &op_rep2##p, .size = &size_op_rep2##p, .rel = &rel_op_rep2##p }, \
		[OP_INNER2] = { .code = &op_inner2##p, .size = &size_op_inner2##p }, \
		[OP_IREP2]  = { .code = &op_irep2##p, .size = &size_op_irep2##p, .rel = &rel_op_irep2##p, .count = &count_op_irep2##p }, \
		[OP_DONE]   = { .code = &op_done##p, .size = &size_op_done##p }, \
	}

static struct jitnfo optable[2][OP_MAX] = {
	OPTABLE(A), OPTABLE(B)
};

static char *compile(struct oplist *ops, unsigned *outsize, unsigned mode_B)
{
	struct op *opl = ops->ops;

	/* compute program size and offsets */

	unsigned size = 0;

	for (unsigned at = 0; at < ops->len; at++)
	{
		struct op *op = &opl[at];
		op->code = size;
		size += *optable[mode_B][op->type].size;
	}

	*outsize = size;

	/* allocate memory for compiled program */

	char *base = smalloc(size);

	/* generate machine code */

	for (unsigned at = 0; at < ops->len; at++)
	{
		struct op *op = &opl[at];
		struct jitnfo *nfo = &optable[mode_B][op->type];

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
