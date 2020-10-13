#include <stddef.h>
#include "redismodule.h"
#include "alias.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_set.h"
#include "selva_node.h"

static int initialize_node(RedisModuleCtx *ctx, RedisModuleKey *key, RedisModuleString *key_name, const Selva_NodeId nodeId) {
    const int is_root = !memcmp(nodeId, ROOT_NODE_ID, SELVA_NODE_ID_SIZE);

    RedisModule_HashSet(key, REDISMODULE_HASH_NX | REDISMODULE_HASH_CFIELDS, "$id", key_name, NULL);

    /* Set the type for root. */
    if (is_root) {
        RedisModuleString *type;

        type = RedisModule_CreateStringPrintf(ctx, "root");
        if (unlikely(!type)) {
            return SELVA_MODIFY_HIERARCHY_ENOMEM;
        }

        RedisModule_HashSet(key, REDISMODULE_HASH_NX | REDISMODULE_HASH_CFIELDS, "type", type, NULL);
    }

    return 0;
}

RedisModuleKey *SelvaNode_Open(RedisModuleCtx *ctx, SelvaModify_Hierarchy *hierarchy, RedisModuleString *id, const Selva_NodeId nodeId, unsigned flags) {
    int err;

    /*
     * If this is a new node we need to create a hierarchy node for it.
     *
     * There is dumb circular dependency here.
     * The modify command will call this function to open and create nodes.
     * However, also hierarchy will call this function to create the node.
     * It ended up like this because nodes and hierarchy are tied together so
     * closely.
     * In theory hierarchy will only call this function when the node already
     * exists but to be extra sure, hierarchy will never pass a pointer to the
     * hierarchy it's working on.
     */
    if (hierarchy && !SelvaModify_HierarchyNodeExists(hierarchy, nodeId)) {
        size_t nr_parents;

        if ((flags & SELVA_NODE_OPEN_CREATE_FLAG) == 0) {
            return NULL;
        }

        nr_parents = unlikely(flags & SELVA_NODE_OPEN_NO_ROOT_FLAG) ? 0 : 1;
        err = SelvaModify_SetHierarchy(ctx, hierarchy, nodeId, nr_parents, ((Selva_NodeId []){ ROOT_NODE_ID }), 0, NULL);
        if (err) {
            fprintf(stderr, "%s:%d key: %s err: %s\n",
                    __FILE__,
                    __LINE__,
                    RedisModule_StringPtrLen(id, NULL),
                    getSelvaErrorStr(err));
            return NULL;
        }
    }

    /*
     * Open the redis key.
     */
    const int open_mode = REDISMODULE_READ | ((flags & SELVA_NODE_OPEN_WRFLD_FLAG) ? REDISMODULE_WRITE : 0);
    RedisModuleKey *key = RedisModule_OpenKey(ctx, id, open_mode);
    if (!key) {
        fprintf(stderr, "%s:%d key: %s err: %s\n",
                __FILE__,
                __LINE__,
                RedisModule_StringPtrLen(id, NULL),
                getSelvaErrorStr(err));
        return NULL;
    }

    /*
     * If the key is empty at this point we assume that the hash should actually
     * exist regardless of the given flags. Either there is something wrong or
     * morelikely the caller is from hierarchy.
     */
    if ((flags & SELVA_NODE_OPEN_WRFLD_FLAG) && RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        err = initialize_node(ctx, key, id, nodeId);
        if (err) {
            fprintf(stderr, "%s: %s\n", __FILE__, getSelvaErrorStr(err));
            RedisModule_CloseKey(key);
            return NULL;
        }
    }

    return key;
}

static char *delete_selva_sets(RedisModuleCtx *ctx, RedisModuleString *id) {
    RedisModuleCallReply * reply;
    TO_STR(id);

    reply = RedisModule_Call(ctx, "HGETALL", "s", id);
    if (reply == NULL) {
        /* FIXME errno handling */
#if 0
        switch (errno) {
        case EINVAL:
        case EPERM:
        default:
        }
#endif
        goto out;
    }

    int replyType = RedisModule_CallReplyType(reply);
    if (replyType != REDISMODULE_REPLY_ARRAY) {
        goto out;
    }

    RedisModuleString *field_name = NULL;
    RedisModuleString *field_value = NULL;
    size_t replyLen = RedisModule_CallReplyLength(reply);
    for (size_t idx = 0; idx < replyLen; idx++) {
        RedisModuleCallReply *elem;
        const char * str;
        size_t len;

        elem = RedisModule_CallReplyArrayElement(reply, idx);
        if (!elem) {
            continue;
        }

        if (RedisModule_CallReplyType(elem) != REDISMODULE_REPLY_STRING) {
            continue;
        }

        str = RedisModule_CallReplyStringPtr(elem, &len);
        if (!str) {
            continue;
        }

        if ((idx & 1) == 0) {
            /* Even indices are field names. */
            field_name = RedisModule_CreateString(ctx, str, len);
        } else {
            /* Odd indices are field values. */
            field_value = RedisModule_CreateString(ctx, str, len);
            TO_STR(field_name, field_value);

            /* Look for the magic value. */
            if (!strcmp(field_value_str, SELVA_SET_KEYWORD)) {
                RedisModuleKey *key;

                key = SelvaSet_Open(ctx, id_str, id_len, field_name_str);
                if (key) {
                    RedisModule_DeleteKey(key);
                    RedisModule_CloseKey(key);
                }
            }

            RedisModule_FreeString(ctx, field_name);
            RedisModule_FreeString(ctx, field_value);
        }
    }

out:
    if (reply) {
        RedisModule_FreeCallReply(reply);
    }

    return 0;
}

