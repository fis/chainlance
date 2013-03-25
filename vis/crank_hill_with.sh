#! /bin/bash

if (( $#<3 )); then
    echo "usage: $0 exe file1 file2 ..."
    exit 1
fi

for (( li=2; li<$#; li++ )); do
    for (( ri=li+1; ri<$#+1; ri++ )); do
        echo "! ${!li} ${!ri}"
        $1 ${!li} ${!ri}
    done
done
