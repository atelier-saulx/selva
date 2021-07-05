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

enum SelvaHierarchy_AggregateType {
    SELVA_AGGREGATE_TYPE_COUNT_NODE = '0',
    SELVA_AGGREGATE_TYPE_COUNT_UNIQUE_FIELD = '1',
    SELVA_AGGREGATE_TYPE_SUM_FIELD = '2',
    SELVA_AGGREGATE_TYPE_AVG_FIELD = '3',
};

struct AggregateCommand_Args {
    struct FindCommand_Args find_args;

    // aggregation state
    enum SelvaHierarchy_AggregateType aggregate_type;
    long long int aggregation_result_int;
    double aggregation_result_double;
    size_t item_count;
};

static int agg_fn_count_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    args->item_count++;
    return 0;
}

static int agg_fn_count_uniq_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    // TODO
}

static int agg_fn_sum_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    struct SelvaObject *fields_obj = args->find_args.fields;
    SVector *fields;
    SelvaObject_GetArrayStr(fields_obj, "0", 1, NULL, &fields);

    struct SVectorIterator *it;
    SVector_ForeachBegin(it, fields);

    RedisModuleString *field;
    while ((field = SVector_Foreach(it))) {
        enum SelvaObjectType field_type = SelvaObject_GetType(obj, field);
        if (field_type == SELVA_OBJECT_LONGLONG) {
            long long lv = 0;
            SelvaObject_GetLongLong(obj, field, &lv);
            args->aggregation_result_double += lv;

            args->item_count++;
        } else if (field_type == SELVA_OBJECT_DOUBLE) {
            double dv = 0;
            SelvaObject_GetDouble(obj, field, &dv);
            args->aggregation_result_double += dv;

            args->item_count++;
        }
    }
}

static int agg_fn_avg_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    struct SelvaObject *fields = args->find_args.fields;
    agg_fn_sum_obj(obj, args);
}

static int apply_agg_fn_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    int err = 0;
    if (args->aggregate_type == SELVA_AGGREGATE_TYPE_COUNT_NODE) {
        err = agg_fn_count_obj(obj, args);
    } else if (args->aggregate_type == SELVA_AGGREGATE_TYPE_COUNT_UNIQUE_FIELD) {
        err = agg_fn_count_uniq_obj(obj, args);
    } else if (args->aggregate_type == SELVA_AGGREGATE_TYPE_SUM_FIELD) {
        err = agg_fn_sum_obj(obj, args);
    } else if (args->aggregate_type == SELVA_AGGREGATE_TYPE_AVG_FIELD) {
        err = agg_fn_avg_obj(obj, args);
    }

    return err;
}

static int apply_agg_fn(struct SelvaModify_HierarchyNode *node, struct AggregateCommand_Args* args) {
    Selva_NodeId nodeId;
    RedisModuleString *id;
    SelvaModify_HierarchyGetNodeId(nodeId, node);
    id = RedisModule_CreateString(args->find_args.ctx, nodeId, Selva_NodeIdLen(nodeId));

    RedisModuleKey *key;
    key = RedisModule_OpenKey(args->find_args.ctx, id, REDISMODULE_READ | REDISMODULE_OPEN_KEY_NOTOUCH);

    struct SelvaObject *node_obj;

    int err = 0;
    err = SelvaObject_Key2Obj(key, &node_obj);

    if (err) {
        goto cleanup;
    }

    err = apply_agg_fn_obj(node_obj, args);

cleanup:
    RedisModule_CloseKey(key);

    return err;
}



