/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <sys/types.h>
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_server.h"
#include "arg_parser.h"
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

static void SelvaRpn_Eval(enum SelvaRpnEvalType type, struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    struct SelvaHierarchy *hierarchy = main_hierarchy;
    struct selva_string **argv = NULL;
    int argc;
    enum rpn_error err;

    finalizer_init(&fin);

    const int ARGV_KEY         = 0;
    const int ARGV_FILTER_EXPR = 1;
    const int ARGV_FILTER_ARGS = 2;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc < 3) {
        selva_send_error_arity(resp);
        return;
    }

    /*
     * Prepare the filter expression.
     */
    struct rpn_ctx *rpn_ctx = NULL;
    struct rpn_expression *filter_expression = NULL;
    const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
    const char *input;

    rpn_ctx = rpn_init(nr_reg);
    if (!rpn_ctx) {
        selva_send_errorf(resp, SELVA_ENOMEM, "filter expression");
        return;
    }

    /*
     * Compile the filter expression.
     */
    input = selva_string_to_str(argv[ARGV_FILTER_EXPR], NULL);
    filter_expression = rpn_compile(input);
    if (!filter_expression) {
        rpn_destroy(rpn_ctx);
        selva_send_errorf(resp, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
        return;
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
}

void SelvaRpn_EvalBoolCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaRpn_Eval(EVAL_TYPE_BOOL, resp, buf, len);
}

void SelvaRpn_EvalDoubleCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaRpn_Eval(EVAL_TYPE_DOUBLE, resp, buf, len);
}

void SelvaRpn_EvalStringCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaRpn_Eval(EVAL_TYPE_STRING, resp, buf, len);
}

void SelvaRpn_EvalSetCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaRpn_Eval(EVAL_TYPE_SET, resp, buf, len);
}

static int RpnEval_OnLoad(void) {
    selva_mk_command(41, "rpn.evalBool", SelvaRpn_EvalBoolCommand);
    selva_mk_command(42, "rpn.evalDouble", SelvaRpn_EvalDoubleCommand);
    selva_mk_command(43, "rpn.evalString", SelvaRpn_EvalStringCommand);
    selva_mk_command(44, "rpn.evalSet", SelvaRpn_EvalSetCommand);

    return 0;
}
SELVA_ONLOAD(RpnEval_OnLoad);
