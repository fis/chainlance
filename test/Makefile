TESTS = test-gearlance test-torquelance test-wrenchlance test-gearlanced

.PHONY : all $(TESTS)

all: reference.out $(TESTS)

test-gearlance: reference.out
	time ./test.sh ../gearlance

test-torquelance: reference.out
	mkdir -p compiled
	$(RM) compiled/*
	time ./test.sh ./test-torquelance.sh
	$(RM) -r compiled

test-wrenchlance: reference.out
	mkdir -p compiled
	$(RM) compiled/*
	time ./test.sh ./test-wrenchlance.sh
	$(RM) -r compiled

test-gearlanced: reference.out
	time ./test-gearlanced.sh "./gearlanced-wrapper.rb ../gearlanced"

reference.out: warriors.sh warriors.idx egojsout.js
	./warriors.sh | while read left right; do \
		nodejs egojsout.js $$left $$right; \
	done > reference.out
