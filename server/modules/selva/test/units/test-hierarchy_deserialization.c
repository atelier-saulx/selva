#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "hierarchy.h"
#include "cdefs.h"
#include "../redis-rdb.h"
#include "../hierarchy-utils.h"

void *HierarchyTypeRDBLoad(RedisModuleIO *io, int encver);

extern void (*RedisModule_SaveUnsigned)(RedisModuleIO *io, uint64_t value);
extern uint64_t (*RedisModule_LoadUnsigned)(RedisModuleIO *io);
extern void (*RedisModule_SaveSigned)(RedisModuleIO *io, int64_t value);
extern int64_t (*RedisModule_LoadSigned)(RedisModuleIO *io);
extern void (*RedisModule_SaveStringBuffer)(RedisModuleIO *io, const char *str, size_t len);
extern char *(*RedisModule_LoadStringBuffer)(RedisModuleIO *io, size_t *lenptr);
extern void (*RedisModule_SaveDouble)(RedisModuleIO *io, double value);
extern double (*RedisModule_LoadDouble)(RedisModuleIO *io);

static void setup(void)
{
    io = RedisRdb_NewIo();
}

static void teardown(void)
{
    SelvaModify_DestroyHierarchy(hierarchy);
    hierarchy = NULL;

    free(findRes);
    findRes = NULL;

    RedisRdb_FreeIo(io);
    io = NULL;
}

static char * test_deserialize_one_node(void)
{
    int n;

    RedisModule_SaveStringBuffer(io, "grphnode_a", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0);
    RedisModule_SaveStringBuffer(io, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE);

    hierarchy = HierarchyTypeRDBLoad(io, 0);
    pu_assert("a hierarchy was returned", hierarchy);

    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);

    pu_assert_equal("returned the right number of heads", n, 1);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("a is a head", SelvaNodeId_GetRes(0), "grphnode_a");

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_deserialize_one_node, PU_RUN);
}
