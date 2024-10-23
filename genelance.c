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

#include <unistd.h>
#include <stdio.h>

#include "gearlance.h"

int main(int argc, char *argv[])
{
	/* check args */

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s prog1.bfjoust prog2.bfjoust ...\n", argv[0]);
		return 1;
	}

	/* parse and precompile the hill */

	unsigned nprogs = argc - 1;

	if (setjmp(fail_buf))
	{
		printf("parse error: %s\n", fail_msg);
		return 1;
	}

	union opcode **hill = smalloc(nprogs * sizeof *hill);
	int *dscores = smalloc(nprogs * sizeof *dscores);

	for (unsigned i = 0; i < nprogs; i++)
	{
		int fd = sopen(argv[1+i]);
		struct oplist *ops = parse(fd);
		hill[i] = core(core_compile_a, ops, 0, 0);
		opl_free(ops);
		close(fd);
	}

	/* run the main loop */

	while (1)
	{
		/* parse and compile an input */

		struct oplist *ops = parse(0);
		if (ops->len == 0 || ops->ops[0].type == OP_DONE)
			break;
		union opcode *code = core(core_compile_b, ops, 0, 0);
		opl_free(ops);

		/* run against all competitions and collect scores */

		int total = 0;
		for (unsigned i = 0; i < nprogs; i++)
		{
			core(core_run, 0, hill[i], code);

			int dscore = 0;

			for (int pol = 0; pol < 2; pol++)
			{
				for (int tlen = MINTAPE; tlen <= MAXTAPE; tlen++)
					dscore -= scores[pol][tlen]; /* scores in terms of A */
			}

			dscores[i] = dscore;
			total += dscore;
		}

		/* output report */

		printf("%d", total);
		for (unsigned i = 0; i < nprogs; i++)
		{
			putchar(i == 0 ? ' ' : ',');
			printf("%d", dscores[i]);
		}
		printf("\n");
		fflush(stdout);
	}

	return 0;
}
