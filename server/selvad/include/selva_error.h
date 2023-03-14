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
 * Operation already in progress.
 */
#define SELVA_EINPROGRESS               (-11)
/**
 * Input/output error.
 */
#define SELVA_EIO                       (-12)

/**
 * Operation already in progress.
 */
#define SELVA_PROTO_EALREADY            (-13)
/**
 * Operation not supported.
 */
#define SELVA_PROTO_ENOTSUP             (-14)
/**
 * Invalid argument/input value.
 */
#define SELVA_PROTO_EINVAL              (-15)
/**
 * Invalid type.
 */
#define SELVA_PROTO_EINTYPE             (-16)
/**
 * Out of memory.
 */
#define SELVA_PROTO_ENOMEM              (-17)
/**
 * Node or entity not found.
 */
#define SELVA_PROTO_ENOENT              (-18)
/**
 * Entity already exist.
 */
#define SELVA_PROTO_EEXIST              (-19)
/**
 * No buffer or resource space available.
 */
#define SELVA_PROTO_ENOBUFS             (-20)
/**
 * Bad message.
 */
#define SELVA_PROTO_EBADMSG             (-21)
/**
 * Not a valid open file descriptor.
 */
#define SELVA_PROTO_EBADF               (-22)
/**
 * Connection reset by peer.
 */
#define SELVA_PROTO_ECONNRESET          (-23)
/**
 * The socket is not connected.
 */
#define SELVA_PROTO_ENOTCONN            (-24)
/**
 * The local end has been shutdown.
 */
#define SELVA_PROTO_EPIPE               (-25)

/**
 * General error.
 */
#define SELVA_HIERARCHY_EGENERAL        (-26)
/**
 * Operation not supported.
 */
#define SELVA_HIERARCHY_ENOTSUP         (-27)
/**
 * Invalid argument/input value.
 */
#define SELVA_HIERARCHY_EINVAL          (-28)
/**
 * Out of memory.
 */
#define SELVA_HIERARCHY_ENOMEM          (-29)
/**
 * Node or entity not found.
 */
#define SELVA_HIERARCHY_ENOENT          (-30)
/**
 * Node or entity already exist.
 */
#define SELVA_HIERARCHY_EEXIST          (-31)
/**
 * Maximum number of recursive traversal calls reached.
 */
#define SELVA_HIERARCHY_ETRMAX          (-32)

/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-33)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-34)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-35)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-36)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-37)

/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-38)

/**
 * Selva object has reached the maximum size.
 */
#define SELVA_OBJECT_EOBIG              (-39)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-40)

/**
 * Selva error code to string.
 * Implemented in libutil.
 */
const char *selva_strerror(int err);
