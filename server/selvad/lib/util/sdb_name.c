/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "util/sdb_name.h"

int sdb_name(char *buf, size_t buf_size, const char *prefix, uint64_t id)
{
    if (prefix) {
        return snprintf(buf, buf_size, "%s-%" PRIu64 ".sdb", prefix, id);
    } else {
        return snprintf(buf, buf_size, "%" PRIu64 ".sdb", id);
    }
}
