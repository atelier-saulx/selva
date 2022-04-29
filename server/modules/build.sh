#!/usr/bin/env bash
set -e

if [[ "$OSTYPE" == "darwin"* ]]; then
    pushd selva
    make clean
    make
    distpath=$(node -e "const p = require('path'); console.log(path.resolve(process.cwd(), '../binaries/darwin_x64'))")
    cp module.so $distpath/selva.so
    popd
fi

pushd selva; make clean; popd
docker build -t selva-server-build-linux .
distpath=$(node -e "const p = require('path'); console.log(path.resolve(process.cwd(), './binaries/linux_x64'))")
echo "DIST |$distpath|"
docker run -e SELVA_IS_DOCKER=1 -e DOCKER_USER=$(stat -c '%u' ./build.sh) -e DOCKER_GROUP=$(stat -c '%g' ./build.sh) -v "$distpath:/dist" -v "$distpath/locale:/locale" selva-server-build-linux
