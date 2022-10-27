/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include "selva_log.h"
#include "module.h"

void *evl_load_module(const char *path)
{
    void *hndl;

    SELVA_LOG(SELVA_LOGL_INFO, "Loading module: \"%s\"", path);
    hndl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!hndl) {
        SELVA_LOG(SELVA_LOGL_ERR, "Loading module failed (\"%s\"): %s", path, dlerror());
        return NULL;
    }

    return hndl;
}
