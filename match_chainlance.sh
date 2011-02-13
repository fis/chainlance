#! /bin/bash

if [ "$#" != "3" ]; then
    echo "Usage: $0 reps prog1.b prog2.b"
    exit 1
fi

./chainlance header.asm $2 $3 > match.asm || exit 1
nasm -g -o match.o -f elf64 -Ox match.asm || exit 1
gcc -g -o match match.o || exit 1

sum=0

for (( i=1; i<=$1; i++ )); do
    ./match
    score=$?
    if (( $score>128 )); then
        sum=$(( $sum+($score-256) ))
    else
        sum=$(( $sum+$score ))
    fi
done

echo $(( 2*$sum/$1 ));
