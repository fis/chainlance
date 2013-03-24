/*
 * genelance bfjoust interpreter; based on gearlance, itself based on
 * cranklance, itself based on chainlance.
 *
 * This one is meant for people writing bfjoust evolvers.  You invoke
 * it as "./genelance prog1 prog2 prog3 ...", where the programs are
 * your evaluation opponents.  They will be parsed and "precompiled"
 * in advance.  After that, the program reads a single (nl-terminated)
 * bfjoust program from stdin, and prints out its fitness.  This
 * continues until EOF or an empty program.  The output will have the
 * form "N D1,D2,D3,..." where N is the total points (integer ranging
 * from -M*42 to M*42, where M is the number of evaluation opponents)
 * and D1,D2,D3,... the individual points (integers from -42 to 42)
 * against a particular program, if you want to weight them
 * differently.
 *
 * Copyright 2013 Heikki Kallasjoki. All rights reserved.
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

static int scores[2][MAXTAPE+1];

/* actual interpretation */

static void run(int nprogs, struct oplist **hill);

/* main application */

int main(int argc, char *argv[])
{
	/* check args */

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s prog1.bfjoust prog2.bfjoust ...\n", argv[0]);
		return 1;
	}

	/* parse the hill */

	int nprogs = argc - 1;

	if (setjmp(fail_buf))
	{
		printf("parse error: %s\n", fail_msg);
		return 1;
	}

	struct oplist **hill = smalloc(nprogs * sizeof *hill);

	for (int i = 0; i < nprogs; i++)
	{
		int fd = sopen(argv[1+i]);
		hill[i] = parse(fd);
	}

	/* run the loop */

	run(nprogs, hill);

	return 0;
}

/* actual interpretation, impl */

static unsigned char tape[MAXTAPE];

static void run(int nprogs, struct oplist **hill)
{
	/* make opcode lists for all the hill programs */

	void ***hillopc = smalloc(nprogs * sizeof *hillopc);

	for (int i = 0; i < nprogs; i++)
	{
		struct oplist *opsA = hill[i];
		struct op *oplA = opsA->ops;
		void **opcA = smalloc((opsA->len+1) * sizeof *opcA);
		hillopc[i] = opcA;

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
	}

	int *dscores = smalloc(nprogs * sizeof *dscores);

outer: ;

	/* slurp in a program */

	struct oplist *challenger = parse(0);
	if (challenger->len == 0)
	{
		opl_free(challenger);
		return;
	}

	/* convert opcode types into pointers for challenger */

	struct oplist *opsB = challenger;
	struct op *oplB = opsB->ops;
	void **opcB = smalloc((opsB->len+1) * sizeof *opcB);

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

	/* run against all the hill members */

	int hill_opponent = 0;

inner:
	if (hill_opponent == nprogs)
	{
		/* all done, clean up and compute final fitness */

		opl_free(opsB);
		free(opcB);

		int total = 0;
		for (int i = 0; i < nprogs; i++)
			total += dscores[i];

		printf("%d", total);
		for (int i = 0; i < nprogs; i++)
			printf("%c%d", i == 0 ? ' ' : ',', dscores[i]);
		printf("\n");

		goto outer;
	}

	struct op *oplA = hill[hill_opponent]->ops;
	void **opcA = hillopc[hill_opponent];

	/* state-holding variables */

	static int repStackA[MAXNEST], repStackB[MAXNEST];

	int ipA = 0, ipB = 0;
	unsigned char *ptrA = 0, *ptrB = 0, bcache = 0;
	int repA = 0, repB = 0, *repSA = 0, *repSB = 0;
	int deathsA = 0, deathsB = 0;
	int cycles = 0;
	int score = 0;

	/* execute with all tape lengths and both relative polarities */

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
	  \
		score = 0; \
		goto *opcA[0]; \
	sym: \
		scores[pol][tapesize] = score; \
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

	/* update score, go to next program */

	int dscore = 0;

	for (int pol = 0; pol < 2; pol++)
	{
		for (int tlen = MINTAPE; tlen <= MAXTAPE; tlen++)
			dscore -= scores[pol][tlen]; /* scores in terms of A */
	}

	dscores[hill_opponent] = dscore;

	hill_opponent++;
	goto inner;

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

op_leftA:
	ptrA--;
	if (ptrA < tape) goto fallA;
	NEXTA;
op_leftB:
	ptrB++;
	if (ptrB >= &tape[tapesize]) goto fallB;
	NEXTB;
op_rightA:
	ptrA++;
	if (ptrA >= &tape[tapesize]) goto fallA;
	NEXTA;
op_rightB:
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

#define PARSE_NEWLINE_AS_EOF 1
#include "parser.c"