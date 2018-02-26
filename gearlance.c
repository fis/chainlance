/*
 * gearlance bfjoust interpreter; based on cranklance, itself based on
 * chainlance.
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

#include "gearlance.h"

/* #define TRACE 1 */

#ifdef CRANK_IT
#define CRANK(x) x
#else
#define CRANK(x) /* no action */
#endif

int scores[2][MAXTAPE+1];

#ifdef CRANK_IT
static struct {
	unsigned long long cycles;
} stats = { 0 };

static struct {
	unsigned char tape_max[2][MAXTAPE];
	unsigned heat_position[2][MAXTAPE];
} xstats = { {{0}}, {{0}} };
#endif

/* main application */

#ifndef NO_MAIN
int main(int argc, char *argv[])
{
	/* check args */

	if (argc != 3)
	{
		fprintf(stderr, "usage: %s prog1.bfjoust prog2.bfjoust\n", argv[0]);
		return 1;
	}

	/* parse competitors */

	int fdA = sopen(argv[1]), fdB = sopen(argv[2]);

	if (setjmp(fail_buf))
	{
		printf("parse error: %s\n", fail_msg);
		return 1;
	}

	struct oplist *opsA = parse(fdA), *opsB = parse(fdB);

	/* for debugging purposes, dump out the parse */

#if 0
	unsigned char opchars[] = {
		[OP_INC] = '+', [OP_DEC] = '-', [OP_LEFT] = '<', [OP_RIGHT] = '>',
		[OP_WAIT] = '.', [OP_LOOP1] = '[', [OP_LOOP2] = ']',
		[OP_REP1] = '(', [OP_REP2] = ')',
		[OP_IREP1] = '(', [OP_INNER1] = '{', [OP_INNER2] = '}', [OP_IREP2] = ')',
		[OP_DONE] = '_',
	};
	for (int at = 0; at < opsA->len; at++)
	{
		struct op *op = &opsA->ops[at];
		printf("%3d:  %c  ", at, opchars[op->type]);
		if (op->match != -1) printf("m%-2d  ", op->match); else printf("     ");
		if (op->inner != -1) printf("i%-2d  ", op->inner); else printf("     ");
		if (op->count != -1) printf("*%d", op->count);
		printf("\n");
	}
	return 0;
#endif

	/* compile and run them */

	union opcode *codeA = core(core_compile_a, opsA, 0, 0);
	union opcode *codeB = core(core_compile_b, opsB, 0, 0);

	core(core_run, 0, codeA, codeB);

	free(codeA);
	free(codeB);

	/* summarize results */

	CRANK(printf("SUMMARY: "));

	int score = 0;

	for (int pol = 0; pol < 2; pol++)
	{
		for (int tlen = MINTAPE; tlen <= MAXTAPE; tlen++)
		{
			putchar(scores[pol][tlen] ? (scores[pol][tlen] > 0 ? '<' : '>') : 'X');
			score += scores[pol][tlen];
		}
		putchar(' ');
	}

#ifdef CRANK_IT
	printf("%d c%lld sl%d sr%d\n", score, stats.cycles, opsA->len, opsB->len);
#else
	printf("%d\n", score);
#endif

	opl_free(opsA);
	opl_free(opsB);

	return 0;
}
#endif /* !NO_MAIN */

/* actual interpretation, impl */

static unsigned char tape[MAXTAPE];

union opcode *core(enum core_action act, struct oplist *ops, union opcode *codeA, union opcode *codeB)
{
	static void * const xtableA[OP_MAX] = {
		[OP_INC] = &&op_incA, [OP_DEC] = &&op_decA,
		[OP_LEFT] = &&op_leftA, [OP_RIGHT] = &&op_rightA,
		[OP_WAIT] = &&op_waitA,
		[OP_LOOP1] = &&op_loop1A, [OP_LOOP2] = &&op_loop2A,
		[OP_REP1] = &&op_rep1A, [OP_REP2] = &&op_rep2A,
		[OP_IREP1] = &&op_rep1A, [OP_INNER1] = &&op_rep2A,
		[OP_INNER2] = &&op_inner2A, [OP_IREP2] = &&op_irep2A,
		[OP_DONE] = &&op_doneA,
	};
	static void * const xtableB[OP_MAX] = {
		[OP_INC] = &&op_incB, [OP_DEC] = &&op_decB,
		[OP_LEFT] = &&op_leftB, [OP_RIGHT] = &&op_rightB,
		[OP_WAIT] = &&op_waitB,
		[OP_LOOP1] = &&op_loop1B, [OP_LOOP2] = &&op_loop2B,
		[OP_REP1] = &&op_rep1B, [OP_REP2] = &&op_rep2B,
		[OP_IREP1] = &&op_rep1B, [OP_INNER1] = &&op_rep2B,
		[OP_INNER2] = &&op_inner2B, [OP_IREP2] = &&op_irep2B,
		[OP_DONE] = &&nextcycle, /* shortcut */
	};

