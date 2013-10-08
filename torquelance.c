/*
 * torquelance bfjoust interpreter; based on gearlance and so forth.
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

#define AV(s) ((unsigned)(unsigned long)&(s))

extern const char header, size_header;

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

	unsigned total = AV(size_header) + size[0] + size[1] + size[2];

	char *base = mmap(0, total, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED)
		die("mmap failed");
	jitfunc *code = (jitfunc *)base;

	char *pbase[3] = {
		base + AV(size_header),
		base + AV(size_header) + size[0],
		base + AV(size_header) + size[0] + size[1]
	};

	memcpy(base, &header, AV(size_header));

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

#include "common.c"
