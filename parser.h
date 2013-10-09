/*
 * cranklance/gearlance bfjoust interpreter; shared parser parts.
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

#ifndef CRANKLANCE_PARSER_H
#define CRANKLANCE_PARSER_H 1

#include <stddef.h>
#include <setjmp.h>

#define MAXCYCLES 100000
#define MAXNEST 4096

#ifndef PARSER_EXTRAFIELDS
#define PARSER_EXTRAFIELDS /* none */
#endif

/* types */

enum optype
{
	OP_INC, OP_DEC,
	OP_LEFT, OP_RIGHT,
	OP_WAIT,
	OP_LOOP1, OP_LOOP2,
	OP_REP1, OP_REP2,
	OP_IREP1, OP_INNER1, OP_INNER2, OP_IREP2,
	OP_DONE, /* appended at end of program */
	OP_MAX
};

struct op
{
	enum optype type;
	int match; /* offset of matching delimiter for [] () ({ }) pairs */
	int inner; /* extra links between matched {} inside () */
	int count; /* static repetition count for the ({}) instructions */
	PARSER_EXTRAFIELDS
};

struct oplist
{
	int len, size;
	struct op *ops;
};

/* oplist handling */

struct oplist *opl_new(void);
void opl_free(struct oplist *o);
void opl_append(struct oplist *o, enum optype type);

/* parsing and preprocessing */

void fail(const char *fmt, ...);

extern char fail_msg[256];
extern jmp_buf fail_buf;

struct oplist *parse(int fd);

#endif /* !CRANKLANCE_PARSER_H */
