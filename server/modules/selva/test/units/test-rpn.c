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
    const size_t nr_operands = RPN_SMALL_OPERAND_POOL_SIZE + 5;
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

static char * test_mul(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "#2 #2 D";

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_integer(NULL, ctx, expr, &res);

    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("2 * 2", res, 4);

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

static char * test_necessarily_or(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "@1 P @2 N";

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    err = rpn_set_reg(ctx, 2, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(0) || 0 == false", res, 0);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    err = rpn_set_reg(ctx, 2, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(0) || 1 == true", res, 0);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    err = rpn_set_reg(ctx, 2, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(1) || 0 == true", res, 1);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    err = rpn_set_reg(ctx, 2, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(1) || 1 == true", res, 1);

    return NULL;
}

static char * test_necessarily_and(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "@1 P @2 M";

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    err = rpn_set_reg(ctx, 2, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(0) && 0 == false", res, 0);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    err = rpn_set_reg(ctx, 2, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(1) && 0 == false", res, 0);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    err = rpn_set_reg(ctx, 2, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(0) && 1 == false", res, 0);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    err = rpn_set_reg(ctx, 2, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("necess(1) || 1 == true", res, 1);

    return NULL;
}

static char * test_possibly_or(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "@1 Q @2 N";

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    err = rpn_set_reg(ctx, 2, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("possib(0) || 0 == false", res, 0);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    err = rpn_set_reg(ctx, 2, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("possib(0) || 1 == true", res, 1);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    err = rpn_set_reg(ctx, 2, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("possib(1) || 0 == true", res, 1);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    err = rpn_set_reg(ctx, 2, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("possib(1) || 1 == true", res, 1);

    return NULL;
}

static char * test_possibly_and(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "@1 Q @2 M";

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    err = rpn_set_reg(ctx, 2, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("possib(0) && 0 == false", res, 0);

    err = rpn_set_reg(ctx, 1, "0", 1, 0);
    err = rpn_set_reg(ctx, 2, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("possib(0) && 1 == true", res, 0);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    err = rpn_set_reg(ctx, 2, "0", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("possib(1) && 0 == false", res, 1);

    err = rpn_set_reg(ctx, 1, "1", 1, 0);
    err = rpn_set_reg(ctx, 2, "1", 1, 0);
    pu_assert_equal("reg is set", err, RPN_ERR_OK);
    err = rpn_integer(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("possib(1) && 1 == true", res, 1);

    return NULL;
}

static char * test_ternary(void)
{
    enum rpn_error err;
    const char expr_str[] = "$3 $2 @1 T";
    RedisModuleString *res = NULL;

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    for (int i = 0; i <= 1; i++) {
        char a[1] = "0";
        a[0] += i;
        char expected[2] = "C";
        expected[0] -= i;

        err = rpn_set_reg(ctx, 1, a, 1, 0);
        pu_assert_equal("reg is set", err, RPN_ERR_OK);
        err = rpn_set_reg(ctx, 2, "B", 2, 0);
        pu_assert_equal("reg is set", err, RPN_ERR_OK);
        err = rpn_set_reg(ctx, 3, "C", 2, 0);
        pu_assert_equal("reg is set", err, RPN_ERR_OK);
        err = rpn_rms(NULL, ctx, expr, &res);
        pu_assert_equal("No error", err, RPN_ERR_OK);

        TO_STR(res);
        pu_assert_str_equal("Ternary result is valid", res_str, expected);
        RedisModule_FreeString(NULL, res);
    }

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

static char * test_selvaset_empty(void)
{
    enum rpn_error err;
    const char expr_str[] = "\"e\" {} a";
    int res;

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_bool(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("Not found", res, 0);

    return NULL;
}

static char * test_selvaset_empty_2(void)
{
    enum rpn_error err;
    const char expr_str[] = "{\"a\"} {\"b\"} #0 P T";
    int res;
    struct SelvaSet set;

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_bool(NULL, ctx, expr, &res);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("Resolves to false", res, 0);

    SelvaSet_Init(&set, SELVA_SET_TYPE_RMSTRING);
    err = rpn_selvaset(NULL, ctx, expr, &set);
    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("Returned empty set", SelvaSet_Size(&set), 0);

    return NULL;
}

static char * test_selvaset_ill(void)
{
    const char expr_str1[] = "{\"abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces\",\"nice\"";
    const char expr_str2[] = "{\"abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces,\"nice\"}";
    const char expr_str3[] = "{\"abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces\", \"nice\"}";
    const char expr_str4[] = "{abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces\", \"nice\"}";
    const char expr_str5[] = "{abc\",\"def\",\"verylongtextisalsoprettynice\",\"this is another one that is fairly long and with spaces\", \"nice\",}";

    pu_assert_equal("Fails", NULL, rpn_compile(expr_str1));
    pu_assert_equal("Fails", NULL, rpn_compile(expr_str2));
    pu_assert_equal("Fails", NULL, rpn_compile(expr_str3));
    pu_assert_equal("Fails", NULL, rpn_compile(expr_str4));
    pu_assert_equal("Fails", NULL, rpn_compile(expr_str5));

    return NULL;
}

static char * test_cond_jump(void)
{
    static const char expr_str[][30] = {
        "#1 #1 A #1 >1  #1 A .1:X",
        "#1 #1 A #0 >1  #1 A .1:X",
        "#1 #1 A #1 >0  #1 A .1:X",
        "#1 #1 A #1 >3  #1 A .1:X",
        "#1 #1 A #1 >-3 #1 A .1:X",
        "#1 R >1 .1:X",
        "#1 R >1 X .1:X",
        "#1 R >1 X .1:R >2 .2:X",
        "#1 R >1 X .1:R >1 .1:X",
    };
    int expected[] = {
        2,
        3,
        -1, /* Invalid label. */
        -1, /* Invalid label. */
        -1, /* Negative numbers should fail to compile. */
        1,
        1,
        1,
        -1, /* Label reuse. */
    };

    for (int i = 0; i < num_elem(expected); i++) {
        long long res;
        enum rpn_error err;

        printf("Testing i = %d\n", i);
        expr = rpn_compile(expr_str[i]);
        if (expected[i] == -1) {
            pu_assert_null("Expected compile to fail", expr);
            continue;
        } else {
            pu_assert_not_null("Expected to compile", expr);
        }

        err = rpn_integer(NULL, ctx, expr, &res);
        rpn_destroy_expression(expr);
        expr = NULL;

        if (expected[i] == -2) {
            pu_assert_equal("Expected execution to fail", err, RPN_ERR_ILLOPN);
        } else {
            pu_assert_equal("No error", err, RPN_ERR_OK);
        }
        pu_assert_equal(expr_str[i], res, expected[i]);
    }

    return NULL;
}

static char * test_dup(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "#2 R D";

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_integer(NULL, ctx, expr, &res);

    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("2 * 2", res, 4);

    return NULL;
}

static char * test_swap(void)
{
    enum rpn_error err;
    long long res;
    const char expr_str[] = "#4 #2 S C";

    expr = rpn_compile(expr_str);
    pu_assert("expr is created", expr);

    err = rpn_integer(NULL, ctx, expr, &res);

    pu_assert_equal("No error", err, RPN_ERR_OK);
    pu_assert_equal("4 / 2", res, 2);

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
    pu_def_test(test_mul, PU_RUN);
    pu_def_test(test_rem, PU_RUN);
    pu_def_test(test_range, PU_RUN);
    pu_def_test(test_necessarily_or, PU_RUN);
    pu_def_test(test_necessarily_and, PU_RUN);
    pu_def_test(test_possibly_or, PU_RUN);
    pu_def_test(test_possibly_and, PU_RUN);
    pu_def_test(test_ternary, PU_RUN);
    pu_def_test(test_selvaset_inline, PU_RUN);
    pu_def_test(test_selvaset_union, PU_RUN);
    pu_def_test(test_selvaset_empty, PU_RUN);
    pu_def_test(test_selvaset_empty_2, PU_RUN);
    pu_def_test(test_selvaset_ill, PU_RUN);
    pu_def_test(test_cond_jump, PU_RUN);
    pu_def_test(test_dup, PU_RUN);
    pu_def_test(test_swap, PU_RUN);
}
