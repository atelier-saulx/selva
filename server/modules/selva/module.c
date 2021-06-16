#include <math.h>
#include <time.h>

#include "cdefs.h"
#include "cstrings.h"
#include "redismodule.h"
#include "typestr.h"
#include "rmutil/util.h"
#include "rmutil/strings.h"
#include "rmutil/test_util.h"
#include "selva.h"
#include "selva_onload.h"
#include "svector.h"
#include "bitmap.h"
#include "module/errors.h"
#include "module/async_task.h"
#include "module/hierarchy.h"
#include "module/modify.h"
#include "module/selva_node.h"
#include "module/selva_object.h"
#include "module/subscriptions.h"

#define FLAG_NO_ROOT    0x01
#define FLAG_NO_MERGE   0x02
#define FLAG_CREATE     0x03
#define FLAG_UPDATE     0x04
#define FLAG_CREATED_AT 0x08
#define FLAG_UPDATED_AT 0x10

#define FISSET_NO_ROOT(m) (((m) & FLAG_NO_ROOT) == FLAG_NO_ROOT)
#define FISSET_NO_MERGE(m) (((m) & FLAG_NO_MERGE) == FLAG_NO_MERGE)
#define FISSET_CREATE(m) (((m) & FLAG_CREATE) == FLAG_CREATE)
#define FISSET_UPDATE(m) (((m) & FLAG_UPDATE) == FLAG_UPDATE)
#define FISSET_CREATED_AT(m) (((m) & FLAG_CREATED_AT) == FLAG_CREATED_AT)
#define FISSET_UPDATED_AT(m) (((m) & FLAG_UPDATED_AT) == FLAG_UPDATED_AT)

SET_DECLARE(selva_onload, Selva_Onload);
SET_DECLARE(selva_onunld, Selva_Onunload);

/*
 * Technically a nodeId is always 10 bytes but sometimes a printable
 * representation without padding zeroes is needed.
 */
size_t Selva_NodeIdLen(const Selva_NodeId nodeId) {
    size_t len = SELVA_NODE_ID_SIZE;

    while (len >= 1 && nodeId[len - 1] == '\0') {
        len--;
    }

    return len;
}

/*
 * Replicate the selva.modify command.
 * This function depends on the argument order of selva.modify.
 */
void replicateModify(RedisModuleCtx *ctx, const struct bitmap *replset, RedisModuleString **orig_argv) {
    unsigned int count = bitmap_popcount(replset);

    /*
     * TODO REDISMODULE_CTX_FLAGS_REPLICATED would be more appropriate here but it's
     * unclear whether it's available only in newer server versions.
     */
    if ((RedisModule_GetContextFlags(ctx) & REDISMODULE_CTX_FLAGS_SLAVE) || count == 0) {
        return; /* Skip. */
    }

    /*
     * Redis doesn't have an external API to call commands nor replication the
     * same way as it delivers them. Also the API is quite horrible because it
     * only provides variadic function for replication. Therefore, we need to
     * do a little hack here make dynamic arguments work.
     */
    const int leading_args = 3; /* [cmd_name, key, flags] */
    RedisModuleString **argv;

    argv = RedisModule_PoolAlloc(ctx, ((size_t)leading_args + count) * sizeof(RedisModuleString *));
    if (!argv) {
        fprintf(stderr, "%s:%d: Replication error: %s\n",
                __FILE__, __LINE__,
                getSelvaErrorStr(SELVA_ENOMEM));
        return;
    }

    /*
     * Copy the leading args.
     */
    int argc = leading_args;
    for (int i = 0; i < argc; i++) {
        argv[i] = orig_argv[i];
    }

    int i_arg_type = leading_args;
    for (int i = 0; i < (int)replset->nbits; i++) {
        if (bitmap_get(replset, i)) {
            argv[argc++] = orig_argv[i_arg_type];
            argv[argc++] = orig_argv[i_arg_type + 1];
            argv[argc++] = orig_argv[i_arg_type + 2];
        }
        i_arg_type += 3;
    }

#if 0
    fprintf(stderr, "%s:%d: Replicating: ", __FILE__, __LINE__);
    for (int i = 0; i < argc; i++) {
        RedisModuleString *arg = argv[i];
        TO_STR(arg);

        fwrite(arg_str, sizeof(char), arg_len, stderr);
        fputc(' ', stderr);
    }
#endif

    RedisModule_ReplicateVerbatimArgs(ctx, argv, argc);
}

