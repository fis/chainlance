GEARLANCES = cranklance gearlance gearlanced
PROGS = chainlance $(GEARLANCES) gearlanced genelance torquelance torquelance-compile wrenchlance-left wrenchlance-right
PARSER = parser.c parser.h common.c common.h
GCC = gcc -std=gnu99 -O2 -fwhole-program -march=native -Wall -Wextra

.PHONY : all clean test rdoc

all: $(PROGS)

chainlance: chainlance.c
	gcc -o chainlance -std=gnu99 -g -Wall chainlance.c

$(GEARLANCES): %: %.c gearlance.c $(PARSER)
	$(GCC) -o $@ $<

genelance: genelance.c gearlance.c $(PARSER)
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
	$(RM) $(PROGS)
	$(RM) torquelance-header.o torquelance-ops.o wrenchlance-ops.o

rdoc:
	rdoc -o rdoc zhillbot.rb zhill

# TODO: obsoleted test code, see test/ instead
test: chainlance
	./chainlance test.b test.b > test.asm
	nasm -o test.o -f elf64 -g -Ox test.asm
	gcc -g -o test test.o
