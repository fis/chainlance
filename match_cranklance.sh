#! /bin/bash

if [ "$#" != "3" ]; then
    echo "Usage: $0 reps prog1.b prog2.b"
    exit 1
fi

sum=0

for (( i=1; i<=$1; i++ )); do
    ./cranklance $2 $3 >/dev/null
    score=$?
    if (( $score>128 )); then
        sum=$(( $sum+($score-256) ))
    else
        sum=$(( $sum+$score ))
    fi
done

echo $(( 2*$sum/$1 ));
