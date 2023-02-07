#!/bin/sh
cd "`dirname $0`"/..
set -e

GGGGC_LIBS="../libggggc.a -pthread"
DEFCC=gcc
DEFCXX=g++
if [ `uname` = "Darwin" ]
then
    GGGGC_LIBS=../libggggc.a
    DEFCC='clang++ -std=c++11 -Wno-deprecated'
    DEFCXX="$DEFCC"
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
        CC="$2" ECFLAGS="-O3 -g $4 $3 -DGGGGC_FEATURE_$1" GGGGC_LIBS="$GGGGC_LIBS"
    make graphpp \
        CXX="$DEFCXX" ECFLAGS="-O3 -g $3 -DGGGGC_FEATURE_$1 -std=c++11" GGGGC_LIBS="$GGGGC_LIBS"

    eRun ./btggggc 16
    eRun ./btggggcth 16
    eRun ./badlll
    eRun ./ggggcbench
    eRun ./lists
    eRun ./maps
    eRun ./graph
    eRun ./graphpp
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
        STD="-std=c99"
        if [ "$DEFCC" != "gcc" ] ; then STD="" ; fi
        doTests "$feature" "$DEFCC" '-O0 -g -Wall -Werror -pedantic -Wno-array-bounds -Wno-unused-function -Wno-nested-anon-types -Wno-c99-extensions -Werror=shadow -DGGGGC_DEBUG_MEMORY_CORRUPTION' $STD
        doTests "$feature" "$DEFCC"
        doTests "$feature" "$DEFCXX"
        #doTests "$feature" "$DEFCC" '-DGGGGC_NO_GNUC_FEATURES'
        doTests "$feature" "$DEFCXX" '-DGGGGC_NO_GNUC_FEATURES'

        doTests "$feature" "$DEFCC" '-DGGGGC_DEBUG_TINY_HEAP'

        doTests "$feature" "$DEFCC" '-DGGGGC_GENERATIONS=1'
        doTests "$feature" "$DEFCC" '-DGGGGC_GENERATIONS=5'
        doTests "$feature" "$DEFCC" '-DGGGGC_COLLECTOR=portablems'

        doTests "$feature" "$DEFCC" '-DGGGGC_USE_MALLOC'
        doTests "$feature" "$DEFCC" '-DGGGGC_USE_SBRK -DGGGGC_NO_THREADS'
    done

fi

exit 0
