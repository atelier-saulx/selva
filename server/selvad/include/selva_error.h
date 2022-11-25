/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"

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
 * Result too large.
 */
#define SELVA_ERANGE                    (-4)
/**
 * Invalid type.
 */
#define SELVA_EINTYPE                   (-5)
/**
 * Name too long.
 */
#define SELVA_ENAMETOOLONG              (-6)
/**
 * Out of memory.
 */
#define SELVA_ENOMEM                    (-7)
/**
 * Node or entity not found.
 */
#define SELVA_ENOENT                    (-8)
/**
 * Node or entity already exist.
 */
#define SELVA_EEXIST                    (-9)
/**
 * No buffer or resource space available.
 */
#define SELVA_ENOBUFS                   (-10)

/**
 * Operation already in progress.
 */
#define SELVA_PROTO_EALREADY            (-11)
/**
 * Operation not supported.
 */
#define SELVA_PROTO_ENOTSUP             (-12)
/**
 * Invalid argument/input value.
 */
#define SELVA_PROTO_EINVAL              (-13)
/**
 * Invalid type.
 */
#define SELVA_PROTO_EINTYPE             (-14)
/**
 * Out of memory.
 */
#define SELVA_PROTO_ENOMEM              (-15)
/**
 * Node or entity not found.
 */
#define SELVA_PROTO_ENOENT              (-16)
/**
 * Entity already exist.
 */
#define SELVA_PROTO_EEXIST              (-17)
/**
 * No buffer or resource space available.
 */
#define SELVA_PROTO_ENOBUFS             (-18)
/**
 * Bad message.
 */
#define SELVA_PROTO_EBADMSG             (-19)
/**
 * Not a valid open file descriptor.
 */
#define SELVA_PROTO_EBADF               (-20)
/**
 * Connection reset by peer.
 */
#define SELVA_PROTO_ECONNRESET          (-21)
/**
 * The socket is not connected.
 */
#define SELVA_PROTO_ENOTCONN            (-22)
/**
 * The local end has been shutdown.
 */
#define SELVA_PROTO_EPIPE               (-23)

/**
 * General error.
 */
#define SELVA_HIERARCHY_EGENERAL        (-24)
/**
 * Operation not supported.
 */
#define SELVA_HIERARCHY_ENOTSUP         (-25)
/**
 * Invalid argument/input value.
 */
#define SELVA_HIERARCHY_EINVAL          (-26)
/**
 * Out of memory.
 */
#define SELVA_HIERARCHY_ENOMEM          (-27)
/**
 * Node or entity not found.
 */
#define SELVA_HIERARCHY_ENOENT          (-28)
/**
 * Node or entity already exist.
 */
#define SELVA_HIERARCHY_EEXIST          (-29)
/**
 * Maximum number of recursive traversal calls reached.
 */
#define SELVA_HIERARCHY_ETRMAX          (-30)

/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-31)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-32)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-33)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-34)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-35)

/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-36)

/**
 * Selva object has reached the maximum size.
 */
#define SELVA_OBJECT_EOBIG              (-37)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-38)

/**
 * Selva error code to string.
 * Implemented in libutil.
 */
const char *selva_strerror(int err);