static int AggregateCommand_NodeCb(struct SelvaModify_HierarchyNode *node, void *arg) {
    Selva_NodeId nodeId;
    struct AggregateCommand_Args *args = (struct AggregateCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->find_args.rpn_ctx;
    int take = (args->find_args.offset > 0) ? !args->find_args.offset-- : 1;

    SelvaModify_HierarchyGetNodeId(nodeId, node);

    if (take && rpn_ctx) {
        int err;

        /* Set node_id to the register */
        rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, 0);

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(args->find_args.ctx, rpn_ctx, args->find_args.filter, &take);
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

        if (!sort) {
            ssize_t *nr_nodes = args->find_args.nr_nodes;
            ssize_t * restrict limit = args->find_args.limit;
            int err;

            err = apply_agg_fn(node, args);

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
            struct FindCommand_OrderedItem *item;

            item = createFindCommand_OrderItem(args->find_args.ctx, args->find_args.lang, node, args->find_args.order_field);
            if (item) {
                SVector_InsertFast(args->find_args.order_result, item);
            } else {
                /*
                 * It's not so easy to make the response fail at this point.
                 * Given that we shouldn't generally even end up here in real
                 * life, it's fairly ok to just log the error and return what
                 * we can.
                 */
                fprintf(stderr, "%s:%d: Out of memory while creating an ordered result item\n",
                        __FILE__, __LINE__);
            }
        }
    }

    return 0;
}

static int AggregateCommand_ArrayNodeCb(struct SelvaObject *obj, void *arg) {
    // TODO
    struct AggregateCommand_Args *args = (struct AggregateCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->find_args.rpn_ctx;
    int take = (args->find_args.offset > 0) ? !args->find_args.offset-- : 1;

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

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(args->find_args.ctx, rpn_ctx, args->find_args.filter, &take);
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
            int err;

            // TODO

            *nr_nodes = *nr_nodes + 1;

            *limit = *limit - 1;
            if (*limit == 0) {
                return 1;
            }
        } else {
            struct FindCommand_OrderedItem *item;
            item = createFindCommand_ObjectBasedOrderItem(args->find_args.ctx, args->find_args.lang, obj, args->find_args.order_field);
            if (item) {
                SVector_InsertFast(args->find_args.order_result, item);
            } else {
                /*
                 * It's not so easy to make the response fail at this point.
                 * Given that we shouldn't generally even end up here in real
                 * life, it's fairly ok to just log the error and return what
                 * we can.
                 */
                fprintf(stderr, "%s:%d: Out of memory while creating an ordered result item\n",
                        __FILE__, __LINE__);
            }
        }
    }

    return 0;
}

static size_t AggregateCommand_AggregateOrderedResult(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        void *arg,
        SelvaModify_Hierarchy *hierarchy,
        ssize_t offset,
        ssize_t limit,
        struct SelvaObject *fields,
        SVector *order_result,
        size_t *nr_fields_out) {
    struct FindCommand_OrderedItem *item;
    struct SVectorIterator it;
    size_t len = 0;

    struct AggregateCommand_Args *args = (struct AggregateCommand_Args *)arg;

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

        struct SelvaModify_HierarchyNode *node;

        /* TODO Consider if having hierarchy node pointers here would be better. */
        node = SelvaHierarchy_FindNode(hierarchy, item->id);
        err = apply_agg_fn(node, args);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, item->id,
                    getSelvaErrorStr(err));
            continue;
        }

        len++;
    }

    return len;
}

static size_t AggregateCommand_AggregateOrderedArrayResult(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        void *arg,
        SelvaModify_Hierarchy *hierarchy,
        ssize_t offset,
        ssize_t limit,
        struct SelvaObject *fields,
        SVector *order_result) {
    struct FindCommand_OrderedItem *item;
    struct SVectorIterator it;
    size_t len = 0;

    struct AggregateCommand_Args *args = (struct AggregateCommand_Args *)arg;

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

        if (item && item->data_obj) {
            // TODO: aggregate instead
            // err = apply_agg_fn(node, arg);
            // err = send_array_object_fields(ctx, lang, hierarchy, item->data_obj, fields);
            len++;
        } else {
            err = SELVA_HIERARCHY_ENOENT;
        }

        if (err) {
            RedisModule_ReplyWithNull(ctx);
            fprintf(stderr, "%s:%d: Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, item->id,
                    getSelvaErrorStr(err));
        }

    }

    return len;
}

