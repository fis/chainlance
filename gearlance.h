/*
 * gearlance bfjoust interpreter; based on cranklance, itself based on
 * chainlance.
 *
 * Copyright (c) 2011-2013 Heikki Kallasjoki
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef GEARLANCE_H
#define GEARLANCE_H 1

#include "common.h"
#include "parser.h"

#define MINTAPE 10
#define MAXTAPE 30
#define NTAPES (MAXTAPE - MINTAPE + 1)

enum core_action
{
	core_compile_a,
	core_compile_b,
	core_run, /* note: this action flips polarity of program B */
};

union opcode
{
	void *xt;
	union opcode *match;
	int count;
};

struct opcodes
{
	unsigned len;
	union opcode ops[];
};

struct opcodes *core(enum core_action act, struct oplist *ops, struct opcodes *codeA, struct opcodes *codeB);

extern int scores[2][MAXTAPE+1];

#endif // GEARLANCE_H
