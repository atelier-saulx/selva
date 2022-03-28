#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "redismodule.h"
#include "cdefs.h"
#include "errors.h"
#include "ptag.h"
#include "selva_object.h"
#include "hierarchy.h"
#include "hierarchy_detached.h"

/*
 * Management for detached (and compressed) hierarchy subtrees.
 */

/**
 * Storage backend type of the detached subtree node.
 * While there is currently only one type it's future proofing the system for
 * storing subtrees on disk.
 */
enum SELVA_HIERARCHY_DETACHED_TYPE {
    /**
     * The node and its subtree is compressed and stored in memory.
     */
    SELVA_HIERARCHY_DETACHED_COMPRESSED = 1,
};

int SelvaHierarchyDetached_Get(struct SelvaHierarchy *hierarchy, const Selva_NodeId node_id, struct compressed_rms **compressed) {
    struct SelvaObject *index = hierarchy->detached.obj;
    void *p;
    int err;

    if (!index) {
        return SELVA_ENOENT;
    }

    err = SelvaObject_GetPointerStr(index, node_id, SELVA_NODE_ID_SIZE, &p);
    if (err) {
        return err;
    }

    assert(p);

    int tag = PTAG_GETTAG(p);
    if (tag == SELVA_HIERARCHY_DETACHED_COMPRESSED) {
        *compressed = PTAG_GETP(p);

        assert(*compressed);
    } else {
        fprintf(stderr, "%s:%d: Invalid tag on detached node_id: %.*s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, node_id);
        return SELVA_EINTYPE;
    }

    return 0;
}

/**
 * Remove a node_id from the detached nodes map.
 * This should be called when the node is actually added back to the hierarchy.
 * Ideally all the pointers to the `compressed` string returned by
 * SelvaHierarchyDetached_Get() should be deleted once the subtree restore is
 * complete.
 * Note that this function doesn't free the RedisModuleString, that should be
 * done by the caller.
 */
void SelvaHierarchyDetached_RemoveNode(SelvaHierarchy *hierarchy, const Selva_NodeId node_id) {
    struct SelvaObject *index = hierarchy->detached.obj;

    if (!index) {
        return;
    }

    (void)SelvaObject_DelKeyStr(index, node_id, SELVA_NODE_ID_SIZE);
}

/**
 * Add a node_id to subtree mapping to the detached nodes map.
 * Obviously the serialized subtree cannot be ready yet when this function is
 * called, therefore the string must be appended at the end of the serialization
 * process.
 */
int SelvaHierarchyDetached_AddNode(SelvaHierarchy *hierarchy, const Selva_NodeId node_id, struct compressed_rms *compressed) {
    struct SelvaObject *index = hierarchy->detached.obj;

    if (!index) {
        index = SelvaObject_New();
        if (!index) {
            return SELVA_ENOMEM;
        }

        hierarchy->detached.obj = index;
    }

    return SelvaObject_SetPointerStr(index, node_id, SELVA_NODE_ID_SIZE, PTAG(compressed, SELVA_HIERARCHY_DETACHED_COMPRESSED), NULL);
}
