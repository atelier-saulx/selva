/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "util/selva_string.h"
#include "util/timestamp.h"
#include "selva_replication.h"
#include "eid.h"

uint64_t replication_new_origin_eid(const struct selva_string *filename)
{
    uint64_t eid;

    if (sscanf(selva_string_to_str(filename, NULL), "%" PRIu64 ".sdb", &eid) != 1) {
        eid = ts_now();
    }

    /* We assume there were no Selva DBs before 1970s. */
    return eid | EID_MSB_MASK;
}

uint64_t replication_new_replica_eid(const struct selva_string *filename)
{
    uint64_t eid;

    if (sscanf(selva_string_to_str(filename, NULL), "replica-%" PRIu64 ".sdb", &eid) == 1) {
        return eid | EID_MSB_MASK;
    }

    return 0;
}
