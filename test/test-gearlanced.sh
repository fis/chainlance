#! /bin/bash

prog="${1:-./gearlanced-wrapper.rb ../build/gearlanced}"
n=$(wc -l warriors.idx | cut -d ' ' -f 1)

coproc $prog $n

while read idx left; do
    echo "set $idx" >&4
    tr -d '\n' < $left >&4
    echo >&4
    read ok <&3
    if [[ "$ok" != "ok" ]]; then echo "BROKE setting $left: $ok" >&2; exit 1; fi
done < <(nl -v 0 warriors.idx) 3<&${COPROC[0]} 4>&${COPROC[1]}

while read right; do
    echo "test" >&5
    tr -d '\n' < $right >&5
    echo >&5
    read ok <&4
    if [[ "$ok" != "ok" ]]; then echo "BROKE testing $right: $ok" >&2; exit 1; fi
    while read idx left; do
        read ref <&3
        read rawhyp <&4
        rwin=$(echo "$rawhyp" | tr -cd '>' | wc -c | cut -d ' ' -f 1)
        lwin=$(echo "$rawhyp" | tr -cd '<' | wc -c | cut -d ' ' -f 1)
        hyp="$(echo "$rawhyp" | tr '<>' '><') $(( $rwin - $lwin ))"
        if [[ "$hyp" != "$ref" ]]; then
            echo "FAILED: $left $right" >&2
            echo "EXPECTED: $ref" >&2
            echo "GOT: $hyp" >&2
        fi
    done < <(nl -v 0 warriors.idx) 3<&3 4<&4
done <warriors.idx 3<reference.out 4<&${COPROC[0]} 5>&${COPROC[1]}
