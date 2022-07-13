#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "redismodule.h"
#include "cdefs.h"
#include "auto_free.h"
#include "arg_parser.h"
#include "errors.h"
#include "hierarchy.h"
#include "rms.h"
#include "rpn.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "selva_trace.h"
#include "traversal.h"
#include "modify.h"

/*
 * TODO Update ops could be implemented with function pointers for improved perf.
 */
struct update_op {
    char type_code;
    RedisModuleString *field;
    union {
        RedisModuleString *value;
        struct SelvaModify_OpSet *set_opts;
        struct SelvaModify_OpIncrement increment_opts_int64;
        struct SelvaModify_OpIncrementDouble increment_opts_double;
        long long ll;
        double d;
        uint32_t u32;
    };
};

struct update_node_cb {
    struct rpn_ctx *rpn_ctx;
    const struct rpn_expression *filter;
    int nr_update_ops;
    struct update_op *update_ops;
};

SELVA_TRACE_HANDLE(cmd_update_bfs_expression);
SELVA_TRACE_HANDLE(cmd_update_refs);
SELVA_TRACE_HANDLE(cmd_update_rest);
SELVA_TRACE_HANDLE(cmd_update_traversal_expression);

static int parse_update_ops(RedisModuleCtx *ctx, RedisModuleString **argv, int argc, struct update_op **update_ops) {
    long long nr_update_ops;

    if (RedisModule_StringToLongLong(argv[0], &nr_update_ops) == REDISMODULE_ERR) {
        return SELVA_EINVAL;
    }

    if (nr_update_ops == 0 || (nr_update_ops * 3) > (argc - 1) || nr_update_ops > 300) { // TODO Tunable?
        return SELVA_EINVAL;
    }

    *update_ops = RedisModule_PoolAlloc(ctx, nr_update_ops * sizeof(struct update_op));

    argv++;
    for (long long i = 0; i < nr_update_ops * 3; i += 3) {
        struct update_op op = {
            .type_code = RedisModule_StringPtrLen(argv[i], NULL)[0],
            .field = argv[i + 1],
            .value = argv[i + 2],
        };

        if (op.type_code == SELVA_MODIFY_ARG_OP_SET) {
            op.set_opts = SelvaModify_OpSet_align(ctx, op.value);
            if (!op.set_opts) {
                op.type_code = SELVA_MODIFY_ARG_INVALID;
            }
        } else if (op.type_code == SELVA_MODIFY_ARG_OP_INCREMENT) {
            size_t len;
            const char *str = RedisModule_StringPtrLen(op.value, &len);

            if (len == sizeof(struct SelvaModify_OpIncrement)) {
                memcpy(&op.increment_opts_int64, (const struct SelvaModify_OpIncrement *)str, len);
            } else {
                op.type_code = SELVA_MODIFY_ARG_INVALID;
            }
        } else if (op.type_code == SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE) {
            size_t len;
            const char *str = RedisModule_StringPtrLen(op.value, &len);

            if (len == sizeof(struct SelvaModify_OpIncrementDouble)) {
                memcpy(&op.increment_opts_double, (const struct SelvaModify_OpIncrementDouble *)str, len);
            } else {
                op.type_code = SELVA_MODIFY_ARG_INVALID;
            }
        } else if (op.type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG ||
                   op.type_code == SELVA_MODIFY_ARG_LONGLONG) {
            size_t value_len;
            const char *value_str = RedisModule_StringPtrLen(op.value, &value_len);
            long long ll;

            if (value_len == sizeof(ll)) {
                memcpy(&op.ll, value_str, sizeof(ll));
            } else {
                op.type_code = SELVA_MODIFY_ARG_INVALID;
            }
        } else if (op.type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE ||
                   op.type_code == SELVA_MODIFY_ARG_DOUBLE) {
            size_t value_len;
            const char *value_str = RedisModule_StringPtrLen(op.value, &value_len);
            double d;

            if (value_len == sizeof(d)) {
                memcpy(&op.d, value_str, sizeof(d));
            } else {
                op.type_code = SELVA_MODIFY_ARG_INVALID;
            }
        } else if (op.type_code == SELVA_MODIFY_ARG_OP_ARRAY_REMOVE) {
            size_t value_len;
            const char *value_str = RedisModule_StringPtrLen(op.value, &value_len);

            if (value_len == sizeof(uint32_t)) {
                memcpy(&op.u32, value_str, sizeof(uint32_t));
            } else {
                op.type_code = SELVA_MODIFY_ARG_INVALID;
            }
        }

        memcpy(&(*update_ops)[i / 3], &op, sizeof(op));
    }

    return (int)nr_update_ops;
}

