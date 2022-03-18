#include <stddef.h>
#include "hierarchy.h"
#include "selva_object.h"
#include "selva_set.h"
#include "selva_set_ops.h"

struct sosfv_in_set {
    struct SelvaSet *set;
    size_t found; /*! How many elements we have found from set. */
};

static int sosfv_in_set(union SelvaObjectSetForeachValue value, enum SelvaSetType type, void *arg) {
    struct sosfv_in_set *data = (struct sosfv_in_set *)arg;

    /*
     * Let n be the cardinality of seta. If we can find n elements from fieldb that
     * are also in seta, then seta is a subset of fieldb. This might result an
     * invalid result if the field is an array.
     */
    switch (type) {
    case SELVA_SET_TYPE_RMSTRING:
        data->found += SelvaSet_Has(data->set, value.rms);
        break;
    case SELVA_SET_TYPE_DOUBLE:
        data->found += SelvaSet_Has(data->set, value.d);
        break;
    case SELVA_SET_TYPE_LONGLONG:
        data->found += SelvaSet_Has(data->set, value.ll);
        break;
    case SELVA_SET_TYPE_NODEID:
        data->found += SelvaSet_Has(data->set, value.node_id);
        break;
    case SELVA_SET_NR_TYPES:
        /* Invalid type? */
        break;
    }

    return 0;
}

int SelvaSet_seta_in_fieldb(
        struct SelvaSet *a,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_b_str,
        size_t field_b_len) {
    struct sosfv_in_set data = {
        .found = 0,
        .set = a,
    };
    struct SelvaObjectSetForeachCallback cb = {
        .cb = sosfv_in_set,
        .cb_arg = &data,
    };
    int err;

    err = SelvaHierarchy_ForeachInField(hierarchy, node, field_b_str, field_b_len, &cb);
    /* TODO Handle errors? */

    return err ? 0 : data.found == SelvaSet_Size(a);
}
