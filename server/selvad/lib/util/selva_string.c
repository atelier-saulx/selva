/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "jemalloc.h"
#include "cdefs.h"
#include "selva_error.h"
#include "util/crc32c.h"
#include "util/selva_string.h"

#define SELVA_STRING_QP(T, F, S, ...) \
    STATIC_IF (IS_POINTER_CONST((S)), \
            (T const *) (F) ((S) __VA_OPT__(,) __VA_ARGS__), \
            (T *) (F) ((S) __VA_OPT__(,) __VA_ARGS__))

struct selva_string {
    enum selva_string_flags flags;
    uint32_t crc;
    size_t len;
    union {
        char *p;
        char emb[sizeof(char *)];
    };
};

/**
 * Get a pointer to the string buffer.
 */
static inline char *get_buf(const struct selva_string *s)
{
    return (s->flags & SELVA_STRING_MUTABLE) ? (char *)s->p : (char *)s->emb;
}

#define get_buf(S) SELVA_STRING_QP(char, get_buf, (S))

/**
 * Calculate the CRC of a selva_string.
 * @param hdr is the header part of a selva_string i.e. without the actual string.
 */
static uint32_t calc_crc(const struct selva_string *hdr, const char *str)
{
    uint32_t res;

    res = crc32c(0, hdr, sizeof(struct selva_string) - sizeof_field(struct selva_string, emb));
    res = crc32c(res, str, hdr->len + 1);
    return res;
}

static void update_crc(struct selva_string *s)
{
    if (s->flags & SELVA_STRING_CRC) {
        s->crc = 0;
        s->crc = calc_crc(s, get_buf(s));
    }
}

struct selva_string *selva_string_create(const char *str, size_t len, enum selva_string_flags flags)
{
    struct selva_string *s;
    enum selva_string_flags xor_mask = SELVA_STRING_FREEZE | SELVA_STRING_MUTABLE;

    if ((flags & ~(SELVA_STRING_CRC | SELVA_STRING_FREEZE | SELVA_STRING_MUTABLE)) ||
        __builtin_popcount(flags & xor_mask) > 1) {
        return NULL; /* Invalid flags */
    }

    if (flags & SELVA_STRING_MUTABLE) {
        s = selva_calloc(1, sizeof(struct selva_string));
        s->p = selva_malloc(len + 1);
    } else {
        const size_t emb_size = sizeof_field(struct selva_string, emb);
        const size_t add = len + 1 <= emb_size ? 0 : len + 1 - emb_size;

        s = selva_malloc(sizeof(struct selva_string) + add);
        memset(s, 0, sizeof(struct selva_string)); /* We only want to clear the header. */
    }

    s->flags = flags;
    s->len = len;

    if (str && len > 0) {
        char *buf = get_buf(s);

        memcpy(buf, str, len);
        buf[s->len] = '\0';
    } else {
        memset(get_buf(s), '\0', s->len + 1);
    }

    update_crc(s);

    return s;
}

struct selva_string *selva_string_createf(const char *fmt, ...)
{
    va_list args;
    int res;
    struct selva_string *s;

    va_start(args, fmt);
    res = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (res < 0) {
        return NULL;
    }

    s = selva_string_create(NULL, res, 0);
    va_start(args, fmt);
    (void)vsnprintf(get_buf(s), s->len + 1, fmt, args);
    va_end(args);

    return s;
}

struct selva_string *selva_string_dup(struct selva_string *s, enum selva_string_flags flags)
{
    return selva_string_create(get_buf(s), s->len, flags);
}

int selva_string_truncate(struct selva_string *s, size_t newlen)
{
    const enum selva_string_flags flags = s->flags;
    const size_t oldlen = s->len;

    if (!(flags & SELVA_STRING_MUTABLE)) {
        return SELVA_ENOTSUP;
    }

    if (newlen >= oldlen) {
        return SELVA_EINVAL;
    } else if (newlen < oldlen) {
        s->len = newlen;
        s->p = selva_realloc(s->p, s->len + 1);
        s->p[s->len] = '\0';

        if (flags & SELVA_STRING_CRC) {
            update_crc(s);
        }
    }

    return 0;
}

int selva_string_append(struct selva_string *s, const char *str, size_t len)
{
    const enum selva_string_flags flags = s->flags;

    if (!(flags & SELVA_STRING_MUTABLE)) {
        return SELVA_ENOTSUP;
    }

    if (len > 0) {
        size_t old_len = s->len;

        s->len += len;
        s->p = selva_realloc(s->p, s->len + 1);
        memcpy(s->p + old_len, str, len);
        s->p[s->len] = '\0';

        if (flags & SELVA_STRING_CRC) {
            update_crc(s);
        }
    }

    return 0;
}

void selva_string_free(struct selva_string *s)
{
    const enum selva_string_flags flags = s->flags;

    if (flags & SELVA_STRING_FREEZE) {
        return;
    }

    if (flags & SELVA_STRING_MUTABLE) {
        selva_free(s->p);
    }
    selva_free(s);
}

const char *selva_string_get(struct selva_string *s, size_t *len)
{
    if (len) {
        *len = s->len;
    }

    return get_buf(s);
}

enum selva_string_flags selva_string_get_flags(struct selva_string *s)
{
    return s->flags;
}

void selva_string_freeze(struct selva_string *s)
{
    s->flags |= SELVA_STRING_FREEZE;
}

void selva_string_en_crc(struct selva_string *s)
{
    s->flags |= SELVA_STRING_CRC;
    update_crc(s);
}

int selva_string_verify_crc(struct selva_string *s)
{
    struct selva_string hdr;

    if (!(s->flags & SELVA_STRING_CRC)) {
        return 0;
    }

    memcpy(&hdr, s, sizeof(*s));
    hdr.crc = 0;

    return s->crc == calc_crc(&hdr, get_buf(s));
}

int selva_string_cmp(const struct selva_string *a, const struct selva_string *b)
{
    return strcmp(get_buf(a), get_buf(b));
}
