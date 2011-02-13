/* chainlance bfjoust "compiler", imperative reimplementation */

#include <fcntl.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXCYCLES 100000
#define MAXNEST 256

/* types */

enum optype
{
	OP_INC, OP_DEC,
	OP_LEFT, OP_RIGHT,
	OP_WAIT,
	OP_LOOP1, OP_LOOP2,
	OP_REP1, OP_REP2,
	OP_INNER1, OP_INNER2
};

struct op
{
	enum optype type;
	int match; /* offset of matching delimiter for [({ })] pairs */
	int rep;   /* immediate surrounding rep */
	int count; /* rep counter for () instructions; for others, -1 == part of inner block of rep, 0 otherwise */
};

struct oplist
{
	int len, size;
	struct op *ops;
};

/* generic helpers */

static void die(const char *fmt, ...);
static void fail(const char *fmt, ...);

static char fail_msg[256];
static jmp_buf fail_buf;

static void *smalloc(size_t size);
static void *srealloc(void *ptr, size_t size);

static int sopen(const char *fname);

/* oplist handling */

static struct oplist *opl_new(void);
static void opl_free(struct oplist *o);
static void opl_append(struct oplist *o, enum optype type);
static void opl_del(struct oplist *o, int start, int end);

/* parsing and preprocessing */

static struct oplist *parse(int fd);

/* actual compilation */

static void compile(FILE *f, struct oplist *ops, char side, const char *prefix, int flip);

/* main application */

int main(int argc, char *argv[])
{
	/* check args */

	if (argc != 4)
	{
		fprintf(stderr, "usage: %s header.asm prog1.bfjoust prog2.bfjoust\n", argv[0]);
		return 1;
	}

	/* parse competitors */

	int fdA = sopen(argv[2]), fdB = sopen(argv[3]);

	if (setjmp(fail_buf))
	{
		printf("parse error: %s\n", fail_msg);
		return 1;
	}

	/* copy header */

	FILE *hdr = fopen(argv[1], "r");
	if (!hdr)
		die("open failed: %s", argv[1]);

	static unsigned char buf[4096];
	size_t got;

	while ((got = fread(buf, 1, sizeof buf, hdr)) > 0)
		fwrite(buf, 1, got, stdout);

	if (got < 0)
		die("read error");

	fclose(hdr);

	/* translate into code */

	struct oplist *opsA = parse(fdA), *opsB = parse(fdB);

	compile(stdout, opsA, 'A', "pa", 0);
	compile(stdout, opsB, 'B', "pb", 0);
	compile(stdout, opsB, 'B', "pf", 1);

	opl_free(opsA);
	opl_free(opsB);
	return 0;
}

/* actual compilation, impl */

