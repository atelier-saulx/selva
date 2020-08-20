#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "async_task.h"
#include "hierarchy.h"

static const char ancestors_field_str[] = "ancestors";
#define ANCESTORS_FIELD_LEN (sizeof(ancestors_field_str) - 1)
#define ANCESTORS_EVENT_PAYLOAD_LEN \
    (sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + ANCESTORS_FIELD_LEN)

static int sendAncestorEvent(Selva_NodeId id, void *arg) {
    struct SelvaModify_AsyncTask *publish_task = (struct SelvaModify_AsyncTask *)(arg + sizeof(int32_t));

    memcpy(publish_task->id, id, SELVA_NODE_ID_SIZE);
    SelvaModify_SendAsyncTask(ANCESTORS_EVENT_PAYLOAD_LEN, (const char *)arg);

    return 0;
}

void SelvaModify_PublishDescendants(struct SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
    size_t payload_len = sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + ANCESTORS_FIELD_LEN;
    char payload_str[payload_len];
    struct SelvaModify_HierarchyCallback cb = {
        .node_cb = sendAncestorEvent,
        .node_arg = payload_str,
    };

    SelvaModify_PreparePublishPayload_Update(payload_str, id, ancestors_field_str, ANCESTORS_FIELD_LEN);
    (void)SelvaModify_TraverseHierarchy(hierarchy, id, SELVA_MODIFY_HIERARCHY_DFS_DESCENDANTS, &cb);
}
