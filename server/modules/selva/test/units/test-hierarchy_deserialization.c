#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "hierarchy.h"
#include "cdefs.h"
#include "redismodule.h"
#include "../redis-rdb.h"
#include "../hierarchy-utils.h"

void *HierarchyTypeRDBLoad(RedisModuleIO *io, int encver);

extern int redis_mock_ctx_flags;
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
    redis_mock_ctx_flags = REDISMODULE_CTX_FLAGS_LOADING;
    io = RedisRdb_NewIo();
}

static void teardown(void)
{
    if (hierarchy) {
        SelvaModify_DestroyHierarchy(hierarchy);
    }
    hierarchy = NULL;

    free(findRes);
    findRes = NULL;

    RedisRdb_FreeIo(io);
    io = NULL;
}

static char * test_deserialize_one_node(void)
{
    int n;

    /* A | 0 | 0 */
    RedisModule_SaveStringBuffer(io, "grphnode_a", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 0); // Number of children
    /* EOF */
    RedisModule_SaveStringBuffer(io, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE);

    hierarchy = HierarchyTypeRDBLoad(io, HIERARCHY_ENCODING_VERSION);
    pu_assert("a hierarchy was returned", hierarchy);

    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 2);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("a is a head", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("root is a head", SelvaNodeId_GetRes(1), "root");

    return NULL;
}

static char * test_deserialize_two_nodes(void)
{
    int n;
    int nr_ancestors;
    int nr_descendants;

    /*
     *  a -> b
     */

    /* A | 1 | B */
    RedisModule_SaveStringBuffer(io, "grphnode_a", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 1);
    RedisModule_SaveStringBuffer(io, "grphnode_b", SELVA_NODE_ID_SIZE);
    /* B | 0 */
    RedisModule_SaveStringBuffer(io, "grphnode_b", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 0);
    /* EOF */
    RedisModule_SaveStringBuffer(io, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE);

    hierarchy = HierarchyTypeRDBLoad(io, HIERARCHY_ENCODING_VERSION);
    pu_assert("a hierarchy was returned", hierarchy);

    /* Assert heads */
    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 2);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("a is a head", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("root is a head", SelvaNodeId_GetRes(1), "root");
    free(findRes);
    findRes = NULL;

    /* Assert ancestors */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("b has a as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    free(findRes);
    findRes = NULL;

    /* Assert descendants */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has b as a descendant", SelvaNodeId_GetRes(0), "grphnode_b");
    free(findRes);
    findRes = NULL;

    return NULL;
}

static char * test_deserialize_cyclic_hierarchy(void)
{
    int n;
    int nr_ancestors;
    int nr_descendants;

    /*
     *  e --> a --> c
     *    |     /
     *    \-> b --> d
     */

    /* E | 2 | A | B */
    RedisModule_SaveStringBuffer(io, "grphnode_e", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 2);
    RedisModule_SaveStringBuffer(io, "grphnode_a", SELVA_NODE_ID_SIZE);
    RedisModule_SaveStringBuffer(io, "grphnode_b", SELVA_NODE_ID_SIZE);
    /* A | 1 | C */
    RedisModule_SaveStringBuffer(io, "grphnode_a", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 1);
    RedisModule_SaveStringBuffer(io, "grphnode_c", SELVA_NODE_ID_SIZE);
    /* C | 0 */
    RedisModule_SaveStringBuffer(io, "grphnode_c", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 0);
    /* B | 2 | C | D */
    RedisModule_SaveStringBuffer(io, "grphnode_b", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 2);
    RedisModule_SaveStringBuffer(io, "grphnode_c", SELVA_NODE_ID_SIZE);
    RedisModule_SaveStringBuffer(io, "grphnode_d", SELVA_NODE_ID_SIZE);
    /* D | 0 */
    RedisModule_SaveStringBuffer(io, "grphnode_d", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 0);
    /* EOF */
    RedisModule_SaveStringBuffer(io, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE);

    hierarchy = HierarchyTypeRDBLoad(io, HIERARCHY_ENCODING_VERSION);
    pu_assert("a hierarchy was returned", hierarchy);


    /* Assert heads */
    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(n);
    pu_assert_str_equal("e is a head", SelvaNodeId_GetRes(0), "grphnode_e");
    pu_assert_str_equal("root is a head", SelvaNodeId_GetRes(1), "root");
    free(findRes);
    findRes = NULL;

    /* Assert ancestors */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 3);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has a as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    pu_assert_str_equal("c has e as an ancestor", SelvaNodeId_GetRes(2), "grphnode_e");
    free(findRes);
    findRes = NULL;

    /* Assert descendants */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    free(findRes);
    findRes = NULL;

    return NULL;
}

static char * test_deserialize_multi_head(void)
{
    int n;
    int nr_ancestors;
    int nr_descendants;

    /*
     *  a --> c
     *    /
     *  b --> d
     */

    /* A | 1 | C */
    RedisModule_SaveStringBuffer(io, "grphnode_a", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 1);
    RedisModule_SaveStringBuffer(io, "grphnode_c", SELVA_NODE_ID_SIZE);
    /* C | 0 */
    RedisModule_SaveStringBuffer(io, "grphnode_c", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 0);
    /* B | 2 | C | D */
    RedisModule_SaveStringBuffer(io, "grphnode_b", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 2);
    RedisModule_SaveStringBuffer(io, "grphnode_c", SELVA_NODE_ID_SIZE);
    RedisModule_SaveStringBuffer(io, "grphnode_d", SELVA_NODE_ID_SIZE);
    /* D | 0 */
    RedisModule_SaveStringBuffer(io, "grphnode_d", SELVA_NODE_ID_SIZE);
    RedisModule_SaveUnsigned(io, 0); // meta
    RedisModule_SaveUnsigned(io, 0);
    /* EOF */
    RedisModule_SaveStringBuffer(io, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE);

    hierarchy = HierarchyTypeRDBLoad(io, HIERARCHY_ENCODING_VERSION);
    pu_assert("a hierarchy was returned", hierarchy);


    /* Assert heads */
    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 3);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(n);
    pu_assert_str_equal("a is a head", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("b is a head", SelvaNodeId_GetRes(1), "grphnode_b");
    pu_assert_str_equal("root is a head", SelvaNodeId_GetRes(2), "root");
    free(findRes);
    findRes = NULL;

    /* Assert ancestors */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has a as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    free(findRes);
    findRes = NULL;

    /* Assert descendants */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("b has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(1), "grphnode_d");
    free(findRes);
    findRes = NULL;

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_deserialize_one_node, PU_RUN);
    pu_def_test(test_deserialize_two_nodes, PU_RUN);
    pu_def_test(test_deserialize_cyclic_hierarchy, PU_RUN);
    pu_def_test(test_deserialize_multi_head, PU_RUN);
}
