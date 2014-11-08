#!/bin/sh
cd "`dirname $0`"/..
set -e

eRun() {
    echo "$@"
    "$@"
}

doTests() {
    make clean
    make CC="$1" ECFLAGS="-O3 -g $2"

    cd tests
    make clean
    make btggggc btggggcth badlll ggggcbench \
        CC="$1" ECFLAGS="$2"

    eRun ./btggggc 16
    eRun ./btggggcth 18
    eRun ./badlll
    eRun ./ggggcbench

    cd ..
}

doTests gcc ''
#doTests g++ ''
doTests gcc '-DGGGGC_NO_GNUC_CLEANUP'
#doTests g++ '-DGGGGC_NO_GNUC_CLEANUP'

exit 0