	/* compilation to threaded code */

	if (act == core_compile_a || act == core_compile_b)
	{
		unsigned *offsets = smalloc(ops->len * sizeof *offsets);
		unsigned len = 0;

		for (unsigned at = 0; at < ops->len; at++)
		{
			offsets[at] = len;

			switch (ops->ops[at].type)
			{
			case OP_IREP2:
				len += 3;
				break;
			case OP_LOOP1: case OP_LOOP2: case OP_REP1: case OP_REP2: case OP_IREP1: case OP_INNER1:
				len += 2;
				break;
			default:
				len += 1;
				break;
			}
		}

		union opcode *code = smalloc(len * sizeof *code);

		void * const *xtable = (act == core_compile_a ? xtableA : xtableB);
		union opcode *opc = code;

		for (unsigned at = 0; at < ops->len; at++)
		{
			struct op *op = &ops->ops[at];

			opc->xt = xtable[op->type];
			opc++;

			switch (op->type)
			{
			case OP_LOOP1: case OP_LOOP2: case OP_REP2: case OP_INNER1:
				opc->match = &code[offsets[op->match+1]];
				opc++;
				break;
			case OP_REP1: case OP_IREP1:
				opc->count = op->count;
				opc++;
				break;
			case OP_IREP2:
				opc->count = op->count; opc++;
				opc->match = &code[offsets[op->match+1]]; opc++;
				break;
			default: /* no extra operands */
				break;
			}
		}

		return code;
	}

	/* state-holding variables */

	static int repStackA[MAXNEST], repStackB[MAXNEST];

	union opcode *ipA = 0, *ipB = 0;
	unsigned char *ptrA = 0, *ptrB = 0, bcache = 0;
	int repA = 0, repB = 0, *repSA = 0, *repSB = 0;
	int deathsA = 0, deathsB = 0;
	int cycles = 0;
	int score = 0;

	/* execute with all tape lengths and both relative polarities */

#ifdef CRANK_IT
	stats.cycles = 0;

#ifndef EXECUTE_STATS
	/* use old text-based statistics output for plain 'cranklance' */
#define EXECUTE_STATS(pol) \
	stats.cycles += (MAXCYCLES - cycles); \
	printf("CYCLES[%d,%d] = %d\n", pol, tapesize, MAXCYCLES - cycles); \
	  \
	printf("TAPEABS[%d,%d] =", pol, tapesize); \
	for (int p = 0; p < tapesize; p++) printf(" %d", tape[p] >= 128 ? tape[p]-256 : tape[p]); \
	printf("\n"); \
	printf("TAPEMAX[%d,%d] =", pol, tapesize); \
	for (int p = 0; p < tapesize; p++) printf(" %u/%u", xstats.tape_max[0][p], xstats.tape_max[1][p]); \
	printf("\n"); \
	printf("TAPEHEAT[%d,%d] =", pol, tapesize); \
	for (int p = 0; p < tapesize; p++) printf(" %u/%u", xstats.heat_position[0][p], xstats.heat_position[1][p]); \
	printf("\n")
#endif
#endif

#define EXECUTE_ALL(sym, pol) \
	ret = &&sym; \
	for (tapesize = MINTAPE; tapesize <= MAXTAPE; tapesize++) \
	{ \
		ipA = &codeA[0], ipB = &codeB[0]; \
	  \
		memset(tape, 0, tapesize); \
		ptrA = &tape[0], ptrB = &tape[tapesize-1]; \
		*ptrA = 128, *ptrB = 128; bcache = 128; \
	  \
		repSA = repStackA, repSB = repStackB; \
		deathsA = 0, deathsB = 0; \
	  \
		cycles = MAXCYCLES; \
		CRANK(memset(xstats.tape_max, 0, sizeof xstats.tape_max)); \
		CRANK(memset(xstats.heat_position, 0, sizeof xstats.heat_position)); \
	  \
		score = 0; \
		goto *ipA->xt; \
	sym: \
		scores[pol][tapesize] = score; \
		CRANK(EXECUTE_STATS(pol)); \
	}

	void *ret;
	int tapesize = 0;

	EXECUTE_ALL(done_normal, 0);

	/* FIXME: try to make this loop not interpret all operands as xts */
	for (union opcode *op = codeB; op->xt != xtableB[OP_DONE]; op++)
	{
		if (op->xt == xtableB[OP_INC]) op->xt = xtableB[OP_DEC];
		else if (op->xt == xtableB[OP_DEC]) op->xt = xtableB[OP_INC];
	}

	EXECUTE_ALL(done_flipped, 1);

