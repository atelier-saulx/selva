# Debugging

## Dumping a Hierarchy Using redis-cli

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

## GDB

The `.gdbinit` file in the module directory will load some useful helpers from
`./gdb` for the debugging session.

Start the server with GDB:

```
gdb --args redis-server --loadmodule ./module.so
```

**print-vector SYMBOL TYPE**

```gdb
(gdb) print-vector stack SelvaModify_HierarchyNode
0: {id: "d\000\000\000\000\000\000\000\000", visit_stamp: 00, parents: { "c\000\000\000\000\000\000\000\000", }, children: { "e\000\000\000\000\000\000\000\000", }}
```

### GDB Tips

**Watchpoints**

Sometimes there are no (more) HW watchpoints available and `watch` doesn't work.
Disable HW watchpoints to use soft watchpoints:

```gdb
set can-use-hw-watchpoints 0
```

**Stepping**

Quite often after setting a breakpoint you'll endup stepping into a long library
function that is not relevant for your debugging. You can step out from the
function by using the `finish` command (or `fini`).

```gdb
fini
```

- [Continuing and Stepping](https://sourceware.org/gdb/current/onlinedocs/gdb/Continuing-and-Stepping.html#Continuing-and-Stepping)

## Valgrind

**Profiling with Valgrind**

First start the server with Valgrind:

```
valgrind --tool=callgrind --simulate-cache=yes --suppressions=./valgrind.sup -s redis-server --loadmodule ./module.so
```

Then run something using the db, e.g. `ts-node index.ts` in `../../perftest/` dir.

Once you are done, kill the redis server (e.g. `CTRL-C`).
This will produce files named `callgrind.out.NUMBER`

Callgrind files can be parsed using `callgrind_annotate`:

```
callgrind_annotate --auto=yes callgrind.out.1953257
```

There are also a number of GUI tools for parsing callgrind files.

## Intel VTune

[Download](https://software.intel.com/content/www/us/en/develop/tools/vtune-profiler.html)
Intel VTune from here. The software requires a per user license that can be registered for free.
Install VTune according to its pre platform instructions.

### Separate Collection and Analysis

Run redis-server with `vtune`:

```
vtune -collect hotspots -call-stack-mode all --result-dir redis-$(date +"%Y%m%dT%H%M") redis-server --loadmodule ./module.so
```

Stop the server with CTRL-C when ready.

Open the `something.vtune` file that was created to the `redis-TIME` directory
with the VTune GUI.
