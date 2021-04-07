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
 * Operation not supported.
 */
#define SELVA_ENOTSUP                   (-2)
/**
 * Invalid argument/input value.
 */
#define SELVA_EINVAL                    (-3)
/**
 * Invalid type.
 */
#define SELVA_EINTYPE                   (-4)
/**
 * Name too long.
 */
#define SELVA_ENAMETOOLONG              (-5)
/**
 * Out of memory.
 */
#define SELVA_ENOMEM                    (-6)
/**
 * Node or entity not found.
 */
#define SELVA_ENOENT                    (-7)
/**
 * Node or entity already exist.
 */
#define SELVA_EEXIST                    (-8)
/**
 * General error.
 */
#define SELVA_HIERARCHY_EGENERAL        (-9)
/**
 * Operation not supported.
 */
#define SELVA_HIERARCHY_ENOTSUP         (-10)
/**
 * Invalid argument/input value.
 */
#define SELVA_HIERARCHY_EINVAL          (-11)
/**
 * Out of memory.
 */
#define SELVA_HIERARCHY_ENOMEM          (-12)
/**
 * Node or entity not found.
 */
#define SELVA_HIERARCHY_ENOENT          (-13)
/**
 * Node or entity already exist.
 */
#define SELVA_HIERARCHY_EEXIST          (-14)
/**
 * Maximum number of recursive traversal calls reached.
 */
#define SELVA_HIERARCHY_ETRMAX          (-15)
/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-16)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-17)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-18)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-19)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-20)
/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-21)
/**
 * Selva object has reached the maximum size.
 */
#define SELVA_OBJECT_EOBIG              (-22)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-23)

struct RedisModuleCtx;

const char *getSelvaErrorStr(int err);
int replyWithSelvaError(struct RedisModuleCtx *ctx, int err);
int replyWithSelvaErrorf(struct RedisModuleCtx *ctx, int err, const char *fmt, ...);

extern const char * const selvaStrError[-SELVA_INVALID_ERROR + 1];

#endif /* SELVA_ERRORS */
