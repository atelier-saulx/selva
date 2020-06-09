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

static void copyId2Buf(char buf[SELVA_NODE_ID_SIZE + 1], Selva_NodeId id) {
    memcpy(buf, id, SELVA_NODE_ID_SIZE);
    buf[SELVA_NODE_ID_SIZE] = '\0';
}

static char str_buf[120];
static char * assert_node(size_t index, Selva_NodeId expectedId, size_t nrChildren, Selva_NodeId *children) {
    RedisModuleIO *ioNode = io;
    char expected[SELVA_NODE_ID_SIZE + 1];
    char actual[SELVA_NODE_ID_SIZE + 1];

    pu_assert("children is set if nrChildren is non-zero", (nrChildren > 0 && !!children) || (nrChildren == 0 && !children));

    for (size_t i = 0; i < index + 1; i++) {
        ioNode = ioNode->next;
        pu_assert("ioNode is non-null", !!ioNode);
    }

    snprintf(str_buf, sizeof(str_buf), "the node id is stored as a string at [%zd]", index);
    pu_assert_equal(str_buf, ioNode->type, REDIS_MODULE_IO_TYPE_STRING);

    copyId2Buf(actual, ioNode->string);
    copyId2Buf(expected, expectedId);
    pu_assert_str_equal("the expected node id is found", actual, expected);

    ioNode = ioNode->next;
    pu_assert("the chain continues", ioNode);
    pu_assert_equal("the correct number of children is set", ioNode->uint64_val, nrChildren);

    for (size_t i = 0; i < nrChildren; i++) {
        ioNode = ioNode->next;

        pu_assert("ioNode representing a child is non-null", !!ioNode);
        pu_assert_equal("the child id is stored as a string", ioNode->type, REDIS_MODULE_IO_TYPE_STRING);

        copyId2Buf(actual, ioNode->string);
        copyId2Buf(expected, children[i]);
        pu_assert_str_equal("the expected child id is found", actual, expected);
    }

    return NULL;
}

static char * test_serialize_one_node(void)
{
    const Selva_NodeId id = "grphnode_a";
    const int res = SelvaModify_SetHierarchy(hierarchy, id, 0, NULL, 0, NULL);

    pu_assert_equal("a node was inserted", res, 0);

    HierarchyTypeRDBSave(io, hierarchy);

    pu_assert_equal("the expected next item pointers are set", RedisRdb_CountIo(io), 2);

    return assert_node(0, id, 0, NULL);
}

static char * test_serialize_two_nodes(void)
{
    char *res;
    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);

    HierarchyTypeRDBSave(io, hierarchy);

    pu_assert_equal("the expected next item pointers are set", RedisRdb_CountIo(io), 5);

    res = assert_node(0, "grphnode_a", 1, ((Selva_NodeId []){ "grphnode_b" }));
    if (res) {
        return res;
    }

    res = assert_node(3, "grphnode_b", 0, NULL);
    if (res) {
        return res;
    }

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_serialize_one_node, PU_RUN);
    pu_def_test(test_serialize_two_nodes, PU_RUN);
}
