#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "arg_parser.h"
#include "errors.h"
#include "hierarchy.h"
#include "rpn.h"
#include "selva.h"
#include "selva_node.h"
#include "selva_onload.h"
#include "subscriptions.h"
#include "svector.h"

enum SelvaModify_Hierarchy_Algo {
    HIERARCHY_BFS,
    HIERARCHY_DFS,
};

struct FindCommand_Args {
    RedisModuleCtx *ctx;

    ssize_t *nr_nodes; /*!< Number of nodes in the result. */
    ssize_t offset; /*!< Start from nth node. */
    ssize_t *limit; /*!< Limit the number of result. */

    struct rpn_ctx *rpn_ctx;
    const rpn_token *filter;

    const RedisModuleString *order_field; /*!< Order by field name; Otherwise NULL. */
    SVector *order_result; /*!< Result of the find. Only used if sorting is requested. */

    struct Selva_SubscriptionMarker *marker; /*!< Used by FindInSub. */
};

enum hierarchy_result_order {
    HIERARCHY_RESULT_ORDER_NONE,
    HIERARCHY_RESULT_ORDER_ASC,
    HIERARCHY_RESULT_ORDER_DESC,
};

enum FindCommand_OrderedItemType {
    ORDERED_ITEM_TYPE_EMPTY,
    ORDERED_ITEM_TYPE_TEXT,
    ORDERED_ITEM_TYPE_DOUBLE,
};

struct FindCommand_OrderedItem {
    Selva_NodeId id;
    enum FindCommand_OrderedItemType data_type;
    double d;
    size_t data_len;
    char data[];
};

typedef int (*orderFunc)(const void ** restrict a_raw, const void ** restrict b_raw);

/*
 * txt = "order"
 * fld = field_name
 * ord = asc|desc
 */
static int parse_order(
        const RedisModuleString **order_by_field,
        enum hierarchy_result_order *order,
        RedisModuleString *txt,
        RedisModuleString *fld,
        RedisModuleString *ord) {
    TO_STR(txt, fld, ord);
    enum hierarchy_result_order tmpOrder;

    if (strcmp("order", txt_str)) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }
    if (unlikely(ord_len < 3)) {
        goto einval;
    }

    if (ord_str[0] == 'a' && !strcmp("asc", ord_str)) {
        tmpOrder = HIERARCHY_RESULT_ORDER_ASC;
    } else if (ord_str[0] == 'd' && !strcmp("desc", ord_str)) {
        tmpOrder = HIERARCHY_RESULT_ORDER_DESC;
    } else {
einval:
        fprintf(stderr, "%s: Invalid order \"%.*s\"\n", __FILE__, (int)ord_len, ord_str);
        return SELVA_MODIFY_HIERARCHY_EINVAL;
    }

    if (fld_len == 0 || fld_str[0] == '\0') {
        tmpOrder = HIERARCHY_RESULT_ORDER_NONE;
        *order_by_field = NULL;
    } else {
        *order_by_field = fld;
    }

    *order = tmpOrder;

    return 0;
}

