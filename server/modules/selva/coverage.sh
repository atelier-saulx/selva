#!/usr/bin/env sh
cd test/units
gcov "../../$1" -m -k -j -q -t --object-directory obj