	return 0;

	/* actual core */

#define NEXTA ipA++; goto *ipB->xt
#define NEXTB ipB++; goto nextcycle

/* #define TRACE 1 */

nextcycle:
	cycles--;

	if (!tape[0]) deathsA++; else if (deathsA == 1) deathsA = 0;
	if (!tape[tapesize-1]) deathsB++; else if (deathsB == 1) deathsB = 0;

#ifdef TRACE
	printf("%6d: ", MAXCYCLES-1-cycles);
	for (int i = 0; i < tapesize; i++)
		printf("%c%02x ",
		       (ptrA - tape) == i
		       ? ((ptrB - tape) == i ? 'X' : 'A')
		       : ((ptrB - tape) == i ? 'B' : ' '),
		       tape[i]);
	printf("  dA %d  dB %d   ipA %td ipB %td   repA %d repB %d\n", deathsA, deathsB, ipA-codeA, ipB-codeB, repA, repB);
#endif

	if (deathsA >= 2 && deathsB >= 2)
		goto *ret;
	else if (deathsA >= 2)
	{
		score--;
		goto *ret;
	}
	else if (deathsB >= 2)
	{
		score++;
		goto *ret;
	}

#ifdef CRANK_IT
	xstats.heat_position[0][ptrA-tape]++;
	xstats.heat_position[1][ptrB-tape]++;
#endif

	if (!cycles)
		goto *ret;

	bcache = *ptrB;
	goto *ipA->xt;

fallA:
	deathsA = 2;
	goto *ipB->xt;

fallB:
	if (!tape[0]) deathsA++;
	if (deathsA >= 2)
		goto *ret;
	score++;
	goto *ret;

op_incA:
	(*ptrA)++;
	NEXTA;
op_incB:
	(*ptrB)++;
	NEXTB;
op_decA:
	(*ptrA)--;
	NEXTA;
op_decB:
	(*ptrB)--;
	NEXTB;

#ifdef CRANK_IT
#define MAXSTAT(ptr,i) { \
		unsigned char max = *ptr >= 128 ? 256 - *ptr : *ptr; \
		if (max > xstats.tape_max[i][ptr-tape]) xstats.tape_max[i][ptr-tape] = max; }
#endif

op_leftA:
	CRANK(MAXSTAT(ptrA, 0));
	ptrA--;
	if (ptrA < tape) goto fallA;
	NEXTA;
op_leftB:
	CRANK(MAXSTAT(ptrB, 1));
	ptrB++;
	if (ptrB >= &tape[tapesize]) goto fallB;
	NEXTB;
op_rightA:
	CRANK(MAXSTAT(ptrA, 0));
	ptrA++;
	if (ptrA >= &tape[tapesize]) goto fallA;
	NEXTA;
op_rightB:
	CRANK(MAXSTAT(ptrB, 1));
	ptrB--;
	if (ptrB < tape) goto fallB;
	NEXTB;

op_waitA:
	NEXTA;
op_waitB:
	NEXTB;

op_loop1A:
	ipA++;
	if (!*ptrA) ipA = ipA->match - 1;
	NEXTA;
op_loop1B:
	ipB++;
	if (!bcache) ipB = ipB->match - 1;
	NEXTB;
op_loop2A:
	ipA++;
	if (*ptrA) ipA = ipA->match - 1;
	NEXTA;
op_loop2B:
	ipB++;
	if (bcache) ipB = ipB->match - 1;
	NEXTB;

	/* simple (..) repeats with no corresponding {..} block */

op_rep1A:
	ipA++;
	*repSA++ = repA; repA = ipA->count;
	goto *(++ipA)->xt;
op_rep1B:
	ipB++;
	*repSB++ = repB; repB = ipB->count;
	goto *(++ipB)->xt;

op_rep2A:
	ipA++;
	if (--repA) ipA = ipA->match - 1;
	else repA = *--repSA;
	goto *(++ipA)->xt;
op_rep2B:
	ipB++;
	if (--repB) ipB = ipB->match - 1;
	else repB = *--repSB;
	goto *(++ipB)->xt;

	/* complex (..{ and }..) repeats; count in different dirs in }..) */

op_inner2A:
	*repSA++ = repA; repA = 1;
	goto *(++ipA)->xt;
op_inner2B:
	*repSB++ = repB; repB = 1;
	goto *(++ipB)->xt;

op_irep2A:
	ipA += 3;
	if (++repA <= ipA[-2].count) ipA = ipA[-1].match;
	else repA = *--repSA;
	goto *ipA->xt;
op_irep2B:
	ipB += 3;
	if (++repB <= ipB[-2].count) ipB = ipB[-1].match;
	else repB = *--repSB;
	goto *ipB->xt;

op_doneA:
	goto *ipB->xt;
}
