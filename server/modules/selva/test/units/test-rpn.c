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
    ctx = rpn_init( nr_reg);
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

    err = rpn_integer(NULL, ctx, expr, &res);

    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("1 + 1", res, 2);

    return NULL;
}

static char * test_add_double(void)
{
    enum rpn_error err;
    double res;
    const char expr_str[] = "#1.5 #0.4 A";
    rpn_token *expr;

    expr = rpn_compile(expr_str, sizeof(expr_str));
    pu_assert("expr is created", expr);

    err = rpn_double(NULL, ctx, expr, &res);

    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("1.5 + 0.4", res, 1.9);

    return NULL;
}

static char * test_rem(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "#8 #42 E";
    rpn_token *expr;

    expr = rpn_compile(expr_str, sizeof(expr_str));
    pu_assert("expr is created", expr);

    err = rpn_integer(NULL, ctx, expr, &res);

    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("42 % 8", res, 2);

    return NULL;
}

static char * test_necessarily(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "@1 P #1 N";
    rpn_token *expr;

    expr = rpn_compile(expr_str, sizeof(expr_str));
    pu_assert("expr is created", expr);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(0) || 1 == false", res, 0);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(1) || 1 == true", res, 1);

    return NULL;
}

static char * test_range(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "#10 @1 #1 i";
    rpn_token *expr;

    expr = rpn_compile(expr_str, sizeof(expr_str));
    pu_assert("expr is created", expr);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("1 <= 0 <= 10 == false", res, 0);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("1 <= 1 <= 10 == true", res, 1);

    err = rpn_set_reg(ctx, 1, "10", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("1 <= 10 <= 10 == true", res, 1);

    err = rpn_set_reg(ctx, 1, "11", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("1 <= 11 <= 10 == false", res, 0);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_init_works, PU_RUN);
    pu_def_test(test_add, PU_RUN);
    pu_def_test(test_add_double, PU_RUN);
    pu_def_test(test_rem, PU_RUN);
    pu_def_test(test_range, PU_RUN);
    pu_def_test(test_necessarily, PU_RUN);
}
