#! /bin/bash

runner=$1
shift

echo "battling:" "$@"

for left; do
    for right; do
        if [ "$left" == "$right" ]; then continue; fi

        score=`$runner 1 $left $right`
        echo "$left vs. $right: $score"
    done
done

#for (( left=1; left<=$#; left++ )); do
#    for (( right=1; right<=$#; right++ )); do
#        if [ $left != $right ]; then
#            l=
#            score=`$runner 
#        fi
#    done
#done