/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "jemalloc.h"
#include "util/finalizer.h"
#include "util/ptag.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_db.h"
#include "selva_object.h"
#include "hierarchy.h"
#include "hierarchy_detached.h"

/*
 * Management for detached (and compressed) hierarchy subtrees.
 */

#define DISK_SUBTREE_PREFIX_SIZE 5

static void gen_prefix(char str[DISK_SUBTREE_PREFIX_SIZE])
{
    static const char *hex_digits = "0123456789abcdef";

    for (int i = 0; i < DISK_SUBTREE_PREFIX_SIZE; i++) {
        str[i] = hex_digits[(rand() % 16)];
    }
}

/**
 * Get path for a compressed subtree of node_id.
 */
static struct selva_string *get_zpath(const Selva_NodeId node_id)
{
    static char disk_subtree_prefix[DISK_SUBTREE_PREFIX_SIZE];
    pid_t pid = getpid();

    if (disk_subtree_prefix[0] == '\0') {
        gen_prefix(disk_subtree_prefix);
    }

    /*
     * Presumably the CWD is where the current regular dump goes, we assume that's
     * where these compressed subtrees should go too.
     * We attempt to create a filename pattern that can never repeat after
     * crashes or involuntary restarts.
     */
    return selva_string_createf("selva_%jd_%.*s_%.*s.z",
                                (intmax_t)pid,
                                (int)(sizeof(disk_subtree_prefix)), disk_subtree_prefix,
                                (int)SELVA_NODE_ID_SIZE, node_id);
}

static ssize_t get_file_size(FILE *fp) {
    long int size;

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    return size == -1L ? SELVA_EGENERAL : (ssize_t)size;
}

/**
 * Read a compressed subtree from the disk.
 * This function also deletes the file.
 * @param zpath is a pointer to the source path of the subtree.
 * @param[out] compressed_out is the output.
 */
static int fread_compressed_subtree(struct selva_string *zpath, struct selva_string **compressed_out)
{
    const char *zpath_str = selva_string_to_str(zpath, NULL);
    FILE *fp;
    ssize_t size;
    struct selva_string *compressed;
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

    size = get_file_size(fp);
    if (size <= 0) {
        err = SELVA_EINVAL;
        goto fail;
    }

    compressed = selva_string_fread(fp, size, SELVA_STRING_COMPRESS);
    if ((ssize_t)selva_string_get_len(compressed) != size) {
        if (ferror(fp)) {
            SELVA_LOG(SELVA_LOGL_ERR, "An error occurred while reading a compressed subtree \"%s\"",
                      zpath_str);
            err = SELVA_EIO;
            goto fail;
        } else { /* EOF */
            SELVA_LOG(SELVA_LOGL_ERR, "Unexpected EOF while reading a compressed subtree \"%s\"",
                      zpath_str);
            err = SELVA_EIO;
            goto fail;
        }
    }

    *compressed_out = compressed;
    err = 0;
fail:
    fclose(fp);
    return err;
}

/**
 * Store a compressed subtree on disk.
 * @returns a pointer to the filename.
 */
static struct selva_string *store_compressed(const Selva_NodeId node_id, struct selva_string *compressed)
{
    struct selva_string *zpath = get_zpath(node_id);
    FILE *fp;
    TO_STR(compressed, zpath);

    /*
     * `x` will reject the operation if the file already exists.
     */
    fp = fopen(zpath_str, "wbx");
    if (!fp) {
        NULL;
    }

    if (fwrite(compressed_str, compressed_len, 1, fp) != 1) {
        selva_string_free(zpath);
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    /*
     * It's slightly unorthodox to free `compressed` here and not where it was
     * allocated but you should think it as if the caller passed the ownership of
     * the data to us, which is what this whole thing should be all about.
     */
    selva_string_free(compressed);

    return zpath;
}

void *SelvaHierarchyDetached_Store(
        const Selva_NodeId node_id,
        struct selva_string *compressed,
        enum SelvaHierarchyDetachedType type)
{
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
        [[fallthrough]];
    case SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM:
    default:
        p = PTAG(compressed, SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM);
    }

    return p;
}

int SelvaHierarchyDetached_Get(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct selva_string **compressed,
        enum SelvaHierarchyDetachedType *type)
{
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
        struct selva_string *zpath = PTAG_GETP(p);

        return fread_compressed_subtree(zpath, compressed);
    } else {
        SELVA_LOG(SELVA_LOGL_WARN, "Invalid tag on detached node_id: %.*s",
                  (int)SELVA_NODE_ID_SIZE, node_id);
        return SELVA_EINTYPE;
    }
}

void SelvaHierarchyDetached_RemoveNode(struct finalizer * restrict fin, SelvaHierarchy * restrict hierarchy, const Selva_NodeId node_id)
{
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
        struct selva_string *zpath = PTAG_GETP(p);
        const char *zpath_str = selva_string_to_str(zpath, NULL);

        /* Remove the file. */
        (void)remove(zpath_str);

        /*
         * Lazy free zpath.
         * We know that we still have one or more references to zpath, but
         * we also know that they'll be all cleaned up during this callstack.
         */
        finalizer_del(fin, zpath);
        selva_string_auto_finalize(fin, zpath);
    }

    (void)SelvaObject_DelKeyStr(index, node_id, SELVA_NODE_ID_SIZE);
}

int SelvaHierarchyDetached_AddNode(SelvaHierarchy *hierarchy, const Selva_NodeId node_id, void *tag_compressed)
{
    struct SelvaObject *index = hierarchy->detached.obj;

    if (!index) {
        index = SelvaObject_New();
        hierarchy->detached.obj = index;
    }

    return SelvaObject_SetPointerStr(index, node_id, SELVA_NODE_ID_SIZE, tag_compressed, NULL);
}
