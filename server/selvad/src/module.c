/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include "module.h"

void *evl_load_module(const char *path)
{
    void *hndl;

    printf("Loading module: \"%s\"\n", path);
    hndl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!hndl) {
        fprintf(stderr, "Loading module failed (\"%s\"): %s\n", path, dlerror());
        return NULL;
    }

    return hndl;
}
