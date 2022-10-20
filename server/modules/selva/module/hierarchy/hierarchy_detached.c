/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "redismodule.h"
#include "jemalloc.h"
#include "ptag.h"
#include "selva.h"
#include "rms.h"
#include "selva_object.h"
#include "hierarchy.h"
#include "hierarchy_detached.h"

/*
 * Management for detached (and compressed) hierarchy subtrees.
 */

/**
 * Get path for a compressed subtree of node_id.
 */
static RedisModuleString *get_zpath(const Selva_NodeId node_id) {
    static char disk_subtree_prefix[5];
    pid_t pid = getpid();

    if (disk_subtree_prefix[0] == '\0') {
        RedisModule_GetRandomHexChars(disk_subtree_prefix, sizeof(disk_subtree_prefix));
    }

    /*
     * Presumably the CWD is where the current Redis dump goes, we assume that's
     * where these compressed subtrees should go too.
     * We attempt to create a filename pattern that can never repeat after
     * crashes or involuntary restarts.
     */
    return RedisModule_CreateStringPrintf(NULL, "selva_%jd_%.*s_%.*s.z",
            (intmax_t)pid,
            (int)(sizeof(disk_subtree_prefix)), disk_subtree_prefix,
            (int)SELVA_NODE_ID_SIZE, node_id);
}

/**
 * Write a compressed subtree to the disk.
 * @param zpath is a pointer to the destination path.
 * @param compressed is a pointer to the compressed subtree.
 */
static int fwrite_compressed_subtree(RedisModuleString *zpath, const struct compressed_rms *compressed) {
    const char *zpath_str = RedisModule_StringPtrLen(zpath, NULL);
    FILE *fp;
    int err;

    /*
     * `x` will reject the operation if the file already exists.
     */
    fp = fopen(zpath_str, "wbx");
    if (!fp) {
        /*
         * This could be also SELVA_EINVAL, it's hard to say what's better.
         */
        return SELVA_EEXIST;
    }

    err = rms_fwrite_compressed(compressed, fp);

    fclose(fp);
    return err;
}

/**
 * Read a compressed subtree from the disk.
 * This function also deletes the file.
 * @param zpath is a pointer to the source path of the subtree.
 * @param[out] compressed_out is the output.
 */
static int fread_compressed_subtree(RedisModuleString *zpath, struct compressed_rms **compressed_out) {
    const char *zpath_str = RedisModule_StringPtrLen(zpath, NULL);
    FILE *fp;
    struct compressed_rms *compressed;
    int err;

    fp = fopen(zpath_str, "rb");
    if (!fp) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to open compressed subtree \"%s\": %s",
                  zpath_str,
                  strerror(errno));
        /*
         * It could look like ENOENT would make more sense here but that's not
         * true because ENOENT would be interpreted as if the node was not
         * stored in the detached hierarchy and thus wouldn't exist at all.
         * However, this is not true, as we are this far already, we certainly
         * know that the node should exist and something is wrong.
         */
        return SELVA_EINVAL;
    }

    compressed = rms_alloc_compressed();
    err = rms_fread_compressed(compressed, fp);
    fclose(fp);

    if (err) {
        rms_free_compressed(compressed);
        compressed = NULL;
    }

    *compressed_out = compressed;
    return err;
}

/**
 * Store a compressed subtree on disk.
 * @returns a pointer to the filename.
 */
static RedisModuleString *store_compressed(const Selva_NodeId node_id, struct compressed_rms *compressed) {
    RedisModuleString *zpath = get_zpath(node_id);
    int err;

    err = fwrite_compressed_subtree(zpath, compressed);
    if (err) {
        RedisModule_FreeString(NULL, zpath);
        return NULL;
    }

    /*
     * It's slightly unorthodox to free `compressed` here and not where it was
     * allocated but you should this it as if the caller passed the ownership of
     * the data to us, which is what this whole thing should be all about.
     */
    rms_free_compressed(compressed);

    return zpath;
}

void *SelvaHierarchyDetached_Store(
        const Selva_NodeId node_id,
        struct compressed_rms *compressed,
        enum SelvaHierarchyDetachedType type) {
    void *p;

    if (!compressed) {
        return NULL;
    }

    switch (type) {
    case SELVA_HIERARCHY_DETACHED_COMPRESSED_DISK:
        if ((p = store_compressed(node_id, compressed))) {
            p = PTAG(p, SELVA_HIERARCHY_DETACHED_COMPRESSED_DISK);
            break;
        }
        SELVA_LOG(SELVA_LOGL_WARN, "Fallback to inmem compression");
        __attribute__((fallthrough));
    case SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM:
    default:
        p = PTAG(compressed, SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM);
    }

    return p;
}

int SelvaHierarchyDetached_Get(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct compressed_rms **compressed,
        enum SelvaHierarchyDetachedType *type) {
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

    const enum SelvaHierarchyDetachedType tag = PTAG_GETTAG(p);

    if (type) {
        *type = tag;
    }

    if (tag == SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM) {
        *compressed = PTAG_GETP(p);
        assert(*compressed);

        return 0;
    } else if (tag == SELVA_HIERARCHY_DETACHED_COMPRESSED_DISK) {
        RedisModuleString *zpath = PTAG_GETP(p);

        return fread_compressed_subtree(zpath, compressed);
    } else {
        SELVA_LOG(SELVA_LOGL_WARN, "Invalid tag on detached node_id: %.*s",
                  (int)SELVA_NODE_ID_SIZE, node_id);
        return SELVA_EINTYPE;
    }
}

void SelvaHierarchyDetached_RemoveNode(RedisModuleCtx *ctx, SelvaHierarchy *hierarchy, const Selva_NodeId node_id) {
    struct SelvaObject *index = hierarchy->detached.obj;
    void *p;

    if (!index) {
        return;
    }

    if (SelvaObject_GetPointerStr(index, node_id, SELVA_NODE_ID_SIZE, &p)) {
        /* Not found. */
        return;
    }

    const int tag = PTAG_GETTAG(p);
    if (tag == SELVA_HIERARCHY_DETACHED_COMPRESSED_DISK) {
        RedisModuleString *zpath = PTAG_GETP(p);
        TO_STR(zpath);

        /* Remove the file. */
        (void)remove(zpath_str);

        /*
         * Trick Redis into using lazy free for zpath.
         * We know that we probably have multiple references to zpath, but we
         * also know that they'll be all cleaned up during this ctx.
         */
        RedisModule_HoldString(ctx, zpath);
        RedisModule_FreeString(NULL, zpath);
    }

    (void)SelvaObject_DelKeyStr(index, node_id, SELVA_NODE_ID_SIZE);
}

int SelvaHierarchyDetached_AddNode(SelvaHierarchy *hierarchy, const Selva_NodeId node_id, void *tag_compressed) {
    struct SelvaObject *index = hierarchy->detached.obj;

    if (!index) {
        index = SelvaObject_New();
        hierarchy->detached.obj = index;
    }

    return SelvaObject_SetPointerStr(index, node_id, SELVA_NODE_ID_SIZE, tag_compressed, NULL);
}
