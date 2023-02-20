/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <alloca.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "util/bitmap.h"
#include "util/cstrings.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "util/svector.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "selva_db.h"
#include "arg_parser.h"
#include "hierarchy.h"
#include "modify.h"
#include "rpn.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "subscriptions.h"
#include "inherit_fields.h"

struct InheritFieldValue_Args {
    size_t first_node; /*!< We ignore the type of the first node. */
    size_t nr_types;
    const Selva_NodeType *types;
    struct selva_string *lang;
    const char *field_name_str;
    size_t field_name_len;
    struct SelvaObjectAny *res;
};

struct InheritSendFields_Args {
    size_t first_node; /*!< We ignore the type of the first node. */
    struct selva_server_response_out *resp;
    size_t nr_fields;
    struct selva_string *lang;
    struct selva_string **field_names;
    struct bitmap *found;
    ssize_t nr_results; /*!< Number of results sent. */
};

struct InheritCommand_Args {
    struct selva_server_response_out *resp;
    size_t first_node; /*!< We ignore the type of the first node. */
    size_t nr_fields;
    size_t nr_types;
    const Selva_NodeType *types;
    struct selva_string *lang;
    struct selva_string **field_names;
    ssize_t nr_results; /*!< Number of results sent. */
};


static int is_type_match(struct SelvaHierarchyNode *node, const Selva_NodeType *types, size_t nr_types)
{
    Selva_NodeId node_id;

    if (nr_types == 0) {
        /* Wildcard */
        return 1;
    }

    SelvaHierarchy_GetNodeId(node_id, node);

    for (size_t i = 0; i < nr_types; i++) {
        if (!memcmp(types[i], node_id, SELVA_NODE_TYPE_SIZE)) {
            return 1;
        }
    }

    return 0;
}

static int Inherit_FieldValue_NodeCb(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct InheritFieldValue_Args *restrict args = (struct InheritFieldValue_Args *)arg;
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    int err;

    /*
     * Check that the node is of an accepted type.
     */
    if (likely(!args->first_node)) {
        if (!is_type_match(node, args->types, args->nr_types)) {
            /*
             * This node type is not accepted and we don't need to check whether
             * the field set.
             */
            return 0;
        }
    } else {
        args->first_node = 0;
    }

    err = Inherit_GetField(hierarchy, args->lang, node, obj, args->field_name_str, args->field_name_len, args->res);
    if (err == 0) {
        return 1; /* found */
    } else if (err != SELVA_ENOENT) {
        Selva_NodeId nodeId;

        SelvaHierarchy_GetNodeId(nodeId, node);

        /*
         * SELVA_ENOENT is expected as not all nodes have all fields set;
         * Any other error is unexpected.
         */
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to get a field value. nodeId: %.*s fieldName: \"%.*s\" error: %s\n",
                  (int)SELVA_NODE_ID_SIZE, nodeId,
                  (int)args->field_name_len, args->field_name_str,
                  selva_strerror(err));
    }

    return 0;
}

int Inherit_FieldValue(
        struct SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const Selva_NodeId node_id,
        const Selva_NodeType *types,
        size_t nr_types,
        const char *field_name_str,
        size_t field_name_len,
        struct SelvaObjectAny *res) {
    struct InheritFieldValue_Args args = {
        .lang = lang,
        .first_node = 1,
        .nr_types = nr_types,
        .types = types,
        .field_name_str = field_name_str,
        .field_name_len = field_name_len,
        .res = res,
    };
    const struct SelvaHierarchyCallback cb = {
        .node_cb = Inherit_FieldValue_NodeCb,
        .node_arg = &args,
    };

    return SelvaHierarchy_Traverse(hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, &cb);
}

static void parse_type_and_field(const char *str, size_t len, const char **types_str, size_t *types_len, const char **name_str, size_t *name_len) {
    if (len < 2 || str[0] != '^') {
        *name_str = NULL;
        *name_len = 0;
        *types_str = NULL;
        *types_len = 0;
        return;
    }

    *types_str = str + 1;
    *name_str = memchr(str + 1, ':', len);
    if (*name_str) {
        (*name_str)++;
    }

    *name_len = (size_t)((str + len) - *name_str);
    *types_len = (size_t)(*name_str - *types_str - 1);
}

