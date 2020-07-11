#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "rpn.h"
#include "cdefs.h"

static struct rpn_ctx *ctx;
static char **reg;
const int nr_reg = 10;

static void setup(void)
{
    reg = RedisModule_Calloc(nr_reg, sizeof(char *));
    ctx = rpn_init(NULL, nr_reg);
}

static void teardown(void)
{
    RedisModule_Free(reg);
    rpn_destroy(ctx);
}

static char * test_init_works(void)
{
    pu_assert_equal("nr_reg is set", ctx->nr_reg, nr_reg);

    return NULL;
}

static char * test_add(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "#1 #1 A";
    rpn_token *expr;

    expr = rpn_compile(expr_str, sizeof(expr_str));
    pu_assert("expr is created", expr);

    err = rpn_integer(ctx, expr, &res);

    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("1 + 1", res, 2);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_init_works, PU_RUN);
    pu_def_test(test_add, PU_RUN);
}
