/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "util/auto_free.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "selva_db.h"
#include "jemalloc.h"
#include "arg_parser.h"
#include "hierarchy.h"
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
    struct selva_string *field;
    union {
        struct selva_string *value;
        struct SelvaModify_OpSet *set_opts;
        struct SelvaModify_OpIncrement increment_opts_int64;
        struct SelvaModify_OpIncrementDouble increment_opts_double;
        long long ll;
        double d;
        uint32_t u32;
    };
};

struct update_node_cb {
    struct selva_server_response_out *resp;
    struct rpn_ctx *rpn_ctx;
    const struct rpn_expression *filter;
    int nr_update_ops;
    struct update_op *update_ops;
};

SELVA_TRACE_HANDLE(cmd_update_bfs_expression);
SELVA_TRACE_HANDLE(cmd_update_refs);
SELVA_TRACE_HANDLE(cmd_update_rest);
SELVA_TRACE_HANDLE(cmd_update_traversal_expression);

static int parse_update_ops(struct finalizer *fin, struct selva_string **argv, int argc, struct update_op **update_ops) {
    long long nr_update_ops;
    int err;

    err = selva_string_to_ll(argv[0], &nr_update_ops);
    if (err) {
        return err;
    }

    if (nr_update_ops == 0 || (nr_update_ops * 3) > (argc - 1) || nr_update_ops > SELVA_CMD_UPDATE_MAX) {
        return SELVA_EINVAL;
    }

    *update_ops = selva_malloc(nr_update_ops * sizeof(struct update_op));
    finalizer_add(fin, *update_ops, selva_free);

    argv++;
    for (long long i = 0; i < nr_update_ops * 3; i += 3) {
        struct update_op op = {
            .type_code = selva_string_to_str(argv[i], NULL)[0],
            .field = argv[i + 1],
            .value = argv[i + 2],
        };

        if (op.type_code == SELVA_MODIFY_ARG_OP_SET) {
            op.set_opts = SelvaModify_OpSet_align(fin, op.value);
            if (!op.set_opts) {
                return SELVA_EINVAL;
            }

            if (op.set_opts->op_set_type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) {
                return SELVA_ENOTSUP;
            }
        } else if (op.type_code == SELVA_MODIFY_ARG_OP_INCREMENT) {
            size_t len;
            const char *str = selva_string_to_str(op.value, &len);

            if (len != sizeof(struct SelvaModify_OpIncrement)) {
                return SELVA_EINVAL;
            }

            memcpy(&op.increment_opts_int64, (const struct SelvaModify_OpIncrement *)str, len);
        } else if (op.type_code == SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE) {
            size_t len;
            const char *str = selva_string_to_str(op.value, &len);

            if (len != sizeof(struct SelvaModify_OpIncrementDouble)) {
                return SELVA_EINVAL;
            }

            memcpy(&op.increment_opts_double, (const struct SelvaModify_OpIncrementDouble *)str, len);
        } else if (op.type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG ||
                   op.type_code == SELVA_MODIFY_ARG_LONGLONG) {
            size_t value_len;
            const char *value_str = selva_string_to_str(op.value, &value_len);
            long long ll;

            if (value_len != sizeof(ll)) {
                return SELVA_EINVAL;
            }

            memcpy(&op.ll, value_str, sizeof(ll));
        } else if (op.type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE ||
                   op.type_code == SELVA_MODIFY_ARG_DOUBLE) {
            size_t value_len;
            const char *value_str = selva_string_to_str(op.value, &value_len);
            double d;

            if (value_len != sizeof(d)) {
                return SELVA_EINVAL;
            }

            memcpy(&op.d, value_str, sizeof(d));
        } else if (op.type_code == SELVA_MODIFY_ARG_OP_ARRAY_REMOVE) {
            size_t value_len;
            const char *value_str = selva_string_to_str(op.value, &value_len);

            if (value_len != sizeof(uint32_t)) {
                return SELVA_EINVAL;
            }

            memcpy(&op.u32, value_str, sizeof(uint32_t));
        }

        memcpy(&(*update_ops)[i / 3], &op, sizeof(op));
    }

    return (int)nr_update_ops;
}