static int parse_algo(enum SelvaModify_Hierarchy_Algo *algo, RedisModuleString *arg) {
    size_t len;
    const char *str = RedisModule_StringPtrLen(arg, &len);

    if (!strcmp("bfs", str)) {
        *algo = HIERARCHY_BFS;
    } else if (!strcmp("dfs", str)) {
        *algo = HIERARCHY_DFS;
    } else {
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

    return 0;
}

static int parse_dir(enum SelvaModify_HierarchyTraversal *dir, enum SelvaModify_Hierarchy_Algo algo, RedisModuleString *arg) {
    TO_STR(arg);

    if (!strcmp("node", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_NODE;
    } else if (!strcmp("children", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_CHILDREN;
    } else if (!strcmp("parents", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_PARENTS;
    } else if (!strcmp("ancestors", arg_str)) {
        *dir = algo == HIERARCHY_BFS
            ? SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS
            : SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS;
    } else if (!strcmp("descendants", arg_str)) {
        *dir = algo == HIERARCHY_BFS
            ? SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS
            : SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS;
    } else if (arg_len > 0) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_REF;
    } else {
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

    return 0;
}

static int FindCommand_compareNone(const void ** restrict a_raw __unused, const void ** restrict b_raw __unused) {
    return 0;
}

static int FindCommand_compareAsc(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct FindCommand_OrderedItem *a = *(const struct FindCommand_OrderedItem **)a_raw;
    const struct FindCommand_OrderedItem *b = *(const struct FindCommand_OrderedItem **)b_raw;
    const char *aStr = a->data;
    const char *bStr = b->data;

    if (a->data_len && b->data_len) {
        if (a->data_type == ORDERED_ITEM_TYPE_DOUBLE &&
            b->data_type == ORDERED_ITEM_TYPE_DOUBLE) {
            double x = a->d;
            double y = b->d;

            if (x < y) {
                return -1;
            } else if (x > y) {
                return 1;
            }
        } else {
            /* TODO different langs may have differing order. */
            const int res = strcmp(aStr, bStr);
            if (res != 0) {
                return res;
            }
        }
    }

    return memcmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

static int FindCommand_compareDesc(const void ** restrict a_raw, const void ** restrict b_raw) {
    return FindCommand_compareAsc(b_raw, a_raw);
}

static orderFunc getOrderFunc(enum hierarchy_result_order order) {
    switch (order) {
    case HIERARCHY_RESULT_ORDER_ASC:
        return FindCommand_compareAsc;
    case HIERARCHY_RESULT_ORDER_DESC:
        return FindCommand_compareDesc;
    case HIERARCHY_RESULT_ORDER_NONE:
    default:
        return FindCommand_compareNone;
    }
}

static struct FindCommand_OrderedItem *createFindCommand_OrderItem(RedisModuleCtx *ctx, Selva_NodeId nodeId, const RedisModuleString *order_field) {
    RedisModuleString *id;
    RedisModuleKey *key;
    struct FindCommand_OrderedItem *item;
    double d = 0.0;
    const char *data = NULL;
    size_t data_len = 0;
    enum FindCommand_OrderedItemType type = ORDERED_ITEM_TYPE_EMPTY;

    id = RedisModule_CreateString(ctx, nodeId, Selva_NodeIdLen(nodeId));
    if (!id) {
        return NULL;
    }

    key = RedisModule_OpenKey(ctx, id, REDISMODULE_READ);
    if (key) {
        RedisModuleString *value = NULL;
        int err;

        err = SelvaNode_GetField(ctx, key, order_field, &value);
        if (!err && value) {
            char *end;

            data = RedisModule_StringPtrLen(value, &data_len);

            /* Check if it's a number. */
            d = strtod(data, &end);
            if (end != data) {
                type = ORDERED_ITEM_TYPE_DOUBLE;
            } else {
                type = ORDERED_ITEM_TYPE_TEXT;
            }
        }

        RedisModule_CloseKey(key);
    }

    item = RedisModule_PoolAlloc(ctx, sizeof(struct FindCommand_OrderedItem) + data_len + 1);
    if (!item) {
        /* FIXME Handle ENOMEM */
        abort();
    }

    memcpy(item->id, nodeId, SELVA_NODE_ID_SIZE);
    item->data_type = type;
    item->data_len = data_len;
    if (data_len > 0) {
        item->d = d;

        memcpy(item->data, data, data_len);
        item->data[data_len] = '\0';
    }

    return item;
}

static int FindCommand_NodeCb(Selva_NodeId nodeId, void *arg, struct SelvaModify_HierarchyMetadata *metadata __unused) {
    struct FindCommand_Args *args = (struct FindCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->rpn_ctx;
    int take = (args->offset > 0) ? !args->offset-- : 1;

    if (take && rpn_ctx) {
        int err;

        /* Set node_id to the register */
        rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, 0);

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(rpn_ctx, args->filter, &take);
        if (err) {
            fprintf(stderr, "%s: Expression failed (node: \"%.*s\"): \"%s\"\n",
                    __FILE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    rpn_str_error[err]);
            return 1;
        }
    }

    if (take) {
        const int sort = !!args->order_field;

        if (!sort) {
            ssize_t *nr_nodes = args->nr_nodes;
            ssize_t * restrict limit = args->limit;

            RedisModule_ReplyWithStringBuffer(args->ctx, nodeId, Selva_NodeIdLen(nodeId));
            *nr_nodes = *nr_nodes + 1;

            *limit = *limit - 1;
            if (*limit == 0) {
                return 1;
            }
        } else {
            struct FindCommand_OrderedItem *item;

            item = createFindCommand_OrderItem(args->ctx, nodeId, args->order_field);
            if (item) {
                SVector_InsertFast(args->order_result, item);
            } else {
                fprintf(stderr, "%s: Out of memory while creating an ordered result item\n", __FILE__);
            }
        }
    }

    return 0;
}

static int FindInSubCommand_NodeCb(Selva_NodeId nodeId, void *arg, struct SelvaModify_HierarchyMetadata *metadata) {
    struct FindCommand_Args *args = (struct FindCommand_Args *)arg;
    struct Selva_SubscriptionMarker *marker = args->marker;
    struct rpn_ctx *rpn_ctx = args->rpn_ctx;
    int take = (args->offset > 0) ? !args->offset-- : 1;

    Selva_Subscriptions_SetMarker(nodeId, metadata, marker);

    if (take && rpn_ctx) {
        int err;

        /* Set node_id to the register */
        rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, 0);

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(rpn_ctx, args->filter, &take);
        if (err) {
            fprintf(stderr, "%s: Expression failed (node: \"%.*s\"): \"%s\"\n",
                    __FILE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    rpn_str_error[err]);
            take = 0;
        }
    }

    if (take) {
        const int sort = !!args->order_field;

        if (!sort) {
            ssize_t * restrict limit = args->limit;

            if (*limit != 0) {
                ssize_t *nr_nodes = args->nr_nodes;

                RedisModule_ReplyWithStringBuffer(args->ctx, nodeId, Selva_NodeIdLen(nodeId));
                *nr_nodes = *nr_nodes + 1;
                *limit = *limit - 1;
            }
        } else {
            struct FindCommand_OrderedItem *item;

            item = createFindCommand_OrderItem(args->ctx, nodeId, args->order_field);
            if (item) {
                SVector_InsertFast(args->order_result, item);
            } else {
                fprintf(stderr, "%s: Out of memory while creating an ordered result item\n", __FILE__);
            }
        }
    }

    return 0;
}

static size_t FindCommand_PrintOrderedResult(RedisModuleCtx *ctx, ssize_t offset, ssize_t limit, SVector *order_result) {
    struct FindCommand_OrderedItem **item_pp;

    /*
     * First handle the offsetting.
     */
    for (ssize_t i = 0; i < offset; i++) {
        SVector_Shift(order_result);
    }
    SVector_ShiftReset(order_result);

    /*
     * Then send out node IDs upto the limit.
     */
    size_t len = 0;
    SVECTOR_FOREACH(item_pp, order_result) {
        struct FindCommand_OrderedItem *item = *item_pp;

        if (limit-- == 0) {
            break;
        }

        RedisModule_ReplyWithStringBuffer(ctx, item->id, Selva_NodeIdLen(item->id));
        len++;
    }

    return len;
}

static int get_skip(enum SelvaModify_HierarchyTraversal dir) {
    switch (dir) {
     /*
      * Find needs to skip the head node of the traverse for some types as we
      * are not interested in the node we already know.
      */
    case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
    case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
    case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
    case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
    case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
        return 1;
    default:
        return 0;
    }
}

/**
 * Find node ancestors/descendants.
 * SELVA.HIERARCHY.find REDIS_KEY dfs|bfs descendants|ancestors [order field asc|desc] [offset 1234] [limit 1234] NODE_IDS [expression] [args...]
 *                                |       |                     |                      |             |            |        |            |
 * Traversal method/algo --------/        |                     |                      |             |            |        |            |
 * Traversal direction ------------------/                      |                      |             |            |        |            |
 * Sort order of the results ----------------------------------/                       |             |            |        |            |
 * Skip the first 1234 - 1 results ---------------------------------------------------/              |            |        |            |
 * Limit the number of results (Optional) ----------------------------------------------------------/             |        |            |
 * One or more node IDs concatenated (10 chars per ID) ----------------------------------------------------------/         |            |
 * RPN filter expression -------------------------------------------------------------------------------------------------/             |
 * Register arguments for the RPN filter ----------------------------------------------------------------------------------------------/
 */
int SelvaHierarchy_FindCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_REDIS_KEY = 1;
    const size_t ARGV_ALGO      = 2;
    const size_t ARGV_DIRECTION = 3;
    const size_t ARGV_ORDER_TXT = 4;
    const size_t ARGV_ORDER_FLD = 5;
    const size_t ARGV_ORDER_ORD = 6;
    size_t ARGV_OFFSET_TXT      = 4;
    size_t ARGV_OFFSET_NUM      = 5;
    size_t ARGV_LIMIT_TXT       = 4;
    size_t ARGV_LIMIT_NUM       = 5;
    size_t ARGV_NODE_IDS        = 4;
    size_t ARGV_FILTER_EXPR     = 5;
    size_t ARGV_FILTER_ARGS     = 6;
#define SHIFT_ARGS(i) \
    ARGV_OFFSET_TXT += i; \
    ARGV_OFFSET_NUM += i; \
    ARGV_LIMIT_TXT += i; \
    ARGV_LIMIT_NUM += i; \
    ARGV_NODE_IDS += i; \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    if (argc < 5) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Select traversal method.
     */
    enum SelvaModify_Hierarchy_Algo algo;
    err = parse_algo(&algo, argv[ARGV_ALGO]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "traversal method");
    }

    /*
     * Get the direction parameter.
     */
    enum SelvaModify_HierarchyTraversal dir;
    const char *ref_field = NULL;
    err = parse_dir(&dir, algo, argv[ARGV_DIRECTION]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "direction");
    }
    if (dir == SELVA_HIERARCHY_TRAVERSAL_REF) {
        /* The arg is a field name. */
        ref_field = RedisModule_StringPtrLen(argv[ARGV_DIRECTION], NULL);
    }

    /*
     * Parse the order arg.
     */
    enum hierarchy_result_order order = HIERARCHY_RESULT_ORDER_NONE;
    const RedisModuleString *order_by_field = NULL;
    if (argc > (int)ARGV_ORDER_ORD) {
        err = parse_order(&order_by_field, &order,
                          argv[ARGV_ORDER_TXT],
                          argv[ARGV_ORDER_FLD],
                          argv[ARGV_ORDER_ORD]);
        if (err == 0) {
            SHIFT_ARGS(3);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "order");
        }
    }

    /*
     * Parse the offset arg.
     */
    ssize_t offset = 0;
    if (argc > (int)ARGV_OFFSET_NUM) {
        err = SelvaArgParser_IntOpt(&offset, "offset", argv[ARGV_OFFSET_TXT], argv[ARGV_OFFSET_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "offset");
        }
    }

    /*
     * Parse the limit arg. -1 = inf
     */
    ssize_t limit = -1;
    if (argc > (int)ARGV_LIMIT_NUM) {
        err = SelvaArgParser_IntOpt(&limit, "limit", argv[ARGV_LIMIT_TXT], argv[ARGV_LIMIT_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "limit");
        }
    }

    /*
     * Prepare the filter expression if given.
     */
    struct rpn_ctx *rpn_ctx = NULL;
    rpn_token *filter_expression = NULL;
    if (argc >= (int)ARGV_FILTER_EXPR + 1) {
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
        const char *input;
        size_t input_len;

        rpn_ctx = rpn_init(ctx, nr_reg);
        if (!rpn_ctx) {
            return replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "filter expression");
        }

        /*
         * Compile the filter expression.
         */
        input = RedisModule_StringPtrLen(argv[ARGV_FILTER_EXPR], &input_len);
        filter_expression = rpn_compile(input, input_len);
        if (!filter_expression) {
            rpn_destroy(rpn_ctx);
            return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
        }

        /*
         * Get the filter expression arguments and set them to the registers.
         */
        for (size_t i = ARGV_FILTER_ARGS; i < (size_t)argc; i++) {
            /* reg[0] is reserved for the current nodeId */
            const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
            size_t str_len;
            const char *str = RedisModule_StringPtrLen(argv[i], &str_len);

            rpn_set_reg(rpn_ctx, reg_i, str, str_len + 1, 0);
        }
    }

    RedisModuleString *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    svector_autofree SVector order_result = { 0 }; /*!< for ordered result. */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        if (!SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, getOrderFunc(order))) {
            replyWithSelvaError(ctx, SELVA_ENOMEM);
            goto out;
        }
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run for each NODE_ID.
     */
    ssize_t nr_nodes = 0;
    size_t skip = get_skip(dir); /* Skip n nodes from the results. */
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        Selva_NodeId nodeId;

        Selva_NodeIdCpy(nodeId, ids_str + i);

        /*
         * Run BFS/DFS.
         */
        ssize_t tmp_limit = -1;
        struct FindCommand_Args args = {
            .ctx = ctx,
            .nr_nodes = &nr_nodes,
            .offset = (order == HIERARCHY_RESULT_ORDER_NONE) ? offset + skip : skip,
            .limit = (order == HIERARCHY_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .order_field = order_by_field,
            .order_result = &order_result,
        };
        const struct SelvaModify_HierarchyCallback cb = {
            .node_cb = FindCommand_NodeCb,
            .node_arg = &args,
        };

        if (limit == 0) {
            break;
        }

        if (dir == SELVA_HIERARCHY_TRAVERSAL_REF && ref_field) {
            err = SelvaModify_TraverseHierarchyRef(ctx, hierarchy, nodeId, ref_field, &cb);
        } else {
            err = SelvaModify_TraverseHierarchy(hierarchy, nodeId, dir, &cb);
        }
        if (err != 0) {
            /* FIXME This will make redis crash */
#if 0
            return replyWithSelvaError(ctx, err);
#endif
            fprintf(stderr, "%s: Find failed for node: \"%.*s\"\n", __FILE__, (int)SELVA_NODE_ID_SIZE, nodeId);
        }
    }

    /*
     * If an ordered request was requested then nothing was send to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        nr_nodes = FindCommand_PrintOrderedResult(ctx, offset, limit, &order_result);
    }

    RedisModule_ReplySetArrayLength(ctx, nr_nodes);

out:
    if (rpn_ctx) {
        RedisModule_Free(filter_expression);
        rpn_destroy(rpn_ctx);
    }

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

/**
 * Find node ancestors/descendants and return fields.
 * SELVA.HIERARCHY.findFields REDIS_KEY dfs|bfs descendants|ancestors [order field asc|desc] [offset 1234] [limit 1234] NODE_IDS FIELDS [expression] [args...]
 *                                      |       |                     |                      |             |            |        |      |            |
 * Traversal method/algo --------------/        |                     |                      |             |            |        |      |            |
 * Traversal direction ------------------------/                      |                      |             |            |        |      |            |
 * Sort order of the results ----------------------------------------/                       |             |            |        |      |            |
 * Skip the first 1234 - 1 results ---------------------------------------------------------/              |            |        |      |            |
 * Limit the number of results (Optional) ----------------------------------------------------------------/             |        |      |            |
 * One or more node IDs concatenated (10 chars per ID) ----------------------------------------------------------------/         |      |            |
 * Fields to be returned or empty string for all -------------------------------------------------------------------------------/       |            |
 * RPN filter expression --------------------------------------------------------------------------------------------------------------/             |
 * Register arguments for the RPN filter -----------------------------------------------------------------------------------------------------------/
 */
