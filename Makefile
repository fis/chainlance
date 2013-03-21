.PHONY : clean test

all: chainlance cranklance gearlance

chainlance: chainlance.c
	gcc -o chainlance -std=gnu99 -g -Wall chainlance.c

cranklance: cranklance.c parser.c parser.h
#	gcc -o cranklance -std=gnu99 -g -O0 -Wall cranklance.c
	gcc -o cranklance -std=gnu99 -O2 -flto -march=native -Wall cranklance.c parser.c

gearlance: gearlance.c parser.c parser.h
	gcc -o gearlance -std=gnu99 -O2 -flto -march=native -Wall cranklance.c parser.c

clean:
	$(RM) chainlance cranklance gearlance

test: chainlance
	./chainlance test.b test.b > test.asm
	nasm -o test.o -f elf64 -g -Ox test.asm
	gcc -g -o test test.o
