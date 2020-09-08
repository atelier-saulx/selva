#include <stddef.h>
#include <sys/types.h>
#include "async_task.h"
#include "cdefs.h"
#include "hierarchy.h"

static const char ancestors_field_str[] = "ancestors";
#define ANCESTORS_FIELD_LEN (sizeof(ancestors_field_str) - 1)
#define ANCESTORS_EVENT_PAYLOAD_LEN \
    (sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + ANCESTORS_FIELD_LEN)

static int sendAncestorEvent(Selva_NodeId id, __unused void *arg, __unused struct SelvaModify_HierarchyMetaData *metadata) {
    SelvaModify_PublishUpdate(id, ancestors_field_str, ANCESTORS_FIELD_LEN);

    return 0;
}

void SelvaModify_PublishDescendants(struct SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
    struct SelvaModify_HierarchyCallback cb = {
        .node_cb = sendAncestorEvent,
        .node_arg = NULL,
    };

    (void)SelvaModify_TraverseHierarchy(hierarchy, id, SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS, &cb);
}
