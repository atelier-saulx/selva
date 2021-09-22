#!/usr/bin/env bash
set -e
docker build -t selva-redis-build-linux .
distpath=$(node -e "const p = require('path'); console.log(path.resolve(process.cwd(), '../modules/binaries/linux_x64'))")
docker run -v $distpath:/dist selva-redis-build-linux
