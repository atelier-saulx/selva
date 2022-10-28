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
- `lib/x` - Builds the library `x`
- `modules` - Builds all loadable modules 

**Phony targets:**
- `clean` - Cleans the build results
- `mostlyclean` - Refrain from deleting libraries
- `check` - Run `cppcheck`

API
---

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
