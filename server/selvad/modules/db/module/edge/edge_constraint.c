/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "jemalloc.h"
#include "util/auto_free.h"
#include "util/cstrings.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_io.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_replication.h"
#include "selva_server.h"
#include "arg_parser.h"
#include "hierarchy.h"
#include "selva_db.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "edge.h"

static void EdgeConstraint_Reply(struct selva_server_response_out *resp, void *p);
static void *so_rdb_load(struct selva_io *io, int encver, void *load_data);
static void so_rdb_save(struct selva_io *io, void *value, void *load_data);

#define DYN_CONSTRAINT_NAME_LEN(field_name_len) \
    (SELVA_NODE_TYPE_SIZE + 1 + field_name_len)

static const struct SelvaObjectPointerOpts obj_opts = {
    .ptr_type_id = SELVA_OBJECT_POINTER_EDGE_CONSTRAINTS,
    .ptr_reply = EdgeConstraint_Reply,
    .ptr_free = NULL, /* We don't allow freeing constraints. */
    .ptr_len = NULL,
    .ptr_save = so_rdb_save,
    .ptr_load = so_rdb_load,
};
SELVA_OBJECT_POINTER_OPTS(obj_opts);

static inline struct SelvaObject *get_dyn_constraints(const struct EdgeFieldConstraints *efc) {
    return (struct SelvaObject *)(efc->dyn_constraints);
}

void Edge_InitEdgeFieldConstraints(struct EdgeFieldConstraints *efc) {
    memset(efc, 0, sizeof(*efc));
    efc->hard_constraints[0].constraint_id = EDGE_FIELD_CONSTRAINT_ID_DEFAULT;
    efc->hard_constraints[1].constraint_id = EDGE_FIELD_CONSTRAINT_SINGLE_REF;
    efc->hard_constraints[1].flags = EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF;
    SelvaObject_Init(efc->dyn_constraints);
}

void Edge_DeinitEdgeFieldConstraints(struct EdgeFieldConstraints *efc) {
    SelvaObject_Destroy(get_dyn_constraints(efc));
    memset(efc, 0, sizeof(*efc));
}

/**
 * Make a dynamic constraint object field name.
 * [`ma`, `my.field`] => `ma.my:field`
 * @param buf should be a buffer with size DYN_CONSTRAINT_NAME_LEN(field_name_len)
 * @param node_type is a node type
 * @param field_name_str is a edge field name
 * @param field_name_len is the length of field_name_str without the terminating character
 */
static char *make_dyn_constraint_name(char *buf, const char node_type[SELVA_NODE_TYPE_SIZE], const char *field_name_str, size_t field_name_len) {
    buf[SELVA_NODE_TYPE_SIZE] = '.';
    memcpy(buf, node_type, SELVA_NODE_TYPE_SIZE);
    memcpy(buf + SELVA_NODE_TYPE_SIZE + 1, field_name_str, field_name_len);
    ch_replace(buf + SELVA_NODE_TYPE_SIZE + 1, field_name_len, '.', ':');

    return buf;
}

static struct EdgeFieldConstraint *create_constraint(const struct EdgeFieldDynConstraintParams *params) {
    const int is_bidir = !!(params->flags & EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL);
    size_t fwd_field_name_len;
    const char *fwd_field_name_str = selva_string_to_str(params->fwd_field_name, &fwd_field_name_len);
    size_t bck_field_name_len = 0;
    const char *bck_field_name_str = NULL;
    struct EdgeFieldConstraint *p;

    if (is_bidir) {
        bck_field_name_str = selva_string_to_str(params->bck_field_name, &bck_field_name_len);
    }

    p = selva_calloc(1,
            sizeof(*p) +
            fwd_field_name_len + bck_field_name_len + 2);
    if (!p) {
        return NULL;
    }

    /*
     * Set the string pointers.
     */
    p->field_name_str = (char *)p + sizeof(*p);
    p->bck_field_name_str = p->field_name_str + fwd_field_name_len + 1;

    p->constraint_id = EDGE_FIELD_CONSTRAINT_DYNAMIC;
    p->flags = params->flags | EDGE_FIELD_CONSTRAINT_FLAG_DYNAMIC;
    memcpy(p->src_node_type, params->src_node_type, SELVA_NODE_TYPE_SIZE);

    p->field_name_len = fwd_field_name_len;
    memcpy(p->field_name_str, fwd_field_name_str, fwd_field_name_len);
    p->field_name_str[fwd_field_name_len] = '\0';