static void delete_node_aliases(RedisModuleCtx *ctx, RedisModuleString *id) {
    RedisModuleString *akey_name;
    RedisModuleKey *key;
    TO_STR(id);

    akey_name = RedisModule_CreateStringPrintf(ctx, "%s.aliases", id_str);
    if (unlikely(!akey_name)) {
        fprintf(stderr, "%s: OOM; Unable to remove aliases of the node: \"%s\"", __FILE__, id_str);
    }

    key = RedisModule_OpenKey(ctx, akey_name, REDISMODULE_WRITE);
    if (key) {
        RedisModuleKey *aliases_key = open_aliases_key(ctx);

        if (aliases_key) {
            delete_aliases(aliases_key, key);
            RedisModule_CloseKey(aliases_key);
        } else {
            fprintf(stderr, "%s: Unable to open aliases\n", __FILE__);
        }

        RedisModule_DeleteKey(key);
        RedisModule_CloseKey(key);
    }
}

static void delete_node_hash(RedisModuleCtx *ctx, RedisModuleString *id) {
    RedisModuleKey *key;

    /*
     * We could use SelvaNode_Open() here but that would be overkill.
     */
    key = RedisModule_OpenKey(ctx, id, REDISMODULE_WRITE);
    if (key) {
        RedisModule_DeleteKey(key);
        RedisModule_CloseKey(key);
    }
}

int SelvaNode_Delete(RedisModuleCtx *ctx, RedisModuleString *id) {
    delete_selva_sets(ctx, id);
    delete_node_aliases(ctx, id);
    delete_node_hash(ctx, id);

    return 0;
}

int SelvaNode_ExistField(RedisModuleCtx *ctx, RedisModuleKey *node_key, RedisModuleString *field) {
    int exists, err;

    err = RedisModule_HashGet(node_key, REDISMODULE_HASH_EXISTS, field, &exists, NULL);

    return err == REDISMODULE_OK && exists;
}

int SelvaNode_GetField(RedisModuleCtx *ctx, RedisModuleKey *node_key, RedisModuleString *field, RedisModuleString **out) {
    if (RedisModule_HashGet(node_key, REDISMODULE_HASH_NONE, field, out, NULL) != REDISMODULE_OK) {
        /* TODO Can we determine the exact cause? */
        return SELVA_EGENERAL;
    }

    return 0;
}

int SelvaNode_SetField(RedisModuleCtx *ctx, RedisModuleKey *node_key, RedisModuleString *field, RedisModuleString *value) {
    TO_STR(field);

    /*
     * If the field name contains a dot then the field is an object of some
     * kind, presumably "object" or "text". In case an object field is found
     * we need to set a special value for the containing object names so that
     * when accessing one we know that it's a part of an object. This is
     * particularly useful for inherit.c
     *
     * Example:
     * title    = "___selva_$object" (SELVA_OBJECT_KEYWORD)
     * title.en = "New title"
     */
    const char *cname = field_str;
    while ((cname = strstr(cname, "."))) {
        const size_t len = (uintptr_t)cname++ - (uintptr_t)field_str;
        RedisModuleString *field_container;
        RedisModuleString *object_field_identifier;

        field_container = RedisModule_CreateString(ctx, field_str, len);
        object_field_identifier = RedisModule_CreateString(ctx, SELVA_OBJECT_KEYWORD, sizeof(SELVA_OBJECT_KEYWORD) - 1);
        (void)RedisModule_HashSet(node_key, REDISMODULE_HASH_NONE, field_container, object_field_identifier, NULL);
    }

    (void)RedisModule_HashSet(node_key, REDISMODULE_HASH_NONE, field, value, NULL);

    return 0;
}

int SelvaNode_DelField(RedisModuleCtx *ctx, RedisModuleKey *node_key, RedisModuleString *field) {
    TO_STR(field);

    /*
     * See the comment in SelvaNode_SetField().
     */
    const char *cname = field_str;
    while ((cname = strstr(cname, "."))) {
        const size_t len = (uintptr_t)cname++ - (uintptr_t)field_str;
        RedisModuleString *field_container;

        field_container = RedisModule_CreateString(ctx, field_str, len);
        (void)RedisModule_HashSet(node_key, REDISMODULE_HASH_NONE, field_container, REDISMODULE_HASH_DELETE, NULL);
    }

    (void)RedisModule_HashSet(node_key, REDISMODULE_HASH_NONE, field, REDISMODULE_HASH_DELETE, NULL);

    return 0;
}
