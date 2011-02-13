.PHONY : clean test

chainlance: chainlance.c
	gcc -o chainlance -std=gnu99 -g -Wall chainlance.c

clean:
	$(RM) chainlance

test: chainlance
	./chainlance test.b test.b > test.asm
	nasm -o test.o -f elf64 -g -Ox test.asm
	gcc -g -o test test.o
