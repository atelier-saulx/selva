/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include "redismodule.h"
#include "jemalloc.h"
#include "auto_free.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void _wrap_RM_Free(void *p) {
    void **pp = (void **)p;

    RedisModule_Free(*pp);
}
#pragma GCC diagnostic pop

void _wrap_selva_free(void *p) {
    void **pp = (void **)p;

    selva_free(*pp);
}
