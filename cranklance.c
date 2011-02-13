/* cranklance bfjoust interpreter; based on chainlance */

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

#define MINTAPE 10
#define MAXTAPE 30

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
	union {
		enum optype type;
		void *code;
	};
	int match; /* offset of matching delimiter for [({ })] pairs */
	int rep;   /* immediate surrounding rep */
	int count;
	/* count is a multi-use field, with the following meanings:
	   rep instructions ({}) -- counter value for the enclosing ()*N block
	   any instruction       -- -1 if part of the {..} block of immediately enclosing rep
	   loop instructions     -- +1 if crossing a {..} block in a (..{..}..)*N construct
	   if none of the above match -- 0 */
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

/* actual interpretation */

static int run(struct oplist *opsA, struct oplist *opsB);

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

	/* run them */

	int score = run(opsA, opsB);
	printf("score: %d\n", score);
	return score;

	opl_free(opsA);
	opl_free(opsB);

	return 0;
}

/* actual interpretation, impl */

static unsigned char tape[MAXTAPE];

static int run(struct oplist *opsA, struct oplist *opsB)
{
	struct op *oplA = opsA->ops, *oplB = opsB->ops;
	int score = 0;

	/* convert opcode types into pointers for both progs */

	for (int at = 0; at < opsA->len; at++)
	{
		struct op *op = &oplA[at];
		switch (op->type)
		{
		case OP_INC:    op->code = &&op_incA;   break;
		case OP_DEC:    op->code = &&op_decA;   break;
		case OP_LEFT:   op->code = &&op_leftA;  break;
		case OP_RIGHT:  op->code = &&op_rightA; break;
		case OP_WAIT:   op->code = &&op_waitA;  break;
		case OP_LOOP1:  op->code = &&op_loop1A; break;
		case OP_LOOP2:  op->code = &&op_loop2A; break;
		case OP_REP1:   op->code = &&op_rep1A;  break;
		case OP_REP2:   op->code = &&op_rep2A;  break;
		case OP_INNER1: op->code = &&op_rep2A;  break; /* compatible code */
		case OP_INNER2: op->code = &&op_rep1A;  break; /* compatible code */
		}
	}

	opl_append(opsA, OP_INC);
	oplA[opsA->len-1].code = &&op_doneA;

	for (int at = 0; at < opsB->len; at++)
	{
		struct op *op = &oplB[at];
		switch (op->type)
		{
		case OP_INC:    op->code = &&op_incB;   break;
		case OP_DEC:    op->code = &&op_decB;   break;
		case OP_LEFT:   op->code = &&op_leftB;  break;
		case OP_RIGHT:  op->code = &&op_rightB; break;
		case OP_WAIT:   op->code = &&op_waitB;  break;
		case OP_LOOP1:  op->code = &&op_loop1B; break;
		case OP_LOOP2:  op->code = &&op_loop2B; break;
		case OP_REP1:   op->code = &&op_rep1B;  break;
		case OP_REP2:   op->code = &&op_rep2B;  break;
		case OP_INNER1: op->code = &&op_rep2B;  break; /* compatible code */
		case OP_INNER2: op->code = &&op_rep1B;  break; /* compatible code */
		}
	}

	opl_append(opsB, OP_INC);
	oplB[opsB->len-1].code = &&nextcycle; /* a slight shortcut */

	/* state-holding variables */

	static int repStackA[MAXNEST], repStackB[MAXNEST];

	int ipA = 0, ipB = 0;
	unsigned char *ptrA = 0, *ptrB = 0;
	int repA = 0, repB = 0, *repSA = 0, *repSB = 0;
	int deathsA = 0, deathsB = 0;
	int cycles = 0;

	/* execute with all tape lenghts and both relative polarities */

	void *ret;
	int tapesize = 0;

	ret = &&done_normal;
	for (tapesize = MINTAPE; tapesize <= MAXTAPE; tapesize++)
	{
		ipA = 0, ipB = 0;

		memset(tape, 0, tapesize);
		ptrA = &tape[0], ptrB = &tape[tapesize-1];
		*ptrA = 128, *ptrB = 128;

		repSA = repStackA, repSB = repStackB;
		deathsA = 0, deathsB = 0;

		cycles = MAXCYCLES;
		goto *oplA[0].code;
	done_normal:
		(void)0;
	}

	for (int at = 0; at < opsB->len; at++)
	{
		struct op *op = &oplB[at];
		if (op->code == &&op_incB) op->code = &&op_decB;
		else if (op->code == &&op_decB) op->code = &&op_incB;
	}

	ret = &&done_flipped;
	for (tapesize = MINTAPE; tapesize <= MAXTAPE; tapesize++)
	{
		ipA = 0, ipB = 0;

		memset(tape, 0, tapesize);
		ptrA = &tape[0], ptrB = &tape[tapesize-1];
		*ptrA = 128, *ptrB = 128;

		repSA = repStackA, repSB = repStackB;
		deathsA = 0, deathsB = 0;

		cycles = MAXCYCLES;
		goto *oplA[0].code;
	done_flipped:
		(void)0;
	}

	return score;

	/* actual core */

#define NEXTA ipA++; goto *oplB[ipB].code
#define NEXTB ipB++; goto nextcycle

/* #define TRACE 1 */

nextcycle:
	if (!tape[0]) deathsA++;
	if (!tape[tapesize-1]) deathsB++;

#ifdef TRACE
	printf("%6d: ", MAXCYCLES-cycles);
	for (int i = 0; i < tapesize; i++)
		printf("%c%02x ",
		       (ptrA - tape) == i
		       ? ((ptrB - tape) == i ? 'X' : 'A')
		       : ((ptrB - tape) == i ? 'B' : ' '),
		       tape[i]);
	printf("  dA %d  dB %d   ipA %d ipB %d\n", deathsA, deathsB, ipA, ipB);
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

	if (!--cycles)
		goto *ret;

	goto *oplA[ipA].code;

fallA:
	deathsA = 2;
	goto *oplB[ipB].code;

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
	if (!*ptrA)
	{
		if (oplA[ipA].count)
			repA = oplA[oplA[ipA].rep].count - (repA - 1);
		ipA = oplA[ipA].match;
	}
	NEXTA;
op_loop1B:
	if (!*ptrB)
	{
		if (oplB[ipB].count)
			repB = oplB[oplB[ipB].rep].count - (repB - 1);
		ipB = oplB[ipB].match;
	}
	NEXTB;
op_loop2A:
	if (*ptrA)
	{
		if (oplA[ipA].count)
			repA = oplA[oplA[ipA].rep].count - (repA - 1);
		ipA = oplA[ipA].match;
	}
	NEXTA;
op_loop2B:
	if (*ptrB)
	{
		if (oplB[ipB].count)
			repB = oplB[oplB[ipB].rep].count - (repB - 1);
		ipB = oplB[ipB].match;
	}
	NEXTB;

	/* these handle OP_INNER1/OP_INNER2 too with suitable .match settings */
	/* TODO: fix repcounts in the (a[b{c}d]e)*N case when [ jumps over... */

op_rep1A:
	*repSA++ = repA; repA = oplA[ipA].count;
	goto *oplA[++ipA].code;
op_rep1B:
	*repSB++ = repB; repB = oplB[ipB].count;
	goto *oplB[++ipB].code;

op_rep2A:
	if (--repA) ipA = oplA[ipA].match;
	else repA = *--repSA;
	goto *oplA[++ipA].code;
op_rep2B:
	if (--repB) ipB = oplB[ipB].match;
	else repB = *--repSB;
	goto *oplB[++ipB].code;

op_doneA:
	goto *oplB[ipB].code;
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
			o->count = ops->ops[o->rep].count;
			ops->ops[ops->ops[o->rep].match].match = at; /* match enclosing ) with } for looping */
			ops->ops[stack[depth]].match = o->rep;       /* match starting { with initial ( for looping */
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

	int stack[MAXNEST], istack[MAXNEST];
	int depth = 0;

	for (int at = 0; at < ops->len; at++)
	{
		struct op *o = &ops->ops[at];

		switch (o->type)
		{
		case OP_LOOP1:
			if (depth == MAXNEST) fail("maximum [] nesting depth exceeded");
			stack[depth] = at;
			istack[depth] = o->rep; /* used for detecting {..}-spanning loops */
			depth++;
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
			if (istack[depth] != o->rep)
			{
				/* flag as inner-block crossing loop */
				o->count = 1;
				ops->ops[stack[depth]].count = 1;
			}
			break;

		case OP_INNER1:
			if (depth > 0 && istack[depth-1] == o->rep)
				istack[depth-1] = -2; /* doesn't match any rep */
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
