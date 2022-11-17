/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "queue.h"

struct finalizer_item {
    void *p;
    void (*dispose)(void *p);
    SLIST_ENTRY(finalizer_item) entries;
};

SLIST_HEAD(finalizer_stack, finalizer_item);

struct finalizer {
    struct finalizer_stack head;
};

void finalizer_init(struct finalizer *f);
void finalizer_add(struct finalizer *f, void *p, void (*dispose)(void *p));
void finalizer_run(struct finalizer *f);
