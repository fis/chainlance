/*
 * gearlanced bfjoust interpreter; based on gearlance, itself based on
 * cranklance, itself based on chainlance.
 *
 * This one is for maintaining a hill of bfjoust programs.  You invoke
 * it as "./gearlanced N", where N is a positive integer denoting the
 * hill size.  The hill will be initialized with N copies of a
 * hypothetical program that will always lose.  After initialization,
 * the program will read commands from stdin, and reply results to
 * stdout.  This "GearTalk" protocol works as follows.
 *
 * The caller must send an 8-byte command header, potentially (for
 * some commands) followed by bfjoust program code.  The program
 * replies with a four-byte reply header, followed by reply data.
 *
 * The first byte of the command header specifies the action to
 * perform: test (0x01), set (0x02) or unset (0x03).  For set and
 * unset, the following three bytes are a hill position index as an
 * unsigned little-endian 24-bit integer; the test command does not
 * use these bytes and they should be set to 0.  For test and set, the
 * last 4 bytes are the length of the following program code, as an
 * unsigned little-endian 32-bit integer; the unset command does not
 * take a program, so these bytes should be set to 0.
 *
 * The first byte of the reply header contains flags.  Bit 0 (least
 * significant) is the success flag: if unset, an error occurred.  In
 * this case, the other three bytes are the length of the error
 * message, again in unsigned 24-bit little-endian format.  The error
 * message text follows the reply header.
 *
 * If the command succeeded, the bit is set.  Other flag bits and
 * reply data depend on the command.
 *
 * The test command (0x01) runs the program (as the "right" program)
 * against all the programs currently on the hill (as "left"
 * programs).  The other three reply header bytes indicate the
 * parameters of the hill: the minimum (10) and maximum (30) tape
 * lengths and (the low 8 bits of) the hill size, respectively.  Let C
 * be the number of tape configurations (max - min + 1, normally 21)
 * and N be the hill size.
 *
 * The reply header is followed by N chunks of results.  For
 * gearlanced, the reply chunk has 2*C (normally 42) bytes, each
 * containing the signed 8-bit value -1 (for a loss of the tested
 * "right" program), 0 (for a tie) or +1 (for a win).  The first half
 * are for the sieve (normal) polarity runs in order of increasing
 * tape length.  The second half are for kettle (inverted) polarity.
 *
 * If the binary is compiled as cranklanced, bit 1 of the reply flags
 * will be set to indicate this.  In this case, before the normal
 * points there will also be 2*C statistics chunks, in the same order
 * as the points.  Letting T be the tape length of each configuration,
 * the statistics chunks are 4+19*T bytes, with the following fields:
 *
 * - 4 bytes, 32le: number of executed cycles
 * - T bytes: absolute value (0..128) of tape at the end of joust
 * - 2*T bytes: largest value on tape when moving away from cell
 * - 2*4*T bytes: heatmap of how long the program stayed in cell
 * - 2*4*T bytes: heatmap of how often the program modified the cell
 *
 * For all the tape maps that have 2*T elements, the first half are
 * the statistics for the "left" program, and the second half for the
 * "right". In both cases the tape is unflipped, i.e., as seen from
 * the perspective of the left program.
 *
 * The total reply data without statistics is 2*C*N (42*N) bytes, and with statistics ...*N (...*N) bytes.
 *
 * The set (0x01) and unset (0x02) either set or blank the indicated
 * program.  A blanked program loses immediately: this is used for
 * efficiency reasons on the program that's being replaced on the
 * hill.  The reply header contains only the success flag (or an
 * error).
 *
 * An EOF condition in place of a command will terminate the program.
 */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gearlance.h"

/* geartalk utilities */

#define ACTION_TEST(x) (((x) & 0xff) == 0x01)
#define ACTION_SET(x) (((x) & 0xff) == 0x02)
#define ACTION_UNSET(x) (((x) & 0xff) == 0x03)

static inline uint32_t get_u32le(unsigned char *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3]) << 24;
}

static inline void put_u32le(unsigned char *p, uint32_t u) {
	p[0] = u; p[1] = u >> 8; p[2] = u >> 16; p[3] = u >> 24;
}

int read_n(unsigned char *buf, size_t count) {
	size_t left = count;
	while (left) {
		ssize_t ret = read(0, buf, left);
		if (ret < 0) {
			perror("read");
			abort();
		} else if (ret == 0 && left < count) {
			fputs("short read\n", stderr);
			abort();
		} else if (ret == 0) {
			return 0;
		}
		left -= ret;
	}
	return 1;
}

void write_n(const unsigned char *buf, size_t count) {
	while (count) {
		ssize_t wrote = write(1, buf, count);
		if (wrote <= 0) abort();
		count -= wrote;
		buf += wrote;
	}
}