int SelvaCommand_Flurpy(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    // init auto memory for created strings
    RedisModule_AutoMemory(ctx);

    RedisModuleString *keyStr =
            RedisModule_CreateString(ctx, "flurpypants", strlen("flurpypants"));
    RedisModuleString *val = RedisModule_CreateString(ctx, "hallo", strlen("hallo"));
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyStr, REDISMODULE_WRITE);
    for (int i = 0; i < 10000; i++) {
        RedisModule_StringSet(key, val);
        // RedisModuleCallReply *r = RedisModule_Call(ctx, "publish", "x", "y");
    }

    RedisModule_CloseKey(key);
    RedisModuleString *reply = RedisModule_CreateString(ctx, "hallo", strlen("hallo"));
    RedisModule_ReplyWithString(ctx, reply);
    return REDISMODULE_OK;
}

static void RedisModuleString2Selva_NodeId(Selva_NodeId nodeId, const RedisModuleString *id) {
    TO_STR(id);

    id_str = RedisModule_StringPtrLen(id, &id_len);
    memset(nodeId, '\0', SELVA_NODE_ID_SIZE);
    memcpy(nodeId, id_str, min(id_len, SELVA_NODE_ID_SIZE));
}

/*
 * Tokenize nul-terminated strings from a string with the size of size.
 */
static const char *sztok(const char *s, size_t size, size_t * restrict i) {
    const char *r = NULL;

    if (*i < size - 1) {
        r = s + *i;
        *i = *i + strnlen(r, size) + 1;
    }

    return r;
}

static int parse_flags(const RedisModuleString *arg) {
    TO_STR(arg);
    int flags = 0;

    for (size_t i = 0; i < arg_len; i++) {
        flags |= arg_str[i] == 'N' ? FLAG_NO_ROOT : 0;
        flags |= arg_str[i] == 'M' ? FLAG_NO_MERGE : 0;
        flags |= arg_str[i] == 'C' ? FLAG_CREATE : 0;
        flags |= arg_str[i] == 'U' ? FLAG_UPDATE : 0;
        flags |= arg_str[i] == 'c' ? FLAG_CREATED_AT : 0;
        flags |= arg_str[i] == 'u' ? FLAG_UPDATED_AT : 0;
    }

    return flags;
}

static int in_mem_range(const void *p, const void *start, size_t size) {
    const ptrdiff_t end = (ptrdiff_t)start + size;

    return (ptrdiff_t)p >= (ptrdiff_t)start && (ptrdiff_t)p <= end;
}

static struct SelvaModify_OpSet *SelvaModify_OpSet_align(RedisModuleCtx *ctx, const struct RedisModuleString *data) {
    TO_STR(data);
    struct SelvaModify_OpSet *op;

    if (!data_str && data_len < sizeof(struct SelvaModify_OpSet)) {
        return NULL;
    }

    op = RedisModule_PoolAlloc(ctx, data_len);
    if (!op) {
        return NULL;
    }

    memcpy(op, data_str, data_len);
    op->$add    = op->$add    ? ((char *)op + (ptrdiff_t)op->$add)    : NULL;
    op->$delete = op->$delete ? ((char *)op + (ptrdiff_t)op->$delete) : NULL;
    op->$value  = op->$value  ? ((char *)op + (ptrdiff_t)op->$value)  : NULL;

    if (!(((!op->$add    && op->$add_len == 0)    || (in_mem_range(op->$add,    op, data_len) && in_mem_range(op->$add    + op->$add_len,    op, data_len))) &&
          ((!op->$delete && op->$delete_len == 0) || (in_mem_range(op->$delete, op, data_len) && in_mem_range(op->$delete + op->$delete_len, op, data_len))) &&
          ((!op->$value  && op->$value_len == 0)  || (in_mem_range(op->$value,  op, data_len) && in_mem_range(op->$value  + op->$value_len,  op, data_len)))
       )) {
        return NULL;
    }

    return op;
}

