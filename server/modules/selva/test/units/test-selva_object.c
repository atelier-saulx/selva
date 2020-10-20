#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "cdefs.h"
#include "redismodule.h"
#include "sds.h"
#include "selva_object.h"

static struct SelvaObject *root_obj;

extern struct SelvaObject *(*SelvaObject_New)(void);
extern void (*SelvaObject_Destroy)(struct SelvaObject *obj);

static void setup(void)
{
    if (!SelvaObject_New) {
        abort();
    }

    root_obj = SelvaObject_New();
    if (!root_obj) {
        abort();
    }
}

static void teardown(void)
{
    SelvaObject_Destroy(root_obj);
}

static char * setget_double(void)
{
    RedisModuleString *key_name = RedisModule_CreateString(NULL, "x", 1);
    double v = 0.0;

    //SelvaObject_SetDouble(root_obj, key_name, 1.0);
    //SelvaObject_GetDouble(root_obj, key_name, &v);

    pu_assert_equal("output value is as expected", v, 1.0);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(setget_double, PU_RUN);
}
