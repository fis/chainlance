.PHONY : clean test

all: chainlance cranklance

chainlance: chainlance.c
	gcc -o chainlance -std=gnu99 -g -Wall chainlance.c

cranklance: cranklance.c
	gcc -o cranklance -std=gnu99 -g -O0 -Wall cranklance.c
#	gcc -o cranklance -std=gnu99 -O2 -fwhole-program -march=native -Wall cranklance.c

clean:
	$(RM) chainlance cranklance

test: chainlance
	./chainlance test.b test.b > test.asm
	nasm -o test.o -f elf64 -g -Ox test.asm
	gcc -g -o test test.o
