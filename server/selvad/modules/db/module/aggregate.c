/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "util/auto_free.h"
#include "util/finalizer.h"
#include "util/funmap.h"
#include "util/ptag.h"
#include "util/selva_string.h"
#include "util/svector.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "selva_db.h"
#include "config.h"
#include "arg_parser.h"
#include "hierarchy.h"
#include "modify.h"
#include "rpn.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "subscriptions.h"
#include "traversal.h"
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
    struct finalizer *fin;
    struct selva_server_response_out *resp;
    struct FindCommand_Args find_args;

    agg_func agg;
    enum SelvaHierarchy_AggregateType aggregate_type;
    int uniq_initialized;

    /*
     * Aggregation state.
     */
    long long int aggregation_result_int;
    double aggregation_result_double;
    size_t item_count;
    struct {
        struct SelvaSet set_rmstring;
        struct SelvaSet set_double;
        struct SelvaSet set_longlong;
    } uniq;
};

static void init_uniq(struct AggregateCommand_Args *args) {
    if (args->aggregate_type == SELVA_AGGREGATE_TYPE_COUNT_UNIQUE_FIELD) {
        SelvaSet_Init(&args->uniq.set_rmstring, SELVA_SET_TYPE_STRING);
        SelvaSet_Init(&args->uniq.set_double, SELVA_SET_TYPE_DOUBLE);
        SelvaSet_Init(&args->uniq.set_longlong, SELVA_SET_TYPE_LONGLONG);
        args->uniq_initialized = 1;
    } else {
        args->uniq_initialized = 0;
    }
}

static void destroy_uniq(struct AggregateCommand_Args *args) {
    if (args->uniq_initialized) {
        SelvaSet_Destroy(&args->uniq.set_rmstring);
        SelvaSet_Destroy(&args->uniq.set_double);
        SelvaSet_Destroy(&args->uniq.set_longlong);
        args->uniq_initialized = 0;
    }
}

static void count_uniq(struct AggregateCommand_Args *args) {
    if (args->uniq_initialized && args->aggregate_type == SELVA_AGGREGATE_TYPE_COUNT_UNIQUE_FIELD) {
        args->aggregation_result_int = SelvaSet_Size(&args->uniq.set_rmstring) +
                                       SelvaSet_Size(&args->uniq.set_double) +
                                       SelvaSet_Size(&args->uniq.set_longlong);
    }
}

static int agg_fn_count_obj(struct SelvaObject *obj __unused, struct AggregateCommand_Args* args) {
    args->item_count++;
    return 0;
}

static int get_first_field_value_double(struct SelvaObject *obj, struct SelvaObject *fields_obj, double *out) {
    SVector *fields;
    struct SVectorIterator it;
    const struct selva_string *field;
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

static int agg_fn_count_uniq_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    struct SelvaObject *fields_obj = args->find_args.send_param.fields;
    SVector *fields;
    struct SVectorIterator it;
    const struct selva_string *field;
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
            if (value.type == SELVA_OBJECT_DOUBLE) {
                SelvaSet_Add(&args->uniq.set_double, value.d);
                break;
            } else if (value.type == SELVA_OBJECT_LONGLONG) {
                SelvaSet_Add(&args->uniq.set_longlong, value.ll);
                break;
            } else if (value.type == SELVA_OBJECT_STRING) {
                struct selva_string *tmp = selva_string_dup(value.str, 0);

                if (SelvaSet_Add(&args->uniq.set_rmstring, tmp)) {
                    selva_string_free(tmp);
                }
                break;
            }
        }
    }

    return 0;
}

static int agg_fn_sum_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    struct SelvaObject *fields_obj = args->find_args.send_param.fields;
    double d;

    if (!get_first_field_value_double(obj, fields_obj, &d)) {
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

    if (!get_first_field_value_double(obj, fields_obj, &d)) {
        if (d < args->aggregation_result_double) {
            args->aggregation_result_double = d;
        }
    }

    return 0;
}

