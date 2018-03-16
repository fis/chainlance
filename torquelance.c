/*
 * torquelance bfjoust interpreter; based on gearlance and so forth.
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

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "parser.h" /* for MAXNEST */

#define MINTAPE 10
#define MAXTAPE 30

static unsigned char tape[MAXTAPE];
static unsigned repSA[MAXNEST], repSB[MAXNEST];

static int scores[2][MAXTAPE+1];

typedef int jitfunc(unsigned long len, unsigned char *tape,
                    char *progA, char *progB,
                    unsigned *repSA, unsigned *repSB);

extern const char header;
extern const unsigned header_size;

int main(int argc, char *argv[])
{
	/* check args */

	if (argc != 4)
	{
		fprintf(stderr, "usage: %s progA.bin progB.bin progB2.bin\n", argv[0]);
		return 1;
	}

	/* open and read competitor sizes */

	int fd[3];
	unsigned size[3];

	for (int prog = 0; prog < 3; prog++)
	{
		struct stat buf;
		fd[prog] = sopen(argv[1+prog]);
		if (fstat(fd[prog], &buf) != 0)
			die("fstat failed: %s", argv[1+prog]);
		size[prog] = buf.st_size;
	}

	/* allocate executable memory and prepare code */

	unsigned total = header_size + size[0] + size[1] + size[2];

	char *base = mmap(0, total, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED)
		die("mmap failed");
	jitfunc *code = (jitfunc *)base;

	char *pbase[3] = {
		base + header_size,
		base + header_size + size[0],
		base + header_size + size[0] + size[1]
	};

	memcpy(base, &header, header_size);

	for (int prog = 0; prog < 3; prog++)
	{
		unsigned at = 0;
		ssize_t got;

		while (at < size[prog] && (got = read(fd[prog], pbase[prog] + at, size[prog] - at)) > 0)
			at += got;
		if (at != size[prog])
			die("read failed: %s", argv[1+prog]);
	}

	/* run all battles */

	for (int pol = 0; pol < 2; pol++)
	{
		for (int tapesize = MINTAPE; tapesize <= MAXTAPE; tapesize++)
		{
			memset(tape, 0, tapesize);
			tape[0] = 128;
			tape[tapesize-1] = 128;

			scores[pol][tapesize] = code(tapesize, tape, pbase[0], pbase[1+pol], repSA, repSB);
		}
	}

	/* summarize results */

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

	printf("%d\n", score);

	return 0;
}
