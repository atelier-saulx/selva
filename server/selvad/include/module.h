/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdio.h>
#include <dlfcn.h>

#include "_evl_export.h"

struct evl_module_info {
    char name[80];
    void *hndl;
    /*
     * SLIST_ENTRY() would do the following but this way we avoid to include
     * the whole header.
     */
    struct {
        struct evl_module_info *sle_next;
    } entries;
};

/**
 * Load a module.
 * @returns A handle to the module if it was was loaded successfully.
 */
EVL_EXPORT(void *, evl_load_module, const char *path);

EVL_EXPORT(const struct evl_module_info *, evl_get_next_module, const struct evl_module_info *mod);

[[nodiscard]]
static inline void *_evl_import(const char *what, const char *from) {
    void *_ref;
    void *p = (void *)0;

    /*
     * Note that dlopen() is refcounting the libary and if we want to unload the
     * loaded libary we should call dlclose() equal number of times to unload.
     */
    _ref = dlopen(from, RTLD_NOW | RTLD_LOCAL);
    if (!_ref) {
        fprintf(stderr, "Module \"%s\" not loaded\n", from ? from : "main");
    } else {
        dlerror();
        p = dlsym(_ref, what);
        if (!p) {
            fprintf(stderr, "Failed to load a symbol (\"%s\"): %s\n", what, dlerror());
        }
    }

    return p;
}

/**
 * Import a symbol `what` from the `from` module.
 * @param from If NULL then the symbol is loaded from main.
 */
#define evl_import(what, from) do { \
    what = ((what) ? what : (typeof(what))_evl_import(#what, from)); \
    if (!what) __builtin_trap(); \
} while (0)

/**
 * Import a symbol from main.
 */
#define evl_import_main(x) \
    evl_import(x, NULL);

/**
 * Import helper.
 * Do all imports using this macro to ensure that all symbols are imported
 * before they are needed in a module.
 */
#define IMPORT() __attribute__((constructor(101))) static void _imports(void)
