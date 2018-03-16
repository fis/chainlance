CC = gcc
COPTS = -std=gnu11 -Wall -Wextra -O2 -march=native -flto -g

.PHONY : all clean test rdoc

all: build/gearlance build/cranklance build/gearlanced build/cranklanced build/genelance build/chainlance build/torquelance build/torquelance-compile build/wrenchlance-left build/wrenchlance-right

build/.dir:
	mkdir -p build
	touch $@

# Currently active lances:

GEARLANCE_SRCS = gearlance.c common.c parser.c
GEARLANCE_DEPS = gearlance.h common.h parser.h

GEARLANCED_SRCS = gearlanced.c $(GEARLANCE_SRCS)
GEARLANCED_DEPS = $(GEARLANCE_DEPS)

build/gearlance: $(GEARLANCE_SRCS) $(GEARLANCE_DEPS) build/.dir
	$(CC) $(COPTS) -o $@ $(GEARLANCE_SRCS)

build/cranklance: $(GEARLANCE_SRCS) $(GEARLANCE_DEPS) build/.dir
	$(CC) $(COPTS) -DCRANK_IT=1 -o $@ $(GEARLANCE_SRCS)

build/gearlanced: $(GEARLANCED_SRCS) $(GEARLANCED_DEPS)
	$(CC) $(COPTS) -DNO_MAIN=1 -DPARSE_STDIN=1 -Ibuild -o $@ $(GEARLANCED_SRCS)

build/cranklanced: $(GEARLANCED_SRCS) $(GEARLANCED_DEPS)
	$(CC) $(COPTS) -DNO_MAIN=1 -DPARSE_STDIN=1 -Ibuild -DCRANK_IT=1 -o $@ $(GEARLANCED_SRCS)

build/genelance: genelance.c $(GEARLANCE_SRCS) $(GEARLANCE_DEPS)
	$(CC) $(COPTS) -DNO_MAIN=1 -DPARSE_NEWLINE_AS_EOF=1 genelance.c $(GEARLANCE_SRCS)

# More or less outdated lances:

build/chainlance: chainlance.c build/.dir
	$(CC) $(COPTS) -o $@ chainlance.c

build/torquelance: torquelance.c torquelance-header.s common.c common.h parser.h build/.dir
	$(CC) $(COPTS) -o $@ torquelance.c torquelance-header.s common.c

build/torquelance-compile: torquelance-compile.c torquelance-ops.s common.c common.h parser.c parser.h build/.dir
	$(CC) $(COPTS) -DPARSER_EXTRAFIELDS='unsigned code;' -o $@ torquelance-compile.c torquelance-ops.s common.c parser.c

build/wrenchlance-left: wrenchlance-left.c wrenchlance-ops.s common.c common.h parser.c parser.h build/.dir
	$(CC) $(COPTS) -DPARSER_EXTRAFIELDS='unsigned code;' -o $@ wrenchlance-left.c wrenchlance-ops.s common.c parser.c

build/wrenchlance-right: wrenchlance-right.c common.c common.h parser.c parser.h build/.dir
	$(CC) $(COPTS) -o $@ wrenchlance-right.c common.c parser.c

# Phony targets:

clean:
	$(RM) -r build

rdoc:
	rdoc -o rdoc zhillbot.rb zhill
