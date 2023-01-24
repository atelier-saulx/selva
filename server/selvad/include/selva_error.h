/*
 * Copyright (c) 2022-2023 SAULX
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
 * Input/output error.
 */
#define SELVA_EIO                       (-11)

/**
 * Operation already in progress.
 */
#define SELVA_PROTO_EALREADY            (-12)
/**
 * Operation not supported.
 */
#define SELVA_PROTO_ENOTSUP             (-13)
/**
 * Invalid argument/input value.
 */
#define SELVA_PROTO_EINVAL              (-14)
/**
 * Invalid type.
 */
#define SELVA_PROTO_EINTYPE             (-15)
/**
 * Out of memory.
 */
#define SELVA_PROTO_ENOMEM              (-16)
/**
 * Node or entity not found.
 */
#define SELVA_PROTO_ENOENT              (-17)
/**
 * Entity already exist.
 */
#define SELVA_PROTO_EEXIST              (-18)
/**
 * No buffer or resource space available.
 */
#define SELVA_PROTO_ENOBUFS             (-19)
/**
 * Bad message.
 */
#define SELVA_PROTO_EBADMSG             (-20)
/**
 * Not a valid open file descriptor.
 */
#define SELVA_PROTO_EBADF               (-21)
/**
 * Connection reset by peer.
 */
#define SELVA_PROTO_ECONNRESET          (-22)
/**
 * The socket is not connected.
 */
#define SELVA_PROTO_ENOTCONN            (-23)
/**
 * The local end has been shutdown.
 */
#define SELVA_PROTO_EPIPE               (-24)

/**
 * General error.
 */
#define SELVA_HIERARCHY_EGENERAL        (-25)
/**
 * Operation not supported.
 */
#define SELVA_HIERARCHY_ENOTSUP         (-26)
/**
 * Invalid argument/input value.
 */
#define SELVA_HIERARCHY_EINVAL          (-27)
/**
 * Out of memory.
 */
#define SELVA_HIERARCHY_ENOMEM          (-28)
/**
 * Node or entity not found.
 */
#define SELVA_HIERARCHY_ENOENT          (-29)
/**
 * Node or entity already exist.
 */
#define SELVA_HIERARCHY_EEXIST          (-30)
/**
 * Maximum number of recursive traversal calls reached.
 */
#define SELVA_HIERARCHY_ETRMAX          (-31)

/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-32)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-33)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-34)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-35)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-36)

/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-37)

/**
 * Selva object has reached the maximum size.
 */
#define SELVA_OBJECT_EOBIG              (-38)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-39)

/**
 * Selva error code to string.
 * Implemented in libutil.
 */
const char *selva_strerror(int err);
