#!/bin/sh
for i in bytetags finalizers jitpstack jitpstack-exttag tagging
do
    git diff origin/master patch-$i -- > $i/$i.diff
done
