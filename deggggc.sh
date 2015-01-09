#!/bin/sh
BASE=`dirname "$0"`
while [ "$1" ]
do
    ( printf '%s\n' '@BEGIN' ; sed 's/^#/@/' "$1" ) |
        gcc -E -C -imacros "$BASE/ggggc/noggggc.h" - |
        sed -n '/^@BEGIN/ { s///; :l; n; /^#/ {d; bl}; s/^@/#/; s/ggggc\/gc.h/ggggc\/noggggc.h/; p; bl }'
    shift
done
