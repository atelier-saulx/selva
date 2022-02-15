#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "redismodule.h"
#include "cstrings.h"
#include "errors.h"
#include "hierarchy.h"
#include "edge.h"
#include "selva_onload.h"
#include "selva_object.h"

static void EdgeConstraint_Reply(struct RedisModuleCtx *ctx, void *p);
static void *so_rdb_load(struct RedisModuleIO *io, int encver, void *load_data);
static void so_rdb_save(struct RedisModuleIO *io, void *value, void *load_data);

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

void Edge_InitEdgeFieldConstraints(struct EdgeFieldConstraints *data) {
    memset(data, 0, sizeof(*data));
    data->hard_constraints[0].constraint_id = EDGE_FIELD_CONSTRAINT_ID_DEFAULT;
    data->hard_constraints[1].constraint_id = EDGE_FIELD_CONSTRAINT_SINGLE_REF;
    data->hard_constraints[1].flags = EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF;
    data->dyn_constraints = SelvaObject_New();

    if (!data->dyn_constraints) {
        fprintf(stderr, "%s:%d: Fatal ENOMEM\n", __FILE__, __LINE__);
        abort();
    }
}

void Edge_DeinitEdgeFieldConstraints(struct EdgeFieldConstraints *data) {
    SelvaObject_Destroy(data->dyn_constraints);
    memset(data, 0, sizeof(*data));
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
    const char *fwd_field_name_str = RedisModule_StringPtrLen(params->fwd_field_name, &fwd_field_name_len);
    size_t bck_field_name_len = 0;
    const char *bck_field_name_str = NULL;
    size_t dst_node_types_len = 0;
    const char *dst_node_types_str = params->dst_node_types ? RedisModule_StringPtrLen(params->dst_node_types, &dst_node_types_len) : NULL;
    struct EdgeFieldConstraint *p;

    assert(dst_node_types_len % SELVA_NODE_TYPE_SIZE == 0);

    if (is_bidir) {
        bck_field_name_str = RedisModule_StringPtrLen(params->bck_field_name, &bck_field_name_len);
    }

    p = RedisModule_Calloc(1,
            sizeof(*p) +
            dst_node_types_len + 2 +
            fwd_field_name_len + bck_field_name_len + 2);
    if (!p) {
        return NULL;
    }

    /*
     * Set the string pointers.
     */
    p->dst_node_types = (char *)p + sizeof(*p);
    p->field_name_str = p->dst_node_types + dst_node_types_len + 2;
    p->bck_field_name_str = p->field_name_str + fwd_field_name_len + 1;

    p->constraint_id = EDGE_FIELD_CONSTRAINT_DYNAMIC;
    p->flags = params->flags | EDGE_FIELD_CONSTRAINT_FLAG_DYNAMIC;
    memcpy(p->src_node_type, params->src_node_type, SELVA_NODE_TYPE_SIZE);

    if (dst_node_types_str && dst_node_types_len > 0) {
        memcpy(p->dst_node_types, dst_node_types_str, dst_node_types_len);
        p->dst_node_types[dst_node_types_len] = '\0';
        p->dst_node_types[dst_node_types_len + 1] = '\0';
    } else {
        p->dst_node_types = NULL;
    }

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

int Edge_NewDynConstraint(struct EdgeFieldConstraints *data, const struct EdgeFieldDynConstraintParams *params) {
    size_t fwd_field_name_len;
    const char *fwd_field_name_str = RedisModule_StringPtrLen(params->fwd_field_name, &fwd_field_name_len);
    const size_t constraint_name_len = DYN_CONSTRAINT_NAME_LEN(fwd_field_name_len);
    char constraint_name_str[constraint_name_len];
    struct EdgeFieldConstraint *p;
    int err;

    make_dyn_constraint_name(constraint_name_str, params->src_node_type, fwd_field_name_str, fwd_field_name_len);

    err = SelvaObject_ExistsStr(data->dyn_constraints, constraint_name_str, constraint_name_len);
    if (err != SELVA_ENOENT) {
        return err;
    }

    p = create_constraint(params);
    if (!p) {
        return SELVA_ENOMEM;
    }

    return SelvaObject_SetPointerStr(data->dyn_constraints, constraint_name_str, constraint_name_len, p, &obj_opts);
}

const struct EdgeFieldConstraint *Edge_GetConstraint(
        const struct EdgeFieldConstraints *data,
        unsigned constraint_id,
        Selva_NodeType node_type,
        const char *field_name_str,
        size_t field_name_len) {
    const struct EdgeFieldConstraint *constraint = NULL;

    if (constraint_id == EDGE_FIELD_CONSTRAINT_DYNAMIC) {
        const size_t constraint_name_len = DYN_CONSTRAINT_NAME_LEN(field_name_len);
        char constraint_name_str[constraint_name_len];
        void *p = NULL;
        int err;

        make_dyn_constraint_name(constraint_name_str, node_type, field_name_str, field_name_len);
        err = SelvaObject_GetPointerStr(data->dyn_constraints, constraint_name_str, constraint_name_len, &p);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to get a dynamic constraint. type: \"%.*s\" field_name: \"%.*s\" err: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_TYPE_SIZE, node_type,
                    (int)field_name_len, field_name_str,
                    getSelvaErrorStr(err));
        }

        constraint = p;
    } else if (constraint_id < num_elem(data->hard_constraints)) {
        constraint = &data->hard_constraints[constraint_id];
    }

    return constraint;
}