static int agg_fn_max_obj(struct SelvaObject *obj, struct AggregateCommand_Args* args) {
    struct SelvaObject *fields_obj = args->find_args.send_param.fields;
    double d;

    if (!get_first_field_value_double(obj, fields_obj, &d)) {
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
        err = rpn_bool(rpn_ctx, args->find_args.filter, &take);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Expression failed (node: \"%.*s\"): \"%s\"\n",
                      (int)SELVA_NODE_ID_SIZE, nodeId,
                      rpn_str_error[err]);
            return 1;
        }
    }

    if (take) {
        const int sort = !!args->find_args.send_param.order_field;

        args->find_args.acc_take++;

        if (!sort) {
            ssize_t *nr_nodes = args->find_args.nr_nodes;
            ssize_t * restrict limit = args->find_args.limit;
            int err;

            err = args->agg(SelvaHierarchy_GetNodeObject(node), args);
            if (err) {
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                          (int)SELVA_NODE_ID_SIZE, nodeId,
                          selva_strerror(err));
            }

            *nr_nodes = *nr_nodes + 1;

            *limit = *limit - 1;
            if (*limit == 0) {
                return 1;
            }
        } else {
            struct TraversalOrderItem *item;

            item = SelvaTraversalOrder_CreateNodeOrderItem(args->fin, args->find_args.lang, node, args->find_args.send_param.order_field);
            if (item) {
                SVector_InsertFast(args->find_args.result, item);
            } else {
                /*
                 * It's not so easy to make the response fail at this point.
                 * Given that we shouldn't generally even end up here in real
                 * life, it's fairly ok to just log the error and return what
                 * we can.
                 */
                SELVA_LOG(SELVA_LOGL_ERR, "Out of memory while creating an order result item\n");
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
        SELVA_LOG(SELVA_LOGL_ERR, "Array subtype not supported: %s\n",
                  SelvaObject_Type2String(subtype, NULL));
        return 1;
    }

    if (take && rpn_ctx) {
        int err;

        /* Set obj to the register */
        err = rpn_set_reg_slvobj(rpn_ctx, 0, obj, 0);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Register set failed: \"%s\"\n",
                      rpn_str_error[err]);
            return 1;
        }
        rpn_set_obj(rpn_ctx, obj);

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(rpn_ctx, args->find_args.filter, &take);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Expression failed: \"%s\"\n",
                      rpn_str_error[err]);
            return 1;
        }
    }

    if (take) {
        const int sort = !!args->find_args.send_param.order_field;

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

            item = SelvaTraversalOrder_CreateObjectOrderItem(args->fin, args->find_args.lang, obj, args->find_args.send_param.order_field);
            if (item) {
                SVector_InsertFast(args->find_args.result, item);
            } else {
                /*
                 * It's not so easy to make the response fail at this point.
                 * Given that we shouldn't generally even end up here in real
                 * life, it's fairly ok to just log the error and return what
                 * we can.
                 */
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to create an order item");
            }
        }
    }

    return 0;
}

static size_t AggregateCommand_AggregateOrderResult(
        struct selva_string *lang __unused,
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
        struct SelvaHierarchyNode *node = PTAG_GETP(item->tagp);
        int err;

        assert(PTAG_GETTAG(item->tagp) == TRAVERSAL_ORDER_ITEM_PTYPE_NODE);

        if (limit-- == 0) {
            break;
        }

        if (node) {
            err = args->agg(SelvaHierarchy_GetNodeObject(node), args);
        } else {
            err = SELVA_HIERARCHY_ENOENT;
        }
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                      (int)SELVA_NODE_ID_SIZE, item->node_id,
                      selva_strerror(err));
            continue;
        }

        len++;
    }

    return len;
}

static size_t AggregateCommand_AggregateOrderArrayResult(
        struct selva_string *lang __unused,
        void *arg,
        SelvaHierarchy *hierarchy __unused,
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
        int err;

        if (limit-- == 0) {
            break;
        }

        assert(PTAG_GETTAG(item->tagp) == TRAVERSAL_ORDER_ITEM_PTYPE_OBJ);
        err = args->agg(PTAG_GETP(item->tagp), args);
        if (err) {
            selva_send_null(args->resp);
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                      (int)SELVA_NODE_ID_SIZE, item->node_id,
                      selva_strerror(err));
        }

        len++;
    }

    return len;
}

static size_t AggregateCommand_SendAggregateResult(const struct AggregateCommand_Args *args) {
    switch (args->aggregate_type) {
    case SELVA_AGGREGATE_TYPE_COUNT_NODE:
        selva_send_ll(args->resp, args->item_count);
        break;
    case SELVA_AGGREGATE_TYPE_COUNT_UNIQUE_FIELD:
        selva_send_ll(args->resp, args->aggregation_result_int);
        break;
    case SELVA_AGGREGATE_TYPE_AVG_FIELD:
        selva_send_double(args->resp, args->aggregation_result_double / (double)args->item_count);
        break;
    default:
        selva_send_double(args->resp, args->aggregation_result_double);
        break;
    }

    return 0;
}

