#pragma once
#ifndef _SELVA_HIERARCHY_INACTIVE_H_
#define _SELVA_HIERARCHY_INACTIVE_H_

#include "selva.h"

struct SelvaHierarchy;

int SelvaHierarchy_InitInactiveNodes(struct SelvaHierarchy *hierarchy, size_t nr_nodes);
void SelvaHierarchy_DeinitInactiveNodes(struct SelvaHierarchy *hierarchy);
void SelvaHierarchy_AddInactiveNodeId(struct SelvaHierarchy *hierarchy, Selva_NodeId node_id);
void SelvaHierarchy_ClearInactiveNodeIds(struct SelvaHierarchy *hierarchy);

#endif /* _SELVA_HIERARCHY_INACTIVE_H_ */
