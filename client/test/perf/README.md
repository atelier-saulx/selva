Selva Performance Tests
=======================

Prerequisites
-------------

- `gcc` or `clang`
- `make` >= 3.81
- `libcrypto` and `libssl`
- `nodejs` >= v14
- `yarn`
- Optional: `Docker`

Start by building everything from the root directory of this repository:

```
$ yarn clean && yarn install && yarn build
$ pushd ./server/modules/selva
$ make clean && make && cp module.so ../binaries/<darwin_x64 OR linux_x64>/selva.so
$ popd
```

The `redis-server`, some libraries, and locales are provided in the repository
for both Darwin and Linux but can be rebuilt by running `yarn build-redis` and
`yarn build-cx` if necessary. The latter command requires Docker.

Running the Performance Tests
-----------------------------

This directory contains a number of performance evaluation tests, that can be
used either as a standalone reference points or with various profiling tools,
to gather data about the bottlenecks in the database.

The tests can be executed using the `yarn test` command. E.g.:

```
yarn test perf/indexing.ts
```

**indexing.ts**

The `indexing.ts` test is currently the main suite for evaluating general
indexing performance. The test includes code that generates a realistic looking
graph full of data. In contrast to other performance tests this one also writes
the test results into a `csv` file that can be analyzed using standard tools.
The output of this tests is an array showing how the growth of the graph affects
a query that's tries to resemble something that would be seen in a real production
environment. The query is executed several times with and without indexing for each
generated graph.