/**
 * Parse $alias query from the command args if one exists.
 * @param out a vector for the query.
 *            The SVector must be initialized before calling this function.
 */
static void parse_alias_query(RedisModuleString **argv, int argc, SVector *out) {
    for (int i = 0; i < argc; i += 3) {
        const RedisModuleString *type = argv[i];
        const RedisModuleString *field = argv[i + 1];
        const RedisModuleString *value = argv[i + 2];

        TO_STR(type, field, value);
        char type_code = type_str[0];

        if (type_code == SELVA_MODIFY_ARG_STRING_ARRAY && !strcmp(field_str, "$alias")) {
            const char *s;
            size_t j = 0;
            while ((s = sztok(value_str, value_len, &j))) {
                SVector_Insert(out, (void *)s);
            }
        }
    }
}

/*
 * Request:
 * id, FLAGS type, field, value [, ... type, field, value]]
 * N = No root
 * M = Merge
 *
 * The behavior and meaning of `value` depends on `type` (enum SelvaModify_ArgType).
 *
 * Response:
 * [
 * id,
 * [err | 0 | 1]
 * ...
 * ]
 *
 * err = error in parsing or executing the triplet
 * OK = the triplet made no changes
 * UPDATED = changes made and replicated
 */
int SelvaCommand_Modify(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    SelvaModify_Hierarchy *hierarchy;
    RedisModuleString *id = NULL;
    RedisModuleKey *id_key = NULL;
    struct SelvaObject *obj = NULL;
    SVECTOR_AUTOFREE(alias_query);
    int trigger_created = 0; /* Will be set to 1 if the node was created during this command. */
    bool new_alias = false; /* Set if $alias will be creating new alias(es). */
    int err = REDISMODULE_OK;

    /*
     * The comparator must be NULL to ensure that the vector is always stored
     * as an array as that is required later on for the modify op.
     */
    SVector_Init(&alias_query, 5, NULL);

    /*
     * We expect two fixed arguments and a number of [type, field, value] triplets.
     */
    if (argc < 3 || (argc - 3) % 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *hkey_name = RedisModule_CreateString(ctx, HIERARCHY_DEFAULT_KEY, sizeof(HIERARCHY_DEFAULT_KEY) - 1);
    hierarchy = SelvaModify_OpenHierarchy(ctx, hkey_name, REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * We use the ID generated by the client as the nodeId by default but later
     * on if an $alias entry is found then the following value will be discarded.
     */
    id = argv[1];

    /*
     * Look for $alias that would replace id.
     */
    parse_alias_query(argv + 3, argc - 3, &alias_query);
    if (SVector_Size(&alias_query) > 0) {
        RedisModuleKey *alias_key = open_aliases_key(ctx);

        if (alias_key && RedisModule_KeyType(alias_key) == REDISMODULE_KEYTYPE_HASH) {
            struct SVectorIterator it;
            char *str;

            /*
             * Replace id with the first match from alias_query.
             */
            SVector_ForeachBegin(&it, &alias_query);
            while ((str = SVector_Foreach(&it))) {
                RedisModuleString *tmp_id;

                if (!RedisModule_HashGet(alias_key, REDISMODULE_HASH_CFIELDS, str, &tmp_id, NULL)) {
                    Selva_NodeId nodeId;

                    RedisModuleString2Selva_NodeId(nodeId, tmp_id);

                    if (SelvaModify_HierarchyNodeExists(hierarchy, nodeId)) {
                        id = tmp_id;

                        /*
                         * If no match was found all the aliases should be assigned.
                         * If a match was found the query vector can be cleared now
                         * to prevent any aliases from being created.
                         */
                        SVector_Clear(&alias_query);

                        break;
                    }
                }
            }
        } else {
#if 0
            /* This is probably ok and it's a sign that there are no aliases in the DB yet. */
            fprintf(stderr, "%s:%d: Unable open aliases key or its type is invalid\n",
                    __FILE__, __LINE__);
#endif
            new_alias = true;
        }

        RedisModule_CloseKey(alias_key);
    }

    Selva_NodeId nodeId;
    const unsigned flags = parse_flags(argv[2]);

    RedisModuleString2Selva_NodeId(nodeId, id);

    id_key = RedisModule_OpenKey(ctx, id, REDISMODULE_READ | REDISMODULE_WRITE);
    if (!id_key) {
        replyWithSelvaErrorf(ctx, err, "ERR Failed to open the key for id: \"%s\"", RedisModule_StringPtrLen(id, NULL));
        return REDISMODULE_OK;
    }

    if (RedisModule_KeyType(id_key) == REDISMODULE_KEYTYPE_EMPTY) {
        if (FISSET_UPDATE(flags)) {
            /* if the specified id doesn't exist but $operation: 'update' specified */
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_OK;
        }

        const size_t nr_parents = FISSET_NO_ROOT(flags) ? 0 : 1;

        err = SelvaNode_Initialize(ctx, id_key, id, nodeId);
        if (err) {
            replyWithSelvaErrorf(ctx, err, "ERR Failed to initialize the node for id: \"%s\"", RedisModule_StringPtrLen(id, NULL));
            return REDISMODULE_OK;
        }

        err = SelvaModify_SetHierarchy(ctx, hierarchy, nodeId, nr_parents, ((Selva_NodeId []){ ROOT_NODE_ID }), 0, NULL);
        if (err < 0) {
            replyWithSelvaErrorf(ctx, err, "ERR Failed to initialize the node hierarchy for id: \"%s\"", RedisModule_StringPtrLen(id, NULL));
            return REDISMODULE_OK;
        }

        trigger_created = 1;
    } else if (FISSET_CREATE(flags)) {
        // if the specified id exists but $operation: 'insert' specified
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }

    err = SelvaObject_Key2Obj(id_key, &obj);
    if (err) {
        TO_STR(id);

        return replyWithSelvaErrorf(ctx, err, "Failed to open the object for id: \"%s\"", id_str);
    }

    const struct SelvaModify_HierarchyMetadata *metadata;

    metadata = SelvaModify_HierarchyGetNodeMetadata(hierarchy, nodeId);
    SelvaSubscriptions_FieldChangePrecheck(ctx, hierarchy, nodeId, metadata);

    if (!trigger_created && FISSET_NO_MERGE(flags)) {
        SelvaNode_ClearFields(ctx, obj);
    }

    /*
     * Replication bitmap.
     *
     * bit  desc
     * 0    replicate the first triplet
     * 1    replicate the second triplet
     * ...  ...
     */
    const int nr_triplets = (argc - 3) / 3;
    struct bitmap *replset = RedisModule_PoolAlloc(ctx, BITMAP_ALLOC_SIZE(nr_triplets));

    if (!replset) {
        return replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "Failed to allocate memory for replication");
    }
    replset->nbits = nr_triplets;
    bitmap_erase(replset);

    bool updated = false;
    int has_push = 0;

    /*
     * Parse the rest of the arguments and run the modify operations.
     * Each part of the command will send a separate response back to the client.
     * Each part is also replicated separately.
     */
    RedisModule_ReplyWithArray(ctx, 1 + nr_triplets);
    RedisModule_ReplyWithString(ctx, id);

    for (int i = 3; i < argc; i += 3) {
        const RedisModuleString *type = argv[i];
        const RedisModuleString *field = argv[i + 1];
        RedisModuleString *value = argv[i + 2];

        TO_STR(type, field, value);
        /* [0] always points to a valid char in RM_String. */
        const char type_code = type_str[0];
        const enum SelvaObjectType old_type = SelvaObject_GetType(obj, field);

        if (is_array_field(field_str, field_len)) {
            int idx = get_array_field_index(field_str, field_len);
            int new_len = get_array_field_start_idx(field_str, field_len);

            if (idx == -1) {
                size_t ary_len = SelvaObject_GetArrayLenStr(obj, field_str, new_len);
                idx = ary_len - 1 + has_push;
                if (idx < 0) {
                    replyWithSelvaErrorf(ctx, err, "Unable to set value to array index %d", idx);
                    continue;
                }
            }

            if (type_code == SELVA_MODIFY_ARG_STRING || type_code == SELVA_MODIFY_ARG_DEFAULT_STRING) {
                //  TODO: handle default
                int err = SelvaObject_AssignArrayIndexStr(obj, field_str, new_len, SELVA_OBJECT_STRING, idx, value);
                if (err) {
                    replyWithSelvaErrorf(ctx, err, "Failed to set a string value");
                    continue;
                }

                RedisModule_RetainString(ctx, value);
            } else if (type_code == SELVA_MODIFY_ARG_DOUBLE || type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE) {
                //  TODO: handle default
                union {
                    char s[sizeof(double)];
                    double d;
                } v = {
                    .d = 0.0,
                };
                memcpy(v.s, value_str, sizeof(v.d));
                void *wrapper;
                memcpy(&wrapper, &v.d, sizeof(v.d));
                int err = SelvaObject_AssignArrayIndexStr(obj, field_str, new_len, SELVA_OBJECT_DOUBLE, idx, wrapper);

                if (err) {
                    replyWithSelvaErrorf(ctx, err, "Failed to set a double value");
                    continue;
                }
            } else if (type_code == SELVA_MODIFY_ARG_LONGLONG || type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG) {
                //  TODO: handle default
                union {
                    char s[sizeof(double)];
                    long long ll;
                } v = {
                    .ll = 0,
                };
                memcpy(v.s, value_str, sizeof(v.ll));
                void *wrapper;
                memcpy(&wrapper, &v.ll, sizeof(v.ll));
                int err = SelvaObject_AssignArrayIndexStr(obj, field_str, new_len, SELVA_OBJECT_LONGLONG, idx, wrapper);

                if (err) {
                    replyWithSelvaErrorf(ctx, err, "Failed to set a long value");
                    continue;
                }
            } else if (type_code == SELVA_MODIFY_ARG_OP_OBJ_META) {
                SelvaObjectMeta_t new_user_meta;
                SelvaObjectMeta_t old_user_meta;

                if (value_len < sizeof(SelvaObjectMeta_t)) {
                    replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "Expected: %s", typeof_str(new_user_meta));
                    continue;
                }

                memcpy(&new_user_meta, value_str, sizeof(SelvaObjectMeta_t));
                err = SelvaObject_SetUserMeta(obj, field, new_user_meta, &old_user_meta);
                if (err) {
                    replyWithSelvaErrorf(ctx, err, "Failed to set key metadata (%.*s.%s)",
                            (int)SELVA_NODE_ID_SIZE, nodeId,
                            RedisModule_StringPtrLen(field, NULL));
                    continue;
                }

                if (new_user_meta != old_user_meta) {
                    RedisModule_ReplyWithSimpleString(ctx, "UPDATED");
                } else {
                    RedisModule_ReplyWithSimpleString(ctx, "OK");
                }

                /*
                 * This triplet needs to be replicated.
                 * We replicate it regardless of any changes just in case for now
                 * and we might stop replicate it later on when we are sure that
                 * it isn't necessary.
                 */
                bitmap_set(replset, i / 3 - 1);
                continue;
            } else {
                replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "ERR Invalid operation type with array syntax: \"%c\"", type_code);
                continue;
            }
        } else if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT) {
            const struct SelvaModify_OpIncrement *incrementOpts = (const struct SelvaModify_OpIncrement *)value_str;

            SelvaModify_ModifyIncrement(obj, field, old_type, incrementOpts);
        } else if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE) {
            const struct SelvaModify_OpIncrementDouble *incrementOpts = (const struct SelvaModify_OpIncrementDouble*)value_str;

            SelvaModify_ModifyIncrementDouble(ctx, obj, field, old_type, incrementOpts);
        } else if (type_code == SELVA_MODIFY_ARG_OP_SET) {
            struct SelvaModify_OpSet *setOpts;

            setOpts = SelvaModify_OpSet_align(ctx, value);
            if (!setOpts) {
                replyWithSelvaErrorf(ctx, SELVA_EINVAL, "Invalid OpSet");
                continue;
            }

            err = SelvaModify_ModifySet(ctx, hierarchy, obj, id, field, setOpts);
            if (err == 0) {
                RedisModule_ReplyWithSimpleString(ctx, "OK");
                continue;
            } else if (err < 0) {
                replyWithSelvaError(ctx, err);
                continue;
            }
        } else if (type_code == SELVA_MODIFY_ARG_STRING_ARRAY) {
            /*
             * Currently the $alias query is the only operation using string arrays.
             * $alias: NOP
             */
            if (new_alias) {
                RedisModule_ReplyWithSimpleString(ctx, "UPDATED");
            } else {
                RedisModule_ReplyWithSimpleString(ctx, "OK");
            }

            /* This triplet needs to be replicated. */
            bitmap_set(replset, i / 3 - 1);

            continue;
        } else if (type_code == SELVA_MODIFY_ARG_OP_DEL) {
            err = SelvaModify_ModifyDel(ctx, hierarchy, obj, id, field);
            if (err == SELVA_ENOENT) {
                /* No need to replicate. */
                RedisModule_ReplyWithSimpleString(ctx, "OK");
                continue;
            } else if (err) {
                TO_STR(field);
                char err_msg[120];

                snprintf(err_msg, sizeof(err_msg), "%s; Failed to delete the field: \"%s\"", getSelvaErrorStr(err), field_str);
                RedisModule_ReplyWithError(ctx, err_msg);
                continue;
            }
        } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING ||
                   type_code == SELVA_MODIFY_ARG_STRING) {
            if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING && old_type != SELVA_OBJECT_NULL) {
                RedisModule_ReplyWithSimpleString(ctx, "OK");
                continue;
            }

            RedisModuleString *old_value;
            if (old_type == SELVA_OBJECT_STRING && !SelvaObject_GetString(obj, field, &old_value)) {
                TO_STR(old_value);

                if (old_value_len == value_len && !memcmp(old_value_str, value_str, value_len)) {
                    RedisModule_ReplyWithSimpleString(ctx, "OK");
                    continue;
                }
            }

            err = SelvaObject_SetString(obj, field, value);
            if (err) {
                replyWithSelvaErrorf(ctx, err, "Failed to set a string value");
                continue;
            }
            RedisModule_RetainString(ctx, value);
        } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG ||
                   type_code == SELVA_MODIFY_ARG_LONGLONG) {
            if (type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG && old_type != SELVA_OBJECT_NULL) {
                RedisModule_ReplyWithSimpleString(ctx, "OK");
                continue;
            }

            if (value_len != sizeof(long long)) {
                replyWithSelvaErrorf(ctx, SELVA_EINVAL, "Invalid length for long long");
                continue;
            }

            union {
                char s[sizeof(long long)];
                long long ll;
            } v = {
                .ll = 0,
            };
            memcpy(v.s, value_str, sizeof(v.ll));

            long long old_value;
            if (old_type == SELVA_OBJECT_LONGLONG && !SelvaObject_GetLongLong(obj, field, &old_value)) {
                if (old_value == v.ll) {
                    RedisModule_ReplyWithSimpleString(ctx, "OK");
                    continue;
                }
            }

            SelvaObject_SetLongLong(obj, field, v.ll);
        } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE ||
                   type_code == SELVA_MODIFY_ARG_DOUBLE) {
            if (type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE && old_type != SELVA_OBJECT_NULL) {
                RedisModule_ReplyWithSimpleString(ctx, "OK");
                continue;
            }

            if (value_len != sizeof(double)) {
                replyWithSelvaErrorf(ctx, SELVA_EINVAL, "Invalid length for double");
                continue;
            }

            union {
                char s[sizeof(double)];
                double d;
            } v = {
                .d = 0.0,
            };
            memcpy(v.s, value_str, sizeof(v.d));

            double old_value;
            if (old_type == SELVA_OBJECT_DOUBLE && !SelvaObject_GetDouble(obj, field, &old_value)) {
                if (old_value == v.d) {
                    RedisModule_ReplyWithSimpleString(ctx, "OK");
                    continue;
                }
            }

            SelvaObject_SetDouble(obj, field, v.d);
        } else if (type_code == SELVA_MODIFY_ARG_OP_OBJ_META) {
            SelvaObjectMeta_t new_user_meta;
            SelvaObjectMeta_t old_user_meta;

            if (value_len < sizeof(SelvaObjectMeta_t)) {
                replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "Expected: %s", typeof_str(new_user_meta));
                continue;
            }

            memcpy(&new_user_meta, value_str, sizeof(SelvaObjectMeta_t));
            err = SelvaObject_SetUserMeta(obj, field, new_user_meta, &old_user_meta);
            if (err) {
                replyWithSelvaErrorf(ctx, err, "Failed to set key metadata (%.*s.%s)",
                                     (int)SELVA_NODE_ID_SIZE, nodeId,
                                     RedisModule_StringPtrLen(field, NULL));
                continue;
            }

            if (new_user_meta != old_user_meta) {
                RedisModule_ReplyWithSimpleString(ctx, "UPDATED");
            } else {
                RedisModule_ReplyWithSimpleString(ctx, "OK");
            }

            /*
             * This triplet needs to be replicated.
             * We replicate it regardless of any changes just in case for now
             * and we might stop replicate it later on when we are sure that
             * it isn't necessary.
             */
            bitmap_set(replset, i / 3 - 1);
            continue;
        } else if (type_code == SELVA_MODIFY_ARG_OP_ARRAY_PUSH) {
            uint32_t item_type;
            memcpy(&item_type, value_str, sizeof(uint32_t));

            int err;
            if (item_type == SELVA_OBJECT_OBJECT) {
                // object
                struct SelvaObject *new_obj = SelvaObject_New();
                if (!new_obj) {
                    replyWithSelvaErrorf(ctx, err, "Failed to push new object to array index (%.*s.%s)",
                            (int)field_len, field_str);
                    continue;
                }

                err = SelvaObject_InsertArrayStr(obj, field_str, field_len, SELVA_OBJECT_OBJECT, new_obj);
            } else {
                has_push = 1;
            }

            if (err) {
                replyWithSelvaErrorf(ctx, err, "Failed to push new object to array index (%.*s.%s)",
                        (int)field_len, field_str);
                continue;
            }
        } else if (type_code == SELVA_MODIFY_ARG_OP_ARRAY_INSERT) {
            uint32_t item_type;
            uint32_t insert_idx;
            memcpy(&item_type, value_str, sizeof(uint32_t));
            memcpy(&insert_idx, value_str + sizeof(uint32_t), sizeof(uint32_t));

            fprintf(stderr, "ARRAY INSERT %d %d\n", item_type, insert_idx);

            int err;
            if (item_type == SELVA_OBJECT_OBJECT) {
                // object
                struct SelvaObject *new_obj = SelvaObject_New();
                if (!new_obj) {
                    replyWithSelvaErrorf(ctx, err, "Failed to push new object to array index (%.*s.%s)",
                            (int)field_len, field_str);
                    continue;
                }

                err = SelvaObject_InsertArrayIndexStr(obj, field_str, field_len, SELVA_OBJECT_OBJECT, insert_idx, new_obj);
            } else {
                // TODO
                err = 1;
                // has_push = 1;
            }

            if (err) {
                replyWithSelvaErrorf(ctx, err, "Failed to push new object to array index (%.*s.%s)",
                        (int)field_len, field_str);
                continue;
            }
        } else if (type_code == SELVA_MODIFY_ARG_OP_ARRAY_REMOVE && value_len == sizeof(uint32_t)) {
            uint32_t v;
            memcpy(&v, value_str, sizeof(uint32_t));

            if (v >= 0) {
                int err = SelvaObject_RemoveArrayIndex(obj, field_str, field_len, v);

                if (err) {
                    replyWithSelvaErrorf(ctx, err, "Failed to remove array index (%.*s.%s)",
                            (int)field_len, field_str);
                    continue;
                }
            }
        } else {
            replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "ERR Invalid type: \"%c\"", type_code);
            continue;
        }

        /* This triplet needs to be replicated. */
        bitmap_set(replset, i / 3 - 1);

        /*
         * Publish that the field was changed.
         * Hierarchy handles events for parents and children.
         */
        if (strcmp(field_str, "parents") && strcmp(field_str, "children")) {
            SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, nodeId, metadata, field_str);
        }