static int Inherit_SendFields_NodeCb(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct InheritSendFields_Args *restrict args = (struct InheritSendFields_Args *)arg;
    const int first_node = args->first_node;
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    int err;

    if (unlikely(first_node)) {
        args->first_node = 0;
    }

    for (size_t i = 0; i < args->nr_fields; i++) {
        /* Field already found. */
        if (bitmap_get(args->found, i)) {
            continue;
        }

        struct selva_string *types_and_field = args->field_names[i];
        const char *types_str;
        size_t types_len;
        const char *field_name_str;
        size_t field_name_len;
        TO_STR(types_and_field);

		parse_type_and_field(types_and_field_str, types_and_field_len, &types_str, &types_len, &field_name_str, &field_name_len);
		if (!types_str || !field_name_str || field_name_len == 0) {
			/* Invalid inherit string. */
			continue;
		}

		if (!first_node && !is_type_match(node, (const char (*)[SELVA_NODE_TYPE_SIZE])types_str, types_len / sizeof(Selva_NodeType))) {
			/*
			 * This node type is not accepted and we don't need to check whether
			 * the field set.
			 * We accept any type for the first node.
			 */
			return 0;
		}

        /*
         * Get and send the field value to the client.
         * The response should always start like this: [node_id, field_name, ...]
         * but we don't send the header yet.
         */
        err = Inherit_SendFieldFind(args->resp, hierarchy, args->lang,
                                    node, obj,
                                    field_name_str, field_name_len, /* Initially full_field is the same as field_name. */
                                    field_name_str, field_name_len);
        if (err == 0) { /* found */
            bitmap_set(args->found, i); /* No need to look for this field anymore. */
            args->nr_results++;

            /* Stop traversing if all fields were found. */
            if (args->nr_results == (ssize_t)args->nr_fields) {
                return 1;
            }
        } else if (err != SELVA_ENOENT) {
            Selva_NodeId nodeId;

            SelvaHierarchy_GetNodeId(nodeId, node);

            /*
             * SELVA_ENOENT is expected as not all nodes have all fields set;
             * Any other error is unexpected.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to get a field value. nodeId: %.*s fieldName: \"%.*s\" error: %s\n",
                      (int)SELVA_NODE_ID_SIZE, nodeId,
                      (int)field_name_len, field_name_str,
                      selva_strerror(err));
        }
    }

    return 0;
}

int Inherit_SendFields(
        struct selva_server_response_out *resp,
        struct SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const Selva_NodeId node_id,
        struct selva_string **types_field_names,
        size_t nr_field_names) {
    struct InheritSendFields_Args args = {
        .resp = resp,
        .lang = lang,
        .first_node = 1,
        .field_names = types_field_names,
        .nr_fields = nr_field_names,
        .nr_results = 0,
    };
    const struct SelvaHierarchyCallback cb = {
        .node_cb = Inherit_SendFields_NodeCb,
        .node_arg = &args,
    };
    int err;

    args.found = alloca(BITMAP_ALLOC_SIZE(nr_field_names));
    args.found->nbits = nr_field_names;
    bitmap_erase(args.found);

    err = SelvaHierarchy_Traverse(hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, &cb);
    if (err) {
        /* TODO Better error handling? */
        SELVA_LOG(SELVA_LOGL_ERR, "Inherit failed: %s", selva_strerror(err));
    }

    return args.nr_results;
}

static int InheritCommand_NodeCb(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct InheritCommand_Args *restrict args = (struct InheritCommand_Args *)arg;
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    int err;

    /*
     * Check that the node is of an accepted type.
     */
    if (likely(!args->first_node)) {
        if (!is_type_match(node, args->types, args->nr_types)) {
            /*
             * This node type is not accepted and we don't need to check whether
             * the field set.
             */
            return 0;
        }
    } else {
        args->first_node = 0;
    }

    for (size_t i = 0; i < args->nr_fields; i++) {
        struct selva_string *field_name = args->field_names[i];

        /* Field already found. */
        if (!field_name) {
            continue;
        }

        /*
         * Get and send the field value to the client.
         * The response should always start like this: [node_id, field_name, ...]
         * but we don't send the header yet.
         */
        TO_STR(field_name);
        err = Inherit_SendField(args->resp, hierarchy, args->lang,
                                node, obj,
                                field_name_str, field_name_len, /* Initially full_field is the same as field_name. */
                                field_name_str, field_name_len);
        if (err == 0) { /* found */
            args->field_names[i] = NULL; /* No need to look for this one anymore. */
            args->nr_results++;

            /* Stop traversing if all fields were found. */
            if (args->nr_results == (ssize_t)args->nr_fields) {
                return 1;
            }
        } else if (err != SELVA_ENOENT) {
            Selva_NodeId nodeId;

            SelvaHierarchy_GetNodeId(nodeId, node);

            /*
             * SELVA_ENOENT is expected as not all nodes have all fields set;
             * Any other error is unexpected.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to get a field value. nodeId: %.*s fieldName: \"%s\" error: %s",
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    selva_string_to_str(field_name, NULL),
                    selva_strerror(err));
        }
    }

    return 0;
}

size_t inheritHierarchyFields(
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        Selva_NodeId node_id,
        size_t nr_types,
        const Selva_NodeType *types,
        size_t nr_field_names,
        struct selva_string **field_names) {
    size_t nr_presolved = 0;

    for (size_t i = 0; i < nr_field_names; i++) {
        struct selva_string *field_name = field_names[i];
        TO_STR(field_name);
        int err;

        err = 1; /* This value will help us know if something matched. */

#define IS_FIELD(name) \
        (!strcmp(field_name_str, name))

        /*
         * If the field_name is a hierarchy field the reply format is:
         * [node_id, field_name, [nodeId1, nodeId2,.. nodeIdn]]
         */
        if (IS_FIELD(SELVA_ANCESTORS_FIELD)) {
            selva_send_array(resp, 3);
            selva_send_str(resp, node_id, Selva_NodeIdLen(node_id));
            selva_send_string(resp, field_name);
            err = HierarchyReply_WithTraversal(resp, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS);
        } else if (IS_FIELD(SELVA_CHILDREN_FIELD)) {
            selva_send_array(resp, 3);
            selva_send_str(resp, node_id, Selva_NodeIdLen(node_id));
            selva_send_string(resp, field_name);
            err = HierarchyReply_WithTraversal(resp, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_CHILDREN);
        } else if (IS_FIELD(SELVA_DESCENDANTS_FIELD)) {
            selva_send_array(resp, 3);
            selva_send_str(resp, node_id, Selva_NodeIdLen(node_id));
            selva_send_string(resp, field_name);
            err = HierarchyReply_WithTraversal(resp, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS);
        } else if (IS_FIELD(SELVA_PARENTS_FIELD)) {
            selva_send_array(resp, 3);
            selva_send_str(resp, node_id, Selva_NodeIdLen(node_id));
            selva_send_string(resp, field_name);
            err = HierarchyReply_WithTraversal(resp, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_PARENTS);
        }

        if (err <= 0) { /* Something was traversed. */
            field_names[i] = NULL; /* This field is now resolved. */
            nr_presolved++;
        }

        if (err < 0) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to get a field value. nodeId: %.*s fieldName: \"%s\" error: %s",
                      (int)SELVA_NODE_ID_SIZE, node_id,
                      field_name_str,
                      selva_strerror(err));
        }
    }

    return nr_presolved;
#undef IS_FIELD
}