    /*
     * Copy the bck_field_name if the field is bidirectional.
     */
    if (is_bidir) {
        p->bck_field_name_len = bck_field_name_len;
        memcpy(p->bck_field_name_str, bck_field_name_str, bck_field_name_len);
        p->bck_field_name_str[bck_field_name_len] = '\0';
    }

    return p;
}

int Edge_NewDynConstraint(struct EdgeFieldConstraints *efc, const struct EdgeFieldDynConstraintParams *params) {
    size_t fwd_field_name_len;
    const char *fwd_field_name_str = selva_string_to_str(params->fwd_field_name, &fwd_field_name_len);
    const size_t constraint_name_len = DYN_CONSTRAINT_NAME_LEN(fwd_field_name_len);
    char constraint_name_str[constraint_name_len];
    struct EdgeFieldConstraint *p;
    int err;

    make_dyn_constraint_name(constraint_name_str, params->src_node_type, fwd_field_name_str, fwd_field_name_len);

    err = SelvaObject_ExistsStr(get_dyn_constraints(efc), constraint_name_str, constraint_name_len);
    if (err != SELVA_ENOENT) {
        return err;
    }

    p = create_constraint(params);
    if (!p) {
        return SELVA_ENOMEM;
    }

    return SelvaObject_SetPointerStr(get_dyn_constraints(efc), constraint_name_str, constraint_name_len, p, &obj_opts);
}

const struct EdgeFieldConstraint *Edge_GetConstraint(
        const struct EdgeFieldConstraints *efc,
        unsigned constraint_id,
        const Selva_NodeType node_type,
        const char *field_name_str,
        size_t field_name_len) {
    const struct EdgeFieldConstraint *constraint = NULL;

    if (constraint_id == EDGE_FIELD_CONSTRAINT_DYNAMIC) {
        const size_t constraint_name_len = DYN_CONSTRAINT_NAME_LEN(field_name_len);
        char constraint_name_str[constraint_name_len];
        void *p = NULL;
        int err;

        make_dyn_constraint_name(constraint_name_str, node_type, field_name_str, field_name_len);
        err = SelvaObject_GetPointerStr(get_dyn_constraints(efc), constraint_name_str, constraint_name_len, &p);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR,
                      "Failed to get a dynamic constraint. type: \"%.*s\" field_name: \"%.*s\" err: %s",
                      (int)SELVA_NODE_TYPE_SIZE, node_type,
                      (int)field_name_len, field_name_str,
                      selva_strerror(err));
        }

        constraint = p;
    } else if (constraint_id < num_elem(efc->hard_constraints)) {
        constraint = &efc->hard_constraints[constraint_id];
    }

    return constraint;
}

static void EdgeConstraint_Reply(struct selva_server_response_out *resp, void *p) {
    const struct EdgeFieldConstraint *constraint = (struct EdgeFieldConstraint *)p;
    char buf[8];

    selva_send_array(resp, 6);

    selva_send_str(resp, "flags", 5);
    snprintf(buf, sizeof(buf), "0x%x", constraint->flags);
    selva_send_str(resp, buf, sizeof(buf) - 1);

    selva_send_str(resp, "field_name", 10);
    selva_send_str(resp, constraint->field_name_str, constraint->field_name_len);

    selva_send_str(resp, "bck_field_name", 14);
    selva_send_str(resp, constraint->bck_field_name_str, constraint->bck_field_name_len);
}

static void rdb_load_src_node_type(struct selva_io *io, Selva_NodeType type) {
    __selva_autofree const char *s;
    size_t len;

    s = selva_io_load_str(io, &len);
    if (len == SELVA_NODE_TYPE_SIZE) {
        memcpy(type, s, SELVA_NODE_TYPE_SIZE);
    } else {
        memset(type, '\0', SELVA_NODE_TYPE_SIZE);
    }
}

static void rdb_save_src_node_type(struct selva_io *io, const Selva_NodeType type) {
    selva_io_save_str(io, type, SELVA_NODE_TYPE_SIZE);
}

/**
 * Deserializer for SelvaObject ptr value.
 */
static void *so_rdb_load(struct selva_io *io, int encver __unused, void *load_data __unused) {
    struct EdgeFieldDynConstraintParams params = { 0 };
    struct EdgeFieldConstraint *constraint;

    params.flags = selva_io_load_unsigned(io);
    rdb_load_src_node_type(io, params.src_node_type);
    selva_string_free(selva_io_load_string(io)); /* Legacy. */
    params.fwd_field_name = selva_io_load_string(io);
    if (params.flags & EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL) {
        params.bck_field_name = selva_io_load_string(io);
    }

    constraint = create_constraint(&params);

    selva_string_free(params.fwd_field_name);
    selva_string_free(params.bck_field_name);

    return constraint;
}