#if 0
        fprintf(stderr, "%s:%d: Updated %.*s %s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, nodeId, field_str);
#endif

        RedisModule_ReplyWithSimpleString(ctx, "UPDATED");
        updated = true;
    }

    /*
     * If the size of alias_query is greater than zero it means that no match
     * was found for $alias and we need to create all the aliases listed in the
     * query.
     */
    if (SVector_Size(&alias_query) > 0) {
        RedisModuleString *aliases_field = RedisModule_CreateString(ctx, SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD) - 1);
        struct SVectorIterator it;
        char *alias;

        SVector_ForeachBegin(&it, &alias_query);
        while ((alias = SVector_Foreach(&it))) {
            int err;

            struct SelvaModify_OpSet opSet = {
                .op_set_type = SELVA_MODIFY_OP_SET_TYPE_CHAR,
                .$add = alias,
                .$add_len = strlen(alias) + 1, /* This is safe because the ultimate source is a RedisModuleString. */
                .$delete = NULL,
                .$delete_len = 0,
                .$value = NULL,
                .$value_len = 0,
            };

            err = SelvaModify_ModifySet(ctx, hierarchy, obj, id, aliases_field, &opSet);
            if (err < 0) {
                TO_STR(id);

                /* Since we are already at the end of the command, it's next to
                 * impossible to rollback the command, so we'll just log any
                 * errors received here.
                 */
                fprintf(stderr, "%s:%d: An error occurred while setting an alias \"%s\" -> %s: %s\n",
                        __FILE__, __LINE__, alias, id_str, getSelvaErrorStr(err));
            }
        }
    }

    if (updated) {
        struct timespec ts;

        clock_gettime(CLOCK_REALTIME, &ts);
        const long long now = (long long)ts.tv_sec * 1000 + (long long)ts.tv_nsec / 1000000;

        if (FISSET_UPDATED_AT(flags)) {
            /* `updatedAt` is always updated on change. */
            SelvaObject_SetLongLongStr(obj, SELVA_UPDATED_AT_FIELD, sizeof(SELVA_UPDATED_AT_FIELD) - 1, now);
        }

        if (trigger_created) {
            if (FISSET_CREATED_AT(flags)) {
                SelvaObject_SetLongLongStr(obj, SELVA_CREATED_AT_FIELD, sizeof(SELVA_CREATED_AT_FIELD) - 1, now);
            }
            Selva_Subscriptions_DeferTriggerEvents(ctx, hierarchy, nodeId, SELVA_SUBSCRIPTION_TRIGGER_TYPE_CREATED);
        } else {
            /*
             * If nodeId wasn't created by this command call then it was an
             * update.
             */
            Selva_Subscriptions_DeferTriggerEvents(ctx, hierarchy, nodeId, SELVA_SUBSCRIPTION_TRIGGER_TYPE_UPDATED);
        }
    }

    replicateModify(ctx, replset, argv);
    SelvaSubscriptions_SendDeferredEvents(hierarchy);
    RedisModule_CloseKey(id_key);

    return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {
    // Register the module itself
    if (RedisModule_Init(ctx, "selva", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    /*
     * This mode is currently not supported by Selva and should not be enabled
     * as it will just ignore all errors and make Redis crash.
     */
#if 0
    RedisModule_SetModuleOptions(ctx, REDISMODULE_OPTIONS_HANDLE_IO_ERRORS);
#endif

    if (RedisModule_CreateCommand(ctx, "selva.modify", SelvaCommand_Modify, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "selva.flurpypants", SelvaCommand_Flurpy, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    Selva_Onload **onload_p;

    SET_FOREACH(onload_p, selva_onload) {
        Selva_Onload *onload = *onload_p;
        int err;

        err = onload(ctx);
        if (err) {
            return err;
        }
    }

    return REDISMODULE_OK;
}

//int RedisModule_OnUnload(RedisModuleCtx *ctx) {
/*
 * Here we could use RedisModule_OnUnload() if it was called on exit, but it
 * isn't. Therefore, we use the destructor attribute that is almost always
 * called before the process terminates. As a side note, OnUnload would be never
 * called for Selva because Redis can't unload modules exporting types or
 * something.
 */
__attribute__((destructor))
int _Selva_OnUnload() {
    Selva_Onunload **onunload_p;

    SET_FOREACH(onunload_p, selva_onunld) {
        Selva_Onunload *onunload = *onunload_p;
        int err;

        err = onunload();
        if (err) {
            return err;
        }
    }

    return REDISMODULE_OK;
}
