#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "cdefs.h"
#include "redismodule.h"
#include "errors.h"
#include "selva_object.h"
#include "svector.h"

/* TODO FIX frees in this test mod. */

static struct SelvaObject *root_obj;

static void setup(void)
{
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

#if 0
    RedisModule_FreeString(NULL, key_name);
#endif
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

#if 0
    RedisModule_FreeString(NULL, key_name);
#endif
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

    err = SelvaObject_SetString(root_obj, key_name, orig);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetString(root_obj, key_name, &value);
    pu_assert_equal("no error", err, 0);

    s1 = RedisModule_StringPtrLen(orig, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

#if 0
    RedisModule_FreeString(NULL, key_name);
    RedisModule_FreeString(NULL, orig);
#endif
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

#if 0
    RedisModule_FreeString(NULL, key_name);
#endif
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

    err = SelvaObject_SetString(root_obj, key_name_1, orig);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_SetLongLong(root_obj, key_name_2, 2);
    pu_assert_equal("no error", err, 0);

    err = SelvaObject_DelKey(root_obj, key_name_1);
    pu_assert_equal("no error", err, 0);

    err = SelvaObject_GetString(root_obj, key_name_1, &s);
    pu_assert_equal("no entry", err, SELVA_ENOENT);

    err = SelvaObject_GetLongLong(root_obj, key_name_2, &v);
    pu_assert_equal("no error", err, 0);
    pu_assert_equal("value is as expected", v, 2);

#if 0
    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, orig);
#endif
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

    err = SelvaObject_SetString(root_obj, key_name, orig);
    pu_assert_equal("no error", err, 0);

    err = SelvaObject_GetString(root_obj, key_name, &value);
    pu_assert_equal("no error", err, 0);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    err = SelvaObject_GetString(root_obj, wrong_key_name1, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);

    err = SelvaObject_GetString(root_obj, wrong_key_name2, &value);
    pu_assert_equal("no entry", err, SELVA_EINTYPE);

#if 0
    RedisModule_FreeString(NULL, key_name);
    RedisModule_FreeString(NULL, wrong_key_name1);
    RedisModule_FreeString(NULL, wrong_key_name2);
    RedisModule_FreeString(NULL, orig);
#endif
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

    err = SelvaObject_SetString(root_obj, key_name_1, orig_1);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetString(root_obj, key_name_1, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_1, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    err = SelvaObject_SetString(root_obj, key_name_2, orig_2);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetString(root_obj, key_name_1, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);
    err = SelvaObject_GetString(root_obj, key_name_2, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_2, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

    err = SelvaObject_SetString(root_obj, key_name_3, orig_3);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetString(root_obj, key_name_1, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);
    err = SelvaObject_GetString(root_obj, key_name_2, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);
    err = SelvaObject_GetString(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

#if 0
    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, key_name_3);
    RedisModule_FreeString(NULL, orig_1);
    RedisModule_FreeString(NULL, orig_2);
    RedisModule_FreeString(NULL, orig_3);
#endif
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

    err = SelvaObject_SetString(root_obj, key_name_1, orig_1);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetString(root_obj, key_name_1, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_1, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetString(root_obj, key_name_2, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);
    err = SelvaObject_GetString(root_obj, key_name_3, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);

    err = SelvaObject_SetString(root_obj, key_name_2, orig_2);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetString(root_obj, key_name_2, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_2, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetString(root_obj, key_name_1, &value);
    pu_assert_equal("type error", err, SELVA_ENOENT);
    err = SelvaObject_GetString(root_obj, key_name_3, &value);
    pu_assert_equal("type error", err, SELVA_EINTYPE);

    err = SelvaObject_SetString(root_obj, key_name_3, orig_3);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_GetString(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetString(root_obj, key_name_1, &value);
    pu_assert_equal("type error", err, SELVA_ENOENT);
    err = SelvaObject_GetString(root_obj, key_name_2, &value);
    pu_assert_equal("type error", err, SELVA_ENOENT);

#if 0
    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, key_name_3);
    RedisModule_FreeString(NULL, orig_1);
    RedisModule_FreeString(NULL, orig_2);
    RedisModule_FreeString(NULL, orig_3);
#endif
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

    (void)SelvaObject_SetString(root_obj, key_name_1, orig_1);
    (void)SelvaObject_SetString(root_obj, key_name_2, orig_2);
    (void)SelvaObject_SetString(root_obj, key_name_3, orig_3);
    (void)SelvaObject_SetString(root_obj, key_name_4, orig_4);

    /* Delete */
    err = SelvaObject_DelKey(root_obj, key_name_x);
    pu_assert_equal("no error on del", err, 0);
    err = SelvaObject_DelKey(root_obj, key_name_x);
    pu_assert_equal("error on double del", err, SELVA_ENOENT);

    /* Assert */
    err = SelvaObject_GetString(root_obj, key_name_x, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetString(root_obj, key_name_1, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetString(root_obj, key_name_2, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetString(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetString(root_obj, key_name_4, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_4, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

#if 0
    RedisModule_FreeString(NULL, key_name_x);
    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, key_name_3);
    RedisModule_FreeString(NULL, key_name_4);
    RedisModule_FreeString(NULL, orig_1);
    RedisModule_FreeString(NULL, orig_2);
    RedisModule_FreeString(NULL, orig_3);
    RedisModule_FreeString(NULL, orig_4);
#endif
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

    (void)SelvaObject_SetString(root_obj, key_name_1, orig_1);
    (void)SelvaObject_SetString(root_obj, key_name_2, orig_2);
    (void)SelvaObject_SetString(root_obj, key_name_3, orig_3);
    (void)SelvaObject_SetString(root_obj, key_name_4, orig_4);

    /* Delete 1 */
    err = SelvaObject_DelKey(root_obj, key_name_1);
    pu_assert_equal("no error on del", err, 0);
    err = SelvaObject_DelKey(root_obj, key_name_1);
    pu_assert_equal("error on double del", err, SELVA_ENOENT);

    /* Assert 1 */
    err = SelvaObject_GetString(root_obj, key_name_1, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetString(root_obj, key_name_2, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_2, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetString(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetString(root_obj, key_name_4, &value);
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
    err = SelvaObject_GetString(root_obj, key_name_1, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetString(root_obj, key_name_2, &value);
    pu_assert_equal("no entry", err, SELVA_ENOENT);
    err = SelvaObject_GetString(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetString(root_obj, key_name_4, &value);
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
    err = SelvaObject_GetString(root_obj, key_name_3, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_3, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);
    err = SelvaObject_GetString(root_obj, key_name_4, &value);
    pu_assert_equal("no error", err, 0);
    s1 = RedisModule_StringPtrLen(orig_4, NULL);
    s2 = RedisModule_StringPtrLen(value, NULL);
    pu_assert_str_equal("output value is as expected", s1, s2);

#if 0
    RedisModule_FreeString(NULL, key_name_x);
    RedisModule_FreeString(NULL, key_name_1);
    RedisModule_FreeString(NULL, key_name_2);
    RedisModule_FreeString(NULL, key_name_3);
    RedisModule_FreeString(NULL, key_name_4);
    RedisModule_FreeString(NULL, orig_1);
    RedisModule_FreeString(NULL, orig_2);
    RedisModule_FreeString(NULL, orig_3);
    RedisModule_FreeString(NULL, orig_4);
#endif
    return NULL;
}

static char *string_array(void)
{
    int err;
    RedisModuleString *key_name = RedisModule_CreateString(NULL, "x", 1);
    RedisModuleString *e1 = RedisModule_CreateString(NULL, "1", 1);
    RedisModuleString *e2 = RedisModule_CreateString(NULL, "2", 1);
    RedisModuleString *e3 = RedisModule_CreateString(NULL, "3", 1);
    RedisModuleString *e4 = RedisModule_CreateString(NULL, "4", 1);

    err = SelvaObject_AddArray(root_obj, key_name, SELVA_OBJECT_STRING, e1);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_AddArray(root_obj, key_name, SELVA_OBJECT_STRING, e2);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_AddArray(root_obj, key_name, SELVA_OBJECT_STRING, e3);
    pu_assert_equal("no error", err, 0);
    err = SelvaObject_AddArray(root_obj, key_name, SELVA_OBJECT_STRING, e4);
    pu_assert_equal("no error", err, 0);

    enum SelvaObjectType subtype;
    SVector *arr;

    err = SelvaObject_GetArray(root_obj, key_name, &subtype, &arr);
    pu_assert_equal("no error", err, 0);
    pu_assert_equal("correct subtype", subtype, SELVA_OBJECT_STRING);
    pu_assert("The SVector pointer was set", arr != NULL);

    pu_assert_ptr_equal("e1", SVector_GetIndex(arr, 0), e1);
    pu_assert_ptr_equal("e2", SVector_GetIndex(arr, 1), e2);
    pu_assert_ptr_equal("e3", SVector_GetIndex(arr, 2), e3);
    pu_assert_ptr_equal("e4", SVector_GetIndex(arr, 3), e4);

#if 0
    RedisModule_FreeString(NULL, key_name);
    RedisModule_FreeString(NULL, e1);
    RedisModule_FreeString(NULL, e2);
    RedisModule_FreeString(NULL, e3);
    RedisModule_FreeString(NULL, e4);
#endif
    return NULL;
}

static int freed;

static void ptr_free(void *p __unused) {
    freed = 1;
}

static size_t ptr_len(void *p __unused) {
    return 42;
}

static char *pointer_values(void) {
    int err;
    struct SelvaObjectPointerOpts opts = {
        .ptr_type_id = 1,
        .ptr_free = ptr_free,
        .ptr_len = ptr_len,
    };
    struct data {
        char *text;
        int value;
    } d = {
        .text = "hello",
        .value = 10,
    };

    err = SelvaObject_SetPointerStr(root_obj, "mykey", 5, &d, &opts);
    pu_assert_equal("no error when setting a pointer", err, 0);

    ssize_t len = SelvaObject_LenStr(root_obj, "mykey", 5);
    pu_assert_equal("got correct len", len, 42);

    struct data *p;
    err = SelvaObject_GetPointerStr(root_obj, "mykey", 5, &p);
    pu_assert_equal("no error when getting a pointer", err, 0);
    pu_assert_ptr_equal("got a pointer to the same data", p, &d);

    err = SelvaObject_DelKeyStr(root_obj, "mykey", 5);
    pu_assert_equal("no error when deleting", err, 0);
    pu_assert_equal("ptr_free() was called", freed, 1);

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
    pu_def_test(string_array, PU_RUN);
    pu_def_test(pointer_values, PU_RUN);
}
