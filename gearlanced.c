/*
 * gearlanced bfjoust interpreter; based on gearlance, itself based on
 * cranklance, itself based on chainlance.
 *
 * This one is for maintaining a hill of bfjoust programs.  You invoke
 * it as "./gearlanced N", where N is a positive integer denoting the
 * hill size.  The hill will be initialized with N copies of a
 * hypothetical program that will always lose.  After initialization,
 * the program will read commands from stdin, and reply results to
 * stdout.  This "GearTalk" protocol uses the messages defined in
 * geartalk.proto, in the following way.  All protobuf messages are
 * sent in the length-delimited format, i.e., prefixed by a varint
 * encoding the length of the following message.
 *
 * The caller must send a delimited "Command" message, potentially
 * (for some commands) followed by a bfjoust program terminated by a
 * '\0' character.  The program replies with a "Reply" message,
 * potentially followed by other messages.  The command and reply
 * formats are:
 *
 * Action.TEST:
 * Input: Command, bfjoust program P.
 * Output: Reply,
 *         [if Reply.ok:
 *          N* ([if Reply.with_statistics:
 *               varint C, C* Statistics],
 *              Joust)]
 * Test program P against those currently on the hill.  If there are
 * no errors, the results for all tape length/polarity matches are
 * sent in the following Joust messages.  If compiled as cranklanced,
 * the Joust message will be preceded by a varint C and C individual
 * Statistics messages, giving detailed statistics about each
 * individual match.  The Reply.with_statistics field will be set to
 * indicate this.  C is one of two values: either 0 (if the opponent
 * slot on the hill is empty), or (normally) 42 (all tape lengths and
 * polarities).  There will be a total of N groups of Joust (and maybe
 * Statistics) messages.
 *
 * Action.SET:
 * Input: Command [.index = I], bfjoust program P.
 * Output: Reply.
 * Set the I'th program (where 0 <= I < N) to P.
 *
 * Action.UNSET:
 * Input: Command [.index = I].
 * Output: Reply.
 * Remove the I'th program (where 0 <= I < N), by setting it to the
 * program that always loses.  This is used to "blank" a program that
 * is being replaced on the hill, for efficiency reasons.
 *
 * An EOF condition in place of a command will terminate the program.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* protobuf helpers used also by gearlance.c code for cranklanced */

#include "geartalk.pb.h"
#include "geartalk.pb.c"

#include <pb_decode.h>
#include <pb_encode.h>

#include <pb_decode.c>
#include <pb_encode.c>
#include <pb_common.c>

static bool stdio_istream_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
	FILE *file = (FILE *)stream->state;
	bool status;

	if (!buf)
	{
		while (count-- && fgetc(file) != EOF)
			;
		return count == 0;
	}

	status = (fread(buf, 1, count, file) == count);
	if (feof(file))
		stream->bytes_left = 0;

	return status;
}

static bool stdio_ostream_callback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
	FILE *file = (FILE *)stream->state;
	return fwrite(buf, 1, count, file) == count;
}

static pb_istream_t stdin_stream;
static pb_ostream_t stdout_stream;

static void pb_put(const pb_field_t fields[], const void *src)
{
	if (!pb_encode_delimited(&stdout_stream, fields, src))
		abort();
}

/* statistics output code for cranklanced */

#ifdef CRANK_IT
#define EXECUTE_STATS(pol) do { \
		Statistics s = Statistics_init_default; \
		s.has_cycles = true; \
		s.cycles = MAXCYCLES - cycles; \
		s.has_tape_abs = true; s.tape_abs.size = tapesize; \
		for (int p = 0; p < tapesize; p++) s.tape_abs.bytes[p] = tape[p] >= 128 ? tape[p]-256 : tape[p]; \
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

/* include gearlance/cranklance core */

#define NO_MAIN 1
#define PARSE_STDIN 1
#include "gearlance.c"

/* main application for gearlanced/cranklanced */

static struct opcodes *readprog(enum core_action act, Reply *reply)
{
	if (setjmp(fail_buf))
	{
		reply->has_error = true;
		snprintf(reply->error, sizeof reply->error, "parse error: %s\n", fail_msg);
		pb_put(Reply_fields, reply);
		fflush(stdout);
		return 0;
	}

