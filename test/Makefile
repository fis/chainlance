.PHONY : all test-gearlance test-torquelance

all: reference.out test-gearlance test-torquelance

test-gearlance: reference.out
	time ./test.sh ../gearlance

test-torquelance: reference.out
	mkdir -p compiled
	$(RM) compiled/*
	time ./test.sh ./test-torquelance.sh
	$(RM) -r compiled

reference.out: warriors.sh warriors.idx egojsout.js
	./warriors.sh | while read left right; do \
		nodejs egojsout.js $$left $$right; \
	done > reference.out
