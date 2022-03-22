#include "hierarchy.h"
#include "selva_object.h"
#include "selva_set.h"
#include "selva_set_ops.h"

struct set_has_cb {
    /**
     * Type of the compared element.
     * SELVA_SET_TYPE_LONGLONG is represented with SELVA_SET_TYPE_DOUBLE.
     */
    enum SelvaSetType type;
    int found; /*!< Have we found it yet? */
    union {
        struct {
            const char *buf;
            size_t len;
        } str; /*!< SELVA_SET_TYPE_RMSTRING and SELVA_SET_TYPE_NODEID. */
        double d; /*!< SELVA_SET_TYPE_LONGLONG and SELVA_SET_TYPE_DOUBLE. */
    };
};

int set_has_cb(union SelvaObjectSetForeachValue value, enum SelvaSetType type, void *arg) {
    struct set_has_cb *data = (struct set_has_cb *)arg;

    if (type == SELVA_SET_TYPE_RMSTRING) {
        size_t len;
        const char *str = RedisModule_StringPtrLen(value.rms, &len);

        if (data->type != SELVA_SET_TYPE_RMSTRING && data->type != SELVA_SET_TYPE_NODEID) {
            return 1; /* Type mismatch. */
        }

        if (data->str.len == len && !memcmp(data->str.buf, str, len)) {
            return (data->found = 1);
        }
    } else if (type == SELVA_SET_TYPE_DOUBLE) {
        if (data->type != SELVA_SET_TYPE_DOUBLE) {
            return 1; /* Type mismatch. */
        }

        if (data->d == value.d) {
            return (data->found = 1);
        }
    } else if (type == SELVA_SET_TYPE_LONGLONG) {
        if (data->type != SELVA_SET_TYPE_DOUBLE) {
            return 1; /* Type mismatch. */
        }

        if (data->d == (double)value.ll) {
            return (data->found = 1);
        }
    } else if (type == SELVA_SET_TYPE_NODEID) {
        Selva_NodeId node_id;

        if ((data->type != SELVA_SET_TYPE_RMSTRING && data->type != SELVA_SET_TYPE_NODEID) ||
            data->str.len > SELVA_NODE_ID_SIZE) {
            return 1; /* Type mismatch. */
        }

        memset(node_id, '\0', SELVA_NODE_ID_SIZE);
        memcpy(node_id, data->str.buf, data->str.len);

        if (!memcmp(node_id, value.node_id, SELVA_NODE_ID_SIZE)) {
            return (data->found = 1);
        }
    }

    return 0;
}

int SelvaSet_field_has_string(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        const char *value_str,
        size_t value_len) {
    struct set_has_cb data = {
        .type = SELVA_SET_TYPE_RMSTRING,
        .found = 0,
        .str.buf = value_str,
        .str.len = value_len,
    };
    struct SelvaObjectSetForeachCallback cb = {
        .cb = set_has_cb,
        .cb_arg = &data,
    };
    int err;

    err = SelvaHierarchy_ForeachInField(hierarchy, node, field_str, field_len, &cb);

    /* TODO Errors? */
    return err ? 0 : !!data.found;
}

int SelvaSet_field_has_double(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        double value) {
    struct set_has_cb data = {
        .type = SELVA_SET_TYPE_DOUBLE,
        .found = 0,
        .d = value,
    };
    struct SelvaObjectSetForeachCallback cb = {
        .cb = set_has_cb,
        .cb_arg = &data,
    };
    int err;

    err = SelvaHierarchy_ForeachInField(hierarchy, node, field_str, field_len, &cb);

    /* TODO Errors? */
    return err ? 0 : !!data.found;
}

int SelvaSet_field_has_longlong(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        long long value) {
    struct set_has_cb data = {
        .type = SELVA_SET_TYPE_DOUBLE, /* We emulate long long with a double for RPN compat. */
        .found = 0,
        .d = (double)value, /* TODO ?? */
    };
    struct SelvaObjectSetForeachCallback cb = {
        .cb = set_has_cb,
        .cb_arg = &data,
    };
    int err;

    err = SelvaHierarchy_ForeachInField(hierarchy, node, field_str, field_len, &cb);

    /* TODO Errors? */
    return err ? 0 : !!data.found;
}
