#!/bin/sh
for b in \
    GGC_PUSH GGC_DPUSH \
    GGC_PUSH_GLOBAL GGC_DPUSH_GLOBAL \
    GGC_POP GGC_DPOP GGC_APOP GGC_ASPOP GGC_ADPOP GGC_AAPOP \
    GGC_YIELD
do
    echo '#undef '$b
    echo '#define '$b'(...)'
done

for b in `seq 1 20`
do
    echo '#undef GGC_PUSH'$b
    echo '#define GGC_PUSH'$b'(...)'
    echo '#undef GGC_DPUSH'$b
    echo '#define GGC_DPUSH'$b'(...)'
done
