LANCES = cranklance gearlance genelance

.PHONY : clean test

all: chainlance $(LANCES)

chainlance: chainlance.c
	gcc -o chainlance -std=gnu99 -g -Wall chainlance.c

$(LANCES): %: %.c parser.c parser.h
#	gcc -o cranklance -std=gnu99 -g -O0 -Wall cranklance.c
	gcc -o $@ -std=gnu99 -O2 -fwhole-program -march=native -Wall $<

clean:
	$(RM) chainlance cranklance gearlance

test: chainlance
	./chainlance test.b test.b > test.asm
	nasm -o test.o -f elf64 -g -Ox test.asm
	gcc -g -o test test.o
