/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include "jemalloc.h"
#include "util/poptop.h"

/**
 * Struct used for the return value of poptop_find().
 */
struct poptop_loc {
    struct poptop_list_el *found;
    struct poptop_list_el *next_free;
};

int poptop_init(struct poptop *l, unsigned int max_size, float initial_cut) {
    l->max_size = max_size;
    l->current_size = 0;
    l->cut_limit = initial_cut;
    l->list = selva_calloc(max_size, sizeof(struct poptop_list_el));

    return !l->list;
}

void poptop_deinit(struct poptop *l) {
    selva_free(l->list);
    memset(l, 0, sizeof(*l));
}

static struct poptop_loc poptop_find(struct poptop * restrict l, const void * restrict p) {
    const typeof(l->max_size) n = l->max_size;
    struct poptop_list_el *list = l->list;
    struct poptop_list_el *found = NULL;
    struct poptop_list_el *next_free = NULL;

    for (unsigned int i = 0; i < n; i++) {
        struct poptop_list_el *el = &list[i];

        if (!el->p && !next_free) {
            next_free = el;
            if (!p) {
                break;
            }
        } else if (el->p == p) {
            found = el;
            break;
        }
    }

    return (struct poptop_loc){
        .found = found,
        .next_free = next_free,
    };
}

void poptop_maybe_add(struct poptop * restrict l, float score, void * restrict p) {
    struct poptop_loc loc = poptop_find(l, p);

    if (loc.found) {
        /* Already inserted. */
        loc.found->score = score;
    } else if (loc.next_free && score >= l->cut_limit) {
        loc.next_free->score = score;
        loc.next_free->p = p;
        l->current_size++;
    }
}

void poptop_remove(struct poptop * restrict l, const void * restrict p) {
    struct poptop_loc loc = poptop_find(l, p);

    if (loc.found) {
        loc.found->p = NULL;
        l->current_size--;
    }
}

static int poptop_list_el_compare(const void *a, const void *b) {
    const struct poptop_list_el *el_a = (const struct poptop_list_el *)a;
    const struct poptop_list_el *el_b = (const struct poptop_list_el *)b;

    if (!el_a->p && !el_b->p) {
        return 0;
    } else if (!el_a->p) {
        return 1;
    } else if (!el_b->p) {
        return -1;
    } else {
        return round(el_a->score - el_b->score);
    }
}

/**
 * Sort the poptop list in l.
 */
static inline void poptop_sort(struct poptop *l) {
    qsort(l->list, l->max_size, sizeof(*l->list), poptop_list_el_compare);
}

/**
 * Get the median score from a sorted poptop list.
 */
static float poptop_median_score(const struct poptop *l, unsigned int last) {
    float median;

    if (last & 1) { /* The number of elements (last + 1) is even. */
        unsigned int i = (last + 1) >> 1;

        median = (l->list[i].score + l->list[i - 1].score) / 2.0f;
    } else { /* The number of elements is odd. */
        median = l->list[last >> 1].score;
    }

    return median;
}

int poptop_maintenance(struct poptop *l) {
    if (l->current_size == l->max_size) {
        poptop_sort(l);
        l->cut_limit = poptop_median_score(l, l->current_size - 1);
        return 1;
    } else {
        return 0;
    }
}

void *poptop_maintenance_drop(struct poptop *l) {
    struct poptop_list_el *list = l->list;
    const typeof(l->max_size) n = l->max_size;
    float cut_limit = l->cut_limit;

    for (unsigned int i = 0; i < n; i++) {
        struct poptop_list_el *el = &list[i];

        if (el->p && el->score < cut_limit) {
            void *p;

            p = el->p;
            el->p = NULL;
            l->current_size--;

            return p;
        }
    }

    return NULL;
}