	struct oplist *ops = parse(0);
	struct opcodes *code = core(act, ops, 0, 0);
	opl_free(ops);

	return code;
}

int main(int argc, char *argv[])
{
	stdin_stream = (pb_istream_t){ &stdio_istream_callback, stdin, SIZE_MAX, 0 };
	stdout_stream = (pb_ostream_t){ &stdio_ostream_callback, stdout, SIZE_MAX, 0, 0 };

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

	struct opcodes **hill = smalloc(hillsize * sizeof *hill);
	for (unsigned i = 0; i < hillsize; i++)
		hill[i] = 0;

	/* main loop */

	Command cmd;

	while (pb_decode_delimited(&stdin_stream, Command_fields, &cmd))
	{
		if (!cmd.has_action || cmd.action == Action_UNKNOWN)
			break;

		Reply reply = Reply_init_default;
		reply.has_ok = true;
		reply.ok = false;
		reply.has_with_statistics = true;
#ifdef CRANK_IT
		reply.with_statistics = true;
#else
		reply.with_statistics = false;
#endif

		if (cmd.action == Action_TEST)
		{
			struct opcodes *code = readprog(core_compile_b, &reply);
			if (!code)
				continue; /* error reply already sent */

			reply.ok = true;
			pb_put(Reply_fields, &reply);

			for (unsigned i = 0; i < hillsize; i++)
			{
#ifdef CRANK_IT
				if (!pb_encode_varint(&stdout_stream, hill[i] ? 2 * NTAPES : 0))
					abort(); /* too bad */
#endif

				if (!hill[i])
				{
					for (unsigned pol = 0; pol < 2; pol++)
						for (unsigned tlen = MINTAPE; tlen <= MAXTAPE; tlen++)
							scores[pol][tlen] = -1;
				}
				else
					core(core_run, 0, hill[i], code);

				Joust joust;
				joust.sieve_points_count = NTAPES;
				joust.kettle_points_count = NTAPES;

				/*
				 * Executing a program will, as a side effect, flip the polarity by rewriting
				 * the opcodes. As a result, this result-parsing loop has to keep switching
				 * the mapping of which block of scores truly corresponds to sieve vs. kettle.
				 */

				int sieve = i % 2, kettle = !sieve;
				for (unsigned tlen = MINTAPE; tlen <= MAXTAPE; tlen++)
				{
					joust.sieve_points[tlen - MINTAPE] = -scores[sieve][tlen];
					joust.kettle_points[tlen - MINTAPE] = -scores[kettle][tlen];
				}

				pb_put(Joust_fields, &joust);
			}

			fflush(stdout);
		}
		else if (cmd.action == Action_SET)
		{
			if (!cmd.has_index || cmd.index >= hillsize)
			{
				reply.has_error = true;
				snprintf(reply.error, sizeof reply.error,
				         cmd.has_index ? "no index for set" : "invalid index for set: %u",
				         cmd.index);
				pb_put(Reply_fields, &reply);
				fflush(stdout);
				continue;
			}

			struct opcodes *code = readprog(core_compile_a, &reply);
			if (!code)
				continue; /* error reply already sent */

			free(hill[cmd.index]);
			hill[cmd.index] = code;

			reply.ok = true;
			pb_put(Reply_fields, &reply);
			fflush(stdout);
		}
		else if (cmd.action == Action_UNSET)
		{
			if (!cmd.has_index || cmd.index >= hillsize)
			{
				reply.has_error = true;
				snprintf(reply.error, sizeof reply.error,
				         cmd.has_index ? "no index for unset" : "invalid index for unset: %u",
				         cmd.index);
				pb_put(Reply_fields, &reply);
				fflush(stdout);
				continue;
			}

			free(hill[cmd.index]);
			hill[cmd.index] = 0;

			reply.ok = true;
			pb_put(Reply_fields, &reply);
			fflush(stdout);
		}
		else
		{
			reply.has_error = true;
			snprintf(reply.error, sizeof reply.error, "unknown command");
			pb_put(Reply_fields, &reply);
			fflush(stdout);
		}
	}

	return 0;
}
