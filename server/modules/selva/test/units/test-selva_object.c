#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "cdefs.h"
#include "redismodule.h"
#include "sds.h"
#include "errors.h"
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
    if (root_obj) {
        SelvaObject_Destroy(root_obj);
    }
    root_obj = NULL;
}

static char * setget_double(void)
{
    RedisModuleString *key_name = RedisModule_CreateString(NULL, "x", 1);
    double v = 0.0;
    int err;

    err = SelvaObject_SetDouble(root_obj, key_name, 1.0);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetDouble(root_obj, key_name, &v);
    pu_assert_equal("no error", err, 0);

    pu_assert_equal("output value is as expected", v, 1.0);

    RedisModule_FreeString(NULL, key_name);
    return NULL;
}

static char * setget_longlong(void)
{
    RedisModuleString *key_name = RedisModule_CreateString(NULL, "x", 1);
    long long v = 0;
    int err;

    err = SelvaObject_SetLongLong(root_obj, key_name, 1);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetLongLong(root_obj, key_name, &v);
    pu_assert_equal("no error", err, 0);

    pu_assert_equal("output value is as expected", v, 1);

    RedisModule_FreeString(NULL, key_name);
    return NULL;
}

static char * setget_string(void)
{
    RedisModuleString *key_name = RedisModule_CreateString(NULL, "x", 1);
    RedisModuleString *orig = RedisModule_CreateString(NULL, "hello", 5);
    RedisModuleString *value;
    const char *s1;
    const char *s2;
    int err;

    err = SelvaObject_SetStr(root_obj, key_name, orig);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetStr(root_obj, key_name, &value);
    pu_assert_equal("no error", err, 0);

    s1 = RedisModule_StringPtrLen(orig, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    RedisModule_FreeString(NULL, key_name);
    RedisModule_FreeString(NULL, orig);
    return NULL;
}

static char * delete_key_1(void)
{
    /*
     * { x: 1 } => null
     */

    RedisModuleString *key_name = RedisModule_CreateString(NULL, "x", 1);
    long long v = 0;
    int err;

    err = SelvaObject_SetLongLong(root_obj, key_name, 1);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_DelKey(root_obj, key_name);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetLongLong(root_obj, key_name, &v);
    pu_assert_equal("no entry", err, SELVA_ENOENT);

    RedisModule_FreeString(NULL, key_name);
    return NULL;
}

static char * delete_key_2(void)
{
    /*
     * { x: "hello", y: 2 } => { y: 2 }
     */

    RedisModuleString *key_name_1 = RedisModule_CreateString(NULL, "x", 1);
    RedisModuleString *key_name_2 = RedisModule_CreateString(NULL, "y", 1);
    RedisModuleString *orig = RedisModule_CreateString(NULL, "hello", 5);
    RedisModuleString *s;
    long long v = 0;
    int err;

    err = SelvaObject_SetStr(root_obj, key_name_1, orig);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_SetLongLong(root_obj, key_name_2, 2);
    pu_assert_equal("no error", err, 0);

    err = SelvaObject_DelKey(root_obj, key_name_1);
    pu_assert_equal("no error", err, 0);

    err = SelvaObject_GetStr(root_obj, key_name_1, &s);
    pu_assert_equal("no entry", err, SELVA_ENOENT);

    err = SelvaObject_GetLongLong(root_obj, key_name_2, &v);
    pu_assert_equal("no error", err, 0);
    pu_assert_equal("value is as expected", v, 2);

    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, orig);
    return NULL;
}

static char * nested_object(void)
{
    RedisModuleString *key_name = RedisModule_CreateString(NULL, "a.b", 3);
    RedisModuleString *wrong_key_name1 = RedisModule_CreateString(NULL, "a.b.c", 5);
    RedisModuleString *wrong_key_name2 = RedisModule_CreateString(NULL, "a", 1);
    RedisModuleString *orig = RedisModule_CreateString(NULL, "hello", 5);
    RedisModuleString *value;
    const char *s1 = RedisModule_StringPtrLen(orig, NULL);
    const char *s2;
    int err;

    err = SelvaObject_SetStr(root_obj, key_name, orig);
    pu_assert_equal("no error", err, 0);

    err = SelvaObject_GetStr(root_obj, key_name, &value);
    pu_assert_equal("no error", err, 0);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    err = SelvaObject_GetStr(root_obj, wrong_key_name1, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);

    err = SelvaObject_GetStr(root_obj, wrong_key_name2, &value);
    pu_assert_equal("no entry", err, SELVA_EINTYPE);

    RedisModule_FreeString(NULL, key_name);
    RedisModule_FreeString(NULL, wrong_key_name1);
    RedisModule_FreeString(NULL, wrong_key_name2);
    RedisModule_FreeString(NULL, orig);
    return NULL;
}

