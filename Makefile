GEARLANCES = gearlance cranklance
GEARLANCEDS = gearlanced cranklanced
PROGS = $(GEARLANCES) $(GEARLANCEDS) genelance chainlance torquelance torquelance-compile wrenchlance-left wrenchlance-right
PARSER = parser.c parser.h common.c common.h
GCC = gcc -std=gnu99 -O2 -fwhole-program -march=native -Wall -Wextra

.PHONY : all clean test rdoc

all: $(PROGS) zhill/geartalk.pb.rb

# Currently active lances:

$(GEARLANCES): %: %.c gearlance.c $(PARSER)
	$(GCC) -o $@ $<

$(GEARLANCEDS): %: %.c gearlanced.c gearlance.c geartalk.pb.c geartalk.pb.h $(PARSER)
	$(GCC) -Inanopb -DPB_FIELD_16BIT=1 -o $@ $<

genelance: genelance.c gearlance.c $(PARSER)
	$(GCC) -o $@ $<

# Protobuf generation for the GearTalk™ protocol:

%.pb.c %.pb.h: %.proto %.options
	protoc --plugin=protoc-gen-nanopb=nanopb/generator/protoc-gen-nanopb --nanopb_out=. $<

zhill/geartalk.pb.rb: geartalk.proto
	rprotoc --out zhill geartalk.proto

# More or less outdated lances:

chainlance: chainlance.c
	gcc -o chainlance -std=gnu99 -g -Wall chainlance.c

torquelance: torquelance.c torquelance-header.o common.c common.h parser.h
	$(GCC) -o $@ $< torquelance-header.o

torquelance-compile: torquelance-compile.c torquelance-ops.o $(PARSER)
	$(GCC) -o $@ $< torquelance-ops.o

wrenchlance-left: wrenchlance-left.c wrenchlance-ops.o $(PARSER)
	$(GCC) -o $@ $< wrenchlance-ops.o

wrenchlance-right: wrenchlance-right.c $(PARSER)
	$(GCC) -o $@ $<

# Phony targets:

clean:
	$(RM) $(PROGS)
	$(RM) torquelance-header.o torquelance-ops.o wrenchlance-ops.o
	$(RM) geartalk.pb.c geartalk.pb.h zhill/geartalk.pb.rb

rdoc:
	rdoc -o rdoc zhillbot.rb zhill

# TODO: obsoleted test code, see test/ instead
test: chainlance
	./chainlance test.b test.b > test.asm
	nasm -o test.o -f elf64 -g -Ox test.asm
	gcc -g -o test test.o
