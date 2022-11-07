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
 * Operation already in progress.
 */
#define SELVA_PROTO_EALREADY            (-10)
/**
 * Operation not supported.
 */
#define SELVA_PROTO_ENOTSUP             (-11)
/**
 * Invalid argument/input value.
 */
#define SELVA_PROTO_EINVAL              (-12)
/**
 * Invalid type.
 */
#define SELVA_PROTO_EINTYPE             (-13)
/**
 * Out of memory.
 */
#define SELVA_PROTO_ENOMEM              (-14)
/**
 * Node or entity not found.
 */
#define SELVA_PROTO_ENOENT              (-15)
/**
 * Entity already exist.
 */
#define SELVA_PROTO_EEXIST              (-16)
/**
 * No buffer or resource space available.
 */
#define SELVA_PROTO_ENOBUFS             (-17)
/**
 * Bad message.
 */
#define SELVA_PROTO_EBADMSG             (-18)
/**
 * Not a valid open file descriptor.
 */
#define SELVA_PROTO_EBADF               (-19)
/**
 * Connection reset by peer.
 */
#define SELVA_PROTO_ECONNRESET          (-20)
/**
 * The socket is not connected.
 */
#define SELVA_PROTO_ENOTCONN            (-21)
/**
 * The local end has been shutdown.
 */
#define SELVA_PROTO_EPIPE               (-22)

/**
 * General error.
 */
#define SELVA_HIERARCHY_EGENERAL        (-23)
/**
 * Operation not supported.
 */
#define SELVA_HIERARCHY_ENOTSUP         (-24)
/**
 * Invalid argument/input value.
 */
#define SELVA_HIERARCHY_EINVAL          (-25)
/**
 * Out of memory.
 */
#define SELVA_HIERARCHY_ENOMEM          (-26)
/**
 * Node or entity not found.
 */
#define SELVA_HIERARCHY_ENOENT          (-27)
/**
 * Node or entity already exist.
 */
#define SELVA_HIERARCHY_EEXIST          (-28)
/**
 * Maximum number of recursive traversal calls reached.
 */
#define SELVA_HIERARCHY_ETRMAX          (-29)

/**
 * General error.
 */
#define SELVA_SUBSCRIPTIONS_EGENERAL    (-30)
/**
 * Invalid argument/input value.
 */
#define SELVA_SUBSCRIPTIONS_EINVAL      (-31)
/**
 * Out of memory.
 */
#define SELVA_SUBSCRIPTIONS_ENOMEM      (-32)
/**
 * Node or entity not found.
 */
#define SELVA_SUBSCRIPTIONS_ENOENT      (-33)
/**
 * Node or entity already exist.
 */
#define SELVA_SUBSCRIPTIONS_EEXIST      (-34)

/**
 * RPN compilation error.
 */
#define SELVA_RPN_ECOMP                 (-35)

/**
 * Selva object has reached the maximum size.
 */
#define SELVA_OBJECT_EOBIG              (-36)
/* This must be the last error */
#define SELVA_INVALID_ERROR             (-37)

EVL_EXPORT(const char *, selva_strerror, int err);
