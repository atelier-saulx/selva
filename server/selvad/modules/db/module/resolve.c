/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include "util/selva_string.h"
#include "util/finalizer.h"
#include "selva_error.h"
#include "selva_server.h"
#include "arg_parser.h"
#include "hierarchy.h"
#include "selva_db.h"
#include "selva_onload.h"
#include "subscriptions.h"
#include "resolve.h"

int SelvaResolve_NodeId(
        SelvaHierarchy *hierarchy,
        struct selva_string **ids,
        size_t nr_ids,
        Selva_NodeId node_id) {
    int res = SELVA_ENOENT;

    if (nr_ids == 0) {
        memcpy(node_id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE);

        return 0;
    }

    for (size_t i = 0; i < nr_ids; i++) {
        const struct selva_string *id = ids[i];
        TO_STR(id);

        /* First check if it's a nodeId. */
        if (id_len <= SELVA_NODE_ID_SIZE) {
            Selva_NodeIdCpy(node_id, id_str);

            /* We assume that root always exists. */
            if (!memcmp(node_id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE)) {
                res = SELVA_RESOLVE_NODE_ID;
                break;
            }

            if (SelvaHierarchy_NodeExists(hierarchy, node_id)) {
                res = SELVA_RESOLVE_NODE_ID;
                break;
            }
        }

        /* Then check if there is an alias with this string. */
        Selva_NodeId tmp_id;
        if (!get_alias(hierarchy, id, tmp_id)) {
            if (SelvaHierarchy_NodeExists(hierarchy, tmp_id)) {
                memcpy(node_id, tmp_id, SELVA_NODE_ID_SIZE);
                res = SELVA_RESOLVE_ALIAS;
                break;
            }
        }
    }

    return res;
}

/*
 * HIERARCHY_KEY SUB_ID IDS...
 */
void SelvaResolve_NodeIdCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv = NULL;
    int argc;

    finalizer_init(&fin);

    const size_t ARGV_SUB_ID = 0;
    const size_t ARGV_IDS = 1;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    Selva_NodeId node_id;
    const int resolved = SelvaResolve_NodeId(hierarchy, argv + ARGV_IDS, argc - ARGV_IDS, node_id);
    if (resolved == SELVA_ENOENT) {
        selva_send_null(resp);
        return;
    } else if (resolved < 0) {
        selva_send_errorf(resp, resolved, "Resolve failed");
        return;
    }

    const struct selva_string *argv_sub_id = argv[ARGV_SUB_ID];
    TO_STR(argv_sub_id);

    if ((resolved & SELVA_RESOLVE_ALIAS) && argv_sub_id_len > 0) {
        struct selva_string *alias_name = argv[ARGV_IDS + (resolved & ~SELVA_RESOLVE_FLAGS)];
        Selva_SubscriptionId sub_id;
        const Selva_SubscriptionMarkerId marker_id = Selva_GenSubscriptionMarkerId(0, selva_string_to_str(alias_name, NULL));
        int err;

        err = SelvaArgParser_SubscriptionId(sub_id, argv_sub_id);
        if (err) {
            selva_send_errorf(resp, err, "Invalid sub_id \"%s\"\n", argv_sub_id_str);
            return;
        }

        err = Selva_AddSubscriptionAliasMarker(hierarchy, sub_id, marker_id, alias_name, node_id);
        if (err && err != SELVA_SUBSCRIPTIONS_EEXIST) {
            selva_send_errorf(resp, err, "Failed to subscribe sub_id: \"%s.%d\" alias_name: %s node_id: %.*s\n",
                              argv_sub_id_str,
                              (int)marker_id,
                              selva_string_to_str(alias_name, NULL),
                              (int)SELVA_NODE_ID_SIZE, node_id);
            return;
        }
    }

    selva_send_str(resp, node_id, Selva_NodeIdLen(node_id));
}

static int SelvaResolve_OnLoad(void) {
    selva_mk_command(CMD_RESOLVE_NODEID_ID, "resolve.nodeid", SelvaResolve_NodeIdCommand);

    return 0;
}
SELVA_ONLOAD(SelvaResolve_OnLoad);
