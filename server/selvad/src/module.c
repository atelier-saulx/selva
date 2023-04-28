/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include "jemalloc.h"
#include "queue.h"
#include "selva_log.h"
#include "module.h"

SLIST_HEAD(modules_list, evl_module_info) modules = SLIST_HEAD_INITIALIZER(modules);

void *evl_load_module(const char *path)
{
    struct evl_module_info *info;
    void *hndl;

    SELVA_LOG(SELVA_LOGL_INFO, "Loading module: \"%s\"", path);
    hndl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!hndl) {
        SELVA_LOG(SELVA_LOGL_ERR, "Loading module failed (\"%s\"): %s", path, dlerror());
        return NULL;
    }

    info = selva_malloc(sizeof(*info));
    snprintf(info->name, sizeof(info->name), "%s", path);
    info->hndl = hndl;
    SLIST_INSERT_HEAD(&modules, info, entries);

    return hndl;
}

const struct evl_module_info *evl_get_next_module(const struct evl_module_info *mod)
{
    return (mod) ? SLIST_NEXT(mod, entries) : SLIST_FIRST(&modules);
}
