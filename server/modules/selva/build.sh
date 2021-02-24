#!/usr/bin/env bash
set -e
docker build -t selva-server-build-linux .
distpath=$(node -e "const p = require('path'); console.log(path.resolve(process.cwd(), '../binaries/linux_x64'))")
docker run -v $distpath:/dist selva-server-build-linux

make clean
make
distpath=$(node -e "const p = require('path'); console.log(path.resolve(process.cwd(), '../binaries/darwin_x64'))")
cp module.so $distpath/selva.so
