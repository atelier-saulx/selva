#include <stddef.h>
#include "redismodule.h"
#include "alias.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_node.h"
#include "selva_object.h"
#include "selva_set.h"

static const char * const excluded_fields[] = {
    SELVA_ID_FIELD,
    SELVA_CREATED_AT_FIELD,
    SELVA_ALIASES_FIELD,
    NULL
};

int SelvaNode_ClearFields(struct SelvaObject *obj) {
    SelvaObject_Clear(obj, excluded_fields);

    return 0;
}
