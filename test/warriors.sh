#! /bin/bash

wi=0
while read w; do
    warriors[wi]="$w"
    wi=$((wi+1))
done < warriors.idx

# actually, test both ways
#for ((li = 0; li < ${#warriors[@]} - 1; li = li+1)); do
#    for ((ri = li+1; ri < ${#warriors[@]}; ri = ri+1)); do
#        echo ${warriors[$li]} ${warriors[$ri]}
#    done
#done

for left in "${warriors[@]}"; do
    for right in "${warriors[@]}"; do
        echo $left $right
    done
done
