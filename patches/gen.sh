#!/bin/sh
for i in finalizers jitpstack tagging
do
    git diff origin/master patch-$i -- > $i/$i.diff
done
