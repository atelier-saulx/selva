/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include "jemalloc.h"
#include "util/auto_free.h"

void _wrap_selva_free(void *p) {
    void **pp = (void **)p;

    selva_free(*pp);
}
