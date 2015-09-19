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
    rm -rf patched
    make patch PATCH_DEST=patched PATCHES="$1"
    cp -a tests patched/tests

    cd patched
    make CC="$2" ECFLAGS="-O3 -g $3"

    cd tests
    make clean
    make btggggc btggggcth badlll ggggcbench \
        CC="$2" ECFLAGS="$3" GGGGC_LIBS="$GGGGC_LIBS"

    eRun ./btggggc 16
    eRun ./btggggcth 16
    eRun ./badlll
    eRun ./ggggcbench
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
    # Test each patchset
    PATCHES=`ls patches`
    for patch in '' $PATCHES
    do
        doTests "$patch" gcc '-O0 -g -Wall -Werror -std=c99 -pedantic -Wno-array-bounds -Werror=shadow -DGGGGC_DEBUG_MEMORY_CORRUPTION'
        doTests "$patch" gcc ''
        doTests "$patch" g++ ''
        doTests "$patch" gcc '-DGGGGC_NO_GNUC_FEATURES'
        doTests "$patch" g++ '-DGGGGC_NO_GNUC_FEATURES'

        doTests "$patch" gcc '-DGGGGC_DEBUG_TINY_HEAP'
        doTests "$patch" gcc '-DGGGGC_GENERATIONS=1'
        doTests "$patch" gcc '-DGGGGC_GENERATIONS=5'
        doTests "$patch" gcc '-DGGGGC_USE_MALLOC'
    done

fi

exit 0
