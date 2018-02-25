#! /bin/bash

left="$1"
right="$2"

lc=compiled/${left//'/'/_}
rc=compiled/${right//'/'/_}

if [ ! -e $lc.bin ]; then
    ../build/wrenchlance-left $left $lc.bin
fi

if [ ! -e $rc ]; then
    ../build/wrenchlance-right $right | \
        gcc -std=gnu99 -fwhole-program -O2 -march=native \
        ../wrenchlance-stub.c ../wrenchlance-header.s -x assembler - \
        -o $rc
fi

$rc $lc.bin
