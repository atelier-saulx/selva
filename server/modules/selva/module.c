#include <math.h>

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
#include "module/subscriptions.h"

#define FLAG_NO_ROOT    0x1
#define FLAG_NO_MERGE   0x2

#define FISSET_NO_ROOT(m) (((m) & FLAG_NO_ROOT) == FLAG_NO_ROOT)
#define FISSET_NO_MERGE(m) (((m) & FLAG_NO_MERGE) == FLAG_NO_MERGE)

#if __SIZEOF_INT128__ != 16
#error The compiler and architecture must have Tetra-Integer support
#endif
typedef unsigned __int128 replset_t;

SET_DECLARE(selva_onload, Selva_Onload);

/*
 * Technically a nodeId is always 10 bytes but sometimes a printable
 * representation without padding zeroes is needed.
 */
size_t Selva_NodeIdLen(Selva_NodeId nodeId) {
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
     * Redis doesn't have an external API to call commands nor replication the
     * same way as it delivers them. Also the API is quite horrible because it
     * only provides variadic function for replication. Therefore, we need to
     * do a little hack here make dynamic arguments work.
     */
    const int max_argc = 128; /* This size depends on the size of replset_t */
    int argc = 0;
    RedisModuleString **argv;
    char fmt[3 + max_argc + 1];

    argv = RedisModule_PoolAlloc(ctx, max_argc * sizeof(RedisModuleString *));
    if (!argv) {
        fprintf(stderr, "Replication: %s\n", getSelvaErrorStr(SELVA_ENOMEM));
    }

    fmt[0] = 's';
    fmt[1] = 's';
    fmt[2] = 's';

    int i_arg_type = 3;
    for (int i = 0; i < max_argc; i++) {
        if (get_replset(r, i)) {
            argv[argc] = orig_argv[i_arg_type];
            argv[argc + 1] = orig_argv[i_arg_type + 1];
            argv[argc + 2] = orig_argv[i_arg_type + 2];
            fmt[argc + 3] = 's';
            fmt[argc + 4] = 's';
            fmt[argc + 5] = 's';
            argc += 3;
        }
        i_arg_type += 3;
    }
    fmt[argc + 3] = '\0';

    // TODO Remove
    fprintf(stderr, "Replicating %d changes, \"%s\"\n", argc / 3, fmt);

    if (argc == 0) {
        /* Nothing to replicate. */
        return;
    }

    // TODO Remove
    for (int i = 0; i < argc; i++) {
        const RedisModuleString *arg = argv[i];

        fprintf(stderr, "arg[%d] = %s\n", i, arg ? RedisModule_StringPtrLen(arg, NULL) : "NULL");
    }

    RedisModule_Replicate(ctx, RedisModule_StringPtrLen(orig_argv[0], NULL), fmt,
                          orig_argv[1], orig_argv[2], orig_argv[3],
                          argv[0],
                          argv[1],
                          argv[2],
                          argv[3],
                          argv[4],
                          argv[5],
                          argv[6],
                          argv[7],
                          argv[8],
                          argv[9],
                          argv[10],
                          argv[11],
                          argv[12],
                          argv[13],
                          argv[14],
                          argv[15],
                          argv[16],
                          argv[17],
                          argv[18],
                          argv[19],
                          argv[20],
                          argv[21],
                          argv[22],
                          argv[23],
                          argv[24],
                          argv[25],
                          argv[26],
                          argv[27],
                          argv[28],
                          argv[29],
                          argv[30],
                          argv[31],
                          argv[32],
                          argv[33],
                          argv[34],
                          argv[35],
                          argv[36],
                          argv[37],
                          argv[38],
                          argv[39],
                          argv[40],
                          argv[41],
                          argv[42],
                          argv[43],
                          argv[44],
                          argv[45],
                          argv[46],
                          argv[47],
                          argv[48],
                          argv[49],
                          argv[50],
                          argv[51],
                          argv[52],
                          argv[53],
                          argv[54],
                          argv[55],
                          argv[56],
                          argv[57],
                          argv[58],
                          argv[59],
                          argv[60],
                          argv[61],
                          argv[62],
                          argv[63],
                          argv[64],
                          argv[65],
                          argv[66],
                          argv[67],
                          argv[68],
                          argv[69],
                          argv[70],
                          argv[71],
                          argv[72],
                          argv[73],
                          argv[74],
                          argv[75],
                          argv[76],
                          argv[77],
                          argv[78],
                          argv[79],
                          argv[80],
                          argv[81],
                          argv[82],
                          argv[83],
                          argv[84],
                          argv[85],
                          argv[86],
                          argv[87],
                          argv[88],
                          argv[89],
                          argv[90],
                          argv[91],
                          argv[92],
                          argv[93],
                          argv[94],
                          argv[95],
                          argv[96],
                          argv[97],
                          argv[98],
                          argv[99],
                          argv[100],
                          argv[101],
                          argv[102],
                          argv[103],
                          argv[104],
                          argv[105],
                          argv[106],
                          argv[107],
                          argv[108],
                          argv[109],
                          argv[110],
                          argv[111],
                          argv[112],
                          argv[113],
                          argv[114],
                          argv[115],
                          argv[116],
                          argv[117],
                          argv[118],
                          argv[119],
                          argv[120],
                          argv[121],
                          argv[122],
                          argv[123],
                          argv[124],
                          argv[125],
                          argv[126],
                          argv[127]);
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
    TO_STR(id);

    id_str = RedisModule_StringPtrLen(id, &id_len);
    memset(nodeId, '\0', SELVA_NODE_ID_SIZE);
    memcpy(nodeId, id_str, min(id_len, SELVA_NODE_ID_SIZE));
}

