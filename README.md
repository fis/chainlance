{chain,gear,crank,torque,gene}lance
===================================

Introduction
------------

First, it would be best to learn about
[BF Joust](http://esolangs.org/wiki/BF_Joust).

This repository holds a family of BF Joust implementations.  One of
them (gearlance) has the (dubious?) distinction of serving as the
implementation behind the "egojoust" hill on `#esoteric` of freenode.
(And also the up-and-coming "zem.fi" replacement hill.)

The following systems, in arbitrary order, are included:

* **gearlance**: A threaded-code computed-goto kind of an interpreter.
* **cranklance**: The above, but including statistics-collection code.
* **gearlanced**: Convenience interface for using gearlance to
  maintain a BF Joust hill efficiently.
* **genelance**: Convenience interface for using gearlance to evaluate
  programs against a fixed set.
* **torquelance**: A chainlance rehash, with separate compilation of
  programs.
* **chainlance**: Old thing, compiles a pair of programs to x86-64
  assembly source.
* **wrenchlance**: Halfway between torquelance and chainlance.

See below for details on individual systems.

Lances
------

### gearlance

"Primary" interpreter.  Based on the tried (tired?) but true concept
of converting the instructions of each program into a list of pointers
to their implementation.  GCC's computed goto is then used to execute
each instruction.  Control flow bounces alternatingly between the left
program, the right program and the per-tick routine `nextcycle`, which
checks for termination conditions.

To use, simply call `gearlance left.bfjoust right.bfjoust`.  The
output will list results of each match (`<` for a left win, `>` for a
right win and `X` for a tie) followed by the combined score calculated
as (left wins) - (right wins), ranging from -42 to 42.

In general, seems to outperform the wrenchlance/torquelance/chainlance
compilation approaches, even when discounting compilation time.  A
proper optimizing compiler for two concurrently (but in a synchronized
way) running BF Joust programs is left as an exercise for the reader.

There exists an unpublished gearlance variant that dispatches based on
a (left instruction, right instruction) pair, which obviously squares
the number of ops.  In theory, this avoids some amount of jumping
around.  In practice, it seems not worth the hassle either.

Etymology: Chains and gears are encountered in similar places, and
gearlance was based on chainlance.

### cranklance

Same code as gearlance, except with tracing functions compiled in.
Used to generate the statistics for the visualization machinery (see
below).  Identical usage; more complicated output format.

Etymology: Related to gearlance, but attempting to evoke imagery of
hand-cranking a machine step by step.

### gearlanced

gearlance was recently retrofitted to support threaded-code
compilation as a separate step from running a joust.  gearlanced (note
the `d`) provides an interface for keeping a hill of pre-parsed and
pre-compiled "left programs", and testing new challengers as a single
"right program".

Etymology: Gearlance, daemon. Even though it is not a daemon.

### genelance

Parsing consumes a significant fraction of the execution time of a
joust.  genelance is a convenience interface for evaluating a large
number of programs against a fixed set of (parsed-in-advance)
opponents.  The intended use case is people writing BF Joust evolvers
who'd like to use jousting results in their fitness function.

See header comment for usage.

Etymology: Boringly, by way of analogue to genetic programming.

### torquelance

A rehash of the chainlance idea of compiling programs to raw machine
code.  In this version, all code is completely position-independent,
so there is no linking step required for executing precompiled
programs.

To use:

    torquelance-compile left.bfjoust progA.bin A
    torquelance-compile right.bfjoust progB.bin B
    torquelance-compile right.bfjoust progB2.bin B2
    torquelance progA.bin progB.bin progB2.bin

The generated `.bin` files are raw x86-64 machine code.  Compiling in
mode `A` selects ops designed for the left program, while modes `B`
and `B2` use those of the right program.  Mode `B2` additionally flips
the polarity by converting all `+` instructions to `-` and vice versa.

The execution engine (`torquelance`) expects precompiled programs in
all three modes.  It will then concatenate an execution header (for a
single match) with the three programs and invoke it appropriately to
run a full tournament.

The individual opcode implementations can be found in
`torquelance-ops.s` and the execution header (including the per-tick
code) in `torquelance-header.s`.  Almost all x86-64 registers have
been used in one way or another.  Liberal use of indirect jumps
through a register are used for the position-independentness.

Control flow rotates between the left program, right program and the
per-tick code, as usual; you could easily consider them as three
coroutines.  After every (non-repeat) instruction, the programs store
their current IP and jump to the next task.  A branch-and-link
instruction sure would be nice.

Etymology: Just a thematic connection of *torque* to the other words.

### chainlance

Chronologically first of the BF Joust systems included.  A basic
ahead-of-time compilation approach: given two BF Joust programs,
generates assembly (x86-64 NASM) code to run a single tournament
between them.  The actual generated code is reasonably similar to that
of torquelance ops.

Etymology: From the combination of *chainsaw* (powerful tool) and
*lance* (for jousting).  A regrettably narcissistic name.

### wrenchlance

Concept: ahead-of-time compile and link the execution engine and the
right program (with both polarities) into an executable.  This lets
you use regular (hopefully also more processor-friendly) relative
branches in it.  The executable will then take a torquelance-style
left program binary as an argument, load it to memory, and run a
tournament against it.

Arguably amusing idea, but does not really seem to be worth it.

To use:

    wrenchlance-left left.bfjoust left.bin
    wrenchlance-right right.bfjoust | \
      gcc -std=gnu99 -fwhole-program -O2 -march=native \
          wrenchlance-stub.c wrenchlance-header.s -x assembler - \
          -o right
    ./right left.bin

Etymology: Something you apply torque with.

Other notes
-----------

### Building

There's a rather rudimentary `Makefile`.  That should work.

On the versions of GCC I've used, `-fwhole-program` still seems to
work better than proper link-time optimization, so even though the
code is split into `common.c` utilities and the `parser.c` parser
code, the programs using those bits #include the corresponding `.c`
files.

### Regression tests

The `test/` directory contains a set of programs, a copy of the
[EgoJSout](http://codu.org/eso/bfjoust/egojsout/index.php) interpreter
adapted to run under Node.js and some scripts.  The scripts should
make it possible to compute reference results for all pairs of
programs, and compare the output of gearlance and torquelance to the
reference results.

### Visualization

Lives in the `vis/` directory.  Needs documentation.  Used to generate
[egostats](http://zem.fi/egostats/) every now and then.

### Python glue

The `zhill/` package, and the `zhillbot.py` script, form the basis of
the zem.fi BF Joust hill.  The `web/` directory contains the static
website parts.  This part is also not documented, and probably not
very useful, except possibly as a (cautionary?) example.
