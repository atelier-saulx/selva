/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include "jemalloc.h"
#include "selva_error.h"
#include "util/selva_string.h"
#include "selva_db.h"
#include "selva_set.h"

int SelvaSet_CompareString(struct SelvaSetElement *a, struct SelvaSetElement *b) {
    struct selva_string *ra = a->value_string;
    struct selva_string *rb = b->value_string;
    TO_STR(ra, rb);

    if (ra_len < rb_len) {
        return -1;
    }
    if (ra_len > rb_len) {
        return 1;
    }
    return memcmp(ra_str, rb_str, ra_len);
}

int SelvaSet_CompareDouble(struct SelvaSetElement *a, struct SelvaSetElement *b) {
    double da = a->value_d;
    double db = b->value_d;

    return da < db ? -1 : da > db ? 1 : 0;
}

int SelvaSet_CompareLongLong(struct SelvaSetElement *a, struct SelvaSetElement *b) {
    long long lla = a->value_ll;
    long long llb = b->value_ll;

    return lla < llb ? -1 : lla > llb ? 1 : 0;
}

int SelvaSet_CompareNodeId(struct SelvaSetElement *a, struct SelvaSetElement *b) {
    return memcmp(a->value_nodeId, b->value_nodeId, SELVA_NODE_ID_SIZE);
}

RB_GENERATE(SelvaSetString, SelvaSetElement, _entry, SelvaSet_CompareString)
RB_GENERATE(SelvaSetDouble, SelvaSetElement, _entry, SelvaSet_CompareDouble)
RB_GENERATE(SelvaSetLongLong, SelvaSetElement, _entry, SelvaSet_CompareLongLong)
RB_GENERATE(SelvaSetNodeId, SelvaSetElement, _entry, SelvaSet_CompareNodeId)

int SelvaSet_AddString(struct SelvaSet *set, struct selva_string *s) {
    struct SelvaSetElement *el;

    if (set->type != SELVA_SET_TYPE_STRING) {
        return SELVA_EINTYPE;
    }

    if (SelvaSet_HasString(set, s)) {
        return SELVA_EEXIST;
    }

    el = selva_calloc(1, sizeof(struct SelvaSetElement));
    el->value_string = s;

    (void)RB_INSERT(SelvaSetString, &set->head_string, el);
    set->size++;

    return 0;
}

int SelvaSet_AddDouble(struct SelvaSet *set, double d) {
    struct SelvaSetElement *el;

    if (set->type != SELVA_SET_TYPE_DOUBLE) {
        return SELVA_EINTYPE;
    }

    if (isnan(d)) {
        return SELVA_EINVAL;
    }

    if (SelvaSet_HasDouble(set, d)) {
        return SELVA_EEXIST;
    }

    el = selva_calloc(1, sizeof(struct SelvaSetElement));
    el->value_d = d;

    (void)RB_INSERT(SelvaSetDouble, &set->head_d, el);
    set->size++;

    return 0;
}

int SelvaSet_AddLongLong(struct SelvaSet *set, long long ll) {
    struct SelvaSetElement *el;

    if (set->type != SELVA_SET_TYPE_LONGLONG) {
        return SELVA_EINTYPE;
    }

    if (SelvaSet_HasLongLong(set, ll)) {
        return SELVA_EEXIST;
    }

    el = selva_calloc(1, sizeof(struct SelvaSetElement));
    el->value_ll = ll;

    (void)RB_INSERT(SelvaSetLongLong, &set->head_ll, el);
    set->size++;

    return 0;
}

int SelvaSet_AddNodeId(struct SelvaSet *set, const Selva_NodeId node_id) {
    struct SelvaSetElement *el;

    if (set->type != SELVA_SET_TYPE_NODEID) {
        return SELVA_EINTYPE;
    }

    if (SelvaSet_HasNodeId(set, node_id)) {
        return SELVA_EEXIST;
    }

    el = selva_calloc(1, sizeof(struct SelvaSetElement));
    memcpy(el->value_nodeId, node_id, SELVA_NODE_ID_SIZE);

    (void)RB_INSERT(SelvaSetNodeId, &set->head_nodeId, el);
    set->size++;

    return 0;
}

