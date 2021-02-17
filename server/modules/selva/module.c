#include <math.h>
#include <time.h>

#include "cdefs.h"
#include "redismodule.h"
#include "rmutil/util.h"
#include "rmutil/strings.h"
#include "rmutil/test_util.h"
#include "selva.h"
#include "selva_onload.h"
#include "svector.h"
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

#if __SIZEOF_INT128__ != 16
#error The compiler and architecture must have Tetra-Integer support
#endif
typedef unsigned __int128 replset_t;

SET_DECLARE(selva_onload, Selva_Onload);

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

static inline void clear_replset(replset_t *r) {
    *r = 0;
}

static inline void set_replset(replset_t *r, int i) {
    *r |= (replset_t)1 << i;
}

static inline int get_replset(const replset_t *r, int i) {
    return (int)((*r >> i) & 1);
}

/*
 * Replicate the selva.modify command.
 * This function depends on the argument order of selva.modify.
 */
void replicateModify(RedisModuleCtx *ctx, const replset_t *r, RedisModuleString **orig_argv) {
    /*
     * REDISMODULE_CTX_FLAGS_REPLICATED would be more approriate here but it's
     * unclear whether it's available only in newer server versions.
     */
    if (RedisModule_GetContextFlags(ctx) & REDISMODULE_CTX_FLAGS_SLAVE) {
        return; /* Skip. */
    }

    /*
     * Redis doesn't have an external API to call commands nor replication the
     * same way as it delivers them. Also the API is quite horrible because it
     * only provides variadic function for replication. Therefore, we need to
     * do a little hack here make dynamic arguments work.
     */
    const int leading_args = 2;
    const int max_argc = 128; /* This size depends on the size of replset_t */
    int argc = 0;
    RedisModuleString **argv;
    char fmt[1 + leading_args + max_argc + 1];

    argv = RedisModule_PoolAlloc(ctx, max_argc * sizeof(RedisModuleString *));
    if (!argv) {
        fprintf(stderr, "%s: Replication: %s\n", __FILE__, getSelvaErrorStr(SELVA_ENOMEM));
        return;
    }

    fmt[0] = 'A'; /* Supress the AOF channel */
    fmt[1] = 's'; /* nodeId */
    fmt[2] = 's'; /* Flags */

    char *fmt_p = fmt + 1 + leading_args;
    int i_arg_type = 1 + leading_args;
    for (int i = 0; i < max_argc; i++) {
        if (get_replset(r, i)) {
            argv[argc++] = orig_argv[i_arg_type];
            argv[argc++] = orig_argv[i_arg_type + 1];
            argv[argc++] = orig_argv[i_arg_type + 2];
            *fmt_p++ = 's';
            *fmt_p++ = 's';
            *fmt_p++ = 's';
        }
        i_arg_type += 3;
    }
    *fmt_p = '\0';

    if (argc == 0) {
        /* Nothing to replicate. */
        return;
    }

    /* This call must have max_argc argv arguments. */
    const int err = RedisModule_Replicate(ctx, RedisModule_StringPtrLen(orig_argv[0], NULL), fmt,
                                          orig_argv[1], orig_argv[2],
                                          argv[0], argv[1], argv[2],
                                          argv[3], argv[4], argv[5],
                                          argv[6], argv[7], argv[8],
                                          argv[9], argv[10], argv[11],
                                          argv[12], argv[13], argv[14],
                                          argv[15], argv[16], argv[17],
                                          argv[18], argv[19], argv[20],
                                          argv[21], argv[22], argv[23],
                                          argv[24], argv[25], argv[26],
                                          argv[27], argv[28], argv[29],
                                          argv[30], argv[31], argv[32],
                                          argv[33], argv[34], argv[35],
                                          argv[36], argv[37], argv[38],
                                          argv[39], argv[40], argv[41],
                                          argv[42], argv[43], argv[44],
                                          argv[45], argv[46], argv[47],
                                          argv[48], argv[49], argv[50],
                                          argv[51], argv[52], argv[53],
                                          argv[54], argv[55], argv[56],
                                          argv[57], argv[58], argv[59],
                                          argv[60], argv[61], argv[62],
                                          argv[63], argv[64], argv[65],
                                          argv[66], argv[67], argv[68],
                                          argv[69], argv[70], argv[71],
                                          argv[72], argv[73], argv[74],
                                          argv[75], argv[76], argv[77],
                                          argv[78], argv[79], argv[80],
                                          argv[81], argv[82], argv[83],
                                          argv[84], argv[85], argv[86],
                                          argv[87], argv[88], argv[89],
                                          argv[90], argv[91], argv[92],
                                          argv[93], argv[94], argv[95],
                                          argv[96], argv[97], argv[98],
                                          argv[99], argv[100], argv[101],
                                          argv[102], argv[103], argv[104],
                                          argv[105], argv[106], argv[107],
                                          argv[108], argv[109], argv[110],
                                          argv[111], argv[112], argv[113],
                                          argv[114], argv[115], argv[116],
                                          argv[117], argv[118], argv[119],
                                          argv[120], argv[121], argv[122],
                                          argv[123], argv[124], argv[125],
                                          argv[126], argv[127]);
    if (err) {
        fprintf(stderr, "%s: Replication failed: %d\n", __FILE__, err);
    }
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

static void RedisModuleString2Selva_NodeId(Selva_NodeId nodeId, RedisModuleString *id) {
    TO_STR(id)

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

static int parse_flags(RedisModuleString *arg) {
    TO_STR(arg)
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

static struct SelvaModify_OpSet *SelvaModify_OpSet_align(RedisModuleCtx *ctx, struct RedisModuleString *data) {
    TO_STR(data)
    struct SelvaModify_OpSet *op;

    if (!data_str && data_len < sizeof(struct SelvaModify_OpSet)) {
        return NULL;
    }

    op = RedisModule_PoolAlloc(ctx, data_len);
    if (!op) {
        return NULL;
    }

    memcpy(op, data_str, data_len);
    op->$add    = op->$add    ? (char *)((char *)op + (ptrdiff_t)op->$add)    : NULL;
    op->$delete = op->$delete ? (char *)((char *)op + (ptrdiff_t)op->$delete) : NULL;
    op->$value  = op->$value  ? (char *)((char *)op + (ptrdiff_t)op->$value)  : NULL;

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
        RedisModuleString *type = argv[i];
        RedisModuleString *field = argv[i + 1];
        RedisModuleString *value = argv[i + 2];

        TO_STR(type, field, value)
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
    svector_autofree SVector alias_query;
    int trigger_created = 0; /* Will be set to 1 if the node was created during this command. */
    int err = REDISMODULE_OK;

    /*
     * The comparator must be NULL to ensure that the vector is always stored
     * as an array as that is required later on for the modify op.
     */
    SVector_Init(&alias_query, 5, NULL);

    /* We expect two fixed arguments and a number of [type, field, value] triplets. */
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
            fprintf(stderr, "%s: Unable open aliases key or its type is invalid\n", __FILE__);
#endif
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
        if (err) {
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
        TO_STR(id)

        replyWithSelvaErrorf(ctx, err, "Failed to open the object for id: \"%s\"", id_str);
    }

    struct SelvaModify_HierarchyMetadata *metadata;

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
    replset_t repl_set;
    clear_replset(&repl_set);
    const int nr_triplets = (argc - 3) / 3;

    /*
     * Parse the rest of the arguments and run the modify operations.
     * Each part of the command will send a separate response back to the client.
     * Each part is also replicated separately.
     */
    RedisModule_ReplyWithArray(ctx, 1 + nr_triplets);
    RedisModule_ReplyWithString(ctx, id);

    for (int i = 3; i < argc; i += 3) {
        bool publish = true;
        RedisModuleString *type = argv[i];
        RedisModuleString *field = argv[i + 1];
        RedisModuleString *value = argv[i + 2];

        TO_STR(type, field, value)
        /* [0] always points to a valid char in RM_String. */
        const char type_code = type_str[0];
        const enum SelvaObjectType old_type = SelvaObject_GetType(obj, field);

        if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT) {
            struct SelvaModify_OpIncrement *incrementOpts = (struct SelvaModify_OpIncrement *)value_str;

            SelvaModify_ModifyIncrement(obj, field, old_type, incrementOpts);
        } else if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE) {
            struct SelvaModify_OpIncrementDouble *incrementOpts = (struct SelvaModify_OpIncrementDouble*)value_str;

            SelvaModify_ModifyIncrementDouble(ctx, obj, field, old_type, incrementOpts);
        } else if (type_code == SELVA_MODIFY_ARG_OP_SET) {
            struct SelvaModify_OpSet *setOpts;

            setOpts = SelvaModify_OpSet_align(ctx, value);
            if (!setOpts) {
                replyWithSelvaErrorf(ctx, SELVA_EINVAL, "Invalid OpSet");
                continue;
            }

            /*
             * Hierarchy will handle events for parents and children.
             */
            if (!strcmp(field_str, "parents") || !strcmp(field_str, "children")) {
                publish = false;
            }

            err = SelvaModify_ModifySet(ctx, hierarchy, obj, id, field, setOpts);
            if (err == 0) {
                publish = false;
                RedisModule_ReplyWithSimpleString(ctx, "OK");
                continue;
            } else if (err < 0) {
                replyWithSelvaError(ctx, err);
                continue;
            }
        } else if (type_code == SELVA_MODIFY_ARG_STRING_ARRAY) {
            /*
             * Currently $alias is the only field using string arrays.
             * $alias: NOP
             */
        } else if (type_code == SELVA_MODIFY_ARG_OP_DEL) {
            err = SelvaModify_ModifyDel(ctx, hierarchy, obj, id, field);
            if (err) {
                TO_STR(field)
                char err_msg[120];

                snprintf(err_msg, sizeof(err_msg), "%s; Failed to delete the field: \"%s\"", getSelvaErrorStr(err), field_str);
                RedisModule_ReplyWithError(ctx, err_msg);
                continue;
            }
        } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING ||
                   type_code == SELVA_MODIFY_ARG_STRING) {
            if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING && old_type != SELVA_OBJECT_NULL) {
                publish = false;
                RedisModule_ReplyWithSimpleString(ctx, "OK");
                continue;
            }

            RedisModuleString *old_value;
            if (old_type == SELVA_OBJECT_STRING && !SelvaObject_GetString(obj, field, &old_value)) {
                TO_STR(old_value)

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
                publish = false;
                RedisModule_ReplyWithSimpleString(ctx, "OK");
                continue;
            }

            if (value_len != sizeof(long long)) {
                replyWithSelvaErrorf(ctx, SELVA_EINVAL, "Invalid length for long long");
                continue;
            }

            TO_STR(value)
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
                publish = false;
                RedisModule_ReplyWithSimpleString(ctx, "OK");
                continue;
            }

            if (value_len != sizeof(double)) {
                replyWithSelvaErrorf(ctx, SELVA_EINVAL, "Invalid length for double");
                continue;
            }

            TO_STR(value)
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
        } else {
            replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "ERR Invalid type: \"%c\"", type_code);
            continue;
        }

        /* This triplet needs to be replicated. */
        set_replset(&repl_set, i / 3 - 1);

        if (publish) {
            SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, nodeId, metadata, field_str);
        }

        RedisModule_ReplyWithSimpleString(ctx, "UPDATED");
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
                .$add_len = strlen(alias) + 1,
                .$delete = NULL,
                .$delete_len = 0,
                .$value = NULL,
                .$value_len = 0,
            };

            err = SelvaModify_ModifySet(ctx, hierarchy, obj, id, aliases_field, &opSet);
            if (err < 0) {
                TO_STR(id)

                /* Since we are already at the end of the command, it's next to
                 * impossible to rollback the command, so we'll just log any
                 * errors received here.
                 */
                fprintf(stderr, "%s:%d: An error occurred while setting an alias \"%s\" -> %s: %s\n",
                        __FILE__, __LINE__, alias, id_str, getSelvaErrorStr(err));
            }
        }
    }

    /*
     * If there is anything set in repl_set it means that something was changed for nodeId.
     */
    if (repl_set) {
        struct timespec ts;

        clock_gettime(CLOCK_REALTIME, &ts);
        const long long now = (long long)ts.tv_sec * 1000 + (long long)ts.tv_nsec / 1000000;

        replicateModify(ctx, &repl_set, argv);

        /*
         * In case this DB node is a replica:
         * Here we are in a hope that even though replicas won't actually
         * replicate anything any further, we should still end up into this
         * `if` branch and thus we should be able to do two things:
         * 1) set `createdAt` and `updatedAt` fields;
         * 2) publish events for any active subscriptions.
         *
         * Thref're, curs'd is the one who is't optimizes out the replication
         * code in the case the hest is running on a replica.
         */

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
    SelvaSubscriptions_SendDeferredEvents(hierarchy);
    RedisModule_CloseKey(id_key);

    return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {
    // Register the module itself
    if (RedisModule_Init(ctx, "selva", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

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
