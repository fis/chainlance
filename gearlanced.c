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
 *         N* (42* Statistics [if Reply.with_statistics],
 *             Joust) [if Reply.ok].
 * Test program P against those currently on the hill.  If there are
 * no errors, the results for all tape length/polarity matches are
 * sent in the following Joust messages.  If compiled as cranklanced,
 * the Joust message will be preceded by 42 individual Statistics
 * messages, giving detailed statistics about each individual match.
 * The Reply.with_statistics field will be set to indicate this.
 * There will be a total of N groups of Joust-and-maybe-Statistics
 * messages.
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

/*
 * OLD LINE-ORIENTED PROTOCOL:
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
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_MAIN 1
#define PARSE_STDIN 1
#include "gearlance.c"

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
		reply.with_statistics = false;

		if (cmd.action == Action_TEST)
		{
			struct opcodes *code = readprog(core_compile_b, &reply);
			if (!code)
				continue; /* error reply already sent */

			reply.ok = true;
			pb_put(Reply_fields, &reply);

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

				Joust joust;
				joust.sieve_points_count = MAXTAPE - MINTAPE + 1;
				joust.kettle_points_count = MAXTAPE - MINTAPE + 1;

				/*
				 * Executing a program will, as a side effect, flip the polarity by rewriting
				 * the opcodes. As a result, this result-parsing loop has to keep switching
				 * the mapping of which block of scores truly corresponds to sieve vs. kettle.
				 */

				int sieve = i % 2, kettle = !sieve;
				for (unsigned tidx = 0, tlen = MINTAPE; tlen <= MAXTAPE; tidx++, tlen++)
				{
					joust.sieve_points[tidx] = -scores[sieve][tlen];
					joust.kettle_points[tidx] = -scores[kettle][tlen];
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