/**
 * Find node in set.
 * SELVA.inherit REDIS_KEY NODE_ID [TYPE1[TYPE2[...]]] [FIELD_NAME1[ FIELD_NAME2[ ...]]]
 */
void SelvaInheritCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    SelvaHierarchy *hierarchy = main_hierarchy;
    struct selva_string **argv;
    int argc;
    Selva_NodeId node_id;
    int err;

    finalizer_init(&fin);

    const int ARGV_LANG          = 1;
    const int ARGV_NODE_ID       = 3;
    const int ARGV_TYPES         = 4;
    const int ARGV_FIELD_NAMES   = 5;

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc < ARGV_FIELD_NAMES + 1) {
        selva_send_error_arity(resp);
        return;
    }

    struct selva_string *lang = argv[ARGV_LANG];

    hierarchy = main_hierarchy;

    /*
     * Get the node_id.
     */
    err = selva_string2node_id(node_id, argv[ARGV_NODE_ID]);
    if (err) {
        selva_send_errorf(resp, err, "node_id");
        return;
    }

    /*
     * Get types.
     */
    size_t nr_types;
    const Selva_NodeType *types = (char const (*)[SELVA_NODE_TYPE_SIZE])selva_string_to_str(argv[ARGV_TYPES], &nr_types);

    if (nr_types % SELVA_NODE_TYPE_SIZE != 0) {
        selva_send_errorf(resp, SELVA_EINVAL, "types");
        return;
    }
    nr_types /= SELVA_NODE_TYPE_SIZE;

    /*
     * Get field names.
     */
    const size_t nr_field_names = argc - ARGV_FIELD_NAMES;
    struct selva_string **field_names = alloca(nr_field_names * sizeof(struct selva_string *));

    memcpy(field_names, argv + ARGV_FIELD_NAMES, nr_field_names * sizeof(struct selva_string *));

    selva_send_array(resp, -1);

    /*
     * Run inherit for ancestors, descendants, parents, and children.
     * These fields will always exist in every node so these can be always
     * resolved at the top level.
     */
    size_t nr_presolved = inheritHierarchyFields(resp, hierarchy, node_id, nr_types, types, nr_field_names, field_names);

    /*
     * Execute a traversal to inherit the requested field values.
     */
    struct InheritCommand_Args args = {
        .resp = resp,
        .lang = lang,
        .first_node = 1,
        .nr_types = nr_types,
        .types = types,
        .field_names = field_names,
        .nr_fields = nr_field_names,
        .nr_results = nr_presolved,
    };
    const struct SelvaHierarchyCallback cb = {
        .node_cb = InheritCommand_NodeCb,
        .node_arg = &args,
    };

    err = SelvaHierarchy_Traverse(hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, &cb);
    /* Sent args.nr_results */
    selva_send_array_end(resp);

    if (err) {
        /*
         * We can't reply with an error anymore, so we just log it.
         */
        SELVA_LOG(SELVA_LOGL_ERR, "Inherit failed: %s", selva_strerror(err));
    }
}

static int Inherit_OnLoad(void) {
    selva_mk_command(CMD_HIERARCHY_INHERIT_ID, SELVA_CMD_MODE_PURE, "hierarchy.inherit", SelvaInheritCommand);

    return 0;
}
SELVA_ONLOAD(Inherit_OnLoad);
