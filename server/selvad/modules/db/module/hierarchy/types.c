/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <sys/types.h>
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_server.h"
#include "selva_db.h"
#include "selva_onload.h"
#include "selva_object.h"
#include "hierarchy.h"

/**
 * This function takes care of sharing/holding name.
 */
static int SelvaHierarchyTypes_Add(struct SelvaHierarchy *hierarchy, const Selva_NodeType type, const char *name_str, size_t name_len) {
    struct SelvaObject *obj = SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy);
    struct selva_string *name = selva_string_create(name_str, name_len, SELVA_STRING_INTERN);

    return SelvaObject_SetStringStr(obj, type, SELVA_NODE_TYPE_SIZE, name);
}

static void SelvaHierarchyTypes_Clear(struct SelvaHierarchy *hierarchy) {
    struct SelvaObject *obj = SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy);

    SelvaObject_Clear(obj, NULL);
}

struct selva_string *SelvaHierarchyTypes_Get(struct SelvaHierarchy *hierarchy, const Selva_NodeType type) {
    struct SelvaObject *obj = SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy);
    struct selva_string *out = NULL;

    (void)SelvaObject_GetStringStr(obj, type, SELVA_NODE_TYPE_SIZE, &out);

    return out;
}

int SelvaHierarchyTypes_AddCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    struct SelvaHierarchy *hierarchy = main_hierarchy;
    struct selva_string *type;
    struct selva_string *name;
    int err;

    const int ARGV_TYPE = 2;
    const int ARGV_NAME = 3;

    if (argc != 4) {
        return selva_send_error_arity(resp);
    }

    type = argv[ARGV_TYPE];
    name = argv[ARGV_NAME];
    TO_STR(type, name);

    if (type_len != 2) {
        return selva_send_error(resp, SELVA_EINTYPE, NULL, 0);
    }

    err = SelvaHierarchyTypes_Add(hierarchy, type_str, name_str, name_len);
    if (err) {
        return selva_send_error(resp, err, NULL, 0);
    }

    return selva_send_ll(resp, 1);
}

int SelvaHierarchyTypes_ClearCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    struct SelvaHierarchy *hierarchy = main_hierarchy;

    SelvaHierarchyTypes_Clear(hierarchy);
    return selva_send_ll(resp, 1);
}

int SelvaHierarchyTypes_ListCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    struct SelvaHierarchy *hierarchy = main_hierarchy;

    return SelvaObject_ReplyWithObject(resp, NULL, SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy), NULL, 0);
}

static int SelvaHierarchyTypes_OnLoad(void) {
    selva_mk_command(33, "hierarchy.types.add", SelvaHierarchyTypes_AddCommand);
    selva_mk_command(34, "hierarchy.types.clear", SelvaHierarchyTypes_ClearCommand);
    selva_mk_command(35, "hierarchy.types.list", SelvaHierarchyTypes_ListCommand);

    return 0;
}
SELVA_ONLOAD(SelvaHierarchyTypes_OnLoad);
