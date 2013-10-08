#! /bin/bash

prog="$1"

./warriors.sh | while read left right; do
    read ref <&3
    hyp=$($prog $left $right)
    if [[ "$hyp" != "$ref" ]]; then
        echo "FAILED: $left $right" >&2
        echo "EXPECTED: $ref" >&2
        echo "GOT: $hyp" >&2
    fi
done 3<reference.out
