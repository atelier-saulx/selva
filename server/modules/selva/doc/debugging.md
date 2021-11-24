# Debugging

## Starting the Server Manually

**On Linux**:

```
LD_LIBRARY_PATH=/usr/local/lib LOCPATH=/home/hbp/repos/selva/server/modules/binaries/linux_x64/locale ../binaries/linux_x64/redis-server-selva --loadmodule ./module.so FIND_INDEXING_INTERVAL 500 FIND_INDEXING_ICB_UPDATE_INTERVAL 100 FIND_INDICES_MAX 100
```

`LD_LIBRARY_PATH` is where your `hiredis` is supposed to be located.
`LOCPATH` is for loading our custom locales.

**On MacOS**:

```
../binaries/darwin_x64/redis-server-selva -- --loadmodule module.so
```

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
LD_LIBRARY_PATH=/usr/local/lib LOCPATH=/home/hbp/repos/selva/server/modules/binaries/linux_x64/locale gdb --args ../binaries/linux_x64/redis-server-selva --loadmodule ./module.so 'FIND_INDEXING_INTERVAL' '500' 'FIND_INDEXING_ICB_UPDATE_INTERVAL' '1000' 'FIND_INDICES_MAX' 100
```

**print-vector SYMBOL TYPE**

```gdb
(gdb) print-vector stack SelvaHierarchyNode
0: {id: "d\000\000\000\000\000\000\000\000", visit_stamp: 00, parents: { "c\000\000\000\000\000\000\000\000", }, children: { "e\000\000\000\000\000\000\000\000", }}
```

### GDB Tips

**NOTE:** On MacOS LLDB is the preferred debugger because it doesn't require
signing and it seems to have fewer bugs.

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

## LLDB

LLDB can attach to a running `redis-server-selva` process with the `attach`
command or it can start a new server. When using attach, remember to use
`cont` command instead of `run` after attaching, so that the process doesn't
restart from `main()` after attaching to it.

LLDB is partially compatible with GDB but many commands behave a bit differently
and some commands don't exist with the same syntax.
See the LLDB [Tutorial](https://lldb.llvm.org/use/tutorial.html).

On MacOS the server can be also started manually with `lldb as follows:

```
lldb ../binaries/darwin_x64/redis-server-selva -- --loadmodule module.so
```

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

[Download](https://software.intel.com/content/www/us/en/develop/tools/oneapi/components/vtune-profiler/download.html)
and install Intel VTune from here.

### Separate Collection and Analysis Manually

Run redis-server with `vtune`:

```
vtune -collect hotspots -call-stack-mode all --result-dir redis-$(date +"%Y%m%dT%H%M") redis-server --loadmodule ./module.so
```

Stop the server with CTRL-C when ready.

Open the `something.vtune` file that was created to the `redis-TIME` directory
with the VTune GUI.

### Remote Collection and Analysis

The remote execution requires that the `ssh` command on your local machine can
connect to the remote host without a password prompt. This can be achieved with
`ssh_agent` or by creating an ssh key without setting a password.

The path to the VTune binaries must be set in `$PATH`. Typically it's something
like `/opt/intel/vtune_profiler_2021/bin64`. VTune can be installed on the
remote by the local host over SSH, as long as the path you have selected is
writable by the user.

In the remote mode you can either provide a startup script for starting the test
or run everything manually and attach to the running `redis-server-selva`
process.

### Tracing

`selva_trace.h` provides a tracing API currently with Intel ITT that's
compatible with Intel VTune. The tracing system and none of the calls are
compiled into the code unless tracing is explicitly enabled during the build
time. This can be done by adding `SELVA_TRACE=1` as an argument to `make` when
building.

**Example**

```sh
make SELVA_TRACE=1 -j4
```

Finally to enable instrumentation while running the Redis server the
`INTEL_LIBITTNOTIFY64` env variable must be set.

**Example**

```sh
INTEL_LIBITTNOTIFY64=/opt/intel/oneapi/vtune/2021.7.1/lib64/runtime/libittnotify_collector.so yarn test test/perf/compression.ts
```

Now VTune can be attached to the server process and the analysis results will
have (optional) categorization by Task name. Also most of the analyses will show
some task statistics on the Summary tab.
