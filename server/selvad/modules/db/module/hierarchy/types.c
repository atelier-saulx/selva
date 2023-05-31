/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "selva_replication.h"
#include "selva_server.h"
#include "selva_db.h"
#include "selva_onload.h"
#include "selva_object.h"
#include "arg_parser.h"
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

void SelvaHierarchyTypes_AddCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    struct SelvaHierarchy *hierarchy = main_hierarchy;
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    struct selva_string *type;
    struct selva_string *name;
    int err;

    finalizer_init(&fin);

    argc = selva_proto_scanf(&fin, buf, len, "%.*s, %p, %p", &type, &name);
    if (argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    TO_STR(type, name);

    if (type_len != 2) {
        selva_send_error(resp, SELVA_EINTYPE, NULL, 0);
        return;
    }

    err = SelvaHierarchyTypes_Add(hierarchy, type_str, name_str, name_len);
    if (err) {
        selva_send_errorf(resp, err, "Failed to add the type");
        return;
    }

    selva_db_is_dirty = 1;
    selva_send_ll(resp, 1);
    selva_replication_replicate(selva_resp_to_ts(resp), selva_resp_to_cmd_id(resp), buf, len);
}

void SelvaHierarchyTypes_ClearCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    if (len != 0) {
        selva_send_error_arity(resp);
    } else {
        SelvaHierarchyTypes_Clear(main_hierarchy);

        selva_db_is_dirty = 1;
        selva_send_ll(resp, 1);
        selva_replication_replicate(selva_resp_to_ts(resp), selva_resp_to_cmd_id(resp), buf, len);
    }
}

void SelvaHierarchyTypes_ListCommand(struct selva_server_response_out *resp, const void *buf __unused, size_t len) {
    if (len != 0) {
        selva_send_error_arity(resp);
    } else {
        SelvaObject_ReplyWithObject(resp, NULL, SELVA_HIERARCHY_GET_TYPES_OBJ(main_hierarchy), NULL, 0);
    }
}

static int SelvaHierarchyTypes_OnLoad(void) {
    selva_mk_command(CMD_ID_HIERARCHY_TYPES_ADD, SELVA_CMD_MODE_MUTATE, "hierarchy.types.add", SelvaHierarchyTypes_AddCommand);
    selva_mk_command(CMD_ID_HIERARCHY_TYPES_CLEAR, SELVA_CMD_MODE_MUTATE, "hierarchy.types.clear", SelvaHierarchyTypes_ClearCommand);
    selva_mk_command(CMD_ID_HIERARCHY_TYPES_LIST, SELVA_CMD_MODE_PURE, "hierarchy.types.list", SelvaHierarchyTypes_ListCommand);

    return 0;
}
SELVA_ONLOAD(SelvaHierarchyTypes_OnLoad);
