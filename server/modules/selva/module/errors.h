#pragma once
#ifndef SELVA_MODIFY_ERRORS
#define SELVA_MODIFY_ERRORS

/*
 * Error codes.
 */

/**
 * General error.
 */
#define SELVA_EGENERAL (-1)
/**
 * General error.
 */
#define SELVA_MODIFY_HIERARCHY_EGENERAL (-2)
/**
 * Operation not supported.
 */
#define SELVA_MODIFY_HIERARCHY_ENOTSUP  (-3)
/**
 * Invalid argument/input value.
 */
#define SELVA_MODIFY_HIERARCHY_EINVAL   (-4)
/**
 * Out of memory.
 */
#define SELVA_MODIFY_HIERARCHY_ENOMEM   (-5)
/**
 * Node or entity not found.
 */
#define SELVA_MODIFY_HIERARCHY_ENOENT   (-6)
/**
 * Node or entity already exist.
 */
#define SELVA_MODIFY_HIERARCHY_EEXIST   (-7)
/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-8)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-9)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-10)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-11)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-12)
/* This must be the last error */
#define SELVA_MODIFY_INVALID_ERROR      (-13)

struct RedisModuleCtx;

int replyWithSelvaError(struct RedisModuleCtx *ctx, int err);

extern const char * const hierarchyStrError[-SELVA_MODIFY_INVALID_ERROR + 1];

#endif /* SELVA_MODIFY_ERRORS */
