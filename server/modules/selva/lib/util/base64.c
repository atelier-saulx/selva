/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2021 SAULX
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "base64.h"

static const unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t base64_encode_s(char *str_out, const char *str_in, size_t len, size_t line_max) {
    const unsigned char *src = (const unsigned char *)str_in;
    unsigned char *dst = (unsigned char *)str_out;
    unsigned char *pos;
    const unsigned char *end, *in;
    size_t line_len;

    if (line_max == 0) {
        line_max = SIZE_MAX;
    }

    end = src + len;
    in = src;
    pos = dst;
    line_len = 0;
    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
        line_len += 4;
        if (line_len >= line_max) {
            *pos++ = '\n';
            line_len = 0;
        }
    }

    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
        line_len += 4;
    }

    if (line_len && line_max != SIZE_MAX) {
        *pos++ = '\n';
    }

    *pos = '\0';
    return pos - dst;
}

char * base64_encode(const char *str_in, size_t len, size_t *out_len)
{
    const size_t line_max = 72;
    char *out;
    size_t n;

    out = RedisModule_Alloc(base64_out_len(len, line_max) + 1);
    if (out == NULL) {
        return NULL;
    }

    n = base64_encode_s(out, str_in, len, line_max);
    if (out_len) {
        *out_len = n;
    }
    return out;
}

char * base64_decode(const char *str_in, size_t len, size_t *out_len)
{
    const unsigned char *src = (const unsigned char *)str_in;
    unsigned char dtable[256], *out, *pos, block[4];
    size_t i, count, olen;
    int pad = 0;

    memset(dtable, 0x80, sizeof(dtable));
    for (i = 0; i < sizeof(base64_table) - 1; i++) {
        dtable[base64_table[i]] = (unsigned char)i;
    }
    dtable['='] = 0;

    count = 0;
    for (i = 0; i < len; i++) {
        if (dtable[src[i]] != 0x80) {
            count++;
        }
    }

    if (count == 0 || count % 4) {
        return NULL;
    }

    olen = count / 4 * 3;
    pos = out = RedisModule_Alloc(olen);
    if (out == NULL) {
        return NULL;
    }

    count = 0;
    for (i = 0; i < len; i++) {
        unsigned char tmp;

        tmp = dtable[src[i]];
        if (tmp == 0x80) {
            continue;
        }

        if (src[i] == '=') {
            pad++;
        }
        block[count] = tmp;
        count++;
        if (count == 4) {
            *pos++ = (block[0] << 2) | (block[1] >> 4);
            *pos++ = (block[1] << 4) | (block[2] >> 2);
            *pos++ = (block[2] << 6) | block[3];
            count = 0;
            if (pad) {
                if (pad == 1) {
                    pos--;
                } else if (pad == 2) {
                    pos -= 2;
                } else {
                    /* Invalid padding */
                    RedisModule_Free(out);
                    return NULL;
                }
                break;
            }
        }
    }

    *out_len = pos - out;
    return (char *)out;
}