void SelvaSet_DestroyElement(struct SelvaSetElement *el) {
    if (!el) {
        return;
    }

    selva_free(el);
}

static void SelvaSet_DestroyString(struct SelvaSet *set) {
    struct SelvaSetString *head = &set->head_string;
    struct SelvaSetElement *el;
    struct SelvaSetElement *next;

    for (el = RB_MIN(SelvaSetString, head); el != NULL; el = next) {
        next = RB_NEXT(SelvaSetString, head, el);
        RB_REMOVE(SelvaSetString, head, el);

        selva_string_free(el->value_string);
        SelvaSet_DestroyElement(el);
    }
    set->size = 0;
}

static void SelvaSet_DestroyDouble(struct SelvaSet *set) {
    struct SelvaSetDouble *head = &set->head_d;
    struct SelvaSetElement *el;
    struct SelvaSetElement *next;

    for (el = RB_MIN(SelvaSetDouble, head); el != NULL; el = next) {
        next = RB_NEXT(SelvaSetDouble, head, el);
        RB_REMOVE(SelvaSetDouble, head, el);

        SelvaSet_DestroyElement(el);
    }
    set->size = 0;
}

static void SelvaSet_DestroyLongLong(struct SelvaSet *set) {
    struct SelvaSetLongLong *head = &set->head_ll;
    struct SelvaSetElement *el;
    struct SelvaSetElement *next;

    for (el = RB_MIN(SelvaSetLongLong, head); el != NULL; el = next) {
        next = RB_NEXT(SelvaSetLongLong, head, el);
        RB_REMOVE(SelvaSetLongLong, head, el);

        SelvaSet_DestroyElement(el);
    }
    set->size = 0;
}

static void SelvaSet_DestroyNodeId(struct SelvaSet *set) {
    struct SelvaSetNodeId *head = &set->head_nodeId;
    struct SelvaSetElement *el;
    struct SelvaSetElement *next;

    for (el = RB_MIN(SelvaSetNodeId, head); el != NULL; el = next) {
        next = RB_NEXT(SelvaSetNodeId, head, el);
        RB_REMOVE(SelvaSetNodeId, head, el);

        SelvaSet_DestroyElement(el);
    }
    set->size = 0;
}

static void (*const SelvaSet_Destructors[])(struct SelvaSet *set) = {
    [SELVA_SET_TYPE_STRING] = SelvaSet_DestroyString,
    [SELVA_SET_TYPE_DOUBLE] = SelvaSet_DestroyDouble,
    [SELVA_SET_TYPE_LONGLONG] = SelvaSet_DestroyLongLong,
    [SELVA_SET_TYPE_NODEID] = SelvaSet_DestroyNodeId,
};

void SelvaSet_Destroy(struct SelvaSet *set) {
    enum SelvaSetType type = set->type;

    if (type >= 0 && type < num_elem(SelvaSet_Destructors)) {
        SelvaSet_Destructors[type](set);
    }
}

struct selva_string *SelvaSet_FindString(struct SelvaSet *set, struct selva_string *s) {
    struct SelvaSetElement find = {
        .value_string = s,
    };
    struct selva_string *res = NULL;

    if (likely(set->type == SELVA_SET_TYPE_STRING)) {
        struct SelvaSetElement *el;

        el = RB_FIND(SelvaSetString, &set->head_string, &find);
        if (el) {
            res = el->value_string;
        }
    }

    return res;
}

int SelvaSet_HasString(struct SelvaSet *set, struct selva_string *s) {
    struct SelvaSetElement find = {
        .value_string = s,
    };

    if (unlikely(set->type != SELVA_SET_TYPE_STRING)) {
        return 0;
    }

    return !!RB_FIND(SelvaSetString, &set->head_string, &find);
}

