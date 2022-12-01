/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <sys/mman.h>
#include <unistd.h>
#include "selva.h"
#include "hierarchy.h"
#include "hierarchy_inactive.h"

int SelvaHierarchy_InitInactiveNodes(struct SelvaHierarchy *hierarchy, size_t nr_nodes) {
    /* See hierarchy.h for comments on how the data is stored and used. */
    hierarchy->inactive.nodes = mmap(NULL, nr_nodes * SELVA_NODE_ID_SIZE,
                                     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
                                     -1, 0);
    if (!hierarchy->inactive.nodes) {
        return SELVA_ENOMEM;
    }

    hierarchy->inactive.nr_nodes = nr_nodes;

    return 0;
}

void SelvaHierarchy_DeinitInactiveNodes(struct SelvaHierarchy *hierarchy) {
    if (hierarchy->inactive.nodes) {
        munmap(hierarchy->inactive.nodes, hierarchy->inactive.nr_nodes * SELVA_NODE_ID_SIZE);
        hierarchy->inactive.nodes = NULL;
    }
}

void SelvaHierarchy_AddInactiveNodeId(struct SelvaHierarchy *hierarchy, const Selva_NodeId node_id) {
    const size_t i = hierarchy->inactive.next;

    if (hierarchy->inactive.nodes && i < hierarchy->inactive.nr_nodes) {
        char *p = hierarchy->inactive.nodes[i];

        memcpy(p, node_id, SELVA_NODE_ID_SIZE);
        hierarchy->inactive.next = i + 1;
    }
}

void SelvaHierarchy_ClearInactiveNodeIds(struct SelvaHierarchy *hierarchy) {
    if (hierarchy->inactive.nodes) {
        memset(hierarchy->inactive.nodes, '\0',
               hierarchy->inactive.nr_nodes * SELVA_NODE_ID_SIZE);
        hierarchy->inactive.next = 0;
    }
}
