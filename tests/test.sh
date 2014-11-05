#!/bin/sh
cd "`dirname $0`"/..
set -e

make clean
make

cd tests
make clean
make btggggc badlll ggggcbench

./btggggc 16
./badlll
./ggggcbench

exit 0
