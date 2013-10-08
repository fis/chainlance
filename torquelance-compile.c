/*
 * torquelance bfjoust interpreter; based on gearlance and so forth.
 * precompiler of bfjoust into PIC x86-64 machine code.
 *
 * Copyright 2011-2013 Heikki Kallasjoki. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY HEIKKI KALLASJOKI ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL HEIKKI KALLASJOKI OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and
 * documentation are those of the authors and should not be
 * interpreted as representing official policies, either expressed or
 * implied, of Heikki Kallasjoki.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

#define PARSER_EXTRAFIELDS unsigned code;
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

	if (strcmp(argv[3], "B2"))
	{
		struct op *opl = ops->ops;

		for (int at = 0; at < ops->len; at++)
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
	op_incA, size_op_incA, op_incB, size_op_incB,
	op_decA, size_op_decA, op_decB, size_op_decB,
	op_leftA, size_op_leftA, op_leftB, size_op_leftB,
	op_rightA, size_op_rightA, op_rightB, size_op_rightB,
	op_waitA, size_op_waitA, op_waitB, size_op_waitB,
	op_loop1A, op_loop1A_rel, size_op_loop1A,
	op_loop1B, op_loop1B_rel, size_op_loop1B,
	op_loop2A, op_loop2A_rel, size_op_loop2A,
	op_loop2B, op_loop2B_rel, size_op_loop2B,
	op_rep1A, op_rep1A_count, size_op_rep1A,
	op_rep1B, op_rep1B_count, size_op_rep1B,
	op_rep2A, op_rep2A_rel, size_op_rep2A,
	op_rep2B, op_rep2B_rel, size_op_rep2B,
	op_inner2A, size_op_inner2A, op_inner2B, size_op_inner2B,
	op_irep2A, op_irep2A_rel, op_irep2A_count, size_op_irep2A,
	op_irep2B, op_irep2B_rel, op_irep2B_count, size_op_irep2B,
	op_doneA, size_op_doneA, op_doneB, size_op_doneB;

#define AV(s) ((unsigned)(unsigned long)(s))

struct jitnfo
{
	const char *code, *size, *rel, *count;
};

#define OPTABLE(p) {	  \
		[OP_INC] = { &op_inc##p, &size_op_inc##p, 0, 0 }, \
		[OP_DEC] = { &op_dec##p, &size_op_dec##p, 0, 0 }, \
		[OP_LEFT] = { &op_left##p, &size_op_left##p, 0, 0 }, \
		[OP_RIGHT] = { &op_right##p, &size_op_right##p, 0, 0 }, \
		[OP_WAIT] = { &op_wait##p, &size_op_wait##p, 0, 0 }, \
		[OP_LOOP1] = { &op_loop1##p, &size_op_loop1##p, &op_loop1##p##_rel, 0 }, \
		[OP_LOOP2] = { &op_loop2##p, &size_op_loop2##p, &op_loop2##p##_rel, 0 }, \
		[OP_REP1] = { &op_rep1##p, &size_op_rep1##p, 0, &op_rep1##p##_count }, \
		[OP_REP2] = { &op_rep2##p, &size_op_rep2##p, &op_rep2##p##_rel, 0 }, \
		[OP_IREP1] = { &op_rep1##p, &size_op_rep1##p, 0, &op_rep1##p##_count }, \
		[OP_INNER1] = { &op_rep2##p, &size_op_rep2##p, &op_rep2##p##_rel, 0 }, \
		[OP_INNER2] = { &op_inner2##p, &size_op_inner2##p, 0, 0 }, \
		[OP_IREP2] = { &op_irep2##p, &size_op_irep2##p, &op_irep2##p##_rel, &op_irep2##p##_count }, \
	}

static struct jitnfo optable[2][OP_MAX] = {
	OPTABLE(A), OPTABLE(B)
};
static struct jitnfo opdone[2] = {
	{ &op_doneA, &size_op_doneA, 0, 0 },
	{ &op_doneB, &size_op_doneB, 0, 0 }
};

static char *compile(struct oplist *ops, unsigned *outsize, unsigned mode_B)
{
	struct op *opl = ops->ops;
	struct jitnfo *done = &opdone[mode_B];

	/* compute program size and offsets */

	unsigned size = 0;

	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &opl[at];
		op->code = size;
		size += AV(optable[mode_B][op->type].size);
	}

	*outsize = size + AV(done->size);

	/* allocate memory for compiled program */

	char *base = smalloc(*outsize);

	/* generate machine code */

	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &opl[at];
		struct jitnfo *nfo = &optable[mode_B][op->type];

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

	memcpy(base + size, done->code, AV(done->size));

	return base;
}

#include "parser.c"
#include "common.c"
