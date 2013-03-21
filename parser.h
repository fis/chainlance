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

#ifndef CRANKLANCE_PARSER_H
#define CRANKLANCE_PARSER_H 1

#include <stddef.h>
#include <setjmp.h>

#define MAXCYCLES 100000
#define MAXNEST 4096

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

void die(const char *fmt, ...);
void fail(const char *fmt, ...);

extern char fail_msg[256];
extern jmp_buf fail_buf;

void *smalloc(size_t size);
void *srealloc(void *ptr, size_t size);

int sopen(const char *fname);

/* oplist handling */

struct oplist *opl_new(void);
void opl_free(struct oplist *o);
void opl_append(struct oplist *o, enum optype type);
void opl_del(struct oplist *o, int start, int end);

/* parsing and preprocessing */

struct oplist *parse(int fd);

#endif /* !CRANKLANCE_PARSER_H */