/**
 * Serializer for SelvaObject ptr value.
 */
static void so_rdb_save(struct selva_io *io, void *value, void *save_data __unused) {
    const struct EdgeFieldConstraint *constraint = (struct EdgeFieldConstraint *)value;

    selva_io_save_unsigned(io, constraint->flags);
    rdb_save_src_node_type(io, constraint->src_node_type);
    selva_io_save_str(io, "", 0); /* Legacy. */
    selva_io_save_str(io, constraint->field_name_str, constraint->field_name_len);
    if (constraint->flags & EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL) {
        selva_io_save_str(io, constraint->bck_field_name_str, constraint->bck_field_name_len);
    }
}

int EdgeConstraint_RdbLoad(struct selva_io *io, int encver, struct EdgeFieldConstraints *data) {
    if (encver < 2) { /* hierarchy encver */
        return 0; /* Only the latest version supports loading metadata. */
    }

    if (!SelvaObjectTypeRDBLoadTo(io, encver, get_dyn_constraints(data), NULL)) {
        return SELVA_ENOENT;
    }

    return 0;
}

void EdgeConstraint_RdbSave(struct selva_io *io, struct EdgeFieldConstraints *data) {
    SelvaObjectTypeRDBSave(io, get_dyn_constraints(data), NULL);
}

void Edge_AddConstraintCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    SelvaHierarchy *hierarchy = main_hierarchy;
    struct selva_string **argv;
    int argc;
    int err;

    finalizer_init(&fin);

    const int ARGV_SRC_NODE_TYPE = 0;
    const int ARGV_CONSTRAINT_FLAGS = 1;
    const int ARGV_FWD_FIELD = 2;
    const int ARGV_BCK_FIELD = 3;

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc != 3 && argc != 4) {
        selva_send_error_arity(resp);
    }

    Selva_NodeType src_type;
    err = SelvaArgParser_NodeType(src_type, argv[ARGV_SRC_NODE_TYPE]);
    if (err) {
        selva_send_errorf(resp, err, "source node type");
        return;
    }

    size_t flags_len;
    const char *flags_str = selva_string_to_str(argv[ARGV_CONSTRAINT_FLAGS], &flags_len);
    enum EdgeFieldConstraintFlag flags = 0;

    for (size_t i = 0; i < flags_len; i++) {
        flags |= flags_str[i] == 'S' ? EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF : 0;
        flags |= flags_str[i] == 'B' ? EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL : 0;
        flags |= flags_str[i] == 'D' ? EDGE_FIELD_CONSTRAINT_FLAG_DYNAMIC : 0; /* Implicit. */
    }

    if ((flags & ~(
         EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF |
         EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL |
         EDGE_FIELD_CONSTRAINT_FLAG_DYNAMIC
        )) != 0) {
        selva_send_errorf(resp, SELVA_EINVAL, "constraint flags");
        return;
    }

    struct EdgeFieldDynConstraintParams params = {
        .flags = flags,
        .src_node_type = SELVA_TYPE_INITIALIZER(src_type),
        .fwd_field_name = argv[ARGV_FWD_FIELD],
        .bck_field_name = argv[ARGV_BCK_FIELD],
    };

    err = Edge_NewDynConstraint(&hierarchy->edge_field_constraints, &params);
    if (err == SELVA_EEXIST) {
        selva_send_ll(resp, 0);
        return;
    } else if (err) {
        selva_send_error(resp, err, NULL, 0);
        return;
    } else {
        selva_db_is_dirty = 1;
        selva_send_ll(resp, 1);
        selva_replication_replicate(selva_resp_to_cmd_id(resp), buf, len);
    }
}

void Edge_ListConstraintsCommand(struct selva_server_response_out *resp, const void *buf __unused, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    int err;

    if (len != 0) {
        selva_send_error_arity(resp);
        return;
    }

    err = SelvaObject_ReplyWithObject(resp, NULL, get_dyn_constraints(&hierarchy->edge_field_constraints), NULL, 0);
    if (err) {
        selva_send_error(resp, err, NULL, 0);
        return;
    }
}

static int EdgeConstraints_OnLoad(void) {
    selva_mk_command(CMD_HIERARCHY_ADDCONSTRAINT_ID, SELVA_CMD_MODE_MUTATE, "hierarchy.addConstraint", Edge_AddConstraintCommand);
    selva_mk_command(CMD_HIERARCHY_LIST_CONSTRAINTS_ID, SELVA_CMD_MODE_PURE, "hierarchy.listConstraints", Edge_ListConstraintsCommand);

    return 0;
}
SELVA_ONLOAD(EdgeConstraints_OnLoad);
