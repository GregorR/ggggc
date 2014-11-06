#!/bin/sh
cd "`dirname $0`"/..
set -e

make clean
make

cd tests
make clean
make btggggc btggggcth badlll ggggcbench

./btggggc 16
./btggggcth 18
./badlll
./ggggcbench

exit 0
