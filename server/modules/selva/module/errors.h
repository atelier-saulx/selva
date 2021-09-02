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
 * No buffer or resource space available.
 */
#define SELVA_ENOBUFS                   (-9)
/**
 * General error.
 */
#define SELVA_HIERARCHY_EGENERAL        (-10)
/**
 * Operation not supported.
 */
#define SELVA_HIERARCHY_ENOTSUP         (-11)
/**
 * Invalid argument/input value.
 */
#define SELVA_HIERARCHY_EINVAL          (-12)
/**
 * Out of memory.
 */
#define SELVA_HIERARCHY_ENOMEM          (-13)
/**
 * Node or entity not found.
 */
#define SELVA_HIERARCHY_ENOENT          (-14)
/**
 * Node or entity already exist.
 */
#define SELVA_HIERARCHY_EEXIST          (-15)
/**
 * Maximum number of recursive traversal calls reached.
 */
#define SELVA_HIERARCHY_ETRMAX          (-16)
/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-17)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-18)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-19)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-20)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-21)
/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-22)
/**
 * Selva object has reached the maximum size.
 */
#define SELVA_OBJECT_EOBIG              (-23)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-24)

struct RedisModuleCtx;

const char *getSelvaErrorStr(int err);
int replyWithSelvaError(struct RedisModuleCtx *ctx, int err);
int replyWithSelvaErrorf(struct RedisModuleCtx *ctx, int err, const char *fmt, ...);

extern const char * const selvaStrError[-SELVA_INVALID_ERROR + 1];

#endif /* SELVA_ERRORS */