int SelvaHierarchy_FindFieldsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_REDIS_KEY = 1;
    const size_t ARGV_ALGO      = 2;
    const size_t ARGV_DIRECTION = 3;
    const size_t ARGV_ORDER_TXT = 4;
    const size_t ARGV_ORDER_FLD = 5;
    const size_t ARGV_ORDER_ORD = 6;
    size_t ARGV_OFFSET_TXT      = 4;
    size_t ARGV_OFFSET_NUM      = 5;
    size_t ARGV_LIMIT_TXT       = 4;
    size_t ARGV_LIMIT_NUM       = 5;
    size_t ARGV_NODE_IDS        = 4;
    size_t ARGV_FIELDS          = 5;
    size_t ARGV_FILTER_EXPR     = 6;
    size_t ARGV_FILTER_ARGS     = 7;
#define SHIFT_ARGS(i) \
    ARGV_OFFSET_TXT     += i; \
    ARGV_OFFSET_NUM     += i; \
    ARGV_LIMIT_TXT      += i; \
    ARGV_LIMIT_NUM      += i; \
    ARGV_NODE_IDS       += i; \
    ARGV_FIELDS         += i; \
    ARGV_FILTER_EXPR    += i; \
    ARGV_FILTER_ARGS    += i

    if (argc < 5) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Select traversal method.
     */
    enum SelvaModify_Hierarchy_Algo algo;
    err = parse_algo(&algo, argv[ARGV_ALGO]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "traversal method");
    }

    /*
     * Get the direction parameter.
     */
    enum SelvaModify_HierarchyTraversal dir;
    const char *ref_field = NULL;
    err = parse_dir(&dir, algo, argv[ARGV_DIRECTION]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "direction");
    }
    if (dir == SELVA_HIERARCHY_TRAVERSAL_REF) {
        /* The arg is a field name. */
        ref_field = RedisModule_StringPtrLen(argv[ARGV_DIRECTION], NULL);
    }

    /*
     * Parse the order arg.
     */
    enum hierarchy_result_order order = HIERARCHY_RESULT_ORDER_NONE;
    const RedisModuleString *order_by_field = NULL;
    if (argc > (int)ARGV_ORDER_ORD) {
        err = parse_order(&order_by_field, &order,
                          argv[ARGV_ORDER_TXT],
                          argv[ARGV_ORDER_FLD],
                          argv[ARGV_ORDER_ORD]);
        if (err == 0) {
            SHIFT_ARGS(3);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "order");
        }
    }

    /*
     * Parse the offset arg.
     */
    ssize_t offset = 0;
    if (argc > (int)ARGV_OFFSET_NUM) {
        err = SelvaArgParser_IntOpt(&offset, "offset", argv[ARGV_OFFSET_TXT], argv[ARGV_OFFSET_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "offset");
        }
    }

    /*
     * Parse the limit arg. -1 = inf
     */
    ssize_t limit = -1;
    if (argc > (int)ARGV_LIMIT_NUM) {
        err = SelvaArgParser_IntOpt(&limit, "limit", argv[ARGV_LIMIT_TXT], argv[ARGV_LIMIT_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "limit");
        }
    }

    /*
     * Prepare the filter expression if given.
     */
    struct rpn_ctx *rpn_ctx = NULL;
    rpn_token *filter_expression = NULL;
    if (argc >= (int)ARGV_FILTER_EXPR + 1) {
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
        const char *input;
        size_t input_len;

        rpn_ctx = rpn_init(ctx, nr_reg);
        if (!rpn_ctx) {
            return replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "filter expression");
        }

        /*
         * Compile the filter expression.
         */
        input = RedisModule_StringPtrLen(argv[ARGV_FILTER_EXPR], &input_len);
        filter_expression = rpn_compile(input, input_len);
        if (!filter_expression) {
            rpn_destroy(rpn_ctx);
            return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
        }

        /*
         * Get the filter expression arguments and set them to the registers.
         */
        for (size_t i = ARGV_FILTER_ARGS; i < (size_t)argc; i++) {
            /* reg[0] is reserved for the current nodeId */
            const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
            size_t str_len;
            const char *str = RedisModule_StringPtrLen(argv[i], &str_len);

            rpn_set_reg(rpn_ctx, reg_i, str, str_len + 1, 0);
        }
    }

    /*
     * Get the NodeIds.
     */
    RedisModuleString *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    /*
     * Get the field names.
     */
    RedisModuleString *fields = argv[ARGV_FIELDS];
    TO_STR(fields);

    /*
     * Create the vector for an ordered result.
     */
    svector_autofree SVector order_result = { 0 };
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        if (!SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, getOrderFunc(order))) {
            replyWithSelvaError(ctx, SELVA_ENOMEM);
            goto out;
        }
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run for each NODE_ID.
     */
    ssize_t nr_nodes = 0;
    size_t skip = get_skip(dir); /* Skip n nodes from the results. */
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        Selva_NodeId nodeId;

        Selva_NodeIdCpy(nodeId, ids_str + i);

        /*
         * Run BFS/DFS.
         */
        ssize_t tmp_limit = -1;
        struct FindCommand_Args args = {
            .ctx = ctx,
            .nr_nodes = &nr_nodes,
            .offset = (order == HIERARCHY_RESULT_ORDER_NONE) ? offset + skip : skip,
            .limit = (order == HIERARCHY_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .order_field = order_by_field,
            .order_result = &order_result,
        };
        const struct SelvaModify_HierarchyCallback cb = {
            .node_cb = FindCommand_NodeCb,
            .node_arg = &args,
        };

        if (limit == 0) {
            break;
        }

        if (dir == SELVA_HIERARCHY_TRAVERSAL_REF && ref_field) {
            err = SelvaModify_TraverseHierarchyRef(ctx, hierarchy, nodeId, ref_field, &cb);
        } else {
            err = SelvaModify_TraverseHierarchy(hierarchy, nodeId, dir, &cb);
        }
        if (err != 0) {
            /* FIXME This will make redis crash */
#if 0
            return replyWithSelvaError(ctx, err);
#endif
            fprintf(stderr, "%s: Find failed for node: \"%.*s\"\n", __FILE__, (int)SELVA_NODE_ID_SIZE, nodeId);
        }
    }

    /*
     * If an ordered request was requested then nothing was send to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        nr_nodes = FindCommand_PrintOrderedResult(ctx, offset, limit, &order_result);
    }

    RedisModule_ReplySetArrayLength(ctx, nr_nodes);

out:
    if (rpn_ctx) {
        RedisModule_Free(filter_expression);
        rpn_destroy(rpn_ctx);
    }

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

/**
 * Find node in set.
 * SELVA.HIERARCHY.findIn REDIS_KEY [order field asc|desc] [offset 1234] [limit 1234] NODE_IDS [filter expression] [args...]
 */
