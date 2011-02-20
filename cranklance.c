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

/* #define TRACE 1 */

static int scores[2][MAXTAPE];

static struct {
	long long cycles;
} stats = {
	.cycles = 0
};

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
	int inner; /* extra links between matched ( .. { and } .. ) */
	int count; /* static repetition count for the ({}) instructions */
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

static void run(struct oplist *opsA, struct oplist *opsB);

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

	/* for debuggin purposes, dump out the parse */

#if 0
	unsigned char opchars[] = {
		[OP_INC] = '+', [OP_DEC] = '-', [OP_LEFT] = '<', [OP_RIGHT] = '>',
		[OP_WAIT] = '.', [OP_LOOP1] = '[', [OP_LOOP2] = ']',
		[OP_REP1] = '(', [OP_REP2] = ')', [OP_INNER1] = '{', [OP_INNER2] = '}'
	};
	for (int at = 0; at < opsA->len; at++)
	{
		struct op *op = &opsA->ops[at];
		printf("%3d:  %c  (%-2d  {%-2d  *%-2d\n", at, opchars[op->type], op->match, op->inner, op->count);
	}
	return 0;
#endif

	/* run them */

	run(opsA, opsB);

	/* summarize results */

	printf("SUMMARY: ");

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

	printf("%d c%lld sl%d sr%d\n", score, stats.cycles, opsA->len, opsB->len);

	opl_free(opsA);
	opl_free(opsB);

	return 0;
}

/* actual interpretation, impl */

static unsigned char tape[MAXTAPE];

static void run(struct oplist *opsA, struct oplist *opsB)
{
	struct op *oplA = opsA->ops, *oplB = opsB->ops;

	/* convert opcode types into pointers for both progs */

	void **opcA = smalloc((opsA->len+1) * sizeof *opcA);
	void **opcB = smalloc((opsB->len+1) * sizeof *opcB);

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

	/* state-holding variables */

	static int repStackA[MAXNEST], repStackB[MAXNEST];

	static unsigned tapestats[2][MAXTAPE];
	static unsigned char tapemax[2][MAXTAPE];

	int ipA = 0, ipB = 0;
	unsigned char *ptrA = 0, *ptrB = 0;
	int repA = 0, repB = 0, *repSA = 0, *repSB = 0;
	int deathsA = 0, deathsB = 0;
	int cycles = 0;
	int score = 0;

	/* execute with all tape lenghts and both relative polarities */

#define EXECUTE_ALL(sym, pol)	  \
	ret = &&sym; \
	for (tapesize = MINTAPE; tapesize <= MAXTAPE; tapesize++) \
	{ \
		ipA = 0, ipB = 0; \
	  \
		memset(tape, 0, tapesize); \
		ptrA = &tape[0], ptrB = &tape[tapesize-1]; \
		*ptrA = 128, *ptrB = 128; \
	  \
		repSA = repStackA, repSB = repStackB; \
		deathsA = 0, deathsB = 0; \
	  \
		cycles = MAXCYCLES; \
		memset(tapestats, 0, sizeof tapestats); \
		memset(tapemax, 0, sizeof tapemax); \
	  \
		score = 0; \
		goto *opcA[0]; \
	sym: \
		scores[pol][tapesize] = score; \
	  \
		stats.cycles += (MAXCYCLES - cycles); \
		printf("CYCLES[%d,%d] = %d\n", pol, tapesize, MAXCYCLES - cycles); \
	  \
		printf("TAPEABS[%d,%d] =", pol, tapesize); \
		for (int p = 0; p < tapesize; p++) printf(" %d", tape[p] >= 128 ? tape[p]-256 : tape[p]); \
		printf("\n"); \
		printf("TAPEMAX[%d,%d] =", pol, tapesize); \
		for (int p = 0; p < tapesize; p++) printf(" %u/%u", tapemax[0][p], tapemax[1][p]); \
		printf("\n"); \
		printf("TAPEHEAT[%d,%d] =", pol, tapesize); \
		for (int p = 0; p < tapesize; p++) printf(" %u/%u", tapestats[0][p], tapestats[1][p]); \
		printf("\n"); \
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

	free(opcA);
	free(opcB);
	return;

	/* actual core */

#define NEXTA ipA++; goto *opcB[ipB]
#define NEXTB ipB++; goto nextcycle

/* #define TRACE 1 */

nextcycle:
	cycles--;

	if (!tape[0]) deathsA++; else if (deathsA == 1) deathsA = 0;
	if (!tape[tapesize-1]) deathsB++; else if (deathsB == 1) deathsB = 0;

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

	tapestats[0][ptrA-tape]++;
	tapestats[1][ptrB-tape]++;

	if (!cycles)
		goto *ret;

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

#define MAXSTAT(ptr,i)	  \
	if (*ptr >= 128) { if (256-*ptr > tapemax[i][ptr-tape]) tapemax[i][ptr-tape] = 256-*ptr; } \
	else { if (*ptr > tapemax[i][ptr-tape]) tapemax[i][ptr-tape] = *ptr; }

op_leftA:
	MAXSTAT(ptrA, 0);
	ptrA--;
	if (ptrA < tape) goto fallA;
	NEXTA;
op_leftB:
	MAXSTAT(ptrB, 1);
	ptrB++;
	if (ptrB >= &tape[tapesize]) goto fallB;
	NEXTB;
op_rightA:
	MAXSTAT(ptrA, 0);
	ptrA++;
	if (ptrA >= &tape[tapesize]) goto fallA;
	NEXTA;
op_rightB:
	MAXSTAT(ptrB, 1);
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
	if (!*ptrB) ipB = oplB[ipB].match;
	NEXTB;
op_loop2A:
	if (*ptrA) ipA = oplA[ipA].match;
	NEXTA;
op_loop2B:
	if (*ptrB) ipB = oplB[ipB].match;
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
	if (ch != '*' && ch != '%')
	{
		/* treat garbage as ()*0 in case it's inside a comment */
		buf[--at] = ch;
		*bat = at-1;
		return 0;
	}

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

static void cleanrep(struct oplist *ops)
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

static void matchloop(struct oplist *ops)
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

static struct oplist *parse(int fd)
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

	o->ops[o->len].type = type;
	o->ops[o->len].match = -1;
	o->ops[o->len].inner = -1;
	o->ops[o->len].count = -1;
	o->len++;
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
		if (op->inner >= start && op->inner < end) die("opl_del: dangling ->inner");
		if (op->match >= end) op->match -= d;
		if (op->inner >= end) op->inner -= d;
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
