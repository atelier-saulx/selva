/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <sys/types.h>
#include "selva_set.h"
#include "selva_set_ops.h"

/**
 * Test if the selva_String set a is a subset of the set b.
 */
static int issub_string(struct SelvaSet *a, struct SelvaSet *b) {
    struct SelvaSetElement *el;
    size_t found = 0;

    SELVA_SET_STRING_FOREACH(el, a) {
        found += SelvaSet_Has(b, el->value_string);
    }

    return found == SelvaSet_Size(a);
}

/**
 * Test if the double set a is a subset of the set b.
 */
static int issub_double(struct SelvaSet *a, struct SelvaSet *b) {
    struct SelvaSetElement *el;
    size_t found = 0;

    SELVA_SET_DOUBLE_FOREACH(el, a) {
        found += SelvaSet_Has(b, el->value_d);
    }

    return found == SelvaSet_Size(a);
}

/**
 * Test if the long long set a is a subset of the set b.
 */
static int issub_longlong(struct SelvaSet *a, struct SelvaSet *b) {
    struct SelvaSetElement *el;
    size_t found = 0;

    SELVA_SET_LONGLONG_FOREACH(el, a) {
        found += SelvaSet_Has(b, el->value_ll);
    }

    return found == SelvaSet_Size(a);
}

/**
 * Test if the Selva_NodeId set a is a subset of the set b.
 */
static int issub_nodeid(struct SelvaSet *a, struct SelvaSet *b) {
    struct SelvaSetElement *el;
    size_t found = 0;

    SELVA_SET_NODEID_FOREACH(el, a) {
        found += SelvaSet_Has(b, el->value_nodeId);
    }

    return found == SelvaSet_Size(a);
}

int SelvaSet_seta_in_setb(struct SelvaSet *a, struct SelvaSet *b) {
    if (a->type != b->type) {
        /* TODO Should we have double vs long long cross compat for RPN? */
        return 0;
    }

    switch (a->type) {
    case SELVA_SET_TYPE_STRING:
        return issub_string(a, b);
    case SELVA_SET_TYPE_DOUBLE:
        return issub_double(a, b);
    case SELVA_SET_TYPE_LONGLONG:
        return issub_longlong(a, b);
    case SELVA_SET_TYPE_NODEID:
        return issub_nodeid(a, b);
    case SELVA_SET_NR_TYPES:
        /* Invalid type? */
        return 0;
    }

    return 0;
}
