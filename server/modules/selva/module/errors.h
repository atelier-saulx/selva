#pragma once
#ifndef SELVA_ERRORS
#define SELVA_ERRORS

/*
 * Error codes.
 */

/**
 * General error.
 */
#define SELVA_EGENERAL                  (-1)
/**
 * Invalid argument/input value.
 */
#define SELVA_EINVAL                    (-2)
/**
 * Out of memory.
 */
#define SELVA_ENOMEM                    (-3)
/**
 * Node or entity not found.
 */
#define SELVA_ENOENT                    (-4)
/**
 * Node or entity already exist.
 */
#define SELVA_EEXIST                    (-5)
/**
 * General error.
 */
#define SELVA_MODIFY_HIERARCHY_EGENERAL (-6)
/**
 * Operation not supported.
 */
#define SELVA_MODIFY_HIERARCHY_ENOTSUP  (-7)
/**
 * Invalid argument/input value.
 */
#define SELVA_MODIFY_HIERARCHY_EINVAL   (-8)
/**
 * Out of memory.
 */
#define SELVA_MODIFY_HIERARCHY_ENOMEM   (-9)
/**
 * Node or entity not found.
 */
#define SELVA_MODIFY_HIERARCHY_ENOENT   (-10)
/**
 * Node or entity already exist.
 */
#define SELVA_MODIFY_HIERARCHY_EEXIST   (-11)
/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-12)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-13)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-14)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-15)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-16)
/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-17)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-18)

struct RedisModuleCtx;

const char *getSelvaErrorStr(int err);
int replyWithSelvaError(struct RedisModuleCtx *ctx, int err);

extern const char * const selvaStrError[-SELVA_INVALID_ERROR + 1];

#endif /* SELVA_ERRORS */
