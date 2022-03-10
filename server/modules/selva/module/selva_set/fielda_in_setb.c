#include "hierarchy.h"
#include "selva_object.h"
#include "selva_set.h"
#include "selva_set_ops.h"

struct sosfv_in_set {
    struct SelvaSet *set;
    int valid; /*!< Is a valid subset. */
};

static int sosfv_in_set(union SelvaObjectSetForeachValue value, enum SelvaSetType type, void *arg) {
    struct sosfv_in_set *data = (struct sosfv_in_set *)arg;
    int has = 0;

    /*
     * If something from fielda is missing from setb then fielda isn't a subset
     * of setb.
     */
    switch (type) {
    case SELVA_SET_TYPE_RMSTRING:
        has = SelvaSet_Has(data->set, value.rms);
        break;
    case SELVA_SET_TYPE_DOUBLE:
        has = SelvaSet_Has(data->set, value.d);
        break;
    case SELVA_SET_TYPE_LONGLONG:
        has = SelvaSet_Has(data->set, value.ll);
        break;
    case SELVA_SET_TYPE_NODEID:
        has = SelvaSet_Has(data->set, value.node_id);
        break;
    case SELVA_SET_NR_TYPES:
        /* Invalid type? */
        break;
    }

    return !(data->valid = has);
}

int SelvaSet_fielda_in_setb(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_a_str,
        size_t field_a_len,
        struct SelvaSet *b) {
    struct sosfv_in_set data = {
        .set = b,
        .valid = 1,
    };
    struct SelvaObjectSetForeachCallback cb = {
        .cb = sosfv_in_set,
        .cb_arg = &data,
    };
    int err;

    err = SelvaHierarchy_ForeachInField(hierarchy, node, field_a_str, field_a_len, &cb);
    /* TODO Handle errors? */

    return err ? 0 : data.valid;
}
