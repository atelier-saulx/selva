#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "cdefs.h"
#include "hierarchy-utils.h"

SelvaHierarchy *hierarchy;
Selva_NodeId *findRes;
RedisModuleIO *io;

Selva_NodeId HIERARCHY_RDB_EOF __nonstring;

/* GCC needs something to be stored in the set. */
static void init_node_metadata(
        Selva_NodeId id __unused,
        struct SelvaHierarchyMetadata *metadata __unused) {
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(init_node_metadata);

/* GCC needs something to be stored in the set. */
static void deinit_node_metadata(
        Selva_NodeId id __unused,
        struct SelvaHierarchyMetadata *metadata __unused) {
}
SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(deinit_node_metadata);

int SelvaNodeId_Compare(const void *a, const void *b) {

    return memcmp((const char *)a, (const char *)b, SELVA_NODE_ID_SIZE);
}

void SelvaNodeId_SortRes(size_t len) {
    qsort(findRes, len, sizeof(Selva_NodeId), SelvaNodeId_Compare);
}

char *SelvaNodeId_GetRes(size_t i) {
    /* cppcheck-suppress threadsafety-threadsafety */
    static char id[sizeof(Selva_NodeId) + 1];

    memcpy(id, findRes[i], sizeof(Selva_NodeId));
    id[sizeof(Selva_NodeId)] = '\0';

    return id;
}

void SelvaNodeId_copy2buf(char buf[SELVA_NODE_ID_SIZE + 1], const Selva_NodeId id) {
    memcpy(buf, id, SELVA_NODE_ID_SIZE);
    buf[SELVA_NODE_ID_SIZE] = '\0';
}

void SelvaModify_PublishSubscriptionUpdate(const Selva_SubscriptionId sub_id __unused) {
}
