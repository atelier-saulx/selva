#include "redismodule.h"
#include "errors.h"
#include "rpn.h"
#include "selva.h"
#include "selva_onload.h"
#include "selva_set.h"

enum SelvaRpnEvalType {
    EVAL_TYPE_BOOL,
    EVAL_TYPE_SET,
};

static int SelvaRpn_Eval(enum SelvaRpnEvalType type, RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    enum rpn_error err;

    const int ARGV_KEY         = 1;
    const int ARGV_FILTER_EXPR = 2;
    const int ARGV_FILTER_ARGS = 3;

    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
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

    /* Set reg[0] */
    RedisModuleString *reg0 = argv[ARGV_KEY];
    TO_STR(reg0);
    rpn_set_reg(rpn_ctx, 0, reg0_str, reg0_len, 0);

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

    if (type == EVAL_TYPE_BOOL) {
        int res;

        err = rpn_bool(ctx, rpn_ctx, filter_expression, &res);
        if (err) {
            goto fail;
        }

        RedisModule_ReplyWithLongLong(ctx, res);
    } else if (type == EVAL_TYPE_SET) {
        struct SelvaSet set;
        struct SelvaSetElement *el;

        SelvaSet_Init(&set, SELVA_SET_TYPE_RMSTRING);
        err = rpn_selvaset(ctx, rpn_ctx, filter_expression, &set);
        if (err) {
            goto fail;
        }

        /* TODO size func/macro? */
        RedisModule_ReplyWithArray(ctx, set.size);
        SELVA_SET_RMS_FOREACH(el, &set) {
            RedisModule_ReplyWithString(ctx, el->value_rms);
        }
    } else {
        replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "Invalid type");
        err = 0;
    }

fail:
    if (err) {
        replyWithSelvaErrorf(ctx, SELVA_EGENERAL, "Expression failed: %s", rpn_str_error[err]);
    }

    if (rpn_ctx) {
#if MEM_DEBUG
        memset(filter_expression, 0, sizeof(*filter_expression));
#endif
        rpn_destroy_expression(filter_expression);
        rpn_destroy(rpn_ctx);
    }

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}


int SelvaRpn_EvalBoolCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaRpn_Eval(EVAL_TYPE_BOOL, ctx, argv, argc);
}

int SelvaRpn_EvalSetCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaRpn_Eval(EVAL_TYPE_SET, ctx, argv, argc);
}

static int RpnEval_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.rpn.evalbool", SelvaRpn_EvalBoolCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.rpn.evalset", SelvaRpn_EvalSetCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(RpnEval_OnLoad);