static void EdgeConstraint_Reply(struct RedisModuleCtx *ctx, void *p) {
    const struct EdgeFieldConstraint *constraint = (struct EdgeFieldConstraint *)p;

    RedisModule_ReplyWithArray(ctx, 6);

    RedisModule_ReplyWithSimpleString(ctx, "flags");
    RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "0x%x", constraint->flags));

    RedisModule_ReplyWithSimpleString(ctx, "field_name");
    RedisModule_ReplyWithStringBuffer(ctx, constraint->field_name_str, constraint->field_name_len);

    RedisModule_ReplyWithSimpleString(ctx, "bck_field_name");
    RedisModule_ReplyWithStringBuffer(ctx, constraint->bck_field_name_str, constraint->bck_field_name_len);
}

static void rdb_load_src_node_type(struct RedisModuleIO *io, Selva_NodeType type) {
    char *s;
    size_t len;

    s = RedisModule_LoadStringBuffer(io, &len);
    if (len == SELVA_NODE_TYPE_SIZE) {
        memcpy(type, s, SELVA_NODE_TYPE_SIZE);
    } else {
        memset(type, '\0', SELVA_NODE_TYPE_SIZE);
    }

    RedisModule_Free(s);
}

static void rdb_save_src_node_type(struct RedisModuleIO *io, const Selva_NodeType type) {
    RedisModule_SaveStringBuffer(io, type, SELVA_NODE_TYPE_SIZE);
}

static void rdb_save_dst_node_types(struct RedisModuleIO *io, const char *types) {
    if (types) {
        RedisModule_SaveStringBuffer(io, types, strlen(types));
    } else {
        RedisModule_SaveStringBuffer(io, "", 0);
    }

}

/**
 * Deserializer for SelvaObject ptr value.
 */
static void *so_rdb_load(struct RedisModuleIO *io, int encver __unused, void *load_data __unused) {
    struct EdgeFieldDynConstraintParams params = { 0 };
    struct EdgeFieldConstraint *constraint;

    params.flags = RedisModule_LoadUnsigned(io);
    rdb_load_src_node_type(io, params.src_node_type);
    params.dst_node_types = RedisModule_LoadString(io);
    params.fwd_field_name = RedisModule_LoadString(io);
    if (params.flags & EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL) {
        params.bck_field_name = RedisModule_LoadString(io);
    }

    constraint = create_constraint(&params);

    RedisModuleCtx *ctx = RedisModule_GetContextFromIO(io);
    RedisModule_FreeString(ctx, params.dst_node_types);
    RedisModule_FreeString(ctx, params.fwd_field_name);
    RedisModule_FreeString(ctx, params.bck_field_name);

    return constraint;
}

