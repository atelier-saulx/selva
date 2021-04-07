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
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-15)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-16)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-17)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-18)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-19)
/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-20)
/**
 * Selva object has reached the maximum size.
 */
#define SELVA_OBJECT_EOBIG              (-21)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-22)

struct RedisModuleCtx;

const char *getSelvaErrorStr(int err);
int replyWithSelvaError(struct RedisModuleCtx *ctx, int err);
int replyWithSelvaErrorf(struct RedisModuleCtx *ctx, int err, const char *fmt, ...);

extern const char * const selvaStrError[-SELVA_INVALID_ERROR + 1];

#endif /* SELVA_ERRORS */