int SelvaHierarchy_AggregateCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    __auto_finalizer struct finalizer fin;
    int err;

    finalizer_init(&fin);

    const int ARGV_LANG      = 1;
    const int ARGV_AGG_FN    = 3;
    const int ARGV_DIRECTION = 4;
    const int ARGV_REF_FIELD = 5;
    int ARGV_INDEX_TXT       = 5;
    __unused int ARGV_INDEX_VAL = 6;
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
        return selva_send_error_arity(resp);
    }

    struct selva_string *lang = argv[ARGV_LANG];
    SVECTOR_AUTOFREE(order_result); /*!< for order result. */
    selvaobject_autofree struct SelvaObject *fields = NULL;
    struct rpn_ctx *traversal_rpn_ctx = NULL;
    struct rpn_expression *traversal_expression = NULL;
    struct rpn_ctx *rpn_ctx = NULL;
    struct selva_string *argv_filter_expr = NULL;
    struct rpn_expression *filter_expression = NULL;
    __selva_autofree selva_stringList index_hints = NULL;
    int nr_index_hints = 0;

    SelvaHierarchy *hierarchy = main_hierarchy;

    /*
     * Parse the traversal arguments.
     */
    enum SelvaTraversal dir;
    const struct selva_string *ref_field = NULL;
    err = SelvaTraversal_ParseDir2(&dir, argv[ARGV_DIRECTION]);
    if (err) {
        selva_send_errorf(resp, err, "Traversal argument");
        goto out;
    }
    if (argc <= ARGV_REF_FIELD &&
        (dir & (SELVA_HIERARCHY_TRAVERSAL_ARRAY |
                SELVA_HIERARCHY_TRAVERSAL_REF |
                SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                SELVA_HIERARCHY_TRAVERSAL_EXPRESSION))) {
        selva_send_error_arity(resp);
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
        const struct selva_string *input = argv[ARGV_REF_FIELD];
        TO_STR(input);

        traversal_rpn_ctx = rpn_init(1);
        traversal_expression = rpn_compile(input_str);
        if (!traversal_expression) {
            selva_send_errorf(resp, SELVA_RPN_ECOMP, "Failed to compile the traversal expression");
            goto out;
        }
        SHIFT_ARGS(1);
    }

    const enum SelvaHierarchy_AggregateType agg_fn_type = selva_string_to_str(argv[ARGV_AGG_FN], NULL)[0];
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
        return selva_send_error(resp, nr_index_hints, NULL, 0);
    } else if (nr_index_hints > 0) {
        SHIFT_ARGS(2 * nr_index_hints);
    }

    /*
     * Parse the order arg.
     */
    enum SelvaResultOrder order = SELVA_RESULT_ORDER_NONE;
    struct selva_string *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = SelvaTraversal_ParseOrderArg(&order_by_field, &order,
                          argv[ARGV_ORDER_TXT],
                          argv[ARGV_ORDER_FLD],
                          argv[ARGV_ORDER_ORD]);
        if (err == 0) {
            SHIFT_ARGS(3);
        } else if (err != SELVA_HIERARCHY_ENOENT) {
            selva_send_errorf(resp, err, "order");
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
            selva_send_errorf(resp, err, "offset");
            goto out;
        }
        if (offset < -1) {
            selva_send_errorf(resp, err, "offset < -1");
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
            selva_send_errorf(resp, err, "limit");
            goto out;
        }
    }

    /*
     * Parse fields.
     */
    if (argc > ARGV_FIELDS_VAL) {
        err = SelvaArgsParser_StringSetList(&fin, &fields, NULL, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
        if (err == 0) {
            if (SelvaObject_Len(fields, NULL) > 1) {
                selva_send_errorf(resp, err, "fields");
                goto out;
            }

            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            selva_send_errorf(resp, err, "fields");
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

        /*
         * Compile the filter expression.
         */
        filter_expression = rpn_compile(selva_string_to_str(argv_filter_expr, NULL));
        if (!filter_expression) {
            selva_send_errorf(resp, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
            goto out;
        }

        /*
         * Get the filter expression arguments and set them to the registers.
         */
        for (int i = ARGV_FILTER_ARGS; i < argc; i++) {
            /* reg[0] is reserved for the current nodeId */
            const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
            size_t str_len;
            const char *str = selva_string_to_str(argv[i], &str_len);

            rpn_set_reg(rpn_ctx, reg_i, str, str_len + 1, 0);
        }
    }

    if (argc <= ARGV_NODE_IDS) {
        selva_send_error(resp, SELVA_HIERARCHY_EINVAL, NULL, 0);
        goto out;
    }

    const struct selva_string *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    if (order != SELVA_RESULT_ORDER_NONE) {
        SelvaTraversalOrder_InitOrderResult(&order_result, order, limit);
    }

    /*
     * Run for each NODE_ID.
     */
    struct AggregateCommand_Args args = {
        .fin = &fin,
        .resp = resp,
        .aggregate_type = agg_fn_type,
        .agg = get_agg_func(agg_fn_type - '0'),
        .aggregation_result_int = 0,
        .aggregation_result_double = initial_double_val,
        .item_count = 0,
    };

    init_uniq(&args);

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
            struct selva_string *dir_expr = NULL;

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
            ind_select = SelvaFindIndex_AutoMulti(hierarchy, dir, dir_expr, nodeId, order, order_by_field, index_hints, nr_index_hints, ind_icb);
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
            .send_param.order = order,
            .send_param.order_field = order_by_field,
            .result = &order_result,
            .process_node = NULL, /* Not used. */
        };

        if (limit == 0) {
            break;
        }

        if (ind_select >= 0) {
            /*
             * There is no need to run the filter again if the indexing was
             * executing the same filter already.
             */
            if (argv_filter_expr && !selva_string_cmp(argv_filter_expr, index_hints[ind_select])) {
                args.find_args.rpn_ctx = NULL;
                args.find_args.filter = NULL;
            }

            err = SelvaFindIndex_Traverse(hierarchy, ind_icb[ind_select], AggregateCommand_NodeCb, &args);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY && ref_field) {
            const struct SelvaObjectArrayForeachCallback ary_cb = {
                .cb = AggregateCommand_ArrayObjectCb,
                .cb_arg = &args,
            };
            TO_STR(ref_field);

            err = SelvaHierarchy_TraverseArray(hierarchy, nodeId, ref_field_str, ref_field_len, &ary_cb);
        } else if (ref_field &&
                   (dir & (SELVA_HIERARCHY_TRAVERSAL_REF |
                           SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                           SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD))) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = AggregateCommand_NodeCb,
                .node_arg = &args,
            };
            TO_STR(ref_field);

            err = SelvaHierarchy_TraverseField(hierarchy, nodeId, dir, ref_field_str, ref_field_len, &cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = AggregateCommand_NodeCb,
                .node_arg = &args,
            };

            err = SelvaHierarchy_TraverseExpressionBfs(hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, NULL, NULL, &cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = AggregateCommand_NodeCb,
                .node_arg = &args,
            };

            err = SelvaHierarchy_TraverseExpression(hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, NULL, NULL, &cb);
        } else {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = AggregateCommand_NodeCb,
                .node_arg = &args,
            };

            err = SelvaHierarchy_Traverse(hierarchy, nodeId, dir, &cb);
        }
        if (err != 0) {
            /*
             * We can't send an error to the client at this point so we'll just log
             * it and ignore the error.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Aggregate failed. err: %s dir: %s node_id: \"%.*s\"",
                      selva_strerror(err),
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
            .fin = &fin,
            .resp = resp,
            .aggregate_type = agg_fn_type,
            .agg = get_agg_func(agg_fn_type - '0'),
            .aggregation_result_int = 0,
            .aggregation_result_double = initial_double_val,
            .item_count = 0,
            .find_args = {
                .send_param.fields = fields,
            }
        };

        init_uniq(&ord_args);
        nr_nodes = (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY)
            ? AggregateCommand_AggregateOrderArrayResult(lang, &ord_args, hierarchy, offset, limit, fields, &order_result)
            : AggregateCommand_AggregateOrderResult(lang, &ord_args, offset, limit, fields, &order_result);
        count_uniq(&args);
        AggregateCommand_SendAggregateResult(&ord_args);
        destroy_uniq(&args);
    } else {
        count_uniq(&args);
        AggregateCommand_SendAggregateResult(&args);
    }

    destroy_uniq(&args);

out:
    if (traversal_rpn_ctx) {
        rpn_destroy(traversal_rpn_ctx);
        rpn_destroy_expression(traversal_expression);
    }
    if (rpn_ctx) {
        rpn_destroy(rpn_ctx);
        rpn_destroy_expression(filter_expression);
    }

    return 0;
#undef SHIFT_ARGS
}

/**
 * Find node in set.
 * SELVA.inherit REDIS_KEY NODE_ID [TYPE1[TYPE2[...]]] [FIELD_NAME1[ FIELD_NAME2[ ...]]]
 */
int SelvaHierarchy_AggregateInCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    __auto_finalizer struct finalizer fin;
    int err;

    finalizer_init(&fin);

    const int ARGV_LANG      = 1;
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
        return selva_send_error_arity(resp);
    }

    struct selva_string *lang = argv[ARGV_LANG];
    SelvaHierarchy *hierarchy = main_hierarchy;

    const enum SelvaHierarchy_AggregateType agg_fn_type = selva_string_to_str(argv[ARGV_AGG_FN], NULL)[0];
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
    struct selva_string *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = SelvaTraversal_ParseOrderArg(&order_by_field, &order,
                          argv[ARGV_ORDER_TXT],
                          argv[ARGV_ORDER_FLD],
                          argv[ARGV_ORDER_ORD]);
        if (err == 0) {
            SHIFT_ARGS(3);
        } else if (err != SELVA_HIERARCHY_ENOENT) {
            return selva_send_errorf(resp, err, "order");
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
            return selva_send_errorf(resp, err, "offset");
        }
        if (offset < -1) {
            return selva_send_errorf(resp, err, "offset < -1");
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
            return selva_send_errorf(resp, err, "limit");
        }
    }

    /*
     * Parse fields.
     */
    selvaobject_autofree struct SelvaObject *fields = NULL;
    if (argc > ARGV_FIELDS_VAL) {
        err = SelvaArgsParser_StringSetList(&fin, &fields, NULL, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
        if (err == 0) {
            if (SelvaObject_Len(fields, NULL) > 1) {
                return selva_send_errorf(resp, err, "fields");
            }

            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return selva_send_errorf(resp, err, "fields");
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

        /*
         * Compile the filter expression.
         */
        input = selva_string_to_str(argv[ARGV_FILTER_EXPR], NULL);
        filter_expression = rpn_compile(input);
        if (!filter_expression) {
            rpn_destroy(rpn_ctx);
            return selva_send_errorf(resp, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
        }

        /*
         * Get the filter expression arguments and set them to the registers.
         */
        for (int i = ARGV_FILTER_ARGS; i < argc; i++) {
            /* reg[0] is reserved for the current nodeId */
            const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
            size_t str_len;
            const char *str = selva_string_to_str(argv[i], &str_len);

            rpn_set_reg(rpn_ctx, reg_i, str, str_len + 1, 0);
        }
    }

    SVECTOR_AUTOFREE(order_result); /*!< for ordered result. */

    if (argc <= ARGV_NODE_IDS) {
        selva_send_error(resp, SELVA_HIERARCHY_EINVAL, NULL, 0);
        goto out;
    }

    const struct selva_string *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    if (order != SELVA_RESULT_ORDER_NONE) {
        SelvaTraversalOrder_InitOrderResult(&order_result, order, limit);
    }

    /*
     * Run the filter for each node.
     */

    struct AggregateCommand_Args args = {
        .fin = &fin,
        .resp = resp,
        .aggregate_type = agg_fn_type,
        .agg = get_agg_func(agg_fn_type - '0'),
        .aggregation_result_int = 0,
        .aggregation_result_double = initial_double_val,
        .item_count = 0,
    };

    init_uniq(&args);

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
            .send_param.order = order,
            .send_param.order_field = order_by_field,
            .result = &order_result,
            .process_node = NULL, /* Not used. */
        };

        args.find_args = find_args;

        node = SelvaHierarchy_FindNode(hierarchy, ids_str + i);
        if (node) {
            (void)AggregateCommand_NodeCb(hierarchy, node, &args);
        }
    }

    /*
     * If an ordered request was requested then nothing was sent to the client yet
     * and we need to do it now.
     */
    if (order != SELVA_RESULT_ORDER_NONE) {
        struct AggregateCommand_Args ord_args = {
            .fin = &fin,
            .resp = resp,
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

        init_uniq(&ord_args);
        AggregateCommand_AggregateOrderResult(lang, &ord_args, offset, limit, fields, &order_result);
        count_uniq(&ord_args);
        AggregateCommand_SendAggregateResult(&ord_args);
        destroy_uniq(&ord_args);
    } else {
        count_uniq(&args);
        AggregateCommand_SendAggregateResult(&args);
    }

    destroy_uniq(&args);

out:
    if (rpn_ctx) {
        rpn_destroy(rpn_ctx);
        rpn_destroy_expression(filter_expression);
    }

    return 0;
#undef SHIFT_ARGS
}

/* FIXME Register commands */
#if 0
static int Aggregate_OnLoad(void) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.aggregate", SelvaHierarchy_AggregateCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.aggregateIn", SelvaHierarchy_AggregateInCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return 0;
}
SELVA_ONLOAD(Aggregate_OnLoad);
#endif
