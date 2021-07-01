#!/usr/bin/env bash
set -e
make -C server/modules/selva clean
# TODO make test fails in GH Actions
#build-wrapper-linux-x86-64 --out-dir bw-output make all test test-gcov -C server/modules/selva -j4
build-wrapper-linux-x86-64 --out-dir bw-output make all -C server/modules/selva -j4
sonar-scanner \
    -Dsonar.organization=atelier-saulx \
    -Dsonar.projectKey=atelier-saulx_selva \
    -Dsonar.sources=server/modules/selva \
    -Dsonar.cfamily.gcov.reportsPath=server/modules/selva/gcov/reports \
    -Dsonar.cfamily.build-wrapper-output=bw-output \
    -Dsonar.cfamily.threads=4 \
    -Dsonar.host.url=https://sonarcloud.io
