#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "rpn.h"
#include "cdefs.h"
#include "selva_set.h"

static struct rpn_expression *expr;
static struct rpn_ctx *ctx;
static char **reg;
static const int nr_reg = 10;

static void setup(void)
{
    reg = RedisModule_Calloc(nr_reg, sizeof(char *));
    ctx = rpn_init( nr_reg);
    expr = NULL;
}

static void teardown(void)
{
    RedisModule_Free(reg);
    rpn_destroy(ctx);
    rpn_destroy_expression(expr);
}

static char * test_init_works(void)
{
    pu_assert_equal("nr_reg is set", ctx->nr_reg, nr_reg);

    return NULL;
}

static char * test_number_valid(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "#1";

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_integer(NULL, ctx, expr, &res);

    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("result is valid", res, 1);

    return NULL;
}

static char * test_number_invalid(void)
{
    const char expr_str[] = "#r";

    expr = rpn_compile(expr_str);
    pu_assert("expr is not created", !expr);

    return NULL;
}

static char * test_operand_pool_overflow(void)
{
    const size_t nr_operands = 5 * RPN_SMALL_OPERAND_POOL_SIZE;
    char expr_str[3 * nr_operands + 1];

    memset(expr_str, '\0', sizeof(expr_str));

    for (size_t i = 0; i < nr_operands; i++) {
        size_t op = i * 3;

        expr_str[op + 0] = '#';
        expr_str[op + 1] = '1';
        expr_str[op + 2] = ' ';
    }

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    return NULL;
}

static char * test_stack_overflow(void)
{
    char expr_str[2 * (RPN_MAX_D * 2) + 3];
    int res;
    enum rpn_error err;

    memset(expr_str, '\0', sizeof(expr_str));

    for (size_t i = 0; i < 2 * RPN_MAX_D; i++) {
        size_t op = i * 2;

        expr_str[op + 0] = 'L';
        expr_str[op + 1] = ' ';
    }
    expr_str[sizeof(expr_str) - 2] = 'L';

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_bool(NULL, ctx, expr, &res);

    pu_assert_equal("should get stack overflow", err, RPN_ERR_BADSTK);

    return NULL;
}

static char * test_add(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "#1 #1 A";

    expr = rpn_compile(expr_str);
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

    expr = rpn_compile(expr_str);
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

    expr = rpn_compile(expr_str);
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

    expr = rpn_compile(expr_str);
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

    expr = rpn_compile(expr_str);
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

static char * test_selvaset_inline(void)
{
    enum rpn_error err;
    const char expr_str[] = "{\"abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces\",\"nice\"}";
    struct SelvaSet set;

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    SelvaSet_Init(&set, SELVA_SET_TYPE_RMSTRING);
    err = rpn_selvaset(NULL, ctx, expr, &set);
    pu_assert_equal("No error", err, RPN_ERR_OK);

    const char *expected[] = {
        "abc",
        "def",
        "verylongtextisalsoprettynice",
        "this is another one that is fairly long and with spaces",
        "nice",
    };

    for (size_t i = 0; i < num_elem(expected); i++) {
        RedisModuleString *rms;

        rms = RedisModule_CreateString(NULL, expected[i], strlen(expected[i]));
        fprintf(stderr, "Has %s\n", expected[i]);
        pu_assert_equal("string is found in the set", 1, SelvaSet_Has(&set, rms));
        RedisModule_FreeString(NULL, rms);
    }
    pu_assert_equal("Set size is correct", num_elem(expected), set.size);

    SelvaSet_Destroy(&set);

    return NULL;
}

static char * test_selvaset_union(void)
{
    enum rpn_error err;
    const char expr_str[] = "{\"a\",\"b\"} {\"c\",\"d\"} z";
    struct SelvaSet set;

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    SelvaSet_Init(&set, SELVA_SET_TYPE_RMSTRING);
    err = rpn_selvaset(NULL, ctx, expr, &set);
    pu_assert_equal("No error", err, RPN_ERR_OK);

    const char *expected[] = { "a", "b", "c", "d" };

    for (size_t i = 0; i < num_elem(expected); i++) {
        RedisModuleString *rms;

        rms = RedisModule_CreateString(NULL, expected[i], strlen(expected[i]));
        pu_assert_equal("string is found in the set", 1, SelvaSet_Has(&set, rms));
        RedisModule_FreeString(NULL, rms);
    }
    pu_assert_equal("Set size is correct", num_elem(expected), set.size);

    SelvaSet_Destroy(&set);

    return NULL;
}

static char * test_selvaset_ill(void)
{
    enum rpn_error err;
    const char expr_str1[] = "{\"abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces\",\"nice\"";
    const char expr_str2[] = "{\"abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces,\"nice\"}";
    const char expr_str3[] = "{\"abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces\", \"nice\"}";
    const char expr_str4[] = "{abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces\", \"nice\"}";
    const char expr_str5[] = "{abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces\", \"nice\",}";
    struct SelvaSet set;

    pu_assert_equal("Fails", NULL,  rpn_compile(expr_str1));
    pu_assert_equal("Fails", NULL,  rpn_compile(expr_str2));
    pu_assert_equal("Fails", NULL,  rpn_compile(expr_str3));
    pu_assert_equal("Fails", NULL,  rpn_compile(expr_str4));
    pu_assert_equal("Fails", NULL,  rpn_compile(expr_str5));

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_init_works, PU_RUN);
    pu_def_test(test_number_valid, PU_RUN);
    pu_def_test(test_number_invalid, PU_RUN);
    pu_def_test(test_operand_pool_overflow, PU_RUN);
    pu_def_test(test_stack_overflow, PU_RUN);
    pu_def_test(test_add, PU_RUN);
    pu_def_test(test_add_double, PU_RUN);
    pu_def_test(test_rem, PU_RUN);
    pu_def_test(test_range, PU_RUN);
    pu_def_test(test_necessarily, PU_RUN);
    pu_def_test(test_selvaset_inline, PU_RUN);
    pu_def_test(test_selvaset_union, PU_RUN);
    pu_def_test(test_selvaset_ill, PU_RUN);
}