int SelvaSet_HasDouble(struct SelvaSet *set, double d) {
    struct SelvaSetElement find = {
        .value_d = d,
    };

    if (unlikely(set->type != SELVA_SET_TYPE_DOUBLE)) {
        return 0;
    }

    if (isnan(d)) {
        return 0;
    }

    return !!RB_FIND(SelvaSetDouble, &set->head_d, &find);
}

int SelvaSet_HasLongLong(struct SelvaSet *set, long long ll) {
    struct SelvaSetElement find = {
        .value_ll = ll,
    };

    if (unlikely(set->type != SELVA_SET_TYPE_LONGLONG)) {
        return 0;
    }

    return !!RB_FIND(SelvaSetLongLong, &set->head_ll, &find);
}

int SelvaSet_HasNodeId(struct SelvaSet *set, const Selva_NodeId node_id) {
    struct SelvaSetElement find = { 0 };

    memcpy(find.value_nodeId, node_id, SELVA_NODE_ID_SIZE);

    if (unlikely(set->type != SELVA_SET_TYPE_NODEID)) {
        return 0;
    }

    return !!RB_FIND(SelvaSetNodeId, &set->head_nodeId, &find);
}

struct SelvaSetElement *SelvaSet_RemoveString(struct SelvaSet *set, struct selva_string *s) {
    struct SelvaSetElement find = {
        .value_string = s,
    };
    struct SelvaSetElement *el = NULL;

    if (likely(set->type == SELVA_SET_TYPE_STRING)) {
        el = RB_FIND(SelvaSetString, &set->head_string, &find);
        if (el && RB_REMOVE(SelvaSetString, &set->head_string, el)) {
            set->size--;
        }
    }

    return el;
}

struct SelvaSetElement *SelvaSet_RemoveDouble(struct SelvaSet *set, double d) {
    struct SelvaSetElement find = {
        .value_d = d,
    };
    struct SelvaSetElement *el = NULL;

    if (likely(set->type == SELVA_SET_TYPE_DOUBLE)) {
        el = RB_FIND(SelvaSetDouble, &set->head_d, &find);
        if (el && RB_REMOVE(SelvaSetDouble, &set->head_d, el)) {
            set->size--;
        }
    }

    return el;
}

struct SelvaSetElement *SelvaSet_RemoveLongLong(struct SelvaSet *set, long long ll) {
    struct SelvaSetElement find = {
        .value_ll = ll,
    };
    struct SelvaSetElement *el = NULL;

    if (likely(set->type == SELVA_SET_TYPE_LONGLONG)) {
        el = RB_FIND(SelvaSetLongLong, &set->head_ll, &find);
        if (el && RB_REMOVE(SelvaSetLongLong, &set->head_ll, el)) {
            set->size--;
        }
    }

    return el;
}

struct SelvaSetElement *SelvaSet_RemoveNodeId(struct SelvaSet *set, const Selva_NodeId node_id) {
    struct SelvaSetElement find = { 0 };
    struct SelvaSetElement *el = NULL;

    memcpy(find.value_nodeId, node_id, SELVA_NODE_ID_SIZE);

    if (likely(set->type == SELVA_SET_TYPE_NODEID)) {
        el = RB_FIND(SelvaSetNodeId, &set->head_nodeId, &find);
        if (el && RB_REMOVE(SelvaSetNodeId, &set->head_nodeId, el)) {
            set->size--;
        }
    }

    return el;
}

