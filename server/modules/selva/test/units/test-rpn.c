#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "rpn.h"
#include "cdefs.h"

static struct rpn_ctx ctx;
static char **reg;
const size_t nr_reg = 10;

static void setup(void)
{
    reg = RedisModule_Calloc(nr_reg, sizeof(char *));
    rpn_init(&ctx, NULL, reg, nr_reg);
}

static void teardown(void)
{
    RedisModule_Free(reg);
    memset(&ctx, 0, sizeof(ctx));
}

static char * test_init_works(void)
{
    pu_assert_equal("nr_reg is set", ctx.nr_reg, nr_reg);

    return NULL;
}

static char * test_add(void)
{
    int err;
    long long res;
    const char expr[] = "#1 #1 A";

    res = rpn_integer(&ctx, expr, sizeof(expr), &res);

    pu_assert_equal("No error", err, 0);
    pu_assert_equal("1 + 1", res, 2);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_init_works, PU_RUN);
    pu_def_test(test_add, PU_RUN);
}
