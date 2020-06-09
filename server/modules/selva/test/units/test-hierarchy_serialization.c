#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "hierarchy.h"
#include "cdefs.h"
#include "../redis-rdb.h"

void HierarchyTypeRDBSave(RedisModuleIO *io, void *value);

SelvaModify_Hierarchy *hierarchy;
Selva_NodeId *findRes;
RedisModuleIO *io;

static int SelvaNodeId_Compare(const void *a, const void *b) {

    return strncmp((const char *)a, (const char *)b, SELVA_NODE_ID_SIZE);
}

static void SelvaNodeId_SortRes(size_t len) {
    qsort(findRes, len, sizeof(Selva_NodeId), SelvaNodeId_Compare);
}

static char *SelvaNodeId_GetRes(size_t i) {
    /* cppcheck-suppress threadsafety-threadsafety */
    static char id[sizeof(Selva_NodeId) + 1];

    memcpy(id, findRes[i], sizeof(Selva_NodeId));
    id[sizeof(Selva_NodeId)] = '\0';

    return id;
}

static void setup(void)
{
    hierarchy = SelvaModify_NewHierarchy();
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

static char * test_serialize_one_node(void)
{
    const Selva_NodeId id = "grphnode_a";
    const int res = SelvaModify_SetHierarchy(hierarchy, id, 0, NULL, 0, NULL);
    pu_assert_equal("a node was inserted", res, 0);

    HierarchyTypeRDBSave(io, hierarchy);
    const node_id = io->next->string;
    const nr_children = io->next->next->uint64_val;

    pu_assert("node_id matches", !strncmp(node_id, id, SELVA_NODE_ID_SIZE));
    pu_assert_equal("nr_children is correct", nr_children, 0);

    return NULL;
}

static char * test_serialize_two_nodes(void)
{
    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);

    HierarchyTypeRDBSave(io, hierarchy);
    const node_id = io->next->string;
    const nr_children = io->next->next->uint64_val;
    const child_id = io->next->next->next->string;

    pu_assert("node_id matches", !strncmp(node_id, "grphnode_a", SELVA_NODE_ID_SIZE));
    pu_assert_equal("nr_children is correct", nr_children, 1);
    pu_assert("child_id is correct", !strncmp(child_id, "grphnode_b", SELVA_NODE_ID_SIZE));

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_serialize_one_node, PU_RUN);
    pu_def_test(test_serialize_two_nodes, PU_RUN);
}