int SelvaHierarchy_FindInCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_REDIS_KEY = 1;
    const size_t ARGV_ORDER_TXT = 2;
    const size_t ARGV_ORDER_FLD = 3;
    const size_t ARGV_ORDER_ORD = 4;
    size_t ARGV_OFFSET_TXT      = 2;
    size_t ARGV_OFFSET_NUM      = 3;
    size_t ARGV_LIMIT_TXT       = 2;
    size_t ARGV_LIMIT_NUM       = 3;
    size_t ARGV_NODE_IDS        = 2;
    size_t ARGV_FILTER_EXPR     = 3;
    size_t ARGV_FILTER_ARGS     = 4;
#define SHIFT_ARGS(i) \
    ARGV_OFFSET_TXT += i; \
    ARGV_OFFSET_NUM += i; \
    ARGV_LIMIT_TXT += i; \
    ARGV_LIMIT_NUM += i; \
    ARGV_NODE_IDS += i; \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

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
    if (argc > (int)ARGV_ORDER_ORD) {
        err = parse_order(&order_by_field, &order,
                          argv[ARGV_ORDER_TXT],
                          argv[ARGV_ORDER_FLD],
                          argv[ARGV_ORDER_ORD]);
        if (err == 0) {
            SHIFT_ARGS(3);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "order");
        }
    }

    /*
     * Parse the offset arg.
     */
    ssize_t offset = 0;
    if (argc > (int)ARGV_OFFSET_NUM) {
        err = SelvaArgParser_IntOpt(&offset, "offset", argv[ARGV_OFFSET_TXT], argv[ARGV_OFFSET_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "offset");
        }
    }

    /*
     * Parse the limit arg. -1 = inf
     */
    ssize_t limit = -1;
    if (argc > (int)ARGV_LIMIT_NUM) {
        err = SelvaArgParser_IntOpt(&limit, "limit", argv[ARGV_LIMIT_TXT], argv[ARGV_LIMIT_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "limit");
        }
    }

    size_t nr_reg = argc - ARGV_FILTER_ARGS + 1;
    struct rpn_ctx *rpn_ctx = rpn_init(ctx, nr_reg);
    if (!rpn_ctx) {
        return replyWithSelvaError(ctx, SELVA_ENOMEM);
    }

    RedisModuleString *ids = argv[ARGV_NODE_IDS];
    RedisModuleString *filter = argv[ARGV_FILTER_EXPR];
    TO_STR(ids, filter);

    /*
     * Compile the filter expression.
     */
    rpn_token *filter_expression = rpn_compile(filter_str, filter_len);
    if (!filter_expression) {
        rpn_destroy(rpn_ctx);
        return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
    }

    /*
     * Get the filter expression arguments and set them to the registers.
     */
    for (size_t i = ARGV_FILTER_ARGS; i < (size_t)argc; i++) {
        /* reg[0] is reserved for the current nodeId */
        const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
        size_t str_len;
        const char *str = RedisModule_StringPtrLen(argv[i], &str_len);

        rpn_set_reg(rpn_ctx, reg_i, str, str_len + 1, 0);
    }

    svector_autofree SVector order_result = { 0 }; /*!< for ordered result. */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        if (!SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, getOrderFunc(order))) {
            replyWithSelvaError(ctx, SELVA_ENOMEM);
            goto out;
        }
    }

    ssize_t array_len = 0;
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run the filter for each node.
     */
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        Selva_NodeId nodeId;
        ssize_t tmp_limit = -1;
        struct FindCommand_Args args = {
            .ctx = ctx,
            .nr_nodes = &array_len,
            .offset = (order == HIERARCHY_RESULT_ORDER_NONE) ? offset : 0,
            .limit = (order == HIERARCHY_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .order_field = order_by_field,
            .order_result = &order_result,
        };

        Selva_NodeIdCpy(nodeId, ids_str + i);
        (void)FindCommand_NodeCb(nodeId, &args, NULL);
    }

    /*
     * If an ordered request was requested then nothing was send to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        array_len = FindCommand_PrintOrderedResult(ctx, offset, limit, &order_result);
    }

    RedisModule_ReplySetArrayLength(ctx, array_len);

out:
    rpn_destroy(rpn_ctx);
    RedisModule_Free(filter_expression);

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

/**
 * Find node in set.
 * SELVA.HIERARCHY.findInSub REDIS_KEY SUB_ID MARKER_ID [order field asc|desc] [offset 1234] [limit 1234]
 */
