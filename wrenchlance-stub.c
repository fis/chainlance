/*
 * wrenchlance bfjoust interpreter; based on gearlance and so forth.
 * stub for building a wrenchlance-right executable.
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

extern int header(unsigned long len, unsigned char *tape,
                  char *progA, char *progB,
                  unsigned *repSA, unsigned *repSB);
extern char progK[], progS[];

int main(int argc, char *argv[])
{
	/* check args */

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s left.bin [left2.bin] [left3.bin] ...\n", argv[0]);
		return 1;
	}

	/* prepare some variables for the rest of the program */

	struct stat buf;

	size_t mmapSize = 1024;
	char *base = NULL;

	for(int i=1; i<argc; i++)
	{
		/* check left warrior size */

		int fd = sopen(argv[i]);
		if (fstat(fd, &buf) != 0)
			die("fstat failed: %s", argv[i]);
		int size = buf.st_size;

		/* expand mmap if necessary */

		if(size > mmapSize || base == NULL) {
			int oldSize = mmapSize;
			while(mmapSize < size) mmapSize *= 2;
			if(base != NULL) if(munmap(base, oldSize)) die("munmap failed");
			base = mmap(0, mmapSize, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
			if (base == MAP_FAILED) die("mmap failed");
		}

		unsigned at = 0;
		ssize_t got;

		while (at < size && (got = read(fd, base + at, size - at)) > 0)
			at += got;
		if (at != size)
			die("read failed: %s", argv[i]);

		close(fd);

		/* run all battles */

		for (int pol = 0; pol < 2; pol++)
		{
			for (int tapesize = MINTAPE; tapesize <= MAXTAPE; tapesize++)
			{
				memset(tape+1, 0, tapesize-2);
				tape[0] = 128;
				tape[tapesize-1] = 128;

				scores[pol][tapesize] = header(tapesize, tape, base, pol ? progK : progS, repSA, repSB);
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
	}

	return 0;
}

#include "common.c"
