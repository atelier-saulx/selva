Selvad
======

Directory Structure
-------------------

- `src/` contains sources for the main executable (event loop and module loader)
- `modules/` contains sources for loadable modules (selva)
- `lib/` contains libraries that can be used in modules (util, deflate, jmalloc)

Build Goals
-----------

The project build uses `make`.

**Targets:**
- `all` - Builds all targets
- `selvad` - Builds the main executable `selvad`
- `lib` - Builds all libraries
- `modules` - Builds all loadable modules 

**Phony targets:**
- `clean` - Cleans the build results
- `mostlyclean` - Refrain from deleting libraries
- `check` - Run `cppcheck`

Running the Server
------------------

```
./selvad
```

**Environment Variables**

| Name |                                | Description               |
|---------------------------------------|---------------------------|
`REDIS_ADDR`                            | for publishing events.    |
`REDIS_PORT`                            | for publishing events.    |
`DEBUG_MODIFY_REPLICATION_DELAY_NS`     |                           |
`HIERARCHY_INITIAL_VECTOR_LEN`          |                           |
`HIERARCHY_EXPECTED_RESP_LEN`           |                           |
`HIERARCHY_COMPRESSION_LEVEL`           |                           |
`HIERARCHY_AUTO_COMPRESS_PERIOD_MS`     |                           |
`HIERARCHY_AUTO_COMPRESS_OLD_AGE_LIM`   |                           |
`FIND_INDICES_MAX`                      |                           |
`FIND_INDEXING_THRESHOLD`               |                           |
`FIND_INDEXING_ICB_UPDATE_INTERVAL`     |                           |
`FIND_INDEXING_INTERVAL`                |                           |
`FIND_INDEXING_POPULARITY_AVE_PERIOD`   |                           |


API
---

### Globally available symbols

Module symbols are never shared between the main program or the modules by
default and the only way to access functions and global variables from modules
and the `event_loop` API is by using the import macros from `module.h` (which
in turn does the right tricks with `dlfcn.h`).

For convenience and to be still compatible with standard C programming workflows
the libc and jmalloc functions are always made available in the global namespace
by the dynamic linker.

### Importing module API functions

```c
#include "module.h"
/* TODO #include required headers */

/* ... */

/*
 * The best practice is to declare all imports in an IMPORT() block at the end
 * of the file (before the __constructor function of the module).
 */
IMPORT() {
    evl_import(func, "mod"); /* Import func from "mod" */
    evl_import_main(selva_log); /* Import a single function from the main program. */
    evl_import_event_loop(); /* Some headers have a helper to import everything at once. */
}
```

### Async IO

- Timers: [demo\_timeout](modules/demo_timeout)
- Promises (async-await): [demo\_await](modules/demo_await)
- Async file IO: [demo\_sock](modules/demo_sock)

Selva Protocol
--------------

The server implementation of `selva_proto` is located in the
[server](modules/server) module. The protocol is documented in
[doc/selva\_proto.md](doc/selva_proto.md).
