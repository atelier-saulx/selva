#include <stddef.h>
#include <stdlib.h>
#include "redismodule.h"
#include "cstrings.h"
#include "errors.h"
#include "hierarchy.h"
#include "edge.h"
#include "selva_onload.h"
#include "selva_object.h"

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

int Edge_NewDynConstraint(struct EdgeFieldConstraints *data, struct EdgeFieldDynConstraintParams *params) {
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
        return SELVA_ENOMEM;
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

    const size_t constraint_name_len = DYN_CONSTRAINT_NAME_LEN(fwd_field_name_len);
    char constraint_name_str[constraint_name_len];

    make_dyn_constraint_name(constraint_name_str, params->fwd_node_type, p->field_name_str, p->field_name_len);
    /* TODO Add opts */
    SelvaObject_SetPointerStr(data->dyn_constraints, constraint_name_str, constraint_name_len, p, NULL);

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
    if (RedisModule_CreateCommand(ctx, "selva.edge.addconstraint", Edge_AddConstraintCommand, "write", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(EdgeConstraints_OnLoad);
