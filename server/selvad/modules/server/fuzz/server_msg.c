// Copyright (c) 2022 SAULX
//
// SPDX-License-Identifier: MIT

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "selva_log.h"
#include "selva_proto.h"
#include "../server.h"

static const uint8_t *tcp_buf;
static size_t tcp_left;

void *selva_malloc(size_t n)
{
    return malloc(n);
}

void *selva_calloc(size_t n, size_t s)
{
    return calloc(n, s);
}

void *selva_realloc(void *ptr, size_t new_size)
{
    return realloc(ptr, new_size);
}

void selva_free(void *p)
{
    free(p);
}

const char *selva_strerror(int err)
{
    return "";
}

void _selva_log(enum selva_log_level level, const char * restrict where, const char * restrict func, const char * restrict fmt, ...)
{
}

uint32_t crc32c(uint32_t crc, void const *buf, size_t len)
{
    return 0;
}

void tcp_cork(int fd)
{
}

void tcp_uncork(int fd)
{
}

ssize_t tcp_read(int fd, void *buf, size_t count)
{
    size_t bytes;

    if (count <= tcp_left) {
        bytes = count;
    } else {
        bytes = tcp_left;
    }

    memcpy(buf, tcp_buf, bytes);
    buf += bytes;
    tcp_left -= bytes;

    return bytes;
}

ssize_t tcp_send(int sockfd, const void *buf, size_t len, int flags)
{
    /* Don't send anything */
    return len;
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    tcp_buf = Data;
    tcp_left = Size;
    struct conn_ctx ctx = { 0 };

    server_recv_message(&ctx);
    free(ctx.recv_msg_buf);

    return 0;
}

__constructor static void init(void)
{
    selva_log = _selva_log;
}
