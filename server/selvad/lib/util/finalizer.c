/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include "queue.h"
#include "jemalloc.h"
#include "util/finalizer.h"

void finalizer_init(struct finalizer *fin)
{
    SLIST_INIT(&fin->head);
}

void finalizer_add(struct finalizer *fin, void *p, void (*dispose)(void *p))
{
    struct finalizer_item *item = selva_malloc(sizeof(struct finalizer_item));

    item->dispose = dispose;
    item->p = p;

    SLIST_INSERT_HEAD(&fin->head, item, entries);
}

void finalizer_del(struct finalizer *fin, void *p) {
    struct finalizer_stack *head = &fin->head;
    struct finalizer_item *item;
    struct finalizer_item *item_tmp;

    SLIST_FOREACH_SAFE(item, head, entries, item_tmp) {
        if (item->p == p) {
            SLIST_REMOVE(head, item, finalizer_item, entries);
            selva_free(item);
            break;
        }
    }
}

void finalizer_run(struct finalizer *fin)
{
    struct finalizer_stack *head = &fin->head;

    while (!SLIST_EMPTY(head)) {
        struct finalizer_item *item = SLIST_FIRST(head);

        item->dispose(item->p);

        SLIST_REMOVE_HEAD(head, entries);
        selva_free(item);
    }
}

void _wrap_finalizer_run(void *p)
{
    finalizer_run((struct finalizer *)p);
}
