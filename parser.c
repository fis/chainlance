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

#include "parser.h"

char fail_msg[256];
jmp_buf fail_buf;

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
			op->match = -1;
			op->inner = stack[depth-idepth];
			if (ops->ops[op->inner].inner != -1) fail("encountered second { on a same level");
			ops->ops[op->inner].inner = at;
			break;

		case OP_INNER2:
			if (!idepth) fail("terminating } without a matching {");
			idepth--;
			isdepth--;
			op->match = istack[isdepth];
			op->inner = -1;
			ops->ops[op->match].match = at;
			break;

		case OP_REP2:
			if (!depth) fail("terminating ) without a matching (");
			if (idepth) fail("starting { without a matching }");
			depth--;
			op->match = stack[depth];
			op->inner = (ops->ops[op->match].inner != -1 ? ops->ops[ops->ops[op->match].inner].match : -1);
			ops->ops[op->match].match = at;
			ops->ops[op->match].count = op->count;
			if (op->inner != -1)
			{
				ops->ops[op->inner].inner = at;
				ops->ops[op->inner].count = op->count;
				ops->ops[ops->ops[op->inner].match].count = op->count;
			}
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
	/* clean out (...)*0 and ()*N */

	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &ops->ops[at];
		if (op->type == OP_REP1 && op->count == 0)
		{
			opl_del(ops, at, op->match+1);
			at--;
		}
	}

	/* TODO: clean ()*N with a multipass thing */
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
		case OP_INNER1:
			idepth++;
			break;

		case OP_INNER2:
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

void opl_del(struct oplist *o, int start, int end)
{
	int d = end - start;
	if (!d)
		return;

	if (end == o->len)
	{
		o->len = start;
		return;
	}

	memmove(&o->ops[start], &o->ops[end], (o->len - end) * sizeof *o->ops);
	o->len -= d;

	for (int at = 0; at < o->len; at++)
	{
		struct op *op = &o->ops[at];
		if (op->match >= start && op->match < end) die("opl_del: dangling ->match");
		if (op->inner >= start && op->inner < end) die("opl_del: dangling ->inner");
		if (op->match >= end) op->match -= d;
		if (op->inner >= end) op->inner -= d;
	}
}

/* generic helpers, impl */

void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);

	exit(1);
}

void fail(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(fail_msg, sizeof fail_msg, fmt, ap);
	va_end(ap);

	longjmp(fail_buf, 1);
}

void *smalloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr) die("out of memory");
	return ptr;
}

void *srealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (!ptr) die("out of memory");
	return ptr;
}

int sopen(const char *fname)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1)
		die("open failed: %s", fname);
	return fd;
}
