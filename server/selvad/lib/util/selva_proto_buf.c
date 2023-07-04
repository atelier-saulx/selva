/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "jemalloc.h"
#include "cdefs.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_proto.h"

static void buf2strings_cleanup(struct finalizer *fin, struct selva_string **list, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        finalizer_del(fin, list[i]);
    }
    selva_free(list);
}

int selva_proto_buf2strings(struct finalizer *fin, const char *buf, size_t bsize, struct selva_string ***out)
{
    struct selva_string **list = NULL;
    size_t list_len = 0;
    size_t i = 0;

    while (i < bsize) {
        enum selva_proto_data_type type;
        size_t data_len;
        int off;

        off = selva_proto_parse_vtype(buf, bsize, i, &type, &data_len);
        if (off <= 0) {
            buf2strings_cleanup(fin, list, list_len);
            return off;
        }

        i += off;

        if (type == SELVA_PROTO_STRING) {
            typeof_field(struct selva_proto_string, flags) flags;
            struct selva_string *s;

            memcpy(&flags, buf + i - off + offsetof(struct selva_proto_string, flags), sizeof(flags));
            s = selva_string_create(buf + i - data_len, data_len, (flags & SELVA_PROTO_STRING_FDEFLATE) ? SELVA_STRING_COMPRESS : 0);
            selva_string_auto_finalize(fin, s);

            list_len++;
            list = selva_realloc(list, (list_len + 1) * sizeof(struct selva_string *));
            list[list_len - 1] = s;
            list[list_len] = NULL;
            *out = list;
        } else if (type == SELVA_PROTO_ARRAY || type == SELVA_PROTO_ARRAY_END) {
            /* NOP */
        } else {
            buf2strings_cleanup(fin, list, list_len);
            /*
             * This is more proper error than SELVA_PROTO_EINVAL because the
             * data could be correct according to the protocol spec but we just
             * can't parse it in the way the caller expects.
             */
            return SELVA_EINVAL;
        }
    }

    if (list) {
        finalizer_add(fin, list, selva_free);
    }
    *out = list;
    return list_len;
}
