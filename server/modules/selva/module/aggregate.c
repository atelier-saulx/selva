#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "cdefs.h"
#include "funmap.h"
#include "selva.h"
#include "config.h"
#include "redismodule.h"
#include "auto_free.h"
#include "arg_parser.h"
#include "errors.h"
#include "hierarchy.h"
#include "modify.h"
#include "rpn.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "subscriptions.h"
#include "svector.h"
#include "traversal.h"
#include "traversal_order.h"
#include "find_index.h"

enum SelvaHierarchy_AggregateType {
    SELVA_AGGREGATE_TYPE_COUNT_NODE = '0',
    SELVA_AGGREGATE_TYPE_COUNT_UNIQUE_FIELD = '1',
    SELVA_AGGREGATE_TYPE_SUM_FIELD = '2',
    SELVA_AGGREGATE_TYPE_AVG_FIELD = '3',
    SELVA_AGGREGATE_TYPE_MIN_FIELD = '4',
    SELVA_AGGREGATE_TYPE_MAX_FIELD = '5',
};

struct AggregateCommand_Args;
typedef int (*agg_func)(struct SelvaObject *, struct AggregateCommand_Args *);

struct AggregateCommand_Args {
    struct RedisModuleCtx *ctx;
    struct FindCommand_Args find_args;

    enum SelvaHierarchy_AggregateType aggregate_type;
    agg_func agg;

    /*
     * Aggregation state.
     */
    long long int aggregation_result_int;
    double aggregation_result_double;
    size_t item_count;
};

static int agg_fn_count_obj(struct SelvaObject *obj __unused, struct AggregateCommand_Args* args) {
    args->item_count++;
    return 0;
}

static int get_first_field_value(struct SelvaObject *obj, struct SelvaObject *fields_obj, double *out) {
    SVector *fields;
    struct SVectorIterator it;
    const RedisModuleString *field;
    int err;

    err = SelvaObject_GetArrayStr(fields_obj, "0", 1, NULL, &fields);
    if (err || !fields) {
        return SELVA_ENOENT;
    }

    SVector_ForeachBegin(&it, fields);
    while ((field = SVector_Foreach(&it))) {
        struct SelvaObjectAny value;

        err = SelvaObject_GetAny(obj, field, &value);
        if (!err) {
            if (value.type == SELVA_OBJECT_LONGLONG) {
                *out = (double)value.ll;
                return 0;
            } else if (value.type == SELVA_OBJECT_DOUBLE) {
                *out = value.d;
                return 0;
            }
        }
    }

    return SELVA_ENOENT;
}

static int agg_fn_count_uniq_obj(struct SelvaObject *obj __unused, struct AggregateCommand_Args* args __unused) {
    /* TODO */
    return 0;
}

static int agg_fn_sum_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    struct SelvaObject *fields_obj = args->find_args.send_param.fields;
    double d;

    if (!get_first_field_value(obj, fields_obj, &d)) {
        args->aggregation_result_double += d;
        args->item_count++;
    }

    return 0;
}

static int agg_fn_avg_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    return agg_fn_sum_obj(obj, args);
}

static int agg_fn_min_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    struct SelvaObject *fields_obj = args->find_args.send_param.fields;
    double d;

    if (!get_first_field_value(obj, fields_obj, &d)) {
        if (d < args->aggregation_result_double) {
            args->aggregation_result_double = d;
        }
    }

    return 0;
}

static int agg_fn_max_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    struct SelvaObject *fields_obj = args->find_args.send_param.fields;
    double d;

    if (!get_first_field_value(obj, fields_obj, &d)) {
        if (d > args->aggregation_result_double) {
            args->aggregation_result_double = d;
        }
    }

    return 0;
}

static int agg_fn_none(struct SelvaObject *obj __unused, struct AggregateCommand_Args* args __unused) {
    return 0;
}

