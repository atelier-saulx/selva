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
 * Invalid type.
 */
#define SELVA_EINTYPE                   (-3)
/**
 * Name too long.
 */
#define SELVA_ENAMETOOLONG              (-4)
/**
 * Out of memory.
 */
#define SELVA_ENOMEM                    (-5)
/**
 * Node or entity not found.
 */
#define SELVA_ENOENT                    (-6)
/**
 * Node or entity already exist.
 */
#define SELVA_EEXIST                    (-7)
/**
 * General error.
 */
#define SELVA_MODIFY_HIERARCHY_EGENERAL (-8)
/**
 * Operation not supported.
 */
#define SELVA_MODIFY_HIERARCHY_ENOTSUP  (-9)
/**
 * Invalid argument/input value.
 */
#define SELVA_MODIFY_HIERARCHY_EINVAL   (-10)
/**
 * Out of memory.
 */
#define SELVA_MODIFY_HIERARCHY_ENOMEM   (-11)
/**
 * Node or entity not found.
 */
#define SELVA_MODIFY_HIERARCHY_ENOENT   (-12)
/**
 * Node or entity already exist.
 */
#define SELVA_MODIFY_HIERARCHY_EEXIST   (-13)
/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-14)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-15)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-16)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-17)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-18)
/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-19)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-20)

struct RedisModuleCtx;

const char *getSelvaErrorStr(int err);
int replyWithSelvaError(struct RedisModuleCtx *ctx, int err);
int replyWithSelvaErrorf(struct RedisModuleCtx *ctx, int err, char *fmt, ...);

extern const char * const selvaStrError[-SELVA_INVALID_ERROR + 1];

#endif /* SELVA_ERRORS */
