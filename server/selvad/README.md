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
- `check` - Run `cppcheck`
