/*
 * gearlanced bfjoust interpreter; based on gearlance, itself based on
 * cranklance, itself based on chainlance.
 *
 * This one is for maintaining a hill of bfjoust programs.  You invoke
 * it as "./gearlanced N", where N is a positive integer denoting the
 * hill size.  The hill will be initialized with N copies of a
 * hypothetical program that will always lose.  After initialization,
 * the program will read commands from stdin, and reply results to
 * stdout.  The protocol is as follows.
 *
 * "test\nP\n":
 * Test program P against those currently on the hill.  Will return
 * either "error: ...\n" or "ok\n" followed by N lines of output.
 * Each line has the form
 *   "CCCCCCCCCCCCCCCCCCCCC CCCCCCCCCCCCCCCCCCCCC\n",
 * where each C corresponds to a particular tape length/polarity
 * combination, and is either '<' (the program on the hill wins),
 * '>' (program P wins) or 'X' (tie).
 *
 * "set I\nP\n":
 * Set the I'th program (where 0 <= I < N) to P.  Returns either
 * "ok\n" or "error: ...\n".
 *
 * "unset I\n":
 * Remove the I'th program (where 0 <= I < N), by setting it to the
 * always-lose one.  This is used to "blank" the program being
 * replaced on the hill, for efficiency reasons.  Returns "ok\n" or
 * "error: ...\n" (only if I is not a number or out of bounds).
 *
 * An EOF condition in place of a command will terminate the program.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_MAIN 1
#define PARSE_STDIN 1
#include "gearlance.c"

static unsigned num(char *str)
{
	char *p;

	errno = 0;
	long l = strtol(str, &p, 10);

	if (errno == 0 && !*p && l >= 0 && l <= INT_MAX)
		return l;

	fprintf(stderr, "unreasonable number: %s\n", str);
	exit(1);
}

static struct opcodes *readprog(enum core_action act)
{
	if (setjmp(fail_buf))
	{
		printf("error: parse error: %s\n", fail_msg);
		return 0;
	}

	struct oplist *ops = parse(0);
	struct opcodes *code = core(act, ops, 0, 0);
	opl_free(ops);

	return code;
}

int main(int argc, char *argv[])
{
	/* grok arguments */

	if (argc != 2)
	{
		fprintf(stderr, "usage: %s N\n", argv[0]);
		return 1;
	}

	unsigned hillsize = num(argv[1]);
	if (hillsize == 0)
	{
		fprintf(stderr, "cowardly refusing to make an empty hill\n");
		return 1;
	}

	/* make sure we're line-buffered for the line-oriented interface */

	setvbuf(stdin, 0, _IOLBF, 4096);
	setvbuf(stdout, 0, _IOLBF, 4096);

	/* construct initial hill */

	struct opcodes **hill = smalloc(hillsize * sizeof *hill);
	for (unsigned i = 0; i < hillsize; i++)
		hill[i] = 0;

	/* main loop */

	char cmd[128]; /* enough for any "unset I" command */

	while (fgets(cmd, sizeof cmd, stdin))
	{
		{
			size_t nl = strlen(cmd);
			if (nl > 0 && cmd[nl-1] == '\n')
				cmd[nl-1] = 0;
		}

		if (strcmp(cmd, "test") == 0)
		{
			struct opcodes *code = readprog(core_compile_b);
			if (!code)
				continue; /* error message already out */

			printf("ok\n");

			for (unsigned i = 0; i < hillsize; i++)
			{
				if (!hill[i])
				{
					for (unsigned pol = 0; pol < 2; pol++)
						for (unsigned tlen = MINTAPE; tlen <= MAXTAPE; tlen++)
							scores[pol][tlen] = -1;
				}
				else
					core(core_run, 0, hill[i], code);

				for (unsigned pol = 0; pol < 2; pol++)
				{
					for (unsigned tlen = MINTAPE; tlen <= MAXTAPE; tlen++)
						putchar(scores[pol][tlen] ? (scores[pol][tlen] > 0 ? '<' : '>') : 'X');
					putchar(pol == 0 ? ' ' : '\n');
				}
			}
		}
		else if (strncmp(cmd, "set ", 4) == 0)
		{
			unsigned prog = num(cmd + 4);
			if (prog >= hillsize) {
				printf("error: program %u does not exist\n", prog);
				continue;
			}

			struct opcodes *code = readprog(core_compile_a);
			if (!code)
				continue; /* error message already out */

			free(hill[prog]);
			hill[prog] = code;

			printf("ok\n");
		}
		else if (strncmp(cmd, "unset ", 6) == 0)
		{
			unsigned prog = num(cmd + 6);
			if (prog > hillsize) {
				printf("error: program %u does not exist\n", prog);
				continue;
			}

			free(hill[prog]);
			hill[prog] = 0;

			printf("ok\n");
		}
		else
		{
			fprintf(stderr, "unreasonable command: %s\n", cmd);
		}
	}

	return 0;
}
