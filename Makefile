LANCES = cranklance gearlance

.PHONY : all clean test

all: chainlance $(LANCES) torquelance torquelance-compile

chainlance: chainlance.c
	gcc -o chainlance -std=gnu99 -g -Wall chainlance.c

$(LANCES): %: %.c gearlance.c parser.c parser.h common.c common.h
#	gcc -o cranklance -std=gnu99 -g -O0 -Wall cranklance.c
	gcc -o $@ -std=gnu99 -O2 -fwhole-program -march=native -Wall -Wextra $<

genelance: genelance.c parser.c parser.h common.c common.h
	gcc -o $@ -std=gnu99 -O2 -fwhole-program -march=native -Wall -Wextra $<

torquelance: torquelance.c torquelance-header.o common.c common.h parser.h
	gcc -o $@ -std=gnu99 -O2 -fwhole-program -march=native -Wall -Wextra $< torquelance-header.o

torquelance-compile: torquelance-compile.c parser.c parser.h common.c common.h torquelance-ops.o
#	gcc -o $@ -std=gnu99 -g -O0 -fwhole-program -march=native -Wall -Wextra $< torquelance-ops.o
	gcc -o $@ -std=gnu99 -O2 -fwhole-program -march=native -Wall -Wextra $< torquelance-ops.o

clean:
	$(RM) chainlance cranklance gearlance genelance
	$(RM) torquelance torquelance-header.o
	$(RM) torquelance-compile torquelance-ops.o

test: chainlance
	./chainlance test.b test.b > test.asm
	nasm -o test.o -f elf64 -g -Ox test.asm
	gcc -g -o test test.o
