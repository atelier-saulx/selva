#include <stddef.h>
#include "hierarchy.h"
#include "errors.h"

int SelvaHierarchy_IsNonEmptyField(const struct SelvaModify_HierarchyNode *node __unused, const char *field_str __unused, size_t field_len __unused) {
    return SELVA_ENOENT;
}

struct SelvaObject *SelvaHierarchy_GetNodeObject(const struct SelvaModify_HierarchyNode *node __unused) {
    return NULL;
}

