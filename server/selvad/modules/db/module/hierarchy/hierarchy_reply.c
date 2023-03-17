/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "selva_server.h"
#include "selva_db.h"
#include "hierarchy.h"

struct send_hierarchy_field_data {
    struct selva_server_response_out *resp;

    size_t skip;
    size_t nr_types;
    const Selva_NodeType *types;
    size_t len;
};

/*
 * Used for ancestors, children, descendants, parents
 */
static int send_hierarchy_field_NodeCb(
        struct SelvaHierarchy *hierarchy __unused,
        struct SelvaHierarchyNode *node,
        void *arg) {
    Selva_NodeId nodeId;
    struct send_hierarchy_field_data *args = (struct send_hierarchy_field_data *)arg;
    int match = 0;

    SelvaHierarchy_GetNodeId(nodeId, node);

    /*
     * Some traversal modes must skip the first entry.
     */
    if (unlikely(args->skip)) {
        args->skip = 0;
        return 0;
    }

    for (size_t i = 0; i < args->nr_types; i++) {
        match |= memcmp(args->types[i], nodeId, SELVA_NODE_TYPE_SIZE) == 0;
    }
    if (!match && args->nr_types > 0) {
        /*
         * This node type is not accepted.
         */
        return 0;
    }

    selva_send_str(args->resp, nodeId, Selva_NodeIdLen(nodeId));
    args->len++;

    return 0;
}

int HierarchyReply_WithTraversal(
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId nodeId,
        size_t nr_types,
        const Selva_NodeType *types,
        enum SelvaTraversal dir) {
    const size_t skip = !!(dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS | SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS));
    struct send_hierarchy_field_data args = {
        .resp = resp,
        .skip = skip,
        .nr_types = nr_types,
        .types = types,
        .len = 0,
    };
    const struct SelvaHierarchyCallback cb = {
        .node_cb = send_hierarchy_field_NodeCb,
        .node_arg = &args,
    };
    int err;

    /*
     * Start a new array reply:
     * [nodeId1, nodeId2,.. nodeIdn]
     */
    selva_send_array(resp, -1);
    err = SelvaHierarchy_Traverse(hierarchy, nodeId, dir, &cb);
    selva_send_array_end(resp);

    return err;
}
