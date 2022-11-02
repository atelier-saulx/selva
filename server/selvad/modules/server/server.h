/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
struct conn_ctx {
    int fd;
};

typedef void (*cmd_function)(struct conn_ctx *ctx, const char *buf, size_t size);