static char * replace_string_with_object(void)
{
    RedisModuleString *key_name_1 = RedisModule_CreateString(NULL, "a", 3);
    RedisModuleString *key_name_2 = RedisModule_CreateString(NULL, "a.b", 3);
    RedisModuleString *key_name_3 = RedisModule_CreateString(NULL, "a.b.c", 5);
    RedisModuleString *orig_1 = RedisModule_CreateString(NULL, "hello", 5);
    RedisModuleString *orig_2 = RedisModule_CreateString(NULL, "hallo", 5);
    RedisModuleString *orig_3 = RedisModule_CreateString(NULL, "ciao", 4);
    RedisModuleString *value;
    const char *s1;
    const char *s2;
    int err;

    err = SelvaObject_SetStr(root_obj, key_name_1, orig_1);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetStr(root_obj, key_name_1, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_1, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    err = SelvaObject_SetStr(root_obj, key_name_2, orig_2);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetStr(root_obj, key_name_1, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);
    err = SelvaObject_GetStr(root_obj, key_name_2, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_2, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    err = SelvaObject_SetStr(root_obj, key_name_3, orig_3);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetStr(root_obj, key_name_1, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);
    err = SelvaObject_GetStr(root_obj, key_name_2, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);
    err = SelvaObject_GetStr(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, key_name_3);
    RedisModule_FreeString(NULL, orig_1);
    RedisModule_FreeString(NULL, orig_2);
    RedisModule_FreeString(NULL, orig_3);
    return NULL;
}

static char * replace_object_with_string(void)
{
    RedisModuleString *key_name_1 = RedisModule_CreateString(NULL, "a.b.c", 5);
    RedisModuleString *key_name_2 = RedisModule_CreateString(NULL, "a.b", 3);
    RedisModuleString *key_name_3 = RedisModule_CreateString(NULL, "a", 3);
    RedisModuleString *orig_1 = RedisModule_CreateString(NULL, "ciao", 4);
    RedisModuleString *orig_2 = RedisModule_CreateString(NULL, "hallo", 5);
    RedisModuleString *orig_3 = RedisModule_CreateString(NULL, "hello", 5);
    RedisModuleString *value;
    const char *s1;
    const char *s2;
    int err;

    err = SelvaObject_SetStr(root_obj, key_name_1, orig_1);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetStr(root_obj, key_name_1, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_1, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetStr(root_obj, key_name_2, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);
    err = SelvaObject_GetStr(root_obj, key_name_3, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);

    err = SelvaObject_SetStr(root_obj, key_name_2, orig_2);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetStr(root_obj, key_name_2, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_2, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetStr(root_obj, key_name_1, &value);
    pu_assert_equal("type error", err, SELVA_ENOENT);
    err = SelvaObject_GetStr(root_obj, key_name_3, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);

    err = SelvaObject_SetStr(root_obj, key_name_3, orig_3);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetStr(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetStr(root_obj, key_name_1, &value);
    pu_assert_equal("type error", err, SELVA_ENOENT);
    err = SelvaObject_GetStr(root_obj, key_name_2, &value);
    pu_assert_equal("type error", err, SELVA_ENOENT);

    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, key_name_3);
    RedisModule_FreeString(NULL, orig_1);
    RedisModule_FreeString(NULL, orig_2);
    RedisModule_FreeString(NULL, orig_3);
    return NULL;
}

static char * delete_object(void)
{
    /*
     * Create:
     * {
     *   x: {
     *     a: "a",
     *     b: "b",
     *   },
     *   y: {
     *     c: "c",
     *     d: "d",
     *   }
     * }
     *
     * Delete:
     * x
     *
     * Expected result 1:
     * {
     *   y: {
     *     c: "c",
     *     d: "d",
     *   }
     * }
     */

    RedisModuleString *key_name_x = RedisModule_CreateString(NULL, "x", 1);
    RedisModuleString *key_name_1 = RedisModule_CreateString(NULL, "x.a", 3);
    RedisModuleString *key_name_2 = RedisModule_CreateString(NULL, "x.b", 3);
    RedisModuleString *key_name_3 = RedisModule_CreateString(NULL, "y.c", 3);
    RedisModuleString *key_name_4 = RedisModule_CreateString(NULL, "y.d", 3);
    RedisModuleString *orig_1 = RedisModule_CreateString(NULL, "a", 1);
    RedisModuleString *orig_2 = RedisModule_CreateString(NULL, "b", 1);
    RedisModuleString *orig_3 = RedisModule_CreateString(NULL, "c", 1);
    RedisModuleString *orig_4 = RedisModule_CreateString(NULL, "d", 1);
    RedisModuleString *value;
    const char *s1;
    const char *s2;
    int err;

    (void)SelvaObject_SetStr(root_obj, key_name_1, orig_1);
    (void)SelvaObject_SetStr(root_obj, key_name_2, orig_2);
    (void)SelvaObject_SetStr(root_obj, key_name_3, orig_3);
    (void)SelvaObject_SetStr(root_obj, key_name_4, orig_4);

    /* Delete */
    err = SelvaObject_DelKey(root_obj, key_name_x);
    pu_assert_equal("no error on del", err, 0);
    err = SelvaObject_DelKey(root_obj, key_name_x);
    pu_assert_equal("error on double del", err, SELVA_ENOENT);

    /* Assert */
    err = SelvaObject_GetStr(root_obj, key_name_x, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetStr(root_obj, key_name_1, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetStr(root_obj, key_name_2, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetStr(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetStr(root_obj, key_name_4, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_4, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    RedisModule_FreeString(NULL, key_name_x);
    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, key_name_3);
    RedisModule_FreeString(NULL, key_name_4);
    RedisModule_FreeString(NULL, orig_1);
    RedisModule_FreeString(NULL, orig_2);
    RedisModule_FreeString(NULL, orig_3);
    RedisModule_FreeString(NULL, orig_4);
    return NULL;
}

static char * delete_nested_key(void)
{
    /*
     * Create:
     * {
     *   x: {
     *     a: "a",
     *     b: "b",
     *   },
     *   y: {
     *     c: "c",
     *     d: "d",
     *   }
     * }
     *
     * Delete:
     * x.a
     *
     * Expected result 1:
     * {
     *   x: {
     *     b: "b",
     *   },
     *   y: {
     *     c: "c",
     *     d: "d",
     *   }
     * }
     *
     * Delete:
     * x.b
     *
     * Expected result 2:
     * {
     *   x: null,
     *   y: {
     *     c: "c",
     *     d: "d",
     *   }
     * }
     *
     * Delete:
     * x
     *
     * Expected result 3:
     * {
     *   y: {
     *     c: "c",
     *     d: "d",
     *   }
     * }
     */

    RedisModuleString *key_name_x = RedisModule_CreateString(NULL, "x", 1);
    RedisModuleString *key_name_1 = RedisModule_CreateString(NULL, "x.a", 3);
    RedisModuleString *key_name_2 = RedisModule_CreateString(NULL, "x.b", 3);
    RedisModuleString *key_name_3 = RedisModule_CreateString(NULL, "y.c", 3);
    RedisModuleString *key_name_4 = RedisModule_CreateString(NULL, "y.d", 3);
    RedisModuleString *orig_1 = RedisModule_CreateString(NULL, "a", 1);
    RedisModuleString *orig_2 = RedisModule_CreateString(NULL, "b", 1);
    RedisModuleString *orig_3 = RedisModule_CreateString(NULL, "c", 1);
    RedisModuleString *orig_4 = RedisModule_CreateString(NULL, "d", 1);
    RedisModuleString *value;
    const char *s1;
    const char *s2;
    int err;

    (void)SelvaObject_SetStr(root_obj, key_name_1, orig_1);
    (void)SelvaObject_SetStr(root_obj, key_name_2, orig_2);
    (void)SelvaObject_SetStr(root_obj, key_name_3, orig_3);
    (void)SelvaObject_SetStr(root_obj, key_name_4, orig_4);

    /* Delete 1 */
    err = SelvaObject_DelKey(root_obj, key_name_1);
    pu_assert_equal("no error on del", err, 0);
    err = SelvaObject_DelKey(root_obj, key_name_1);
    pu_assert_equal("error on double del", err, SELVA_ENOENT);

    /* Assert 1 */
    err = SelvaObject_GetStr(root_obj, key_name_1, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetStr(root_obj, key_name_2, &value);
    s1 = RedisModule_StringPtrLen(orig_2, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetStr(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetStr(root_obj, key_name_4, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_4, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    /* Delete 2 */
    err = SelvaObject_DelKey(root_obj, key_name_2);
    pu_assert_equal("no error on del", err, 0);
    err = SelvaObject_DelKey(root_obj, key_name_2);
    pu_assert_equal("error on double del", err, SELVA_ENOENT);

    /* Assert 2 */
    err = SelvaObject_GetStr(root_obj, key_name_1, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetStr(root_obj, key_name_2, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetStr(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetStr(root_obj, key_name_4, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_4, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    /* Delete 2 */
    err = SelvaObject_DelKey(root_obj, key_name_x);
    pu_assert_equal("no error on del", err, 0);
    err = SelvaObject_DelKey(root_obj, key_name_x);
    pu_assert_equal("error on double del", err, SELVA_ENOENT);

    /* Assert 3 */
    err = SelvaObject_GetStr(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetStr(root_obj, key_name_4, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_4, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    RedisModule_FreeString(NULL, key_name_x);
    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, key_name_3);
    RedisModule_FreeString(NULL, key_name_4);
    RedisModule_FreeString(NULL, orig_1);
    RedisModule_FreeString(NULL, orig_2);
    RedisModule_FreeString(NULL, orig_3);
    RedisModule_FreeString(NULL, orig_4);
    return NULL;
}

void all_tests(void)
{
    pu_def_test(setget_double, PU_RUN);
    pu_def_test(setget_longlong, PU_RUN);
    pu_def_test(setget_string, PU_RUN);
    pu_def_test(delete_key_1, PU_RUN);
    pu_def_test(delete_key_2, PU_RUN);
    pu_def_test(nested_object, PU_RUN);
    pu_def_test(replace_string_with_object, PU_RUN);
    pu_def_test(replace_object_with_string, PU_RUN);
    pu_def_test(delete_object, PU_RUN);
    pu_def_test(delete_nested_key, PU_RUN);
}