/**
 * Serializer for SelvaObject ptr value.
 */
static void so_rdb_save(struct RedisModuleIO *io, void *value, void *save_data __unused) {
    const struct EdgeFieldConstraint *constraint = (struct EdgeFieldConstraint *)value;

    RedisModule_SaveUnsigned(io, constraint->flags);
    rdb_save_src_node_type(io, constraint->src_node_type);
    rdb_save_dst_node_types(io, constraint->dst_node_types);
    RedisModule_SaveStringBuffer(io, constraint->field_name_str, constraint->field_name_len);
    if (constraint->flags & EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL) {
        RedisModule_SaveStringBuffer(io, constraint->bck_field_name_str, constraint->bck_field_name_len);
    }
}

int EdgeConstraint_RdbLoad(struct RedisModuleIO *io, int encver, struct EdgeFieldConstraints *data) {
    if (encver < 2) { /* hierarchy encver */
        return 0; /* Only the latest version supports loading metadata. */
    }

    SelvaObject_Destroy(data->dyn_constraints); /* This was created in init and can be discarded now. */
    data->dyn_constraints = SelvaObjectTypeRDBLoad(io, encver, NULL);
    if (!data->dyn_constraints) {
        return SELVA_ENOMEM;
    }

    return 0;
}

void EdgeConstraint_RdbSave(struct RedisModuleIO *io, struct EdgeFieldConstraints *data) {
    SelvaObjectTypeRDBSave(io, data->dyn_constraints, NULL);
}

int Edge_AddConstraintCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    const int ARGV_REDIS_KEY = 1;
    const int ARGV_SRC_NODE_TYPE = 2;
    const int ARGV_CONSTRAINT_FLAGS = 3;
    const int ARGV_FWD_FIELD = 4;
    const int ARGV_BCK_FIELD = 5;
    const int ARGC_DST_NODE_TYPES = 6;
    int err;

    if (argc != 6 && argc != 7) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    Selva_NodeType src_type;
    err = SelvaArgParser_NodeType(src_type, argv[ARGV_SRC_NODE_TYPE]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "source node type");
    }

    size_t flags_len;
    const char *flags_str = RedisModule_StringPtrLen(argv[ARGV_CONSTRAINT_FLAGS], &flags_len);
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
        return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "constraint flags");
    }

    struct EdgeFieldDynConstraintParams params = {
        .flags = flags,
        .src_node_type = SELVA_TYPE_INITIALIZER(src_type),
        .fwd_field_name = argv[ARGV_FWD_FIELD],
        .bck_field_name = argv[ARGV_BCK_FIELD],
    };

    if (argc > ARGC_DST_NODE_TYPES) {
        size_t dst_node_types_len;

        (void)RedisModule_StringPtrLen(argv[ARGC_DST_NODE_TYPES], &dst_node_types_len);
        if (dst_node_types_len % SELVA_NODE_TYPE_SIZE != 0) {
            return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "destination node type(s)");
        }

        params.dst_node_types = argv[ARGC_DST_NODE_TYPES];
    }

    err = Edge_NewDynConstraint(&hierarchy->edge_field_constraints, &params);
    if (err == SELVA_EEXIST) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    } else if (err) {
        return replyWithSelvaError(ctx, err);
    } else {
        RedisModule_ReplyWithLongLong(ctx, 1);
        return RedisModule_ReplicateVerbatim(ctx);
    }
}

int Edge_ListConstraintsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    const int ARGV_REDIS_KEY = 1;
    int err;

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    err = SelvaObject_ReplyWithObject(ctx, NULL, hierarchy->edge_field_constraints.dyn_constraints, NULL, 0);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    return REDISMODULE_OK;
}

static int EdgeConstraints_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.addconstraint", Edge_AddConstraintCommand, "write", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.listconstraints", Edge_ListConstraintsCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(EdgeConstraints_OnLoad);
