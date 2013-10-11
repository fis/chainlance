GEARLANCES = cranklance gearlance
PARSER = parser.c parser.h common.c common.h
GCC = gcc -std=gnu99 -O2 -fwhole-program -march=native -Wall -Wextra

.PHONY : all clean test

all: chainlance $(GEARLANCES) torquelance torquelance-compile \
	wrenchlance-left wrenchlance-right

chainlance: chainlance.c
	gcc -o chainlance -std=gnu99 -g -Wall chainlance.c

$(GEARLANCES): %: %.c gearlance.c $(PARSER)
	$(GCC) -o $@ $<

genelance: genelance.c $(PARSER)
	$(GCC) -o $@ $<

torquelance: torquelance.c torquelance-header.o common.c common.h parser.h
	$(GCC) -o $@ $< torquelance-header.o

torquelance-compile: torquelance-compile.c torquelance-ops.o $(PARSER)
	$(GCC) -o $@ $< torquelance-ops.o

wrenchlance-left: wrenchlance-left.c wrenchlance-ops.o $(PARSER)
	$(GCC) -o $@ $< wrenchlance-ops.o

wrenchlance-right: wrenchlance-right.c $(PARSER)
	$(GCC) -o $@ $<

clean:
	$(RM) chainlance cranklance gearlance genelance
	$(RM) torquelance torquelance-header.o
	$(RM) torquelance-compile torquelance-ops.o
	$(RM) wrenchlance-left wrenchlance-right wrenchlance-ops.o

test: chainlance
	./chainlance test.b test.b > test.asm
	nasm -o test.o -f elf64 -g -Ox test.asm
	gcc -g -o test test.o
