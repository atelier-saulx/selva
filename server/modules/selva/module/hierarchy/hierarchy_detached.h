#pragma once
#ifndef _SELVA_HIERARCHY_DETACHED_H_
#define _SELVA_HIERARCHY_DETACHED_H_

#include "selva.h"

/**
 * Storage backend type of the detached subtree node.
 */
enum SelvaHierarchyDetachedType {
    /**
     * The node and its subtree is compressed and stored in memory.
     */
    SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM = 1,
    /**
     * The node and its subtree is compressed and stored on disk.
     */
    SELVA_HIERARCHY_DETACHED_COMPRESSED_DISK = 2,
};

struct RedisModuleCtx;
struct SelvaHierarchy;
struct compressed_rms;

/**
 * Check if a detached index exists.
 * @returns truthy if index exists; Otherwise zero.
 */
static inline int SelvaHierarchyDetached_IndexExists(const struct SelvaHierarchy *hierarchy) {
    return !!hierarchy->detached.obj;
}

/**
 * Prepare compressed subtree to be stored in the detached hierarchy.
 * If the type is SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM this function will only
 * make a tagged pointer from the original pointer;
 * If the type is SELVA_HIERARCHY_DETACHED_COMPRESSED_DISK then the compressed
 * subtree is written to the disk, a properly prepared tagged pointer is
 * returned, and the original compressed data is freed.
 * If writing to the disk fails the function will use
 * SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM as a fallback.
 * @returns A tagged pointer to the compressed subtree.
 */
void *SelvaHierarchyDetached_Store(
        const Selva_NodeId node_id,
        struct compressed_rms *compressed,
        enum SelvaHierarchyDetachedType type);

/**
 * Get the compressed subtree string of node_id.
 * @param[out] type Returns the storage type. Can be NULL.
 */
int SelvaHierarchyDetached_Get(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct compressed_rms **compressed,
        enum SelvaHierarchyDetachedType *type);

/**
 * Remove a node_id from the detached nodes map.
 * This should be called when the node is actually added back to the hierarchy.
 * Ideally all the pointers to the `compressed` string returned by
 * SelvaHierarchyDetached_Get() should be deleted once the subtree restore is
 * complete.
 * Note that this function doesn't free the compressed_rms, that should be
 * done by the caller.
 */
void SelvaHierarchyDetached_RemoveNode(struct RedisModuleCtx *ctx, struct SelvaHierarchy *hierarchy, const Selva_NodeId node_id);

/**
 * Add a node_id to subtree mapping to the detached nodes map.
 * Obviously the serialized subtree cannot be ready yet when this function is
 * called, therefore the string must be appended at the end of the serialization
 * process.
 * @param tag_compressed is a struct compressed_rms that has been tagged with a SelvaHierarchyDetachedType.
 */
int SelvaHierarchyDetached_AddNode(struct SelvaHierarchy *hierarchy, const Selva_NodeId node_id, void *tag_compressed);

#endif /* _SELVA_HIERARCHY_DETACHED_H_ */
