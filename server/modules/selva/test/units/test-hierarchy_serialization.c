#include <punit.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "hierarchy.h"
#include "cdefs.h"
#include "../redis-rdb.h"
#include "../hierarchy-utils.h"

void HierarchyTypeRDBSave(RedisModuleIO *io, void *value);

static char str_buf[120];
static char * assert_node(size_t index, const Selva_NodeId expectedId, size_t nrChildren, const Selva_NodeId *children) {
    RedisModuleIO *ioNode = io;
    char expected[SELVA_NODE_ID_SIZE + 1];
    char actual[SELVA_NODE_ID_SIZE + 1];
    const size_t msg_size = sizeof(str_buf) - SELVA_NODE_ID_SIZE + 2;
    char *msg_buf = str_buf + SELVA_NODE_ID_SIZE + 2;

    /* Add the log prefix to the buffer */
    snprintf(str_buf, sizeof(str_buf), "%.*s: ", (int)SELVA_NODE_ID_SIZE, expectedId);
    for (size_t i = 0; i < SELVA_NODE_ID_SIZE; i++) {
        if (!isalnum(str_buf[i])) {
            str_buf[i] = '_';
        }
    }

    snprintf(msg_buf, msg_size, "children is set if nrChildren is non-zero");
    pu_assert(str_buf, (nrChildren > 0 && !!children) || (nrChildren == 0 && !children));

    for (size_t i = 0; i < index + 1; i++) {
        ioNode = ioNode->next;
        pu_assert("ioNode is non-null", !!ioNode);
    }

    snprintf(msg_buf, msg_size, "the node id is stored as a string at [%zu]", index);
    pu_assert_equal(str_buf, ioNode->type, REDIS_MODULE_IO_TYPE_STRING);

    SelvaNodeId_copy2buf(actual, ioNode->string);
    SelvaNodeId_copy2buf(expected, expectedId);

    snprintf(msg_buf, msg_size, "the expected node id is found");
    pu_assert_str_equal(str_buf, actual, expected);

    if (!memcmp(ioNode->string, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE)) {
        return NULL;
    }

    ioNode = ioNode->next;

    snprintf(msg_buf, msg_size, "the chain continues");
    pu_assert(str_buf, ioNode);

    snprintf(msg_buf, msg_size, "the correct number of children is set");
    pu_assert_equal(str_buf, ioNode->uint64_val, nrChildren);

    for (size_t i = 0; i < nrChildren; i++) {
        ioNode = ioNode->next;

        snprintf(msg_buf, msg_size, "ioNode representing a child is non-null");
        pu_assert(str_buf, !!ioNode);

        snprintf(msg_buf, msg_size, "the child id is stored as a string");
        pu_assert_equal(str_buf, ioNode->type, REDIS_MODULE_IO_TYPE_STRING);

        SelvaNodeId_copy2buf(actual, ioNode->string);
        SelvaNodeId_copy2buf(expected, children[i]);

        snprintf(msg_buf, msg_size, "the expected child id is found");
        pu_assert_str_equal(str_buf, actual, expected);
    }

    return NULL;
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
    char *res;
    const Selva_NodeId id = "grphnode_a";
    SelvaModify_SetHierarchy(hierarchy, id, 0, NULL, 0, NULL);

    HierarchyTypeRDBSave(io, hierarchy);

    pu_assert_equal("the expected next item pointers are set", RedisRdb_CountIo(io), 5);

    res = assert_node(0, id, 0, NULL);
    if (res) {
        return res;
    }

    res = assert_node(4, HIERARCHY_RDB_EOF, 0, NULL);
    if (res) {
        return res;
    }

    return NULL;
}

static char * test_serialize_two_nodes(void)
{
    char *res;
    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);

    HierarchyTypeRDBSave(io, hierarchy);

    pu_assert_equal("the expected next item pointers are set", RedisRdb_CountIo(io), 8);

    res = assert_node(0, "grphnode_a", 1, ((Selva_NodeId []){ "grphnode_b" }));
    if (res) {
        return res;
    }

    res = assert_node(3, "grphnode_b", 0, NULL);
    if (res) {
        return res;
    }

    res = assert_node(7, HIERARCHY_RDB_EOF, 0, NULL);
    if (res) {
        return res;
    }

    return NULL;
}

static char * test_serialize_acyclic_1(void)
{
    /*
     *  a -> b --> c
     *   \-->--/
     */
    char *res;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_AddHierarchy(hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);

    HierarchyTypeRDBSave(io, hierarchy);

    pu_assert_equal("the expected next item pointers are set", RedisRdb_CountIo(io), 12);

    res = assert_node(0, "grphnode_a", 2, ((Selva_NodeId []){ "grphnode_b", "grphnode_c" }));
    if (res) {
        return res;
    }

    res = assert_node(6, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_c"}));
    if (res) {
        return res;
    }

    res = assert_node(4, "grphnode_c", 0, NULL);
    if (res) {
        return res;
    }

    res = assert_node(11, HIERARCHY_RDB_EOF, 0, NULL);
    if (res) {
        return res;
    }

    return NULL;
}

static char * test_serialize_acyclic_2(void)
{
    /*
     *  a --> c
     *     /
     *  b --> d
     */
    char *res;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_c" }));
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);

    HierarchyTypeRDBSave(io, hierarchy);

    pu_assert_equal("the expected next item pointers are set", RedisRdb_CountIo(io), 14);

    res = assert_node(0, "grphnode_a", 1, ((Selva_NodeId []){ "grphnode_c" }));
    if (res) {
        return res;
    }

    res = assert_node(3, "grphnode_c", 0, NULL);
    if (res) {
        return res;
    }

    res = assert_node(5, "grphnode_b", 2, ((Selva_NodeId []){ "grphnode_c", "grphnode_d" }));
    if (res) {
        return res;
    }

    res = assert_node(9, "grphnode_d", 0, NULL);
    if (res) {
        return res;
    }

    res = assert_node(13, HIERARCHY_RDB_EOF, 0, NULL);
    if (res) {
        return res;
    }

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_serialize_one_node, PU_RUN);
    pu_def_test(test_serialize_two_nodes, PU_RUN);
    pu_def_test(test_serialize_acyclic_1, PU_RUN);
    pu_def_test(test_serialize_acyclic_2, PU_RUN);
}