static enum selva_op_repl_state update_op(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct update_op *op) {
    const char type_code = op->type_code;
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    RedisModuleString *field = op->field;
    TO_STR(field);

    if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT) {
        const struct SelvaModify_OpIncrement *incrementOpts = &op->increment_opts_int64;
        int err;

        err = SelvaObject_IncrementLongLong(obj, field, incrementOpts->$default, incrementOpts->$increment);
        if (err) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE) {
        const struct SelvaModify_OpIncrementDouble *incrementOpts = &op->increment_opts_double;
        int err;

        err = SelvaObject_IncrementDouble(obj, field, incrementOpts->$default, incrementOpts->$increment);
        if (err) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_SET) {
        Selva_NodeId node_id;
        int err;

        SelvaHierarchy_GetNodeId(node_id, node);

        err = SelvaModify_ModifySet(ctx, hierarchy, node_id, node, obj, field, op->set_opts);
        if (err <= 0) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_DEL) {
        int err;

        err = SelvaModify_ModifyDel(ctx, hierarchy, node, obj, field);
        if (err) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING ||
               type_code == SELVA_MODIFY_ARG_STRING) {
        const enum SelvaObjectType old_type = SelvaObject_GetTypeStr(obj, field_str, field_len);
        RedisModuleString *value = op->value;
        size_t value_len;
        const char *value_str = RedisModule_StringPtrLen(value, &value_len);
        RedisModuleString *old_value;
        RedisModuleString *shared;

        if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING && old_type != SELVA_OBJECT_NULL) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        if (old_type == SELVA_OBJECT_STRING && !SelvaObject_GetString(obj, field, &old_value)) {
            TO_STR(old_value);

            if (old_value_len == value_len && !memcmp(old_value_str, value_str, value_len)) {
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        }

        shared = Share_RMS(field_str, field_len, value);
        if (shared) {
            int err;

            err = SelvaObject_SetString(obj, field, shared);
            if (err) {
                RedisModule_FreeString(NULL, shared);
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        } else {
            int err;

            err = SelvaObject_SetString(obj, field, value);
            if (err) {
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }

            RedisModule_RetainString(ctx, value);
        }
    } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG ||
               type_code == SELVA_MODIFY_ARG_LONGLONG) {
        long long ll = op->ll;

        if (type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG) {
            int err;

            err = SelvaObject_SetLongLongDefault(obj, field, ll);
            if (err) {
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        } else {
            long long old_value;
            int err;

            if (!SelvaObject_GetLongLong(obj, field, &old_value)) {
                if (old_value == ll) {
                    return SELVA_OP_REPL_STATE_UNCHANGED;
                }
            }

            err = SelvaObject_SetLongLong(obj, field, ll);
            if (err) {
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        }
    } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE ||
               type_code == SELVA_MODIFY_ARG_DOUBLE) {
        double d = op->d;

        if (type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE) {
            int err;

            err = SelvaObject_SetDoubleDefault(obj, field, d);
            if (err) {
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        } else {
            double old_value;
            int err;

            if (!SelvaObject_GetDouble(obj, field, &old_value)) {
                if (old_value == d) {
                    return SELVA_OP_REPL_STATE_UNCHANGED;
                }
            }

            err = SelvaObject_SetDouble(obj, field, d);
            if (err) {
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_OBJ_META) {
        return SelvaModify_ModifyMetadata(ctx, obj, field, op->value);
    } else if (type_code == SELVA_MODIFY_ARG_OP_ARRAY_REMOVE) {
        int err;

        err = SelvaObject_RemoveArrayIndexStr(obj, field_str, field_len, op->u32);
        if (err) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else {
        /* TODO Invalid type. */
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    return SELVA_OP_REPL_STATE_UPDATED;
}

static int update_node_cb(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    const struct update_node_cb *args = (const struct update_node_cb *)arg;
    const int nr_update_ops = args->nr_update_ops;
    const struct update_op *update_ops = args->update_ops;
    struct rpn_ctx *rpn_ctx = args->rpn_ctx;

    if (rpn_ctx) {
        Selva_NodeId nodeId;
        int res;
        int err;

        SelvaHierarchy_GetNodeId(nodeId, node);

        /* Set node_id to the register */
        rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
        rpn_set_hierarchy_node(rpn_ctx, hierarchy, node);
        rpn_set_obj(rpn_ctx, SelvaHierarchy_GetNodeObject(node));

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(ctx, rpn_ctx, args->filter, &res);
        if (err) {
            fprintf(stderr, "%s:%d: Expression failed (node: \"%.*s\"): \"%s\"\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    rpn_str_error[err]);
            return 1;
        }

        if (!res) {
            return 0;
        }
    }

    SelvaSubscriptions_FieldChangePrecheck(ctx, hierarchy, node);

    /*
     * TODO Some modify op codes are not supported:
     * - Anything handled by modify_array_op()
     * - SELVA_MODIFY_ARG_OP_ARRAY_PUSH
     * - SELVA_MODIFY_ARG_OP_ARRAY_INSERT
     * - SELVA_MODIFY_ARG_STRING_ARRAY
     * - SELVA_MODIFY_ARG_OP_EDGE_META
     */
    for (int i = 0; i < nr_update_ops; i++) {
        const struct update_op *op = &update_ops[i];
        enum selva_op_repl_state repl_state;

        repl_state = update_op(ctx, hierarchy, node, op);
        if (repl_state == SELVA_OP_REPL_STATE_UPDATED) {
            size_t field_len;
            const char *field_str = RedisModule_StringPtrLen(op->field, &field_len);

            if (strcmp(field_str, SELVA_PARENTS_FIELD) && strcmp(field_str, SELVA_CHILDREN_FIELD)) {
                SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, node, field_str, field_len);
            }
        }
    }

    /*
     * Let's send the events accumulated so far to avoid growing the buffers
     * too much.
     */
    SelvaSubscriptions_SendDeferredEvents(hierarchy);

    return 0;
}

int SelvaCommand_Update(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_REDIS_KEY = 1;
    const int ARGV_DIRECTION = 2;
    const int ARGV_REF_FIELD = 3;
    int ARGV_EDGE_FILTER_TXT = 3;
    int ARGV_EDGE_FILTER_VAL = 4;
    int ARGV_NR_UPDATE_OPS   = 3;
    int ARGV_UPDATE_OPS      = 4;
    int ARGV_NODE_IDS        = 3;
    int ARGV_FILTER_EXPR     = 4;
    int ARGV_FILTER_ARGS     = 5;
#define SHIFT_ARGS(i) \
    ARGV_EDGE_FILTER_TXT += i; \
    ARGV_EDGE_FILTER_VAL += i; \
    ARGV_NR_UPDATE_OPS += i; \
    ARGV_UPDATE_OPS += i; \
    ARGV_NODE_IDS += i; \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    if (argc < 6) {
        return RedisModule_WrongArity(ctx);
    }

    __auto_free_rpn_ctx struct rpn_ctx *traversal_rpn_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *traversal_expression = NULL;
    __auto_free_rpn_ctx struct rpn_ctx *edge_filter_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *edge_filter = NULL;

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
        return REDISMODULE_OK;
    }
    if (argc <= ARGV_REF_FIELD &&
        (dir & (SELVA_HIERARCHY_TRAVERSAL_REF |
                SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                SELVA_HIERARCHY_TRAVERSAL_EXPRESSION))) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_OK;
    }
    if (dir & (SELVA_HIERARCHY_TRAVERSAL_REF |
               SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
               SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD)) {
        ref_field = argv[ARGV_REF_FIELD];
        SHIFT_ARGS(1);
    } else if (dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                      SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) {
        const RedisModuleString *input = argv[ARGV_REF_FIELD];
        TO_STR(input);

        traversal_rpn_ctx = rpn_init(1);
        traversal_expression = rpn_compile(input_str);
        if (!traversal_expression) {
            return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the traversal expression");
        }
        SHIFT_ARGS(1);
    }

    if (argc > ARGV_EDGE_FILTER_VAL) {
        const char *expr_str;

        err = SelvaArgParser_StrOpt(&expr_str, "edge_filter", argv[ARGV_EDGE_FILTER_TXT], argv[ARGV_EDGE_FILTER_VAL]);
        if (err == 0) {
            SHIFT_ARGS(2);

            if (!(dir & (SELVA_HIERARCHY_TRAVERSAL_EXPRESSION |
                         SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION))) {
                return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "edge_filter can be only used with expression traversals");
            }

            edge_filter_ctx = rpn_init(1);
            edge_filter = rpn_compile(expr_str);
            if (!edge_filter) {
                return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "edge_filter");
            }
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "edge_filter");
        }
    }

    /*
     * Parse the update ops.
     */
    struct update_op *update_ops;
    const int nr_update_ops = parse_update_ops(ctx, argv + ARGV_NR_UPDATE_OPS, argc - ARGV_NR_UPDATE_OPS, &update_ops);
    if (nr_update_ops < 0 || !update_ops) {
        return replyWithSelvaErrorf(ctx, nr_update_ops, "update_ops");
    }
    SHIFT_ARGS(1 + nr_update_ops * 3);

    /*
     * Prepare the filter expression if given.
     */
    RedisModuleString *argv_filter_expr = NULL;
    __auto_free_rpn_ctx struct rpn_ctx *rpn_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *filter_expression = NULL;
    if (argc >= ARGV_FILTER_EXPR + 1) {
        argv_filter_expr = argv[ARGV_FILTER_EXPR];
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;

        rpn_ctx = rpn_init(nr_reg);
        filter_expression = rpn_compile(RedisModule_StringPtrLen(argv_filter_expr, NULL));
        if (!filter_expression) {
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

    if (argc <= ARGV_NODE_IDS) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_EINVAL);
    }

    const RedisModuleString *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    /*
     * Run for each NODE_ID.
     */
    ssize_t nr_nodes = 0;
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        Selva_NodeId nodeId;

        Selva_NodeIdCpy(nodeId, ids_str + i);
        if (nodeId[0] == '\0') {
            /* Just skip empty IDs. */
            continue;
        }

        /* TODO */
        struct update_node_cb args = {
            .nr_update_ops = nr_update_ops,
            .update_ops = update_ops,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
        };

        if ((dir & (SELVA_HIERARCHY_TRAVERSAL_REF |
                    SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                    SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD))
                   && ref_field) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = update_node_cb,
                .node_arg = &args,
            };
            TO_STR(ref_field);

            SELVA_TRACE_BEGIN(cmd_update_refs);
            err = SelvaHierarchy_TraverseField(ctx, hierarchy, nodeId, dir, ref_field_str, ref_field_len, &cb);
            SELVA_TRACE_END(cmd_update_refs);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = update_node_cb,
                .node_arg = &args,
            };

            SELVA_TRACE_BEGIN(cmd_update_bfs_expression);
            err = SelvaHierarchy_TraverseExpressionBfs(ctx, hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, edge_filter_ctx, edge_filter, &cb);
            SELVA_TRACE_END(cmd_update_bfs_expression);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = update_node_cb,
                .node_arg = &args,
            };

            SELVA_TRACE_BEGIN(cmd_update_traversal_expression);
            err = SelvaHierarchy_TraverseExpression(ctx, hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, edge_filter_ctx, edge_filter, &cb);
            SELVA_TRACE_END(cmd_update_traversal_expression);
        } else {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = update_node_cb,
                .node_arg = &args,
            };

            SELVA_TRACE_BEGIN(cmd_update_rest);
            err = SelvaHierarchy_Traverse(ctx, hierarchy, nodeId, dir, &cb);
            SELVA_TRACE_END(cmd_update_rest);
        }
        if (err != 0) {
            /*
             * We can't send an error to the client at this point so we'll just log
             * it and ignore the error.
             */
            fprintf(stderr, "%s:%d: Update failed. err: %s dir: %s node_id: \"%.*s\"\n",
                    __FILE__, __LINE__,
                    getSelvaErrorStr(err),
                    SelvaTraversal_Dir2str(dir),
                    (int)SELVA_NODE_ID_SIZE, nodeId);
        }
    }

    RedisModule_ReplyWithLongLong(ctx, nr_nodes);
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

static int Update_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.update", SelvaCommand_Update, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Update_OnLoad);