static const char *sztok(const char *s, size_t size, size_t * restrict i) {
    const char *r = NULL;

    if (*i < size - 1) {
        r = s + *i;
        *i = *i + strnlen(r, size) + 1;
    }

    return r;
}

static int parse_flags(RedisModuleString *arg) {
    TO_STR(arg);
    int flags = 0;

    for (size_t i = 0; i < arg_len; i++) {
        flags |= arg_str[i] == 'N' ? FLAG_NO_ROOT : 0;
        flags |= arg_str[i] == 'M' ? FLAG_NO_MERGE : 0;
    }

    return flags;
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

static RedisModuleKey *open_node(RedisModuleCtx *ctx, SelvaModify_Hierarchy *hierarchy, RedisModuleString *id, Selva_NodeId nodeId, int no_root) {
    /*
     * If this is a new node we need to create a hierarchy node for it.
     */
    if (!SelvaModify_HierarchyNodeExists(hierarchy, nodeId)) {
        size_t nr_parents = unlikely(no_root) ? 0 : 1;

        int err = SelvaModify_SetHierarchy(ctx, hierarchy, nodeId, nr_parents, ((Selva_NodeId []){ ROOT_NODE_ID }), 0, NULL);
        if (err) {
            replyWithSelvaError(ctx, err);
            return NULL;
        }
    }

    return RedisModule_OpenKey(ctx, id, REDISMODULE_WRITE);
}

/*
 * Request:
 * id, FLAGS type, key, value [, ... type, key, value]]
 * N = No root
 * M = Merge
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
    svector_autofree SVector alias_query;
    int err = REDISMODULE_OK;

    SVector_Init(&alias_query, 5, NULL);

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
            char **it;

            /*
             * Replace id with the first match from alias_query.
             */
            SVECTOR_FOREACH(it, &alias_query) {
                RedisModuleString *tmp_id;

                if (!RedisModule_HashGet(alias_key, REDISMODULE_HASH_CFIELDS, *it, &tmp_id, NULL)) {
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
            fprintf(stderr, "%s: Unable open aliases key or its type is invalid\n", __FILE__);
        }

        RedisModule_CloseKey(alias_key);
    }

    const unsigned flags = parse_flags(argv[2]);
    const int no_root = FISSET_NO_ROOT(flags);
    Selva_NodeId nodeId;

    RedisModuleString2Selva_NodeId(nodeId, id);
    id_key = open_node(ctx, hierarchy, id, nodeId, no_root);
    if (!id_key) {
        TO_STR(id);
        char err_msg[80];

        snprintf(err_msg, sizeof(err_msg), "ERR Failed to open the key for id: \"%s\"", id_str);
        RedisModule_ReplyWithError(ctx, err_msg);
        return REDISMODULE_ERR;
    }

    struct SelvaModify_HierarchyMetadata *metadata;

    /*
     * TODO Getting the metadata this way might slow us down a bit.
     */
    metadata = SelvaModify_HierarchyGetNodeMetadata(hierarchy, nodeId);
    SelvaSubscriptions_FieldChangePrecheck(hierarchy, nodeId, metadata);


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
     * TODO Each part of the command will send a separate response back to the client.
     * Each part is also replicated separately.
     */
    RedisModule_ReplyWithArray(ctx, 1 + nr_triplets);
    RedisModule_ReplyWithString(ctx, id);

    for (int i = 3; i < argc; i += 3) {
        bool publish = true;
        RedisModuleString *type = argv[i];
        RedisModuleString *field = argv[i + 1];
        RedisModuleString *value = argv[i + 2];

        TO_STR(type, field, value);
        char type_code = type_str[0];

        size_t current_value_len = 0;
        RedisModuleString *current_value = NULL;
        const char *current_value_str = NULL;

        if (!RedisModule_HashGet(id_key, REDISMODULE_HASH_NONE, field, &current_value, NULL)) {
            current_value_str = RedisModule_StringPtrLen(current_value, &current_value_len);
        }

        if (type_code != SELVA_MODIFY_ARG_OP_INCREMENT && type_code != SELVA_MODIFY_ARG_OP_SET &&
            current_value_str && current_value_len == value_len &&
            !memcmp(current_value_str, value_str, value_len)) {
#if 0
            printf("Current value is equal to the specified value for key %s and value %s\n",
                   field_str, value_str);
#endif
            RedisModule_ReplyWithSimpleString(ctx, "OK");
            continue;
        }

        if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT) {
            struct SelvaModify_OpIncrement *incrementOpts = (struct SelvaModify_OpIncrement *)value_str;
            SelvaModify_ModifyIncrement(ctx, id_key, field, current_value, incrementOpts);
        } else if (type_code == SELVA_MODIFY_ARG_OP_SET) {
            struct SelvaModify_OpSet *setOpts;

            setOpts = SelvaModify_OpSet_align(value);
            if (!setOpts) {
                char err_msg[80];

                snprintf(err_msg, sizeof(err_msg), "ERR Invalid argument at: %d", i);
                RedisModule_ReplyWithError(ctx, err_msg);
                continue;
            }

            /*
             * Hierarchy will handle events for parents and children.
             */
            if (!strcmp(field_str, "parents") || !strcmp(field_str, "children")) {
                publish = false;
            }

            err = SelvaModify_ModifySet(ctx, hierarchy, id_key, id, field, setOpts);
            if (err) {
                replyWithSelvaError(ctx, err);
                continue;
            }
        } else if (type_code == SELVA_MODIFY_ARG_STRING_ARRAY) {
            /*
             * $merge:
             */
            if (FISSET_NO_MERGE(flags) && !strcmp(field_str, "$merge")) {
                /* TODO Implement $merge */
            }
            /*
             * $alias: NOP
             */
        } else if (type_code == SELVA_MODIFY_ARG_OP_DEL) {
            err = SelvaModify_ModifyDel(ctx, hierarchy, id_key, id, field, value_str);
            if (err) {
                TO_STR(field);
                char err_msg[120];

                snprintf(err_msg, sizeof(err_msg), "%s; Failed to delete the field: \"%s\"", getSelvaErrorStr(err), field_str);
                RedisModule_ReplyWithError(ctx, err_msg);
                continue;
            }
        } else {
            if (type_code == SELVA_MODIFY_ARG_DEFAULT) {
                if (current_value != NULL) {
                    publish = false;
                } else {
                    RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);
                }
            } else if (type_code == SELVA_MODIFY_ARG_VALUE) {
                RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);
            } else {
                char err_msg[80];

                snprintf(err_msg, sizeof(err_msg), "ERR Invalid type: \"%c\"", type_code);
                RedisModule_ReplyWithError(ctx, err_msg);
                continue;
            }
        }

        /* This triplet needs to be replicated. */
        set_replset(&repl_set, i / 3 - 1);

        if (publish) {
            SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, nodeId, metadata, field_str);
        }

        RedisModule_ReplyWithSimpleString(ctx, "UPDATED");
    }

    /*
     * If the size of alias_query is greater than zero it means that no match
     * was found for $alias and we need to create all the aliases listed in the
     * query.
     * We know that the aliases are in an array so it's enough to get the
     * address of the first alias to have access to the whole array.
     */
    if (SVector_Size(&alias_query) > 0) {
        RedisModuleString *field = RedisModule_CreateString(ctx, "aliases", 7);
        struct SelvaModify_OpSet opSet = {
            .is_reference = 0,
            .$add = SVector_Peek(&alias_query),
            .$add_len = SVector_Size(&alias_query),
            .$delete = NULL,
            .$delete_len = 0,
            .$value = NULL,
            .$value_len = 0,
        };

        (void)SelvaModify_ModifySet(ctx, hierarchy, id_key, id, field, &opSet);
    }

    replicateModify(ctx, &repl_set, argv);
    SelvaSubscriptions_SendDeferredEvents(hierarchy);
    RedisModule_CloseKey(id_key);

    return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {
    // Register the module itself
    if (RedisModule_Init(ctx, "selva", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "selva.modify", SelvaCommand_Modify, "readonly", 1, 1, 1) ==
            REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "selva.flurpypants", SelvaCommand_Flurpy, "readonly", 1, 1,
                                                                1) == REDISMODULE_ERR) {
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
