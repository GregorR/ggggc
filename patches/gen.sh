#!/bin/sh
for i in bytetags finalizers jitpstack tagging
do
    git diff origin/master patch-$i -- > $i/$i.diff
done
