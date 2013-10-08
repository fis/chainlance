/*
 * cranklance/gearlance bfjoust interpreter; shared parser parts.
 *
 * Copyright 2011 Heikki Kallasjoki. All rights reserved.
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

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "parser.h"

char fail_msg[256];
jmp_buf fail_buf;

/* forced repetition count for empty loops.
   (0 and 1 are sensible values.) */
#define EMPTY_LOOP_COUNT 0

/* parsing and preprocessing, impl */

struct oplist *readops(int fd)
{
	/* lexical tokenizing into initial oplist */

	/* helper functions to do buffered reading with ungetch support */

	static unsigned char buf[65536];
	static unsigned buf_at = 0, buf_size = 0;

	int nextc(void)
	{
		if (buf_at >= buf_size)
		{
			ssize_t t = read(fd, buf, sizeof buf);
			if (t < 0) die("read error");
			if (t == 0) return -1;

			buf_at = 0;
			buf_size = t;
		}

#ifdef PARSE_NEWLINE_AS_EOF
		if (buf[buf_at] == '\n') return buf_at++, -1;
#endif
		return buf[buf_at++];
	}

	int nextcmd(void)
	{
		while (1)
		{
			int c = nextc();
			switch (c)
			{
			case -1:
			case '+': case '-': case '<': case '>': case '.': case ',': case '[': case ']':
			case '(': case ')': case '{': case '}': case '*': case '%':
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				return c;

			default:
				/* ignore this character */
				break;
			}
		}
	}

	void unc(int c)
	{
#ifdef PARSE_NEWLINE_AS_EOF
		if (c == -1) c = '\n';
#endif
		buf[--buf_at] = c;
	}

	/* helper function to read the count after a repetition operation */

	int readrepc(void)
	{
		int c = 0, neg = 0, ch;

		ch = nextcmd();
		if (ch != '*' && ch != '%')
		{
			/* treat garbage as ()*0 in case it's inside a comment */
			unc(ch);
			return 0;
		}

		ch = nextcmd();
		if (ch == '-')
		{
			neg = 1;
			ch = nextc();
		}

		while (1)
		{
			if (ch < '0' || ch > '9')
				break;

			c = c*10 + (ch - '0');
			if (c > MAXCYCLES)
			{
				c = MAXCYCLES;
				ch = 0;
				break;
			}

			ch = nextc();
		}

		unc(ch);

		return neg ? -c : c;
	}

	/* main code to read the list of ops */

	struct oplist *ops = opl_new();
	int ch;

	while ((ch = nextcmd()) >= 0)
	{
		int c;

		switch (ch)
		{
		case '+': opl_append(ops, OP_INC);    break;
		case '-': opl_append(ops, OP_DEC);    break;
		case '<': opl_append(ops, OP_LEFT);   break;
		case '>': opl_append(ops, OP_RIGHT);  break;
		case '.': opl_append(ops, OP_WAIT);   break;
		case '[': opl_append(ops, OP_LOOP1);  break;
		case ']': opl_append(ops, OP_LOOP2);  break;
		case '(': opl_append(ops, OP_REP1);   break;
		case ')':
			/* need to extract the count */
			c = readrepc();
			if (c < 0) c = MAXCYCLES;
			opl_append(ops, OP_REP2);
			ops->ops[ops->len-1].count = c;
			break;
		case '{': opl_append(ops, OP_INNER1); break;
		case '}': opl_append(ops, OP_INNER2); break;
		default:
			/* ignore unexpected commands */
			break;
		}
	}

	return ops;
}

void matchrep(struct oplist *ops)
{
	/* match (..) pairs and inner {..} blocks */

	int stack[MAXNEST], istack[MAXNEST], idstack[MAXNEST];
	int depth = 0, idepth = 0, isdepth = 0;

	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &ops->ops[at];

		switch (op->type) /* in order of occurrence */
		{
		case OP_REP1:
			if (depth == MAXNEST) fail("maximum () nesting depth exceeded");
			stack[depth] = at;
			idstack[depth] = idepth;
			op->match = -1;
			op->inner = -1;
			depth++;
			idepth = 0;
			break;

		case OP_INNER1:
			istack[isdepth++] = at;
			idepth++;
			if (idepth > depth) fail("encountered { without suitable enclosing (");
			op->match = stack[depth-idepth];
			op->inner = -1;
			if (ops->ops[op->match].match != -1) fail("encountered second { on a same level");
			ops->ops[op->match].type = OP_IREP1;
			ops->ops[op->match].match = at;
			break;

		case OP_INNER2:
			if (!idepth) fail("terminating } without a matching {");
			idepth--;
			isdepth--;
			op->match = -1;
			op->inner = istack[isdepth];
			ops->ops[op->inner].inner = at;
			break;

		case OP_REP2:
			if (!depth) fail("terminating ) without a matching (");
			if (idepth) fail("starting { without a matching }");
			depth--;
			if (ops->ops[stack[depth]].type == OP_IREP1)
			{
				op->type = OP_IREP2;
				op->inner = stack[depth];
				op->match = ops->ops[ops->ops[op->inner].match].inner;
				ops->ops[op->inner].inner = at;
				ops->ops[op->inner].count = op->count;
				ops->ops[ops->ops[op->inner].match].count = op->count;
			}
			else
			{
				op->inner = -1;
				op->match = stack[depth];
			}
			ops->ops[op->match].match = at;
			ops->ops[op->match].count = op->count;
			idepth = idstack[depth];
			break;

		default:
			/* do nothing */
			break;
		}
	}

	if (depth != 0)
		fail("starting ( without a matching )");
}

