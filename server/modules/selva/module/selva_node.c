#include "redismodule.h"
#include "selva_node.h"
#include "selva_object.h"

/* TODO this could live somewhere else */

static const char * const excluded_fields[] = {
    SELVA_ID_FIELD,
    SELVA_TYPE_FIELD,
    SELVA_CREATED_AT_FIELD,
    SELVA_ALIASES_FIELD,
    NULL
};

int SelvaNode_ClearFields(struct SelvaObject *obj) {
    SelvaObject_Clear(obj, excluded_fields);

    return 0;
}
