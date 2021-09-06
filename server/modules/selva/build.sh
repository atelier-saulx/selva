#!/usr/bin/env bash
set -e
make clean
make
distpath=$(node -e "const p = require('path'); console.log(path.resolve(process.cwd(), '../binaries/darwin_x64'))")
cp module.so $distpath/selva.so

make clean
docker build -t selva-server-build-linux .
distpath=$(node -e "const p = require('path'); console.log(path.resolve(process.cwd(), '../binaries/linux_x64'))")
docker run -e SELVA_IS_DOCKER=1 -v $distpath:/dist selva-server-build-linux