static agg_func agg_funcs[] = {
    agg_fn_count_obj,
    agg_fn_count_uniq_obj,
    agg_fn_sum_obj,
    agg_fn_avg_obj,
    agg_fn_min_obj,
    agg_fn_max_obj,
    agg_fn_none,
};

GENERATE_STATIC_FUNMAP(get_agg_func, agg_funcs, int, num_elem(agg_funcs) - 2);

static int AggregateCommand_NodeCb(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    Selva_NodeId nodeId;
    struct AggregateCommand_Args *args = (struct AggregateCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->find_args.rpn_ctx;
    int take = (args->find_args.offset > 0) ? !args->find_args.offset-- : 1;

    SelvaHierarchy_GetNodeId(nodeId, node);

    args->find_args.acc_tot++;
    if (take && rpn_ctx) {
        int err;

        /* Set node_id to the register */
        rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
        rpn_set_hierarchy_node(rpn_ctx, hierarchy, node);
        rpn_set_obj(rpn_ctx, SelvaHierarchy_GetNodeObject(node));

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(ctx, rpn_ctx, args->find_args.filter, &take);
        if (err) {
            fprintf(stderr, "%s:%d: Expression failed (node: \"%.*s\"): \"%s\"\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    rpn_str_error[err]);
            return 1;
        }
    }

    if (take) {
        const int sort = !!args->find_args.order_field;

        args->find_args.acc_take++;

        if (!sort) {
            ssize_t *nr_nodes = args->find_args.nr_nodes;
            ssize_t * restrict limit = args->find_args.limit;
            int err;

            err = args->agg(SelvaHierarchy_GetNodeObject(node), args);
            if (err) {
                fprintf(stderr, "%s:%d: Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, nodeId,
                        getSelvaErrorStr(err));
            }

            *nr_nodes = *nr_nodes + 1;

            *limit = *limit - 1;
            if (*limit == 0) {
                return 1;
            }
        } else {
            struct TraversalOrderItem *item;

            item = SelvaTraversalOrder_CreateOrderItem(ctx, args->find_args.lang, node, args->find_args.order_field);
            if (item) {
                SVector_InsertFast(args->find_args.order_result, item);
            } else {
                /*
                 * It's not so easy to make the response fail at this point.
                 * Given that we shouldn't generally even end up here in real
                 * life, it's fairly ok to just log the error and return what
                 * we can.
                 */
                fprintf(stderr, "%s:%d: Out of memory while creating an order result item\n",
                        __FILE__, __LINE__);
            }
        }
    }

    return 0;
}

static int AggregateCommand_ArrayObjectCb(
        union SelvaObjectArrayForeachValue value,
        enum SelvaObjectType subtype,
        void *arg) {
    struct SelvaObject *obj = value.obj;
    struct AggregateCommand_Args *args = (struct AggregateCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->find_args.rpn_ctx;
    int take = (args->find_args.offset > 0) ? !args->find_args.offset-- : 1;

    if (subtype != SELVA_OBJECT_OBJECT) {
        fprintf(stderr, "%s:%d: Array subtype not supported: %s\n",
                __FILE__, __LINE__,
                SelvaObject_Type2String(subtype, NULL));
        return 1;
    }

    if (take && rpn_ctx) {
        int err;

        /* Set obj to the register */
        err = rpn_set_reg_slvobj(rpn_ctx, 0, obj, 0);
        if (err) {
            fprintf(stderr, "%s:%d: Register set failed: \"%s\"\n",
                    __FILE__, __LINE__,
                    rpn_str_error[err]);
            return 1;
        }
        rpn_set_obj(rpn_ctx, obj);

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(args->ctx, rpn_ctx, args->find_args.filter, &take);
        if (err) {
            fprintf(stderr, "%s:%d: Expression failed: \"%s\"\n",
                    __FILE__, __LINE__,
                    rpn_str_error[err]);
            return 1;
        }
    }

    if (take) {
        const int sort = !!args->find_args.order_field;

        if (!sort) {
            ssize_t *nr_nodes = args->find_args.nr_nodes;
            ssize_t * restrict limit = args->find_args.limit;

            (void)args->agg(obj, args);

            *nr_nodes = *nr_nodes + 1;

            *limit = *limit - 1;
            if (*limit == 0) {
                return 1;
            }
        } else {
            struct TraversalOrderItem *item;
            item = SelvaTraversalOrder_CreateObjectBasedOrderItem(args->ctx, args->find_args.lang, obj, args->find_args.order_field);
            if (item) {
                SVector_InsertFast(args->find_args.order_result, item);
            } else {
                /*
                 * It's not so easy to make the response fail at this point.
                 * Given that we shouldn't generally even end up here in real
                 * life, it's fairly ok to just log the error and return what
                 * we can.
                 */
                fprintf(stderr, "%s:%d: Out of memory while creating an order result item\n",
                        __FILE__, __LINE__);
            }
        }
    }

    return 0;
}

static size_t AggregateCommand_AggregateOrderResult(
        RedisModuleCtx *ctx __unused,
        RedisModuleString *lang __unused,
        void *arg,
        ssize_t offset,
        ssize_t limit,
        struct SelvaObject *fields __unused,
        SVector *order_result) {
    struct AggregateCommand_Args *args = (struct AggregateCommand_Args *)arg;
    struct TraversalOrderItem *item;
    struct SVectorIterator it;
    size_t len = 0;

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
    SVector_ForeachBegin(&it, order_result);
    while ((item = SVector_Foreach(&it))) {
        struct SelvaHierarchyNode *node = item->node;
        int err;

        if (limit-- == 0) {
            break;
        }

        if (node) {
            err = args->agg(SelvaHierarchy_GetNodeObject(node), args);
        } else {
            err = SELVA_HIERARCHY_ENOENT;
        }
        if (err) {
            fprintf(stderr, "%s:%d: Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, item->node_id,
                    getSelvaErrorStr(err));
            continue;
        }

        len++;
    }

    return len;
}

static size_t AggregateCommand_AggregateOrderArrayResult(
        RedisModuleCtx *ctx,
        RedisModuleString *lang __unused,
        void *arg __unused,
        SelvaHierarchy *hierarchy __unused,
        ssize_t offset,
        ssize_t limit,
        struct SelvaObject *fields __unused,
        SVector *order_result) {
    struct TraversalOrderItem *item;
    struct SVectorIterator it;
    size_t len = 0;

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
    SVector_ForeachBegin(&it, order_result);
    while ((item = SVector_Foreach(&it))) {
        int err;
        if (limit-- == 0) {
            break;
        }

        if (item->data_obj) {
            struct AggregateCommand_Args *args = (struct AggregateCommand_Args *)arg;

            err = args->agg(item->data_obj, args);
            len++;
        } else {
            err = SELVA_HIERARCHY_ENOENT;
        }

        if (err) {
            RedisModule_ReplyWithNull(ctx);
            fprintf(stderr, "%s:%d: Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, item->node_id,
                    getSelvaErrorStr(err));
        }

    }

    return len;
}

static size_t AggregateCommand_PrintAggregateResult(RedisModuleCtx *ctx, const struct AggregateCommand_Args *args) {
    switch (args->aggregate_type) {
    case SELVA_AGGREGATE_TYPE_COUNT_NODE:
        RedisModule_ReplyWithLongLong(ctx, args->item_count);
        break;
    case SELVA_AGGREGATE_TYPE_COUNT_UNIQUE_FIELD:
        RedisModule_ReplyWithLongLong(ctx, args->aggregation_result_int);
        break;
    case SELVA_AGGREGATE_TYPE_AVG_FIELD:
        RedisModule_ReplyWithDouble(ctx, args->aggregation_result_double / (double)args->item_count);
        break;
    default:
        RedisModule_ReplyWithDouble(ctx, args->aggregation_result_double);
        break;
    }

    return 0;
}

int SelvaHierarchy_AggregateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_LANG      = 1;
    const int ARGV_REDIS_KEY = 2;
    const int ARGV_AGG_FN    = 3;
    const int ARGV_DIRECTION = 4;
    const int ARGV_REF_FIELD = 5;
    int ARGV_INDEX_TXT       = 5;
    int ARGV_INDEX_VAL       = 6;
    int ARGV_ORDER_TXT       = 5;
    int ARGV_ORDER_FLD       = 6;
    int ARGV_ORDER_ORD       = 7;
    int ARGV_OFFSET_TXT      = 5;
    int ARGV_OFFSET_NUM      = 6;
    int ARGV_LIMIT_TXT       = 5;
    int ARGV_LIMIT_NUM       = 6;
    int ARGV_FIELDS_TXT      = 5;
    int ARGV_FIELDS_VAL      = 6;
    int ARGV_NODE_IDS        = 5;
    int ARGV_FILTER_EXPR     = 6;
    int ARGV_FILTER_ARGS     = 7;
#define SHIFT_ARGS(i) \
    ARGV_INDEX_TXT += i; \
    ARGV_INDEX_VAL += i; \
    ARGV_ORDER_TXT += i; \
    ARGV_ORDER_FLD += i; \
    ARGV_ORDER_ORD += i; \
    ARGV_OFFSET_TXT += i; \
    ARGV_OFFSET_NUM += i; \
    ARGV_LIMIT_TXT += i; \
    ARGV_LIMIT_NUM += i; \
    ARGV_FIELDS_TXT += i; \
    ARGV_FIELDS_VAL += i; \
    ARGV_NODE_IDS += i; \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    if (argc < 6) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *lang = argv[ARGV_LANG];
    SVECTOR_AUTOFREE(order_result); /*!< for order result. */
    selvaobject_autofree struct SelvaObject *fields = NULL;
    struct rpn_ctx *traversal_rpn_ctx = NULL;
    struct rpn_expression *traversal_expression = NULL;
    struct rpn_ctx *rpn_ctx = NULL;
    RedisModuleString *argv_filter_expr = NULL;
    struct rpn_expression *filter_expression = NULL;
    __auto_free RedisModuleString **index_hints = NULL;
    int nr_index_hints = 0;

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Parse the traversal arguments.
     */
    enum SelvaTraversal dir;
    const RedisModuleString *ref_field = NULL;
    err = SelvaTraversal_ParseDir2(&dir, argv[ARGV_DIRECTION]);
    if (err) {
        replyWithSelvaErrorf(ctx, err, "Traversal argument");
        goto out;
    }
    if (argc <= ARGV_REF_FIELD &&
        (dir & (SELVA_HIERARCHY_TRAVERSAL_ARRAY |
                SELVA_HIERARCHY_TRAVERSAL_REF |
                SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                SELVA_HIERARCHY_TRAVERSAL_EXPRESSION))) {
        RedisModule_WrongArity(ctx);
        goto out;
    }
    if (dir & (SELVA_HIERARCHY_TRAVERSAL_ARRAY |
               SELVA_HIERARCHY_TRAVERSAL_REF |
               SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
               SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD)) {
        ref_field = argv[ARGV_REF_FIELD];
        SHIFT_ARGS(1);
    } else if (dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                      SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) {
        const RedisModuleString *input = argv[ARGV_REF_FIELD];
        TO_STR(input);

        traversal_rpn_ctx = rpn_init(1);
        if (!traversal_rpn_ctx) {
            replyWithSelvaError(ctx, SELVA_ENOMEM);
            goto out;
        }

        traversal_expression = rpn_compile(input_str);
        if (!traversal_expression) {
            replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the traversal expression");
            goto out;
        }
        SHIFT_ARGS(1);
    }

    const enum SelvaHierarchy_AggregateType agg_fn_type = RedisModule_StringPtrLen(argv[ARGV_AGG_FN], NULL)[0];
    double initial_double_val = 0;
    if (agg_fn_type == SELVA_AGGREGATE_TYPE_MAX_FIELD) {
        initial_double_val = DBL_MIN;
    } else if (agg_fn_type == SELVA_AGGREGATE_TYPE_MIN_FIELD) {
        initial_double_val = DBL_MAX;
    }

    /*
     * Parse the indexing hint.
     */
    nr_index_hints = SelvaArgParser_IndexHints(&index_hints, argv + ARGV_INDEX_TXT, argc - ARGV_INDEX_TXT);
    if (nr_index_hints < 0) {
        return replyWithSelvaError(ctx, SELVA_ENOMEM);
    } else if (nr_index_hints > 0) {
        SHIFT_ARGS(2 * nr_index_hints);
    }

    /*
     * Parse the order arg.
     */
    enum SelvaResultOrder order = SELVA_RESULT_ORDER_NONE;
    RedisModuleString *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = SelvaTraversal_ParseOrderArg(&order_by_field, &order,
                          argv[ARGV_ORDER_TXT],
                          argv[ARGV_ORDER_FLD],
                          argv[ARGV_ORDER_ORD]);
        if (err == 0) {
            SHIFT_ARGS(3);
        } else if (err != SELVA_HIERARCHY_ENOENT) {
            replyWithSelvaErrorf(ctx, err, "order");
            goto out;
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
            replyWithSelvaErrorf(ctx, err, "offset");
            goto out;
        }
        if (offset < -1) {
            replyWithSelvaErrorf(ctx, err, "offset < -1");
            goto out;
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
            replyWithSelvaErrorf(ctx, err, "limit");
            goto out;
        }
    }

    /*
     * Parse fields.
     */
    if (argc > ARGV_FIELDS_VAL) {
        err = SelvaArgsParser_StringSetList(ctx, &fields, NULL, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
        if (err == 0) {
            if (SelvaObject_Len(fields, NULL) > 1) {
                replyWithSelvaErrorf(ctx, err, "fields");
                goto out;
            }

            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            replyWithSelvaErrorf(ctx, err, "fields");
            goto out;
        }
    }

    /*
     * Prepare the filter expression if given.
     */
    if (argc >= ARGV_FILTER_EXPR + 1) {
        argv_filter_expr = argv[ARGV_FILTER_EXPR];
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;

        rpn_ctx = rpn_init(nr_reg);
        if (!rpn_ctx) {
            replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "filter expression");
            goto out;
        }

        /*
         * Compile the filter expression.
         */
        filter_expression = rpn_compile(RedisModule_StringPtrLen(argv_filter_expr, NULL));
        if (!filter_expression) {
            replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
            goto out;
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
    }

    if (argc <= ARGV_NODE_IDS) {
        replyWithSelvaError(ctx, SELVA_HIERARCHY_EINVAL);
        goto out;
    }

    const RedisModuleString *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    if (order != SELVA_RESULT_ORDER_NONE) {
        err = SelvaTraversalOrder_InitOrderResult(&order_result, order, limit);
        if (err) {
            replyWithSelvaError(ctx, err);
            goto out;
        }
    }

    /*
     * Run for each NODE_ID.
     */
    struct AggregateCommand_Args args = {
        .ctx = ctx,
        .aggregate_type = agg_fn_type,
        .agg = get_agg_func(agg_fn_type - '0'),
        .aggregation_result_int = 0,
        .aggregation_result_double = initial_double_val,
        .item_count = 0,
    };

    ssize_t nr_nodes = 0;
    size_t merge_nr_fields = 0;
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        Selva_NodeId nodeId;

        Selva_NodeIdCpy(nodeId, ids_str + i);
        if (nodeId[0] == '\0') {
            /* Just skip empty IDs. */
            continue;
        }

        /*
         * Note that SelvaArgParser_IndexHints() limits the nr_index_hints to
         * FIND_INDICES_MAX_HINTS_FIND
         */
        struct SelvaFindIndexControlBlock *ind_icb[max(nr_index_hints, 1)];
        int ind_select = -1; /* Selected index. The smallest of all found. */

        memset(ind_icb, 0, max(nr_index_hints, 1) * sizeof(struct SelvaFindIndexControlBlock *));

        /* find_indices_max == 0 => indexing disabled */
        if (nr_index_hints > 0 && selva_glob_config.find_indices_max > 0) {
            RedisModuleString *dir_expr = NULL;

            if (dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                       SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) {
                /*
                 * We know it's valid because it was already parsed and compiled once.
                 * However, the indexing subsystem can't use the already compiled
                 * expression because its lifetime is unpredictable and it's not easy
                 * to change that.
                 */
                dir_expr = argv[ARGV_REF_FIELD];
            }

            /*
             * Select the best index res set.
             */
            ind_select = SelvaFindIndex_AutoMulti(ctx, hierarchy, dir, dir_expr, nodeId, order, order_by_field, index_hints, nr_index_hints, ind_icb);
        }

        /*
         * If the index is already ordered then we don't need to sort the
         * response. This won't work if we have multiple nodeIds because
         * obviously the order might differ and we may not have an ordered
         * index for each id.
         */
        if (ind_select >= 0 &&
            ids_len == SELVA_NODE_ID_SIZE &&
            SelvaFindIndex_IsOrdered(ind_icb[ind_select], order, order_by_field)) {
            order = SELVA_RESULT_ORDER_NONE;
            order_by_field = NULL; /* This controls sorting in the callback. */
        }

        /*
         * Run BFS/DFS.
         */
        ssize_t tmp_limit = -1;
        const size_t skip = ind_select >= 0 ? 0 : SelvaTraversal_GetSkip(dir); /* Skip n nodes from the results. */
        args.find_args = (struct FindCommand_Args){
            .lang = lang,
            .nr_nodes = &nr_nodes,
            .offset = (order == SELVA_RESULT_ORDER_NONE) ? offset + skip : skip,
            .limit = (order == SELVA_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .send_param.fields = fields,
            .send_param.excluded_fields = NULL,
            .merge_nr_fields = &merge_nr_fields,
            .order_field = order_by_field,
            .order_result = &order_result,
        };

        if (limit == 0) {
            break;
        }

        if (ind_select >= 0) {
            /*
             * There is no need to run the filter again if the indexing was
             * executing the same filter already.
             */
            if (argv_filter_expr && !RedisModule_StringCompare(argv_filter_expr, index_hints[ind_select])) {
                args.find_args.rpn_ctx = NULL;
                args.find_args.filter = NULL;
            }

            err = SelvaFindIndex_Traverse(ctx, hierarchy, ind_icb[ind_select], AggregateCommand_NodeCb, &args);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY && ref_field) {
            const struct SelvaObjectArrayForeachCallback ary_cb = {
                .cb = AggregateCommand_ArrayObjectCb,
                .cb_arg = &args,
            };
            TO_STR(ref_field);

            err = SelvaHierarchy_TraverseArray(ctx, hierarchy, nodeId, ref_field_str, ref_field_len, &ary_cb);
        } else if (ref_field &&
                   (dir & (SELVA_HIERARCHY_TRAVERSAL_REF |
                           SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                           SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD))) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = AggregateCommand_NodeCb,
                .node_arg = &args,
            };
            TO_STR(ref_field);

            err = SelvaHierarchy_TraverseField(ctx, hierarchy, nodeId, dir, ref_field_str, ref_field_len, &cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = AggregateCommand_NodeCb,
                .node_arg = &args,
            };

            err = SelvaHierarchy_TraverseExpressionBfs(ctx, hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, NULL, NULL, &cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = AggregateCommand_NodeCb,
                .node_arg = &args,
            };

            err = SelvaHierarchy_TraverseExpression(ctx, hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, NULL, NULL, &cb);
        } else {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = AggregateCommand_NodeCb,
                .node_arg = &args,
            };

            err = SelvaHierarchy_Traverse(ctx, hierarchy, nodeId, dir, &cb);
        }
        if (err != 0) {
            /*
             * We can't send an error to the client at this point so we'll just log
             * it and ignore the error.
             */
            fprintf(stderr, "%s:%d: Aggregate failed. err: %s dir: %s node_id: \"%.*s\"\n",
                    __FILE__, __LINE__,
                    getSelvaErrorStr(err),
                    SelvaTraversal_Dir2str(dir),
                    (int)SELVA_NODE_ID_SIZE, nodeId);
        }

        /*
         * Do index accounting.
         */
        SelvaFindIndex_AccMulti(ind_icb, nr_index_hints, ind_select, args.find_args.acc_take, args.find_args.acc_tot);
    }

    /*
     * If an order request was requested then nothing was send to the client yet
     * and we need to do it now.
     */
    if (order != SELVA_RESULT_ORDER_NONE) {
        struct AggregateCommand_Args ord_args = {
            .ctx = ctx,
            .aggregate_type = agg_fn_type,
            .agg = get_agg_func(agg_fn_type - '0'),
            .aggregation_result_int = 0,
            .aggregation_result_double = initial_double_val,
            .item_count = 0,
            .find_args = {
                .send_param.fields = fields,
            }
        };

        nr_nodes = (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY)
            ? AggregateCommand_AggregateOrderArrayResult(ctx, lang, &ord_args, hierarchy, offset, limit, fields, &order_result)
            : AggregateCommand_AggregateOrderResult(ctx, lang, &ord_args, offset, limit, fields, &order_result);

        AggregateCommand_PrintAggregateResult(ctx, &ord_args);
    } else {
        AggregateCommand_PrintAggregateResult(ctx, &args);
    }

