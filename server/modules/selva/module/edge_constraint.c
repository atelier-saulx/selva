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

static const struct SelvaObjectPointerOpts obj_opts = {
    .ptr_type_id = SELVA_OBJECT_POINTER_EDGE,
    .ptr_reply = EdgeConstraint_Reply,
    .ptr_free = NULL, /* We don't allow freeing constraints. */
    .ptr_len = NULL,
    .ptr_save = so_rdb_save,
    .ptr_load = so_rdb_load,
};

void Edge_InitEdgeFieldConstraints(struct EdgeFieldConstraints *data) {
    memset(data, 0, sizeof(*data));
    data->hard_constraints[1].flags = EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF;
    data->dyn_constraints = SelvaObject_New();

    if (!data->dyn_constraints) {
        fprintf(stderr, "%s:%d: Fatal ENOMEM\n", __FILE__, __LINE__);
        abort();
    }
}

#define DYN_CONSTRAINT_NAME_LEN(field_name_len) \
    (SELVA_NODE_TYPE_SIZE + 1 + field_name_len)

/**
 * Make a dynamic constraint object field name.
 * [`ma`, `my.field`] => `ma.my:field`
 * @param buf should be a buffer with size DYN_CONSTRAINT_NAME_LEN(field_name_len)
 * @param node_type is a node type
 * @param field_name_str is a edge field name
 * @param field_name_len is the length of field_name_str without the terminating character
 */
static char *make_dyn_constraint_name(char *buf, char node_type[SELVA_NODE_TYPE_SIZE], const char *field_name_str, size_t field_name_len) {
    buf[SELVA_NODE_TYPE_SIZE] = '.';
    memcpy(buf, node_type, SELVA_NODE_TYPE_SIZE);
    memcpy(buf + SELVA_NODE_TYPE_SIZE + 1, field_name_str, field_name_len);
    ch_replace(buf + SELVA_NODE_TYPE_SIZE + 1, field_name_len, '.', ':');

    return buf;
}

static struct EdgeFieldConstraint *create_constraint(struct EdgeFieldDynConstraintParams *params) {
    size_t fwd_field_name_len;
    const char *fwd_field_name_str = RedisModule_StringPtrLen(params->fwd_field_name, &fwd_field_name_len);
    size_t bck_field_name_len = 0;
    const char *bck_field_name_str = NULL;
    struct EdgeFieldConstraint *p;

    if (params->flags & EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL &&
        params->bck_constraint_id == EDGE_FIELD_CONSTRAINT_DYNAMIC) {
        bck_field_name_str = RedisModule_StringPtrLen(params->bck_field_name, &bck_field_name_len);
    }

    p = RedisModule_Calloc(1, sizeof(*p) + fwd_field_name_len + bck_field_name_len + 2);
    if (!p) {
        return NULL;
    }

    p->flags = params->flags | EDGE_FIELD_CONSTRAINT_FLAG_DYNAMIC;
    p->field_name_str = (char *)p + sizeof(*p);
    p->field_name_len = fwd_field_name_len;
    memcpy(p->field_name_str, fwd_field_name_str, fwd_field_name_len);
    p->field_name_str[fwd_field_name_len] = '\0';

    if (p->flags & EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL) {
        p->bck_constraint_id = params->bck_constraint_id;

        if (p->bck_constraint_id == EDGE_FIELD_CONSTRAINT_DYNAMIC) {
            memcpy(p->bck_node_type, params->bck_node_type, SELVA_NODE_TYPE_SIZE);

            /*
             * Copy the bck_field_name.
             */
            p->bck_field_name_str = p->field_name_str + fwd_field_name_len + 1;
            p->bck_field_name_len = bck_field_name_len;
            memcpy(p->bck_field_name_str, bck_field_name_str, bck_field_name_len);
            p->bck_field_name_str[bck_field_name_len] = '\0';
        }
    }

    return p;
}

int Edge_NewDynConstraint(struct EdgeFieldConstraints *data, struct EdgeFieldDynConstraintParams *params) {
    size_t fwd_field_name_len;
    (void)RedisModule_StringPtrLen(params->fwd_field_name, &fwd_field_name_len);
    const size_t constraint_name_len = DYN_CONSTRAINT_NAME_LEN(fwd_field_name_len);
    char constraint_name_str[constraint_name_len];
    struct EdgeFieldConstraint *p;

    p = create_constraint(params);
    if (!p) {
        return SELVA_ENOMEM;
    }

    make_dyn_constraint_name(constraint_name_str, params->fwd_node_type, p->field_name_str, p->field_name_len);
    SelvaObject_SetPointerStr(data->dyn_constraints, constraint_name_str, constraint_name_len, p, &obj_opts);

    return 0;
}

const struct EdgeFieldConstraint *Edge_GetConstraint(const struct EdgeFieldConstraints *data, unsigned constraint_id, Selva_NodeType node_type, const char *field_name_str, size_t field_name_len) {
    const struct EdgeFieldConstraint *p = NULL;

    if (constraint_id == EDGE_FIELD_CONSTRAINT_DYNAMIC) {
        const size_t constraint_name_len = DYN_CONSTRAINT_NAME_LEN(field_name_len);
        char constraint_name_str[constraint_name_len];
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
    } else if (constraint_id < num_elem(data->hard_constraints)) {
        p = &data->hard_constraints[constraint_id];
    } else {
        p = NULL;
    }

    return p;
}