void write_error(char *fmt, ...) {
	unsigned char reply[4 + 256];

	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf((char*)reply + 4, sizeof reply - 4, fmt, ap);
	va_end(ap);

	put_u32le(reply, (uint32_t)len << 8);
	write_n(reply, 4 + len);
}

/* statistics output code for cranklanced */

#ifdef CRANK_IT
#define EXECUTE_STATS(pol) do { \
		geartalk_Statistics s = Statistics_init_default; \
		s.has_cycles = true; \
		s.cycles = MAXCYCLES - cycles; \
		s.has_tape_abs = true; s.tape_abs.size = tapesize; \
		for (int p = 0; p < tapesize; p++) s.tape_abs.bytes[p] = tape[p]; \
		s.has_tape_max = true; s.tape_max.size = 2 * tapesize; \
		for (int p = 0; p < tapesize; p++) { \
			s.tape_max.bytes[p] = xstats.tape_max[0][p]; \
			s.tape_max.bytes[tapesize+p] = xstats.tape_max[1][p]; \
		} \
		s.heat_position_count = 2 * tapesize; \
		for (int p = 0; p < tapesize; p++) { \
			s.heat_position[p] = xstats.heat_position[0][p]; \
			s.heat_position[tapesize+p] = xstats.heat_position[1][p]; \
		} \
		pb_put(Statistics_fields, &s); \
	} while (0)
#endif

/* main application for gearlanced/cranklanced */

static union opcode *readprog(enum core_action act, uint32_t len)
{
	if (setjmp(fail_buf))
	{
		write_error("parse error: %s\n", fail_msg);
		return 0;
	}

	union opcode *code = 0;
	struct oplist *ops = parse(&len);
	if (len == 0) code = core(act, ops, 0, 0);
	else write_error("parse did not consume entire program");
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

	unsigned hillsize = (unsigned)strtol(argv[1], 0, 10);
	if (hillsize == 0 || hillsize > 1000)
	{
		fprintf(stderr, "sorry, hillsize %u looks too suspicious\n", hillsize);
		return 1;
	}

	/* construct initial hill */

	union opcode **hill = smalloc(hillsize * sizeof *hill);
	for (unsigned i = 0; i < hillsize; i++)
		hill[i] = 0;

	/* main loop */

	unsigned char cmd[8];

	while (read_n(cmd, 8))
	{
		uint32_t action = get_u32le(cmd);
		unsigned char reply[4] = {0x01, 0x00, 0x00, 0x00};

		if (ACTION_TEST(action))
		{
			union opcode *code = readprog(core_compile_b, get_u32le(cmd + 4));
			if (!code)
				continue; /* error reply already sent */

#ifdef CRANK_IT
			reply[0] |= 0x02;
#endif
			reply[1] = MINTAPE;
			reply[2] = MAXTAPE;
			reply[3] = hillsize;
			write_n(reply, sizeof reply);

			for (unsigned i = 0; i < hillsize; i++)
			{
				if (!hill[i])
				{
					// TODO: fake statistics for cranklance
					for (unsigned pol = 0; pol < 2; pol++)
						for (unsigned tlen = MINTAPE; tlen <= MAXTAPE; tlen++)
							scores[pol][tlen] = 1;  // always a win
				}
				else
					core(core_run, 0, hill[i], code);

				int8_t reply_points[2][NTAPES];

				/*
				 * Executing a program will, as a side effect, flip the polarity by rewriting
				 * the opcodes. As a result, this result-parsing loop has to keep switching
				 * the mapping of which block of scores truly corresponds to sieve vs. kettle.
				 */

				int sieve = i % 2, kettle = !sieve;
				for (unsigned tlen = MINTAPE; tlen <= MAXTAPE; tlen++)
				{
					reply_points[0][tlen - MINTAPE] = -scores[sieve][tlen];
					reply_points[1][tlen - MINTAPE] = -scores[kettle][tlen];
				}

				write_n((unsigned char*)&reply_points[0][0], sizeof reply_points);
			}
		}
		else if (ACTION_SET(action))
		{
			uint32_t index = action >> 8;
			if (index >= hillsize)
			{
				write_error("invalid index for set: %u", (unsigned)index);
				continue;
			}

			union opcode *code = readprog(core_compile_a, get_u32le(cmd + 4));
			if (!code)
				continue; /* error reply already sent */

			free(hill[index]);
			hill[index] = code;

			write_n(reply, sizeof reply);
		}
		else if (ACTION_UNSET(action))
		{
			uint32_t index = action >> 8;
			if (index >= hillsize)
			{
				write_error("invalid index for unset: %u", (unsigned)index);
				continue;
			}

			free(hill[index]);
			hill[index] = 0;

			write_n(reply, sizeof reply);
		}
		else
		{
			write_error("unknown action: 0x%04x", (unsigned)action);
		}
	}

	return 0;
}
