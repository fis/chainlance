#! /bin/bash

left="$1"
right="$2"

lc=compiled/${left//'/'/_}
rc=compiled/${right//'/'/_}

if [ ! -e $lc.A ]; then
    ../build/torquelance-compile $left $lc.A A
fi

if [ ! -e $rc.B ]; then
    ../build/torquelance-compile $right $rc.B B
fi

if [ ! -e $rc.B2 ]; then
    ../build/torquelance-compile $right $rc.B2 B2
fi

../build/torquelance $lc.A $rc.B $rc.B2
