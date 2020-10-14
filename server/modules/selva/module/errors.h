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
 * Out of memory.
 */
#define SELVA_ENOMEM                    (-4)
/**
 * Node or entity not found.
 */
#define SELVA_ENOENT                    (-5)
/**
 * Node or entity already exist.
 */
#define SELVA_EEXIST                    (-6)
/**
 * General error.
 */
#define SELVA_MODIFY_HIERARCHY_EGENERAL (-7)
/**
 * Operation not supported.
 */
#define SELVA_MODIFY_HIERARCHY_ENOTSUP  (-8)
/**
 * Invalid argument/input value.
 */
#define SELVA_MODIFY_HIERARCHY_EINVAL   (-9)
/**
 * Out of memory.
 */
#define SELVA_MODIFY_HIERARCHY_ENOMEM   (-10)
/**
 * Node or entity not found.
 */
#define SELVA_MODIFY_HIERARCHY_ENOENT   (-11)
/**
 * Node or entity already exist.
 */
#define SELVA_MODIFY_HIERARCHY_EEXIST   (-12)
/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-13)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-14)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-15)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-16)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-17)
/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-18)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-19)

struct RedisModuleCtx;

const char *getSelvaErrorStr(int err);
int replyWithSelvaError(struct RedisModuleCtx *ctx, int err);

extern const char * const selvaStrError[-SELVA_INVALID_ERROR + 1];

#endif /* SELVA_ERRORS */
