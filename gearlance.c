/*
 * gearlance bfjoust interpreter; based on cranklance, itself based on
 * chainlance.
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

#include "parser.h"

#define MINTAPE 10
#define MAXTAPE 30

/* #define TRACE 1 */

#ifdef CRANK_IT
#define CRANK(x) x
#else
#define CRANK(x) /* no action */
#endif

static int scores[2][MAXTAPE+1];

#ifdef CRANK_IT
static struct {
	long long cycles;
} stats = {
	.cycles = 0
};
#endif

/* actual interpretation */

static void run(struct oplist *opsA, struct oplist *opsB);

/* main application */

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
		[OP_REP1] = '(', [OP_REP2] = ')', [OP_INNER1] = '{', [OP_INNER2] = '}'
	};
	for (int at = 0; at < opsA->len; at++)
	{
		struct op *op = &opsA->ops[at];
		printf("%3d:  %c  ", at, opchars[op->type]);
		if (op->match != -1) printf("(%-2d  ", op->match); else printf("     ");
		if (op->inner != -1) printf("{%-2d  ", op->inner); else printf("     ");
		if (op->count != -1) printf("*%d", op->count);
		printf("\n");
	}
	return 0;
#endif

	/* run them */

	run(opsA, opsB);

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

/* actual interpretation, impl */

static unsigned char tape[MAXTAPE];