void cleanrep(struct oplist *ops)
{
	/* turn contentless loops into *0's.
	   transform ({a}b)%N to ()*0a(b)*N.
	   transform (a{b})%N to (a)*Nb()*0. */

	int last_real = -1;

	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &ops->ops[at];
		switch (op->type)
		{
		case OP_INC: case OP_DEC: case OP_LEFT: case OP_RIGHT:
		case OP_LOOP1: case OP_LOOP2: case OP_WAIT:
			last_real = at;
			break;
		case OP_REP1: case OP_IREP1: case OP_INNER2:
			/* no action */
			break;
		case OP_REP2:
			if (last_real < op->match)
			{
				/* empty () loop */
				op->count = 0;
				ops->ops[op->match].count = 0;
			}
			break;
		case OP_INNER1:
			if (last_real < op->match)
			{
				/* empty ({ part */
				int rep1 = op->match, inner1 = at, inner2 = op->inner, rep2 = ops->ops[op->match].inner;
				ops->ops[rep1].type = OP_REP1;
				ops->ops[rep1].count = EMPTY_LOOP_COUNT;
				ops->ops[rep1].inner = -1;
				ops->ops[inner1].type = OP_REP2;
				ops->ops[inner1].count = EMPTY_LOOP_COUNT;
				ops->ops[inner1].inner = -1;
				ops->ops[inner2].type = OP_REP1;
				ops->ops[inner2].inner = -1;
				ops->ops[rep2].type = OP_REP2;
				ops->ops[rep2].inner = -1;
			}
			break;
		case OP_IREP2:
			if (last_real < op->match)
			{
				/* empty }) part */
				int rep1 = op->inner, inner1 = ops->ops[op->match].inner, inner2 = op->match, rep2 = at;
				ops->ops[rep1].type = OP_REP1;
				ops->ops[rep1].inner = -1;
				ops->ops[inner1].type = OP_REP2;
				ops->ops[inner1].inner = -1;
				ops->ops[inner2].type = OP_REP1;
				ops->ops[inner2].count = EMPTY_LOOP_COUNT;
				ops->ops[inner2].inner = -1;
				ops->ops[rep2].type = OP_REP2;
				ops->ops[rep2].count = EMPTY_LOOP_COUNT;
				ops->ops[rep2].inner = -1;
			}
			break;
		default:
			fail("impossible: unknown op->type");
		}
	}

	/* clean out (...)*0 loops */

	int orig_len = ops->len;
	for (int at = 0, to = 0; at < orig_len; at++, to++)
	{
		struct op *op = &ops->ops[at];

		if ((op->type == OP_REP1 || op->type == OP_IREP1 || op->type == OP_INNER2) && op->count == 0)
		{
			int del_to = op->match;      /* delete this far */
			ops->len -= del_to - at + 1; /* fixup length */
			at = del_to;                 /* skip the loop */
			to--;                        /* don't copy anything */
		}
		else if (to < at)
		{
			if (op->match != -1)
				ops->ops[op->match].match = to;
			if (op->inner != -1)
				ops->ops[op->inner].inner = to;
			ops->ops[to] = *op;
		}
	}
}

void matchloop(struct oplist *ops)
{
	/* match [..] pairs */

	int stack[MAXNEST], idstack[MAXNEST];
	int depth = 0, idepth = 0;

	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &ops->ops[at];

		switch (op->type)
		{
		case OP_LOOP1:
			if (depth == MAXNEST) fail("maximum [] nesting depth exceeded");
			stack[depth] = at;
			idstack[depth] = idepth;
			op->match = -1;
			depth++;
			idepth = 0;
			break;

		case OP_REP1:
		case OP_IREP1:
		case OP_INNER1:
			idepth++;
			break;

		case OP_INNER2:
		case OP_IREP2:
		case OP_REP2:
			if (!idepth) fail("[..] crossing out of a ({..}) level");
			idepth--;
			break;

		case OP_LOOP2:
			if (!depth) fail("terminating ] without a matching [");
			if (idepth) fail("[..] crossing into a ({..}) level");
			depth--;
			op->match = stack[depth];
			ops->ops[op->match].match = at;
			idepth = idstack[depth];
			break;

		default:
			/* do nothing */
			break;
		}
	}

	if (depth != 0)
		fail("starting [ without a matching ]");
}

struct oplist *parse(int fd)
{
	struct oplist *ops = readops(fd);

	/* handle (...) constructions first */

	matchrep(ops);
	cleanrep(ops);

	/* handle [...] constructions now that rep/inner levels are known */

	matchloop(ops);

	return ops;
}

void fail(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(fail_msg, sizeof fail_msg, fmt, ap);
	va_end(ap);

	longjmp(fail_buf, 1);
}

/* oplist handling, impl */

struct oplist *opl_new(void)
{
	struct oplist *o = smalloc(sizeof *o);

	o->len = 0;
	o->size = 32;
	o->ops = smalloc(o->size * sizeof *o->ops);

	return o;
}

void opl_free(struct oplist *o)
{
	free(o->ops);
	free(o);
}

void opl_append(struct oplist *o, enum optype type)
{
	if (o->len == o->size)
	{
		o->size += o->size >> 1;
		o->ops = srealloc(o->ops, o->size * sizeof *o->ops);
	}

	o->ops[o->len].type = type;
	o->ops[o->len].match = -1;
	o->ops[o->len].inner = -1;
	o->ops[o->len].count = -1;
	o->len++;
}
