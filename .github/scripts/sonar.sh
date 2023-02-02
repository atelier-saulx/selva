#!/usr/bin/env bash
set -e
make -C server/selvad clean
# TODO make test fails in GH Actions
#build-wrapper-linux-x86-64 --out-dir bw-output make all test test-gcov -C server/modules/selva -j4
build-wrapper-linux-x86-64 --out-dir bw-output make all -C server/selvad
sonar-scanner \
    -Dsonar.organization=atelier-saulx \
    -Dsonar.projectKey=atelier-saulx_selva \
    -Dsonar.sources=server/selvad \
    '-Dsonar.exclusions=server/selvad/lib/deflate/**,server/selvad/lib/jemalloc/**,server/selvad/include/jemalloc_*.h' \
    -Dsonar.cfamily.gcov.reportsPath=server/selvad/gcov/reports \
    -Dsonar.cfamily.build-wrapper-output=bw-output \
    -Dsonar.cfamily.threads=4 \
    -Dsonar.host.url=https://sonarcloud.io