static size_t AggregateCommand_PrintAggregateResult(RedisModuleCtx *ctx, struct AggregateCommand_Args *args) {
    if (args->aggregate_type == SELVA_AGGREGATE_TYPE_COUNT_NODE) {
        RedisModule_ReplyWithLongLong(ctx, args->aggregation_result_int);
    } else if (args->aggregate_type == SELVA_AGGREGATE_TYPE_COUNT_UNIQUE_FIELD) {
        RedisModule_ReplyWithLongLong(ctx, args->aggregation_result_int);
    } else if (args->aggregate_type == SELVA_AGGREGATE_TYPE_SUM_FIELD) {
        RedisModule_ReplyWithDouble(ctx, args->aggregation_result_double);
    } else if (args->aggregate_type == SELVA_AGGREGATE_TYPE_AVG_FIELD) {
        RedisModule_ReplyWithDouble(ctx, args->aggregation_result_double / args->item_count);
    }
    return 0;
}

/**
 * Find node in set.
 * SELVA.inherit REDIS_KEY NODE_ID [TYPE1[TYPE2[...]]] [FIELD_NAME1[ FIELD_NAME2[ ...]]]
 */
int SelvaHierarchy_Aggregate(RedisModuleCtx *ctx, int recursive, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_LANG      = 1;
    const int ARGV_REDIS_KEY = 2;
    const int ARGV_AGG_FN    = 3;
    const int ARGV_DIRECTION = 4;
    const int ARGV_ORDER_TXT = 5;
    const int ARGV_ORDER_FLD = 6;
    const int ARGV_ORDER_ORD = 7;
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
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Select traversal method.
     */
    enum SelvaTraversalAlgo algo = HIERARCHY_BFS;
    // err = parse_algo(&algo, argv[ARGV_ALGO]);
    // if (err) {
    //     return replyWithSelvaErrorf(ctx, err, "traversal method");
    // }
    RedisModuleString *agg_fn_rms = argv[ARGV_AGG_FN];
    TO_STR(agg_fn_rms);
    const char agg_fn_val = agg_fn_rms_str[0];

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
		err = SelvaArgsParser_StringSetList(ctx, &fields, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
        if (err == 0) {
            if (SelvaObject_Len(fields, NULL) != 1) {
                return replyWithSelvaErrorf(ctx, err, "fields");
            }

            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "fields");
        }
    }

    struct rpn_ctx *rpn_ctx = NULL;
    struct rpn_expression *filter_expression = NULL;
    struct rpn_ctx *recursive_rpn_ctx = NULL;
    struct rpn_expression *recursive_rpn_expr = NULL;

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

    RedisModuleString *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        if (!SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, getOrderFunc(order))) {
            replyWithSelvaError(ctx, SELVA_ENOMEM);
            goto out;
        }
    }

    /*
     * In the recursive mode the direction/field_name field contains
     * an expression that should to evaluate into a set of field names.
     */
    if (recursive) {
        const char *field_selector;

        recursive_rpn_ctx = rpn_init(1);
        if (!recursive_rpn_ctx) {
            replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "field selector expression");
            goto out;
        }

        field_selector = RedisModule_StringPtrLen(argv[ARGV_DIRECTION], NULL);
        recursive_rpn_expr = rpn_compile(field_selector);
        if (!recursive_rpn_expr) {
            rpn_destroy(recursive_rpn_ctx);
            replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the field selector expression");
            goto out;
        }
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run for each NODE_ID.
     */


    struct AggregateCommand_Args args = {
        .aggregate_type = agg_fn_val,
        .aggregation_result_int = 0,
        .aggregation_result_double = 0,
        .item_count = 0,
        /* .find_args = find_args */
    };


    ssize_t nr_nodes = 0;
    size_t merge_nr_fields = 0;
    const char *array_traversal_ref_field = NULL;
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        enum SelvaModify_HierarchyTraversal dir = SELVA_HIERARCHY_TRAVERSAL_NONE;
        Selva_NodeId nodeId;
        RedisModuleString *ref_field = NULL;

        Selva_NodeIdCpy(nodeId, ids_str + i);

        if (!recursive) {
            /*
             * Get the direction parameter.
             */
            err = parse_dir(ctx, hierarchy, &dir, &ref_field, nodeId, algo, argv[ARGV_DIRECTION]);
            if (err) {
                fprintf(stderr, "%s:%d: Error \"%s\" while selecting the field and dir for the node \"%.*s\", skipping\n",
                        __FILE__, __LINE__,
                        getSelvaErrorStr(err),
                        (int)SELVA_NODE_ID_SIZE, nodeId);
                /* Skip this node */
                continue;
            }
        } else {
            /* recursive can use this for get_skip() */
            dir = SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS;
        }

        /*
         * Run BFS/DFS.
         */
        ssize_t tmp_limit = -1;
        const size_t skip = get_skip(dir); /* Skip n nodes from the results. */
        struct FindCommand_Args find_args = {
            .ctx = ctx,
            .lang = lang,
            .hierarchy = hierarchy,
            .nr_nodes = &nr_nodes,
            .offset = (order == HIERARCHY_RESULT_ORDER_NONE) ? offset + skip : skip,
            .limit = (order == HIERARCHY_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .merge_nr_fields = &merge_nr_fields,
            .order_field = order_by_field,
            .order_result = &order_result,
        };
        args.find_args = find_args;

        const struct SelvaModify_HierarchyCallback cb = {
            .node_cb = AggregateCommand_NodeCb,
            .node_arg = &args,
        };

        if (limit == 0) {
            break;
        }

        if (recursive) {
            err = SelvaHierarchy_TraverseExpression(ctx, hierarchy, nodeId, recursive_rpn_ctx, recursive_rpn_expr, &cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_REF && ref_field) {
            TO_STR(ref_field);

            err = SelvaModify_TraverseHierarchyRef(ctx, hierarchy, nodeId, ref_field_str, ref_field_len, &cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY && ref_field) {
            TO_STR(ref_field);
            array_traversal_ref_field = ref_field_str;
            const struct SelvaModify_ArrayObjectCallback ary_cb = {
                .node_cb = AggregateCommand_ArrayNodeCb,
                .node_arg = &args,
            };

            err = SelvaModify_TraverseArray(ctx, hierarchy, nodeId, ref_field_str, ref_field_len, &ary_cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD && ref_field) {
            TO_STR(ref_field);

            err = SelvaModify_TraverseHierarchyEdge(hierarchy, nodeId, ref_field_str, ref_field_len, &cb);
        } else {
            err = SelvaModify_TraverseHierarchy(hierarchy, nodeId, dir, &cb);
        }
        if (err != 0) {
            /*
             * We can't send an error to the client at this point so we'll just log
             * it and ignore the error.
             */
            fprintf(stderr, "%s:%d: Find failed for node: \"%.*s\"\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId);
        }
    }

    /*
     * If an ordered request was requested then nothing was send to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        struct AggregateCommand_Args args = {
            .aggregate_type = agg_fn_val,
            .aggregation_result_int = 0,
            .aggregation_result_double = 0,
            .item_count = 0,
            .find_args = {
                // we always need context
                .ctx = ctx,
                .fields = fields
            }
        };

        nr_nodes = array_traversal_ref_field
            ? AggregateCommand_AggregateOrderedArrayResult(ctx, lang, &args, hierarchy, offset, limit, fields, &order_result)
            : AggregateCommand_AggregateOrderedResult(ctx, lang, &args, hierarchy, offset, limit, fields, &order_result, &merge_nr_fields);

        AggregateCommand_PrintAggregateResult(ctx, &args);
    } else {
        AggregateCommand_PrintAggregateResult(ctx, &args);
    }

    /* nr_nodes is never negative at this point so we can safely cast it. */
    // RedisModule_ReplySetArrayLength(ctx, (size_t)nr_nodes);

out:
    if (rpn_ctx) {
        rpn_destroy(rpn_ctx);
        rpn_destroy_expression(filter_expression);
    }
    if (recursive_rpn_ctx) {
        rpn_destroy(recursive_rpn_ctx);
        rpn_destroy_expression(recursive_rpn_expr);
    }

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

int SelvaHierarchy_AggregateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaHierarchy_Aggregate(ctx, 0, argv, argc);
}

int SelvaHierarchy_AggregateRecursiveCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaHierarchy_Aggregate(ctx, 1, argv, argc);
}

static int Aggregate_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.aggregate", SelvaHierarchy_AggregateCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "selva.aggregateRecursive", SelvaHierarchy_AggregateRecursiveCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Aggregate_OnLoad);
