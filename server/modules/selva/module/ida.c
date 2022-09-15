/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: (MIT WITH selva-exception) OR AGPL-3.0-only
 */
#include "jemalloc.h"
#include "bitmap.h"
#include "errors.h"
#include "ida.h"

struct ida {
    struct bitmap id_map;
};

struct ida *ida_init(ida_t max) {
    struct ida *ida;

    ida = selva_malloc(sizeof(struct ida) - sizeof_field(struct ida, id_map) + BITMAP_ALLOC_SIZE(max));
    ida->id_map.nbits = max;

    for (int i = 0; i < max; i++) {
        bitmap_set(&ida->id_map, i);
    }

    return ida;
}

void ida_destroy(struct ida *ida) {
    selva_free(ida);
}

ida_t ida_alloc(struct ida *ida) {
    ida_t next = bitmap_ffs(&ida->id_map);

    if (next < 0) {
        return SELVA_ENOBUFS;
    }

    bitmap_clear(&ida->id_map, next);

    return next;
}

void ida_free(struct ida *ida, ida_t id) {
    bitmap_set(&ida->id_map, id);
}
