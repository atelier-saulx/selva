#pragma once
#ifndef _SELVA_HIERARCHY_DETACHED_H_
#define _SELVA_HIERARCHY_DETACHED_H_

#include "selva.h"

struct SelvaHierarchy;
struct compressed_rms;

int SelvaHierarchyDetached_Get(struct SelvaHierarchy *hierarchy, const Selva_NodeId node_id, struct compressed_rms **compressed);

/**
 * Remove a node_id from the detached nodes map.
 * This should be called when the node is actually added back to the hierarchy.
 * Ideally all the pointers to the `compressed` string returned by
 * SelvaHierarchyDetached_Get() should be deleted once the subtree restore is
 * complete.
 * Note that this function doesn't free the compressed_rms, that should be
 * done by the caller.
 */
void SelvaHierarchyDetached_RemoveNode(SelvaHierarchy *hierarchy, const Selva_NodeId node_id);

/**
 * Add a node_id to subtree mapping to the detached nodes map.
 * Obviously the serialized subtree cannot be ready yet when this function is
 * called, therefore the string must be appended at the end of the serialization
 * process.
 */
int SelvaHierarchyDetached_AddNode(struct SelvaHierarchy *hierarchy, const Selva_NodeId node_id, struct compressed_rms *compressed);

#endif /* _SELVA_HIERARCHY_DETACHED_H_ */
