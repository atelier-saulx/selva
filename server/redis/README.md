README
======

Building
--------

```
./build.sh
```

*Manually:*

```
$ git am <SOMEWHERE>/selva/server/redis/0001-Add-RM_ReplicateVerbatimArgs.patch
$ make PROG_SUFFIX="-selva"
$ cp src/redis-server-selva <SOMEWHERE>/selva/server/modules/binaries/darwin_x64/
```