static void EdgeConstraint_Reply(struct RedisModuleCtx *ctx, void *p) {
    struct EdgeFieldConstraint *constraint = (struct EdgeFieldConstraint *)p;

    RedisModule_ReplyWithArray(ctx, 10);

    RedisModule_ReplyWithSimpleString(ctx, "flags");
    RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "0x%x", constraint->flags));

    RedisModule_ReplyWithSimpleString(ctx, "field_name");
    RedisModule_ReplyWithStringBuffer(ctx, constraint->field_name_str, constraint->field_name_len);

    RedisModule_ReplyWithSimpleString(ctx, "bck_constraint_id");
    RedisModule_ReplyWithLongLong(ctx, constraint->bck_constraint_id);

    RedisModule_ReplyWithSimpleString(ctx, "bck_node_type");
    RedisModule_ReplyWithStringBuffer(ctx, constraint->bck_node_type, SELVA_NODE_TYPE_SIZE);

    RedisModule_ReplyWithSimpleString(ctx, "bck_field_name");
    RedisModule_ReplyWithStringBuffer(ctx, constraint->bck_field_name_str, constraint->bck_field_name_len);
}

/**
 * Deserializer for SelvaObject ptr value.
 */
static void *so_rdb_load(struct RedisModuleIO *io, int encver __unused, void *load_data __unused) {
    struct EdgeFieldDynConstraintParams params = { 0 };
    char *tmp_str;
    size_t tmp_len;

    params.flags = RedisModule_LoadUnsigned(io);
    params.bck_constraint_id = RedisModule_LoadUnsigned(io);

    /* create_constraint() doesn't need params.fwd_node_type */

    tmp_str = RedisModule_LoadStringBuffer(io, &tmp_len);
    if (tmp_len != SELVA_NODE_TYPE_SIZE) {
        return NULL;
    }
    memcpy(params.bck_node_type, tmp_str, SELVA_NODE_TYPE_SIZE);
    RedisModule_Free(tmp_str);

    params.fwd_field_name = RedisModule_LoadString(io);
    if (params.bck_constraint_id == EDGE_FIELD_CONSTRAINT_DYNAMIC) {
        params.bck_field_name = RedisModule_LoadString(io);
    }

    struct EdgeFieldConstraint *constraint;
    constraint = create_constraint(&params);

    RedisModuleCtx *ctx = RedisModule_GetContextFromIO(io);
    RedisModule_FreeString(ctx, params.fwd_field_name);
    RedisModule_FreeString(ctx, params.bck_field_name);

    return constraint;
}

/**
 * Serializer for SelvaObject ptr value.
 */
static void so_rdb_save(struct RedisModuleIO *io, void *value, void *save_data __unused) {
    struct EdgeFieldConstraint *constraint = (struct EdgeFieldConstraint *)value;

    RedisModule_SaveUnsigned(io, constraint->flags);
    RedisModule_SaveUnsigned(io, constraint->bck_constraint_id);
    RedisModule_SaveStringBuffer(io, constraint->bck_node_type, SELVA_NODE_TYPE_SIZE);
    RedisModule_SaveStringBuffer(io, constraint->field_name_str, constraint->field_name_len);
    if (constraint->bck_constraint_id == EDGE_FIELD_CONSTRAINT_DYNAMIC) {
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
    const int ARGV_FWD_NODE_TYPE = 2;
    const int ARGV_FWD_FIELD = 3;
    const int ARGV_CONSTRAINT_FLAGS = 4;
    const int ARGV_BCK_CONSTRAINT_ID = 5;
    const int ARGV_BCK_NODE_TYPE = 7;
    const int ARGV_BCK_FIELD = 7;
    int err;

    if (argc != 8) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    Selva_NodeType fwd_type;
    err = SelvaArgParser_NodeType(fwd_type, argv[ARGV_FWD_NODE_TYPE]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "fwd_node_type");
    }

    long long flags;
    if (RedisModule_StringToLongLong(argv[ARGV_CONSTRAINT_FLAGS], &flags)) {
        return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "constraint flags");
    }
    if ((flags & ~(
         EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF |
         EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL |
         EDGE_FIELD_CONSTRAINT_FLAG_DYNAMIC
        )) != 0) {
        return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "constraint flags");
    }

    Selva_NodeType bck_type;
    err = SelvaArgParser_NodeType(bck_type, argv[ARGV_BCK_NODE_TYPE]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "bck_node_type");
    }

    long long bck_constraint_id;
    if (RedisModule_StringToLongLong(argv[ARGV_BCK_CONSTRAINT_ID], &bck_constraint_id)) {
        return replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "bck_constraint_id");
    }
    if (bck_constraint_id < 0) {
        return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "bck_constraint_id");
    }

    struct EdgeFieldDynConstraintParams params = {
        .flags = flags,
        .fwd_node_type = SELVA_TYPE_INITIALIZER(fwd_type),
        .fwd_field_name = argv[ARGV_FWD_FIELD],
        .bck_constraint_id = bck_constraint_id,
        .bck_node_type = SELVA_TYPE_INITIALIZER(bck_type),
        .bck_field_name = argv[ARGV_BCK_FIELD],
    };

    err = Edge_NewDynConstraint(&hierarchy->edge_field_constraints, &params);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    RedisModule_ReplyWithLongLong(ctx, 1);

    return RedisModule_ReplicateVerbatim(ctx);
}

static int EdgeConstraints_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.addconstraint", Edge_AddConstraintCommand, "write", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(EdgeConstraints_OnLoad);