int SelvaHierarchy_FindInSubCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_REDIS_KEY = 1;
    const size_t ARGV_SUB_ID    = 2;
    const size_t ARGV_MARKER_ID = 3;
    const size_t ARGV_ORDER_TXT = 4;
    const size_t ARGV_ORDER_FLD = 5;
    const size_t ARGV_ORDER_ORD = 6;
    size_t ARGV_OFFSET_TXT      = 4;
    size_t ARGV_OFFSET_NUM      = 5;
    size_t ARGV_LIMIT_TXT       = 4;
    size_t ARGV_LIMIT_NUM       = 5;
#define SHIFT_ARGS(i) \
    ARGV_OFFSET_TXT += i; \
    ARGV_OFFSET_NUM += i; \
    ARGV_LIMIT_TXT += i; \
    ARGV_LIMIT_NUM += i

    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "subID");
    }

    /*
     * Get the marker id.
     */
    Selva_SubscriptionMarkerId marker_id;
    err = SelvaArgParser_MarkerId(&marker_id, argv[ARGV_MARKER_ID]);


    /*
     * Find the subscription marker.
     */
    struct Selva_SubscriptionMarker *marker;
    marker = SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id);
    if (!marker) {
        return replyWithSelvaErrorf(ctx, SELVA_SUBSCRIPTIONS_EINVAL, "markerID");
    }

    /*
     * Parse the order arg.
     */
    enum hierarchy_result_order order = HIERARCHY_RESULT_ORDER_NONE;
    const RedisModuleString *order_by_field = NULL;
    if (argc > (int)ARGV_ORDER_ORD) {
        err = parse_order(&order_by_field, &order,
                          argv[ARGV_ORDER_TXT],
                          argv[ARGV_ORDER_FLD],
                          argv[ARGV_ORDER_ORD]);
        if (err == 0) {
            SHIFT_ARGS(3);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "order");
        }
    }

    /*
     * Parse the offset arg.
     */
    ssize_t offset = 0;
    if (argc > (int)ARGV_OFFSET_NUM) {
        err = SelvaArgParser_IntOpt(&offset, "offset", argv[ARGV_OFFSET_TXT], argv[ARGV_OFFSET_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "offset");
        }
    }

    /*
     * Parse the limit arg. -1 = inf
     */
    ssize_t limit = -1;
    if (argc > (int)ARGV_LIMIT_NUM) {
        err = SelvaArgParser_IntOpt(&limit, "limit", argv[ARGV_LIMIT_TXT], argv[ARGV_LIMIT_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "limit");
        }
    }

    svector_autofree SVector order_result = { 0 }; /* No need to init for ORDER_NODE */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        if (!SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, getOrderFunc(order))) {
            return replyWithSelvaError(ctx, SELVA_ENOMEM);
        }
    }

    ssize_t array_len = 0;
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run the traverse function.
     */
    ssize_t tmp_limit = -1;
    size_t skip = get_skip(marker->dir); /* Skip n nodes from the results. */
    struct FindCommand_Args args = {
        .ctx = ctx,
        .nr_nodes = &array_len,
        .offset = (order == HIERARCHY_RESULT_ORDER_NONE) ? offset + skip : skip,
        .limit = (order == HIERARCHY_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
        .rpn_ctx = marker->filter_ctx,
        .filter = marker->filter_expression,
        .order_field = order_by_field,
        .order_result = &order_result,
        .marker = marker,
    };
    const struct SelvaModify_HierarchyCallback cb = {
        .node_cb = FindInSubCommand_NodeCb,
        .node_arg = &args,
    };

    /* RFE We expect here that all markers have a valid nodeId. */
    if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_REF && marker->ref_field) {
        err = SelvaModify_TraverseHierarchyRef(ctx, hierarchy, marker->node_id, marker->ref_field, &cb);
    } else {
        err = SelvaModify_TraverseHierarchy(hierarchy, marker->node_id, marker->dir, &cb);
    }
    if (err != 0) {
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
        /* FIXME This will make redis crash */
#if 0
        return replyWithSelvaError(ctx, err);
#endif
        fprintf(stderr, "%s: FindInSub failed. sub_id: \"%s\" marker_id: %d err: \"%s\"\n",
                __FILE__, Selva_SubscriptionId2str(str, sub_id), (int)marker_id, selvaStrError[-err]);
    }

    /*
     * If an ordered request was requested then nothing was send to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        array_len = FindCommand_PrintOrderedResult(ctx, offset, limit, &order_result);
    }

    RedisModule_ReplySetArrayLength(ctx, array_len);

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

static int Find_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.find", SelvaHierarchy_FindCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.findFields", SelvaHierarchy_FindCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.findIn", SelvaHierarchy_FindInCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.findInSub", SelvaHierarchy_FindInSubCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Find_OnLoad);
