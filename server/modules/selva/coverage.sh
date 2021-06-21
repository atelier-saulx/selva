#!/usr/bin/env bash
#gcov "../../$1" -m -k -j -q -t --object-directory obj
mkdir -p gcov/reports
cd gcov/reports
for o in `ls ../../test/units/obj |grep '\.o' |sed 's/\.o$//'`; do
    gcov "$o" --object-directory ../../test/units/obj
done
