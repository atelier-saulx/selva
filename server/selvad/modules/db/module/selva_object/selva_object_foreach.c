/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "util/svector.h"
#include "selva_db.h"
#include "selva_error.h"
#include "selva_object.h"

int SelvaObject_ArrayForeach(
        struct SelvaObject *obj,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectArrayForeachCallback *cb) {
    enum SelvaObjectType subtype;
    SVector *vec;
    int err;

    err = SelvaObject_GetArrayStr(obj, field_str, field_len, &subtype, &vec);
    if (err) {
        return err;
    }

    if (subtype == SELVA_OBJECT_DOUBLE) {
        struct SVectorIterator it;

        SVector_ForeachBegin(&it, vec);
        do {
            const void *p;
            union SelvaObjectArrayForeachValue v;

            p = SVector_Foreach(&it);
            memcpy(&v.d, p, sizeof(v.d));

            if (cb->cb(v, subtype, cb->cb_arg)) {
                break;
            }

        } while (!SVector_Done(&it));
    } else if (subtype == SELVA_OBJECT_LONGLONG) {
        struct SVectorIterator it;

        SVector_ForeachBegin(&it, vec);
        do {
            const void *p;
            union SelvaObjectArrayForeachValue v;

            p = SVector_Foreach(&it);
            memcpy(&v.ll, p, sizeof(v.ll));

            if (cb->cb(v, subtype, cb->cb_arg)) {
                break;
            }

        } while (!SVector_Done(&it));
    } else if (subtype == SELVA_OBJECT_STRING) {
        struct SVectorIterator it;

        SVector_ForeachBegin(&it, vec);
        do {
            union SelvaObjectArrayForeachValue v;

            v.s = SVector_Foreach(&it);

            if (cb->cb(v, subtype, cb->cb_arg)) {
                break;
            }

        } while (!SVector_Done(&it));
    } else if (subtype == SELVA_OBJECT_OBJECT) {
        struct SVectorIterator it;

        SVector_ForeachBegin(&it, vec);
        do {
            union SelvaObjectArrayForeachValue v;

            v.obj = SVector_Foreach(&it);

            if (cb->cb(v, subtype, cb->cb_arg)) {
                break;
            }

        } while (!SVector_Done(&it));
    } else {
        /* subtype not supported. */
        return SELVA_ENOTSUP;
    }

    return 0;
}

int SelvaObject_SetForeach(
        struct SelvaObject *obj,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectSetForeachCallback *cb) {
    struct SelvaSet *set;
    enum SelvaSetType type;

    set = SelvaObject_GetSetStr(obj, field_str, field_len);
    if (!set) {
        return SELVA_ENOENT;
    }

    type = set->type;
    if (type == SELVA_SET_TYPE_STRING) {
        struct SelvaSetElement *el;

        SELVA_SET_STRING_FOREACH(el, set) {
            union SelvaObjectSetForeachValue value = { .s = el->value_string };

            if (cb->cb(value, type, cb->cb_arg)) {
                break;
            }
        }
    } else if (type == SELVA_SET_TYPE_DOUBLE) {
        struct SelvaSetElement *el;

        SELVA_SET_DOUBLE_FOREACH(el, set) {
            union SelvaObjectSetForeachValue value = { .d = el->value_d };

            if (cb->cb(value, type, cb->cb_arg)) {
                break;
            }
        }
    } else if (type == SELVA_SET_TYPE_LONGLONG) {
        struct SelvaSetElement *el;

        SELVA_SET_LONGLONG_FOREACH(el, set) {
            union SelvaObjectSetForeachValue value = { .ll = el->value_ll };

            if (cb->cb(value, type, cb->cb_arg)) {
                break;
            }
        }
    } else if (type == SELVA_SET_TYPE_NODEID) {
        struct SelvaSetElement *el;

        SELVA_SET_NODEID_FOREACH(el, set) {
            union SelvaObjectSetForeachValue value;

            memcpy(&value.node_id, el->value_nodeId, SELVA_NODE_ID_SIZE);
            if (cb->cb(value, type, cb->cb_arg)) {
                break;
            }
        }
    } else {
        return SELVA_ENOTSUP;
    }

    return 0;
}