static void compile(FILE *f, struct oplist *ops, char side, const char *prefix, int flip)
{
	fprintf(f, "Prog%c%s:\n", side, flip ? "2" : "");

	for (int at = 0; at < ops->len; at++)
	{
		struct op *op = &ops->ops[at];
		int add_switch = 1;

		switch (op->type)
		{
		case OP_INC:
		case OP_DEC:
			fprintf(f, "\t%s byte [rTapeBase + rTape%c]\n",
			        ((op->type == OP_INC && !flip) || (op->type == OP_DEC && flip)) ? "inc" : "dec",
			        side);
			break;

		case OP_LEFT:
		case OP_RIGHT:
			if ((op->type == OP_LEFT && side == 'A') || (op->type == OP_RIGHT && side == 'B'))
			{
				fprintf(f, "\tdec rTape%cb\n", side);
				fprintf(f, "\tjs fall%c\n", side);
			}
			else
			{
				fprintf(f, "\tinc rTape%cb\n", side);
				fprintf(f, "\tcmp rTape%cb, rTapeSizeb\n", side);
				fprintf(f, "\tjnc fall%c\n", side);
			}
			break;

		case OP_WAIT:
			break; /* do nothing */

		case OP_LOOP1:
			if (side == 'A')
				fprintf(f, "\tcmp [rTapeBase + rTapeA], 0\n");
			else
				fprintf(f, "\ttest rCell2, rCell2\n");
			fprintf(f, "\tjz %s%d\n", prefix, op->match);
			fprintf(f, "%s%d:\n", prefix, at);
			break;

		case OP_LOOP2:
			if (side == 'A')
				fprintf(f, "\tcmp [rTapeBase + rTapeA], 0\n");
			else
				fprintf(f, "\ttest rCell2, rCell2\n");
			fprintf(f, "\tjnz %s%d\n", prefix, op->match);
			fprintf(f, "%s%d:\n", prefix, at);
			break;

		case OP_REP1:
			if (op->rep != -1)
				fprintf(f, "\tmov [rRepS%c], rRep%cd\n\tlea rRepS%c, [rRepS%c+4]\n",
				        side, side, side, side);
			fprintf(f, "\tmov rRep%cd, %d\n", side, op->count < 0 ? -op->count : op->count);
			fprintf(f, "%s%da:\n", prefix, at);
			add_switch = 0;
			break;

		case OP_REP2:
			fprintf(f, "\tdec rRep%cd\n", side);
			fprintf(f, "\tjnz %s%d%c\n", prefix, op->match,
			        op->count < 0 ? 'b' : 'a');
			if (op->rep != -1)
				fprintf(f, "\tlea rRepS%c, [rRepS%c-4]\n\tmov rRep%cd, [rRepS%c]\n",
				        side, side, side, side);
			add_switch = 0;
			break;

		case OP_INNER1:
			fprintf(f, "\tdec rRep%cd\n", side);
			fprintf(f, "\tjnz %s%da\n", prefix, op->rep);
			add_switch = 0;
			break;

		case OP_INNER2:
			fprintf(f, "\tmov rRep%cd, %d\n", side, op->count < 0 ? -op->count : op->count);
			fprintf(f, "%s%db:\n", prefix, op->rep);
			add_switch = 0;
			break;
		}

		if (add_switch || at == ops->len - 1)
		{
			if (at == ops->len - 1)
				fprintf(f, "%s_end:\n", prefix);
			fprintf(f, "\tCTX_Prog%c\n", side);
			if (at == ops->len - 1)
				fprintf(f, "\tjmp %s_end\n", prefix);
		}
	}
}

/* parsing and preprocessing, impl */

static int readrepc(unsigned char *buf, ssize_t bsize, int *bat, int fd)
{
	/* read and parse a () count */

	int at = *bat;

	int nextc(void)
	{
		if (at < bsize)
			return buf[at++];

		unsigned char c;
		ssize_t t = read(fd, &c, 1);
		if (t < 0) die("read error");
		if (t == 0) return -1;
		return c;
	}

	int c = 0, ch;

	ch = nextc();
	if (ch != '*' && ch != '%') fail("bad char after (): %c", ch);

	while (1)
	{
		ch = nextc();
		if (ch < '0' || ch > '9')
			break;

		c = c*10 + (ch - '0');
		if (c > MAXCYCLES)
		{
			c = MAXCYCLES;
			ch = 0;
			break;
		}
	}

	buf[--at] = ch;
	*bat = at-1;

	return c;
}

static struct oplist *readops(int fd)
{
	/* lexical tokenizing into initial oplist */

	static unsigned char buf[65536];
	struct oplist *ops = opl_new();

	while (1)
	{
		ssize_t got = read(fd, buf, sizeof buf);
		if (got < 0) die("read error");
		if (got == 0) break;

		for (int i = 0; i < got; i++)
		{
			int c;

			switch (buf[i])
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
				i++;
				c = readrepc(buf, got, &i, fd);
				opl_append(ops, OP_REP2);
				ops->ops[ops->len-1].count = c;
				break;
			case '{': opl_append(ops, OP_INNER1); break;
			case '}': opl_append(ops, OP_INNER2); break;
			}
		}
	}

	return ops;
}

static void matchrep(struct oplist *ops)
{
	/* match (..) pairs */

	int stack[MAXNEST];
	int depth = 0;

	for (int at = 0; at < ops->len; at++)
	{
		struct op *o = &ops->ops[at];

		o->rep = (depth > 0 ? stack[depth-1] : -1);

		switch (o->type)
		{
		case OP_REP1:
			if (depth == MAXNEST) fail("maximum () nesting depth exceeded");
			stack[depth++] = at;
			break;

		case OP_REP2:
			if (depth == 0) fail("terminating ) without a matching (");
			depth--;
			ops->ops[at].rep = (depth > 0 ? stack[depth-1] : -1);
			ops->ops[at].match = stack[depth];
			ops->ops[stack[depth]].match = at;
			ops->ops[stack[depth]].count = ops->ops[at].count;
			break;

		default:
			/* do nothing */
			break;
		}
	}

	if (depth != 0)
		fail("starting ( without a matching )");
}

