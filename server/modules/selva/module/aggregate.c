#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "selva.h"
#include "redismodule.h"
#include "arg_parser.h"
#include "errors.h"
#include "hierarchy.h"
#include "modify.h"
#include "rpn.h"
#include "selva_node.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "subscriptions.h"
#include "svector.h"
#include "traversal.h"

struct AggregateCommand_Args {
    RedisModuleCtx *ctx;
    RedisModuleString *lang;

    RedisModuleString **field_names;
    size_t nr_fields;
    ssize_t nr_results; /*!< Number of results sent. */
};

static int AggregateCommand_NodeCb(struct SelvaModify_HierarchyNode *node, void *arg) {
    Selva_NodeId nodeId;
    RedisModuleKey *key = NULL;
    struct SelvaObject *obj;
    struct AggregateCommand_Args *restrict args = (struct AggregateCommand_Args *)arg;
    int err;

    SelvaModify_HierarchyGetNodeId(nodeId, node);
    err = open_node_key(args->ctx, nodeId, &key, &obj);
    if (err) {
        fprintf(stderr, "%s:%d: Failed to open a node object. nodeId: %.*s error: %s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, nodeId,
                getSelvaErrorStr(err));
        /* Ignore errors. */
        return 0;
    }

    // TODO

    RedisModule_CloseKey(key);
    return 0;
}

/**
 * Find node in set.
 * SELVA.inherit REDIS_KEY NODE_ID [TYPE1[TYPE2[...]]] [FIELD_NAME1[ FIELD_NAME2[ ...]]]
 */
int SelvaAggregateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_LANG      = 1;
    const int ARGV_REDIS_KEY = 2;
    const int ARGV_ORDER_TXT = 3;
    const int ARGV_ORDER_FLD = 4;
    const int ARGV_ORDER_ORD = 5;
    int ARGV_OFFSET_TXT      = 3;
    int ARGV_OFFSET_NUM      = 4;
    int ARGV_LIMIT_TXT       = 3;
    int ARGV_LIMIT_NUM       = 4;
    int ARGV_FIELDS_TXT      = 3;
    int ARGV_FIELDS_VAL      = 4;
    int ARGV_NODE_IDS        = 3;
    int ARGV_FILTER_EXPR     = 4;
    int ARGV_FILTER_ARGS     = 5;
#define SHIFT_ARGS(i) \
    ARGV_OFFSET_TXT += i; \
    ARGV_OFFSET_NUM += i; \
    ARGV_LIMIT_TXT += i; \
    ARGV_LIMIT_NUM += i; \
    ARGV_FIELDS_TXT += i; \
    ARGV_FIELDS_VAL += i; \
    ARGV_NODE_IDS += i; \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    if (argc < 5) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *lang = argv[ARGV_LANG];

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Parse the order arg.
     */
    enum hierarchy_result_order order = HIERARCHY_RESULT_ORDER_NONE;
    const RedisModuleString *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = parse_order(&order_by_field, &order,
                          argv[ARGV_ORDER_TXT],
                          argv[ARGV_ORDER_FLD],
                          argv[ARGV_ORDER_ORD]);
        if (err == 0) {
            SHIFT_ARGS(3);
        } else if (err != SELVA_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "order");
        }
    }

    /*
     * Parse the offset arg.
     */
    ssize_t offset = 0;
    if (argc > ARGV_OFFSET_NUM) {
        err = SelvaArgParser_IntOpt(&offset, "offset", argv[ARGV_OFFSET_TXT], argv[ARGV_OFFSET_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "offset");
        }
    }

    /*
     * Parse the limit arg. -1 = inf
     */
    ssize_t limit = -1;
    if (argc > ARGV_LIMIT_NUM) {
        err = SelvaArgParser_IntOpt(&limit, "limit", argv[ARGV_LIMIT_TXT], argv[ARGV_LIMIT_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "limit");
        }
    }

    /*
     * Parse fields.
     */
    selvaobject_autofree struct SelvaObject *fields = NULL;
    if (argc > ARGV_FIELDS_VAL) {
        err = SelvaArgsParser_StringSetList(ctx, &fields, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "fields");
        }
    }

    size_t nr_reg = argc - ARGV_FILTER_ARGS + 1;
    struct rpn_ctx *rpn_ctx = rpn_init(nr_reg);
    if (!rpn_ctx) {
        return replyWithSelvaError(ctx, SELVA_ENOMEM);
    }

    const RedisModuleString *ids = argv[ARGV_NODE_IDS];
    const RedisModuleString *filter = argv[ARGV_FILTER_EXPR];
    TO_STR(ids, filter);

    /*
     * Compile the filter expression.
     */
    struct rpn_expression *filter_expression = rpn_compile(filter_str);
    if (!filter_expression) {
        rpn_destroy(rpn_ctx);
        return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
    }

    /*
     * Get the filter expression arguments and set them to the registers.
     */
    for (int i = ARGV_FILTER_ARGS; i < argc; i++) {
        /* reg[0] is reserved for the current nodeId */
        const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
        size_t str_len;
        const char *str = RedisModule_StringPtrLen(argv[i], &str_len);

        rpn_set_reg(rpn_ctx, reg_i, str, str_len + 1, 0);
    }

    SVECTOR_AUTOFREE(order_result); /*!< for ordered result. */
    if (order != HIERARCHY_RESULT_ORDER_NONE &&
        !SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, getOrderFunc(order))) {
        replyWithSelvaError(ctx, SELVA_ENOMEM);
        goto out;
    }

    ssize_t array_len = 0;
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run the filter for each node.
     */
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        struct SelvaModify_HierarchyNode *node;
        ssize_t tmp_limit = -1;
        struct FindCommand_Args args = {
            .ctx = ctx,
            .lang = lang,
            .hierarchy = hierarchy,
            .nr_nodes = &array_len,
            .offset = (order == HIERARCHY_RESULT_ORDER_NONE) ? offset : 0,
            .limit = (order == HIERARCHY_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .merge_strategy = MERGE_STRATEGY_NONE,
            .merge_path = NULL,
            .merge_nr_fields = 0,
            .fields = fields,
            .order_field = order_by_field,
            .order_result = &order_result,
        };

        node = SelvaHierarchy_FindNode(hierarchy, ids_str + i);
        // if (node) {
        //     (void)FindCommand_NodeCb(node, &args);
        // }
    }

    /*
     * If an ordered request was requested then nothing was sent to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        // array_len = FindCommand_PrintOrderedResult(ctx, lang, hierarchy, offset, limit, MERGE_STRATEGY_NONE, NULL, fields, &order_result, NULL);
    }

    RedisModule_ReplySetArrayLength(ctx, array_len);

out:
    rpn_destroy(rpn_ctx);
#if MEM_DEBUG
    memset(filter_expression, 0, sizeof(*filter_expression));
#endif
    rpn_destroy_expression(filter_expression);

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

static int Aggregate_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.aggregate", SelvaAggregateCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Aggregate_OnLoad);