static enum selva_op_repl_state update_op(
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct update_op *op) {
    const char type_code = op->type_code;
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    struct selva_string *field = op->field;
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

        err = SelvaModify_ModifySet(hierarchy, node_id, node, obj, field, op->set_opts);
        if (err <= 0) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_DEL) {
        int err;

        err = SelvaModify_ModifyDel(hierarchy, node, obj, field);
        if (err) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING ||
               type_code == SELVA_MODIFY_ARG_STRING) {
        const enum SelvaObjectType old_type = SelvaObject_GetTypeStr(obj, field_str, field_len);
        struct selva_string *value = op->value;
        size_t value_len;
        const char *value_str = selva_string_to_str(value, &value_len);
        struct selva_string *old_value;

        if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING && old_type != SELVA_OBJECT_NULL) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        if (old_type == SELVA_OBJECT_STRING && !SelvaObject_GetString(obj, field, &old_value)) {
            TO_STR(old_value);

            if (old_value_len == value_len && !memcmp(old_value_str, value_str, value_len)) {
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        }

        if (SELVA_IS_TYPE_FIELD(field_str, field_len)) {
            struct selva_string *shared;
            int err;

            shared = selva_string_create(value_str, value_len, SELVA_STRING_INTERN);
            err = SelvaObject_SetString(obj, field, shared);
            if (err) {
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        } else {
            int err;

            /* FIXME Do we really need to dup here? */
            err = SelvaObject_SetString(obj, field, selva_string_dup(value, 0));
            if (err) {
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        }
    } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG) {
        long long ll = op->ll;
        int err;

        err = SelvaObject_SetLongLongDefault(obj, field, ll);
        if (err) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_LONGLONG) {
        long long ll = op->ll;
        int err;

        err = SelvaObject_UpdateLongLong(obj, field, ll);
        if (err) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE) {
        double d = op->d;
        int err;

        err = SelvaObject_SetDoubleDefault(obj, field, d);
        if (err) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_DOUBLE) {
        double d = op->d;
        int err;

        err = SelvaObject_UpdateDouble(obj, field, d);
        if (err) {
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_OBJ_META) {
        return SelvaModify_ModifyMetadata(resp, obj, field, op->value);
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
        err = rpn_bool(rpn_ctx, args->filter, &res);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Expression failed (node: \"%.*s\"): \"%s\"",
                      (int)SELVA_NODE_ID_SIZE, nodeId,
                      rpn_str_error[err]);
            return 1;
        }

        if (!res) {
            return 0;
        }
    }

    SelvaSubscriptions_FieldChangePrecheck(hierarchy, node);

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

        repl_state = update_op(args->resp, hierarchy, node, op);
        if (repl_state == SELVA_OP_REPL_STATE_UPDATED) {
            size_t field_len;
            const char *field_str = selva_string_to_str(op->field, &field_len);

            if (strcmp(field_str, SELVA_PARENTS_FIELD) && strcmp(field_str, SELVA_CHILDREN_FIELD)) {
                SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node, field_str, field_len);
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

void SelvaCommand_Update(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    SelvaHierarchy *hierarchy = main_hierarchy;
    struct selva_string **argv;
    int argc;
    int err;

    finalizer_init(&fin);

    const int ARGV_DIRECTION = 0;
    const int ARGV_REF_FIELD = 1;
    int ARGV_EDGE_FILTER_TXT = 1;
    int ARGV_EDGE_FILTER_VAL = 2;
    int ARGV_NR_UPDATE_OPS   = 1;
    __unused int ARGV_UPDATE_OPS = 2;
    int ARGV_NODE_IDS        = 2;
    int ARGV_FILTER_EXPR     = 2;
    int ARGV_FILTER_ARGS     = 3;
#define SHIFT_ARGS(i) \
    ARGV_EDGE_FILTER_TXT += i; \
    ARGV_EDGE_FILTER_VAL += i; \
    ARGV_NR_UPDATE_OPS += i; \
    ARGV_UPDATE_OPS += i; \
    ARGV_NODE_IDS += i; \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc < 4) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    __auto_free_rpn_ctx struct rpn_ctx *traversal_rpn_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *traversal_expression = NULL;
    __auto_free_rpn_ctx struct rpn_ctx *edge_filter_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *edge_filter = NULL;

    /*
     * Parse the traversal arguments.
     */
    enum SelvaTraversal dir;
    const struct selva_string *ref_field = NULL;
    err = SelvaTraversal_ParseDir2(&dir, argv[ARGV_DIRECTION]);
    if (err) {
        selva_send_errorf(resp, err, "Traversal argument");
        return;
    }
    if (argc <= ARGV_REF_FIELD &&
        (dir & (SELVA_HIERARCHY_TRAVERSAL_REF |
                SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                SELVA_HIERARCHY_TRAVERSAL_EXPRESSION))) {
        selva_send_error_arity(resp);
        return;
    }
    if (dir & (SELVA_HIERARCHY_TRAVERSAL_REF |
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
            return;
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
                selva_send_errorf(resp, SELVA_EINVAL, "edge_filter can be only used with expression traversals");
                return;
            }

            edge_filter_ctx = rpn_init(1);
            edge_filter = rpn_compile(expr_str);
            if (!edge_filter) {
                selva_send_errorf(resp, SELVA_RPN_ECOMP, "edge_filter");
                return;
            }
        } else if (err != SELVA_ENOENT) {
            selva_send_errorf(resp, err, "edge_filter");
            return;
        }
    }

    /*
     * Parse the update ops.
     */
    struct update_op *update_ops;
    const int nr_update_ops = parse_update_ops(&fin, argv + ARGV_NR_UPDATE_OPS, argc - ARGV_NR_UPDATE_OPS, &update_ops);
    if (nr_update_ops < 0 || !update_ops) {
        selva_send_errorf(resp, nr_update_ops, "update_ops");
        return;
    }
    SHIFT_ARGS(1 + nr_update_ops * 3);

    /*
     * Prepare the filter expression if given.
     */
    struct selva_string *argv_filter_expr = NULL;
    __auto_free_rpn_ctx struct rpn_ctx *rpn_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *filter_expression = NULL;
    if (argc >= ARGV_FILTER_EXPR + 1) {
        argv_filter_expr = argv[ARGV_FILTER_EXPR];
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;

        rpn_ctx = rpn_init(nr_reg);
        filter_expression = rpn_compile(selva_string_to_str(argv_filter_expr, NULL));
        if (!filter_expression) {
            selva_send_errorf(resp, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
            return;
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
        selva_send_errorf(resp, SELVA_HIERARCHY_EINVAL, "node_ids missing");
        return;
    }

    const struct selva_string *ids = argv[ARGV_NODE_IDS];
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
            .resp = resp,
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
            err = SelvaHierarchy_TraverseField(hierarchy, nodeId, dir, ref_field_str, ref_field_len, &cb);
            SELVA_TRACE_END(cmd_update_refs);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = update_node_cb,
                .node_arg = &args,
            };

            SELVA_TRACE_BEGIN(cmd_update_bfs_expression);
            err = SelvaHierarchy_TraverseExpressionBfs(hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, edge_filter_ctx, edge_filter, &cb);
            SELVA_TRACE_END(cmd_update_bfs_expression);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = update_node_cb,
                .node_arg = &args,
            };

            SELVA_TRACE_BEGIN(cmd_update_traversal_expression);
            err = SelvaHierarchy_TraverseExpression(hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, edge_filter_ctx, edge_filter, &cb);
            SELVA_TRACE_END(cmd_update_traversal_expression);
        } else {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = update_node_cb,
                .node_arg = &args,
            };

            SELVA_TRACE_BEGIN(cmd_update_rest);
            err = SelvaHierarchy_Traverse(hierarchy, nodeId, dir, &cb);
            SELVA_TRACE_END(cmd_update_rest);
        }
        if (err != 0) {
            /*
             * We can't send an error to the client at this point so we'll just log
             * it and ignore the error.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Update failed. err: %s dir: %s node_id: \"%.*s\"",
                      selva_strerror(err),
                      SelvaTraversal_Dir2str(dir),
                      (int)SELVA_NODE_ID_SIZE, nodeId);
        }
    }

    selva_send_ll(resp, nr_nodes);
    /* TODO Replicate */
#if 0
    RedisModule_ReplicateVerbatim(ctx);
#endif
#undef SHIFT_ARGS
}

static int Update_OnLoad(void) {
    selva_mk_command(CMD_UPDATE_ID, SELVA_CMD_MODE_MUTATE, "update", SelvaCommand_Update);

    return 0;
}
SELVA_ONLOAD(Update_OnLoad);
