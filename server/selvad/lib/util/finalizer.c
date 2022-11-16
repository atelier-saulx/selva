/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include "queue.h"
#include "jemalloc.h"
#include "util/finalizer.h"

void finalizer_init(struct finalizer *f)
{
    SLIST_INIT(&f->head);
}

void finalizer_add(struct finalizer *f, void *p, void (*dispose)(void *p))
{
    struct finalizer_item *item = selva_malloc(sizeof(struct finalizer_item));

    item->dispose = dispose;
    item->p = p;

    SLIST_INSERT_HEAD(&f->head, item, entries);
}

void finalizer_run(struct finalizer *f)
{
    struct finalizer_stack *head = &f->head;

    while (!SLIST_EMPTY(head)) {
        struct finalizer_item *item = SLIST_FIRST(head);

        item->dispose(item->p);

        SLIST_REMOVE_HEAD(head, entries);
        selva_free(item);
    }
}
