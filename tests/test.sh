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
    eRun ./btggggcth 16
    eRun ./badlll
    eRun ./ggggcbench

    cd ..
}

if [ "$1" ]
then
    # User-requested test compilers
    for cc in "$@"
    do
        doTests "$cc" ''
    done

else
    # Default GCC tests
    doTests gcc ''
    doTests g++ ''
    doTests gcc '-DGGGGC_NO_GNUC_FEATURES'
    doTests g++ '-DGGGGC_NO_GNUC_FEATURES'

fi

exit 0