static void cleanrep(struct oplist *ops)
{
	/* clean out (...)*0 and ()*N */

	for (int at = 0; at < ops->len; at++)
	{
		struct op *o = &ops->ops[at];
		if (o->type == OP_REP1 && o->count == 0)
		{
			opl_del(ops, at, o->match+1);
			at--;
		}
	}

	/* TODO: clean ()*N with a multipass thing */
}

static void checkrep(struct oplist *ops)
{
	/* make sure each (...) block has just 0 or 1 top-level {...}s */

	int stack[MAXNEST];
	int depth = -1;

	for (int at = 0; at < ops->len; at++)
	{
		struct op *o = &ops->ops[at];

		switch (o->type)
		{
		case OP_REP1:
			depth++;
			stack[depth] = -1;
			break;

		case OP_REP2:
			if (stack[depth] >= 0) fail("starting { without a terminating } on single (...) level");
			if (stack[depth] == -2) o->count = -o->count;
			depth--;
			break;

		case OP_INNER1:
			if (depth < 0) fail("starting { without a surrounding ()");
			if (stack[depth] != -1) fail("multiple starting {s on a single (...) level");
			ops->ops[o->rep].count = -ops->ops[o->rep].count;
			o->count = ops->ops[o->rep].count;
			stack[depth] = at;
			break;

		case OP_INNER2:
			if (depth < 0) fail("terminating { without a surrounding ()");
			if (stack[depth] < 0) fail("terminating } without a matching {");
			ops->ops[at].match = stack[depth];
			ops->ops[stack[depth]].match = at;
			o->count = ops->ops[o->rep].count;
			stack[depth] = -2;
			break;

		default:
			if (depth >= 0 && stack[depth] >= 0)
				o->count = -1;
			else
				o->count = 0;
			break;
		}
	}
}

static void matchloop(struct oplist *ops)
{
	/* match [..] pairs */

	int stack[MAXNEST];
	int depth = 0;

	for (int at = 0; at < ops->len; at++)
	{
		struct op *o = &ops->ops[at];

		switch (o->type)
		{
		case OP_LOOP1:
			if (depth == MAXNEST) fail("maximum [] nesting depth exceeded");
			stack[depth++] = at;
			break;

		case OP_LOOP2:
			if (depth == 0) fail("terminating ] without a matching [");
			depth--;
			if (ops->ops[at].rep != ops->ops[stack[depth]].rep)
				fail("matching [...] across (...) levels");
			if (ops->ops[at].count != ops->ops[stack[depth]].count)
				fail("matching [...] across (..{..}..) inner-body boundary");
			ops->ops[at].match = stack[depth];
			ops->ops[stack[depth]].match = at;
			break;

		default:
			/* do nothing */
			break;
		}
	}

	if (depth != 0)
		fail("starting [ without a matching ]");
}

static struct oplist *parse(int fd)
{
	struct oplist *ops = readops(fd);

	/* handle (...) constructions first */

	matchrep(ops);
	cleanrep(ops);
	checkrep(ops);

	/* handle [...] constructions now that rep/inner levels are known */

	matchloop(ops);

	return ops;
}

/* oplist handling, impl */

static struct oplist *opl_new(void)
{
	struct oplist *o = smalloc(sizeof *o);

	o->len = 0;
	o->size = 32;
	o->ops = smalloc(o->size * sizeof *o->ops);

	return o;
}

static void opl_free(struct oplist *o)
{
	free(o->ops);
	free(o);
}

static void opl_append(struct oplist *o, enum optype type)
{
	if (o->len == o->size)
	{
		o->size += o->size >> 1;
		o->ops = srealloc(o->ops, o->size * sizeof *o->ops);
	}

	o->ops[o->len++].type = type;
}

static void opl_del(struct oplist *o, int start, int end)
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
		if (op->rep >= start && op->rep < end) die("opl_del: dangling ->rep");
		if (op->match >= end) op->match -= d;
		if (op->rep >= end) op->rep -= d;
	}
}

/* generic helpers, impl */

static void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);

	exit(1);
}

static void fail(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(fail_msg, sizeof fail_msg, fmt, ap);
	va_end(ap);

	longjmp(fail_buf, 1);
}

static void *smalloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr) die("out of memory");
	return ptr;
}

static void *srealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (!ptr) die("out of memory");
	return ptr;
}

static int sopen(const char *fname)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1)
		die("open failed: %s", fname);
	return fd;
}
