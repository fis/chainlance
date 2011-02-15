#! /bin/bash

if (( $#<2 )); then
    echo "usage: $0 file1 file2 ..."
    exit 1
fi

for (( li=1; li<$#-1; li++ )); do
    for (( ri=li+1; ri<$#; ri++ )); do
        echo -n "${!li} ${!ri}: "
        ./cranklance ${!li} ${!ri}
    done
done
