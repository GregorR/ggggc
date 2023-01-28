#!/bin/sh
cd "`dirname $0`"/..
set -e

GGGGC_LIBS="../libggggc.a -pthread"
if [ `uname` = "Darwin" ]
then
    GGGGC_LIBS=../libggggc.a
fi

eRun() {
    echo "$@"
    "$@"
}

doTests() {
    (
    make clean
    make CC="$2" ECFLAGS="-O3 -g $3 -DGGGGC_FEATURE_$1"

    cd tests
    make clean
    make btggggc btggggcth badlll ggggcbench lists maps graph \
        CC="$2" ECFLAGS="-O3 -g $3 -DGGGGC_FEATURE_$1" GGGGC_LIBS="$GGGGC_LIBS"

    eRun ./btggggc 16
    eRun ./btggggcth 16
    eRun ./badlll
    eRun ./ggggcbench
    eRun ./lists
    eRun ./maps
    eRun ./graph
    )
}

if [ "$1" ]
then
    # User-requested test compilers
    for cc in "$@"
    do
        doTests '' "$cc" ''
    done

else
    # Test each feature combo
    FEATURES="FINALIZERS TAGGING EXTTAG JITPSTACK"
    for feature in '' $FEATURES
    do
        doTests "$feature" gcc '-O0 -g -Wall -Werror -std=c99 -pedantic -Wno-array-bounds -Wno-unused-function -Werror=shadow -DGGGGC_DEBUG_MEMORY_CORRUPTION'
        doTests "$feature" gcc ''
        doTests "$feature" g++ ''
        #doTests "$feature" gcc '-DGGGGC_NO_GNUC_FEATURES'
        doTests "$feature" g++ '-DGGGGC_NO_GNUC_FEATURES'

        doTests "$feature" gcc '-DGGGGC_DEBUG_TINY_HEAP'

        doTests "$feature" gcc '-DGGGGC_GENERATIONS=1'
        doTests "$feature" gcc '-DGGGGC_GENERATIONS=5'
        doTests "$feature" gcc '-DGGGGC_COLLECTOR=portablems'

        doTests "$feature" gcc '-DGGGGC_USE_MALLOC'
        doTests "$feature" gcc '-DGGGGC_USE_SBRK -DGGGGC_NO_THREADS'
    done

fi

exit 0