int SelvaSet_Merge(struct SelvaSet *dst, struct SelvaSet *src) {
    enum SelvaSetType type = src->type;
    struct SelvaSetElement *tmp;
    struct SelvaSetElement *el;

    if (type != dst->type) {
        return SELVA_EINTYPE;
    }

    if (type == SELVA_SET_TYPE_STRING) {
        SELVA_SET_STRING_FOREACH_SAFE(el, src, tmp) {
            if (!SelvaSet_Has(dst, el->value_string)) {
                RB_REMOVE(SelvaSetString, &src->head_string, el);
                src->size--;
                RB_INSERT(SelvaSetString, &dst->head_string, el);
                dst->size++;
            }
        }
    } else if (type == SELVA_SET_TYPE_DOUBLE) {
        SELVA_SET_DOUBLE_FOREACH_SAFE(el, src, tmp) {
            if (!SelvaSet_Has(dst, el->value_d)) {
                RB_REMOVE(SelvaSetDouble, &src->head_d, el);
                src->size--;
                RB_INSERT(SelvaSetDouble, &dst->head_d, el);
                dst->size++;
            }
        }
    } else if (type == SELVA_SET_TYPE_LONGLONG) {
        SELVA_SET_LONGLONG_FOREACH_SAFE(el, src, tmp) {
            if (!SelvaSet_Has(dst, el->value_ll)) {
                RB_REMOVE(SelvaSetLongLong, &src->head_ll, el);
                src->size--;
                RB_INSERT(SelvaSetLongLong, &dst->head_ll, el);
                dst->size++;
            }
        }
    } else if (type == SELVA_SET_TYPE_NODEID) {
        SELVA_SET_NODEID_FOREACH_SAFE(el, src, tmp) {
            if (!SelvaSet_Has(dst, el->value_nodeId)) {
                RB_REMOVE(SelvaSetNodeId, &src->head_nodeId, el);
                src->size--;
                RB_INSERT(SelvaSetNodeId, &dst->head_nodeId, el);
                dst->size++;
            }
        }
    }

    return 0;
}

int SelvaSet_Union(struct SelvaSet *res, ...) {
    const enum SelvaSetType type = res->type;
    va_list argp;
    int err = 0;

    va_start(argp, res);

    /*
     * We only accept empty set for the result set.
     */
    if (!res || res->size > 0) {
        err = SELVA_EINVAL;
        goto out;
    }

    if (type == SELVA_SET_TYPE_STRING) {
        struct SelvaSet *set;

        while ((set = va_arg(argp, struct SelvaSet *))) {
            struct SelvaSetElement *el;

            if (set->type != type) {
                continue;
            }

            SELVA_SET_STRING_FOREACH(el, set) {
                struct selva_string *string;

                string = selva_string_dup(el->value_string, selva_string_get_flags(el->value_string));
                err = SelvaSet_Add(res, string);
                if (err) {
                    selva_string_free(string);
                    goto out;
                }
            }
        }
    } else if (type == SELVA_SET_TYPE_DOUBLE) {
        struct SelvaSet *set;

        while ((set = va_arg(argp, struct SelvaSet *))) {
            struct SelvaSetElement *el;

            if (set->type != type) {
                continue;
            }

            SELVA_SET_DOUBLE_FOREACH(el, set) {
                err = SelvaSet_Add(res, el->value_d);
                if (err) {
                    goto out;
                }
            }
        }
    } else if (type == SELVA_SET_TYPE_LONGLONG) {
        struct SelvaSet *set;

        while ((set = va_arg(argp, struct SelvaSet *))) {
            struct SelvaSetElement *el;

            if (set->type != type) {
                continue;
            }

            SELVA_SET_LONGLONG_FOREACH(el, set) {
                err = SelvaSet_Add(res, el->value_ll);
                if (err) {
                    goto out;
                }
            }
        }
    } else if (type == SELVA_SET_TYPE_NODEID) {
        struct SelvaSet *set;

        while ((set = va_arg(argp, struct SelvaSet *))) {
            struct SelvaSetElement *el;

            if (set->type != type) {
                continue;
            }

            SELVA_SET_NODEID_FOREACH(el, set) {
                err = SelvaSet_Add(res, el->value_nodeId);
                if (err) {
                    goto out;
                }
            }
        }
    }

out:
    va_end(argp);
    return err;
}
