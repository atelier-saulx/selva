Selva Redis Module
==================

Quick Start
-----------

Here's what you need to do to build your first module:

1. `make`
2. Run redis loading the module: `redis-server --loadmodule ./module.so`

Documentation
-------------

### Redis

| File                                 | Description                           |
|--------------------------------------|---------------------------------------|
| [API.md](doc/redis/API.md) | The official manual for writing Redis modules, copied from the Redis repo. Read this before starting, as it's more than an API reference. |
| [FUNCTIONS.md](doc/redis/FUNCTIONS.md) | Generated API reference documentation for both the Redis module API, and LibRMUtil. |
| [TYPES.md](doc/redis/TYPES.md) | Describes the API for creating new data structures inside Redis modules, copied from the Redis repo. |
| [BLOCK.md](doc/redis/BLOCK.md) | Describes the API for blocking a client while performing asynchronous tasks on a separate thread. |


Debugging
---------

### Dumping data using redis-cli

**hiearachy-dot-dump.js**

Dump a hierarchy object in dot format.

Dump the whole tree:

```
node hiearachy-dot-dump.js test
```

Limit the number of nodes traversed:

```
node hiearachy-dot-dump.js test 5
```

Dump the tree towards descendants starting from the node `a`:

```
node hiearachy-dot-dump.js test descendants a
```

Dump the three towards ancestors starting from the node `g`:

```
node hiearachy-dot-dump.js test ancestors g
```

Limit the number of nodes traversed:

```
node hiearachy-dot-dump.js test ancestors g 10
```

Render a png of a hierarchy:

```
node --max-old-space-size=4000 ./hiearachy-dot-dump.js test 300 | dot -Tpng > dump.png
```


### GDB

Start the server as follows:

```
gdb --args redis-server --loadmodule ./module.so
```

**print-vector SYMBOL TYPE**

```gdb
(gdb) print-vector stack SelvaModify_HierarchyNode
0: {id: "d\000\000\000\000\000\000\000\000", visit_stamp: 00, parents: { "c\000\000\000\000\000\000\000\000", }, children: { "e\000\000\000\000\000\000\000\000", }}
```


### Valgrind

**Profiling with Valgrind**

First start the server with Valgrind:

```
valgrind --tool=callgrind --simulate-cache=yes -s redis -server --loadmodule ./module.so
```

Then run something using the db, e.g. `ts-node index.ts` in `../../perftest/` dir.

Once you are done, kill the redis server (e.g. `CTRL-C`).
This will produce files named `callgrind.out.NUMBER`

Callgrind fiels can be parsed using `callgrind_annotate`:

```
callgrind_annotate --auto=yes callgrind.out.1953257
```

There are also a number of GUI tools for parsing callgrind files.