out:
    if (traversal_rpn_ctx) {
        rpn_destroy(traversal_rpn_ctx);
        rpn_destroy_expression(traversal_expression);
    }
    if (rpn_ctx) {
        rpn_destroy(rpn_ctx);
        rpn_destroy_expression(filter_expression);
    }

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

/**
 * Find node in set.
 * SELVA.inherit REDIS_KEY NODE_ID [TYPE1[TYPE2[...]]] [FIELD_NAME1[ FIELD_NAME2[ ...]]]
 */
int SelvaHierarchy_AggregateInCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_LANG      = 1;
    const int ARGV_REDIS_KEY = 2;
    const int ARGV_AGG_FN    = 3;
    const int ARGV_ORDER_TXT = 4;
    const int ARGV_ORDER_FLD = 5;
    const int ARGV_ORDER_ORD = 6;
    int ARGV_OFFSET_TXT      = 4;
    int ARGV_OFFSET_NUM      = 5;
    int ARGV_LIMIT_TXT       = 4;
    int ARGV_LIMIT_NUM       = 5;
    int ARGV_FIELDS_TXT      = 4;
    int ARGV_FIELDS_VAL      = 5;
    int ARGV_NODE_IDS        = 4;
    int ARGV_FILTER_EXPR     = 5;
    int ARGV_FILTER_ARGS     = 6;
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

    if (argc < 6) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *lang = argv[ARGV_LANG];

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    const enum SelvaHierarchy_AggregateType agg_fn_type = RedisModule_StringPtrLen(argv[ARGV_AGG_FN], NULL)[0];
    double initial_double_val = 0;
    if (agg_fn_type == SELVA_AGGREGATE_TYPE_MAX_FIELD) {
        initial_double_val = DBL_MIN;
    } else if (agg_fn_type == SELVA_AGGREGATE_TYPE_MIN_FIELD) {
        initial_double_val = DBL_MAX;
    }


    /*
     * Parse the order arg.
     */
    enum SelvaResultOrder order = SELVA_RESULT_ORDER_NONE;
    RedisModuleString *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = SelvaTraversal_ParseOrderArg(&order_by_field, &order,
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
        if (offset < -1) {
            return replyWithSelvaErrorf(ctx, err, "offset < -1");
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
        err = SelvaArgsParser_StringSetList(ctx, &fields, NULL, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
        if (err == 0) {
            if (SelvaObject_Len(fields, NULL) > 1) {
                return replyWithSelvaErrorf(ctx, err, "fields");
            }

            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "fields");
        }
    }

    struct rpn_ctx *rpn_ctx = NULL;
    struct rpn_expression *filter_expression = NULL;

    /*
     * Prepare the filter expression if given.
     */
    if (argc >= ARGV_FILTER_EXPR + 1) {
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
        const char *input;

        rpn_ctx = rpn_init(nr_reg);
        if (!rpn_ctx) {
            return replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "filter expression");
        }

        /*
         * Compile the filter expression.
         */
        input = RedisModule_StringPtrLen(argv[ARGV_FILTER_EXPR], NULL);
        filter_expression = rpn_compile(input);
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
    }

    SVECTOR_AUTOFREE(order_result); /*!< for ordered result. */

    if (argc <= ARGV_NODE_IDS) {
        replyWithSelvaError(ctx, SELVA_HIERARCHY_EINVAL);
        goto out;
    }

    const RedisModuleString *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    if (order != SELVA_RESULT_ORDER_NONE) {
        err = SelvaTraversalOrder_InitOrderResult(&order_result, order, limit);
        if (err) {
            replyWithSelvaError(ctx, err);
            goto out;
        }
    }

    /*
     * Run the filter for each node.
     */

    struct AggregateCommand_Args args = {
        .ctx = ctx,
        .aggregate_type = agg_fn_type,
        .agg = get_agg_func(agg_fn_type - '0'),
        .aggregation_result_int = 0,
        .aggregation_result_double = initial_double_val,
        .item_count = 0,
    };
    ssize_t array_len = 0;
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        struct SelvaHierarchyNode *node;
        ssize_t tmp_limit = -1;
        struct FindCommand_Args find_args = {
            .lang = lang,
            .nr_nodes = &array_len,
            .offset = (order == SELVA_RESULT_ORDER_NONE) ? offset : 0,
            .limit = (order == SELVA_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .send_param.merge_strategy = MERGE_STRATEGY_NONE,
            .send_param.merge_path = NULL,
            .merge_nr_fields = NULL,
            .send_param.fields = fields,
            .send_param.excluded_fields = NULL,
            .order_field = order_by_field,
            .order_result = &order_result,
        };

        args.find_args = find_args;

        node = SelvaHierarchy_FindNode(hierarchy, ids_str + i);
        if (node) {
            (void)AggregateCommand_NodeCb(ctx, hierarchy, node, &args);
        }
    }

    /*
     * If an ordered request was requested then nothing was sent to the client yet
     * and we need to do it now.
     */
    if (order != SELVA_RESULT_ORDER_NONE) {
        struct AggregateCommand_Args ord_args = {
            .ctx = ctx,
            .aggregate_type = agg_fn_type,
            .agg = get_agg_func(agg_fn_type - '0'),
            .aggregation_result_int = 0,
            .aggregation_result_double = initial_double_val,
            .item_count = 0,
            .find_args = {
                /* we always need context */
                .send_param.fields = fields,
                .send_param.excluded_fields = NULL,
            }
        };

        AggregateCommand_AggregateOrderResult(ctx, lang, &ord_args, offset, limit, fields, &order_result);
        AggregateCommand_PrintAggregateResult(ctx, &ord_args);
    } else {
        AggregateCommand_PrintAggregateResult(ctx, &args);
    }

out:
    if (rpn_ctx) {
        rpn_destroy(rpn_ctx);
        rpn_destroy_expression(filter_expression);
    }

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

static int Aggregate_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.aggregate", SelvaHierarchy_AggregateCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.aggregateIn", SelvaHierarchy_AggregateInCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Aggregate_OnLoad);
