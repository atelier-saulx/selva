#include "traversal.h"

int parse_dir(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        enum SelvaModify_HierarchyTraversal *dir,
        RedisModuleString **field_name_out,
        Selva_NodeId nodeId,
        enum SelvaTraversalAlgo algo,
        const RedisModuleString *field_name) {
    const char *p1 = RedisModule_StringPtrLen(field_name, NULL); /* Beginning of a field_name or a list of field_names. */
    const char *p2 = get_next_field_name(p1); /* Last char of the first field_name. */

    /*
     * Open the node object.
     */
    RedisModuleString *rms_node_id;
    RedisModuleKey *node_key;
    struct SelvaObject *obj;
    int err = 0;

    /*
     * Open the node key.
     * We may not need this but we don't know it yet.
     */
    rms_node_id = RedisModule_CreateString(ctx, nodeId, Selva_NodeIdLen(nodeId));
    node_key = RedisModule_OpenKey(ctx, rms_node_id, REDISMODULE_READ | REDISMODULE_OPEN_KEY_NOTOUCH);
    err = SelvaObject_Key2Obj(node_key, &obj);
    if (err) {
        return err;
    }

    do {
        const size_t sz = (size_t)((ptrdiff_t)p2 - (ptrdiff_t)p1);

        if (sz == 4 && !strncmp("node", p1, 4)) {
            *dir = SELVA_HIERARCHY_TRAVERSAL_NODE;
            break;
        } else if (sz == 8 && !strncmp("children", p1, 8)) {
            *dir = SELVA_HIERARCHY_TRAVERSAL_CHILDREN;
            break;
        } else if (sz == 7 && !strncmp("parents", p1, 7)) {
            *dir = SELVA_HIERARCHY_TRAVERSAL_PARENTS;
            break;
        } else if (sz == 9 && !strncmp("ancestors", p1, 9)) {
            *dir = algo == HIERARCHY_BFS
                ? SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS
                : SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS;
            break;
        } else if (sz == 11 && !strncmp("descendants", p1, 11)) {
            *dir = algo == HIERARCHY_BFS
                ? SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS
                : SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS;
            break;
        } else if (sz > 0) {
            /*
             * Check if the field_name is a custom edge field name.
             * TODO This should check that the field is non-empty.
             */
            if (Edge_GetField(SelvaHierarchy_FindNode(hierarchy, nodeId), p1, sz)) {
                RedisModuleString *rms;

                if (algo != HIERARCHY_BFS) {
                    err = SELVA_HIERARCHY_EINVAL;
                    break;
                }

                rms = RedisModule_CreateString(ctx, p1, sz);
                if (!rms) {
                    err = SELVA_HIERARCHY_ENOMEM;
                    break;
                }

                err = 0;
                *field_name_out = rms;
                *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD;
                break;
            } else {
                /*
                 * Check if the field_name is a regular field containing
                 * a set of nodeId strings.
                 */
                enum SelvaObjectType type;

                /* TODO Actually verify that it contains strings */
                type = SelvaObject_GetTypeStr(obj, p1, sz);
                if (type == SELVA_OBJECT_SET) {
                    RedisModuleString *rms;

#if 0
                    fprintf(stderr, "%s:%d: Field exists. node: %.*s field: %.*s type: %d\n",
                            __FILE__, __LINE__,
                            (int)SELVA_NODE_ID_SIZE, nodeId,
                            (int)sz, p1,
                            type);
#endif

                    rms = RedisModule_CreateString(ctx, p1, sz);
                    if (!rms) {
                        err = SELVA_HIERARCHY_ENOMEM;
                        break;
                    }

                    err = 0;
                    *field_name_out = rms;
                    *dir = SELVA_HIERARCHY_TRAVERSAL_REF;
                    break;
                } else if (type == SELVA_OBJECT_ARRAY) {
                    RedisModuleString *rms;

#if 0
                    fprintf(stderr, "%s:%d: Field exists. node: %.*s field: %.*s type: %d\n",
                            __FILE__, __LINE__,
                            (int)SELVA_NODE_ID_SIZE, nodeId,
                            (int)sz, p1,
                            type);
#endif

                    rms = RedisModule_CreateString(ctx, p1, sz);
                    if (!rms) {
                        err = SELVA_HIERARCHY_ENOMEM;
                        break;
                    }

                    err = 0;
                    *field_name_out = rms;
                    *dir = SELVA_HIERARCHY_TRAVERSAL_ARRAY;
                    break;
                }
            }
        } else {
            err = SELVA_HIERARCHY_EINVAL;
            break;
        }

        if (*p2 == '\0') {
            /* If this was the last field_name, we give up. */
            err = SELVA_HIERARCHY_ENOENT;
            break;
        }

        /* Find the next field_name in the string. */
        p1 = p2 + 1;
        p2 = get_next_field_name(p1);
    } while (p1 != p2);

    RedisModule_CloseKey(node_key);
    return err;
}
