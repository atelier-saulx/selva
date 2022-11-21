/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <sys/types.h>
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_server.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "hierarchy.h"
#include "rpn.h"

enum SelvaRpnEvalType {
    EVAL_TYPE_BOOL,
    EVAL_TYPE_DOUBLE,
    EVAL_TYPE_STRING,
    EVAL_TYPE_SET,
};

static int SelvaRpn_Eval(enum SelvaRpnEvalType type, struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    struct SelvaHierarchy *hierarchy;
    enum rpn_error err;

    const int ARGV_KEY         = 1;
    const int ARGV_FILTER_EXPR = 2;
    const int ARGV_FILTER_ARGS = 3;

    if (argc < 3) {
        return selva_send_error_arity(resp);
    }

    hierarchy = main_hierarchy;

    /*
     * Prepare the filter expression.
     */
    struct rpn_ctx *rpn_ctx = NULL;
    struct rpn_expression *filter_expression = NULL;
    const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
    const char *input;

    rpn_ctx = rpn_init(nr_reg);
    if (!rpn_ctx) {
        return selva_send_errorf(resp, SELVA_ENOMEM, "filter expression");
    }

    /*
     * Compile the filter expression.
     */
    input = selva_string_to_str(argv[ARGV_FILTER_EXPR], NULL);
    filter_expression = rpn_compile(input);
    if (!filter_expression) {
        rpn_destroy(rpn_ctx);
        return selva_send_errorf(resp, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
    }

    /* Set reg[0] */
    struct selva_string *reg0 = argv[ARGV_KEY];
    TO_STR(reg0);


    rpn_set_reg(rpn_ctx, 0, reg0_str, reg0_len, 0);
    if (reg0_len >= SELVA_NODE_ID_SIZE) {
        struct SelvaHierarchyNode *node;

        node = SelvaHierarchy_FindNode(hierarchy, reg0_str);
        if (node) {
            rpn_set_hierarchy_node(rpn_ctx, hierarchy, node);
            rpn_set_obj(rpn_ctx, SelvaHierarchy_GetNodeObject(node));
        }
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

    if (type == EVAL_TYPE_BOOL) {
        int res;

        err = rpn_bool(rpn_ctx, filter_expression, &res);
        if (err) {
            goto fail;
        }

        selva_send_ll(resp, res);
    } else if (type == EVAL_TYPE_DOUBLE) {
        double res;

        err = rpn_double(rpn_ctx, filter_expression, &res);
        if (err) {
            goto fail;
        }

        selva_send_double(resp, res);
    } else if (type == EVAL_TYPE_STRING) {
        struct selva_string *res;

        err = rpn_string(rpn_ctx, filter_expression, &res);
        if (err) {
            goto fail;
        }

        selva_send_string(resp, res);
        selva_string_free(res);
    } else if (type == EVAL_TYPE_SET) {
        struct SelvaSet set;
        struct SelvaSetElement *el;

        SelvaSet_Init(&set, SELVA_SET_TYPE_STRING);
        err = rpn_selvaset(rpn_ctx, filter_expression, &set);
        if (err) {
            goto fail;
        }

        selva_send_array(resp, SelvaSet_Size(&set));
        SELVA_SET_STRING_FOREACH(el, &set) {
            selva_send_string(resp, el->value_string);
        }
    } else {
        selva_send_errorf(resp, SELVA_EINTYPE, "Invalid type");
        err = 0;
    }

fail:
    if (err) {
        selva_send_errorf(resp, SELVA_EGENERAL, "Expression failed: %s", rpn_str_error[err]);
    }

    if (rpn_ctx) {
#if MEM_DEBUG
        memset(filter_expression, 0, sizeof(*filter_expression));
#endif
        rpn_destroy_expression(filter_expression);
        rpn_destroy(rpn_ctx);
    }

    return 0;
}

/* FIXME RPN Commands */
#if 0
int SelvaRpn_EvalBoolCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaRpn_Eval(EVAL_TYPE_BOOL, ctx, argv, argc);
}

int SelvaRpn_EvalDoubleCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaRpn_Eval(EVAL_TYPE_DOUBLE, ctx, argv, argc);
}

int SelvaRpn_EvalStringCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaRpn_Eval(EVAL_TYPE_STRING, ctx, argv, argc);
}

int SelvaRpn_EvalSetCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaRpn_Eval(EVAL_TYPE_SET, ctx, argv, argc);
}

static int RpnEval_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.rpn.evalbool", SelvaRpn_EvalBoolCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.rpn.evaldouble", SelvaRpn_EvalDoubleCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.rpn.evalstring", SelvaRpn_EvalStringCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.rpn.evalset", SelvaRpn_EvalSetCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return 0;
}
SELVA_ONLOAD(RpnEval_OnLoad);
#endif