static void run(struct oplist *opsA, struct oplist *opsB)
{
	struct op *oplA = opsA->ops, *oplB = opsB->ops;

	/* convert opcode types into pointers for both progs */

	void **opcA = smalloc((opsA->len+1) * sizeof *opcA);
	void **opcB = smalloc((opsB->len+1) * sizeof *opcB);

	for (int at = 0; at < opsA->len; at++)
	{
		struct op *op = &oplA[at];
		void **opc = &opcA[at];
		switch (op->type)
		{
		case OP_INC:    *opc = &&op_incA;    break;
		case OP_DEC:    *opc = &&op_decA;    break;
		case OP_LEFT:   *opc = &&op_leftA;   break;
		case OP_RIGHT:  *opc = &&op_rightA;  break;
		case OP_WAIT:   *opc = &&op_waitA;   break;
		case OP_LOOP1:  *opc = &&op_loop1A;  break;
		case OP_LOOP2:  *opc = &&op_loop2A;  break;
		case OP_REP1:   *opc = &&op_rep1A;   break;
		case OP_REP2:
			if (op->inner != -1) *opc = &&op_irep2A;
			else                 *opc = &&op_rep2A;
			break;
		case OP_INNER1: *opc = &&op_inner1A; break;
		case OP_INNER2: *opc = &&op_inner2A; break;
		}
	}

	opcA[opsA->len] = &&op_doneA;

	for (int at = 0; at < opsB->len; at++)
	{
		struct op *op = &oplB[at];
		void **opc = &opcB[at];
		switch (op->type)
		{
		case OP_INC:    *opc = &&op_incB;    break;
		case OP_DEC:    *opc = &&op_decB;    break;
		case OP_LEFT:   *opc = &&op_leftB;   break;
		case OP_RIGHT:  *opc = &&op_rightB;  break;
		case OP_WAIT:   *opc = &&op_waitB;   break;
		case OP_LOOP1:  *opc = &&op_loop1B;  break;
		case OP_LOOP2:  *opc = &&op_loop2B;  break;
		case OP_REP1:   *opc = &&op_rep1B;   break;
		case OP_REP2:
			if (op->inner != -1) *opc = &&op_irep2B;
			else                 *opc = &&op_rep2B;
			break;
		case OP_INNER1: *opc = &&op_inner1B; break;
		case OP_INNER2: *opc = &&op_inner2B; break;
		}
	}

	opcB[opsB->len] = &&nextcycle; /* a slight shortcut */

	/* state-holding variables */

	static int repStackA[MAXNEST], repStackB[MAXNEST];

#ifdef CRANK_IT
	static unsigned tapestats[2][MAXTAPE];
	static unsigned char tapemax[2][MAXTAPE];
#endif

	int ipA = 0, ipB = 0;
	unsigned char *ptrA = 0, *ptrB = 0, bcache = 0;
	int repA = 0, repB = 0, *repSA = 0, *repSB = 0;
	int deathsA = 0, deathsB = 0;
	int cycles = 0;
	int score = 0;

	/* execute with all tape lengths and both relative polarities */

#ifdef CRANK_IT
#define EXECUTE_STATS(pol)	  \
	stats.cycles += (MAXCYCLES - cycles); \
	printf("CYCLES[%d,%d] = %d\n", pol, tapesize, MAXCYCLES - cycles); \
	  \
	printf("TAPEABS[%d,%d] =", pol, tapesize); \
	for (int p = 0; p < tapesize; p++) printf(" %d", tape[p] >= 128 ? tape[p]-256 : tape[p]); \
	printf("\n"); \
	printf("TAPEMAX[%d,%d] =", pol, tapesize); \
	for (int p = 0; p < tapesize; p++) printf(" %u/%u", tapemax[0][p], tapemax[1][p]); \
	printf("\n"); \
	printf("TAPEHEAT[%d,%d] =", pol, tapesize); \
	for (int p = 0; p < tapesize; p++) printf(" %u/%u", tapestats[0][p], tapestats[1][p]); \
	printf("\n")
#endif

#define EXECUTE_ALL(sym, pol)	  \
	ret = &&sym; \
	for (tapesize = MINTAPE; tapesize <= MAXTAPE; tapesize++) \
	{ \
		ipA = 0, ipB = 0; \
	  \
		memset(tape, 0, tapesize); \
		ptrA = &tape[0], ptrB = &tape[tapesize-1]; \
		*ptrA = 128, *ptrB = 128; bcache = 128; \
	  \
		repSA = repStackA, repSB = repStackB; \
		deathsA = 0, deathsB = 0; \
	  \
		cycles = MAXCYCLES; \
		CRANK(memset(tapestats, 0, sizeof tapestats)); \
		CRANK(memset(tapemax, 0, sizeof tapemax)); \
	  \
		score = 0; \
		goto *opcA[0]; \
	sym: \
		scores[pol][tapesize] = score; \
		CRANK(EXECUTE_STATS(pol)); \
	}

	void *ret;
	int tapesize = 0;

	EXECUTE_ALL(done_normal, 0);

	for (int at = 0; at < opsB->len; at++)
	{
		enum optype op = oplB[at].type;
		if (op == OP_INC) opcB[at] = &&op_decB;
		else if (op == OP_DEC) opcB[at] = &&op_incB;
	}

	EXECUTE_ALL(done_flipped, 1);

	free(opcA);
	free(opcB);
	return;

	/* actual core */

#define NEXTA ipA++; goto *opcB[ipB]
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
	printf("  dA %d  dB %d   ipA %d ipB %d   repA %d repB %d\n", deathsA, deathsB, ipA, ipB, repA, repB);
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
	tapestats[0][ptrA-tape]++;
	tapestats[1][ptrB-tape]++;
#endif

	if (!cycles)
		goto *ret;

	bcache = *ptrB;
	goto *opcA[ipA];

fallA:
	deathsA = 2;
	goto *opcB[ipB];

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
#define MAXSTAT(ptr,i)	  \
	if (*ptr >= 128) { if (256-*ptr > tapemax[i][ptr-tape]) tapemax[i][ptr-tape] = 256-*ptr; } \
	else { if (*ptr > tapemax[i][ptr-tape]) tapemax[i][ptr-tape] = *ptr; }
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
	if (!*ptrA) ipA = oplA[ipA].match;
	NEXTA;
op_loop1B:
	if (!bcache) ipB = oplB[ipB].match;
	NEXTB;
op_loop2A:
	if (*ptrA) ipA = oplA[ipA].match;
	NEXTA;
op_loop2B:
	if (bcache) ipB = oplB[ipB].match;
	NEXTB;

	/* simple (..) repeats with no corresponding {..} block */

op_rep1A:
	*repSA++ = repA; repA = oplA[ipA].count;
	goto *opcA[++ipA];
op_rep1B:
	*repSB++ = repB; repB = oplB[ipB].count;
	goto *opcB[++ipB];

op_rep2A:
	if (--repA) ipA = oplA[ipA].match;
	else repA = *--repSA;
	goto *opcA[++ipA];
op_rep2B:
	if (--repB) ipB = oplB[ipB].match;
	else repB = *--repSB;
	goto *opcB[++ipB];

	/* complex (..{ and }..) repeats; use .inner for target, count in different dirs */

op_inner1A:
	if (--repA) ipA = oplA[ipA].inner;
	else repA = *--repSA;
	goto *opcA[++ipA];
op_inner1B:
	if (--repB) ipB = oplB[ipB].inner;
	else repB = *--repSB;
	goto *opcB[++ipB];

op_inner2A:
	*repSA++ = repA; repA = 1;
	goto *opcA[++ipA];
op_inner2B:
	*repSB++ = repB; repB = 1;
	goto *opcB[++ipB];

op_irep2A:
	if (++repA <= oplA[ipA].count) ipA = oplA[ipA].inner;
	else repA = *--repSA;
	goto *opcA[++ipA];
op_irep2B:
	if (++repB <= oplB[ipB].count) ipB = oplB[ipB].inner;
	else repB = *--repSB;
	goto *opcB[++ipB];

op_doneA:
	goto *opcB[ipB];
}

#include "parser.c"
