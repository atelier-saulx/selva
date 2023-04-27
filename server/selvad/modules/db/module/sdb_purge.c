/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _DEFAULT_SOURCE
#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "util/cstrings.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_onload.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "selva_io.h"

static struct selva_string *get_last_good(void)
{
    uint8_t hash[SELVA_IO_HASH_SIZE];
    struct selva_string *filename = NULL;

    (void)selva_io_last_good_info(hash, &filename);

    return filename;
}

static int sdb_filter(const struct dirent *d)
{
    return d->d_type == DT_REG && str_endswith(d->d_name, ".sdb");
}

static void purge_sdb_cmd(struct selva_server_response_out *resp, const void *buf, size_t len)
{
    __auto_finalizer struct finalizer fin;
    uint64_t nr_keep = 0;
    int argc;

    finalizer_init(&fin);

    argc = selva_proto_scanf(&fin, buf, len, "%" PRIu64, &nr_keep);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc > 0 && argc != 1) {
        selva_send_error_arity(resp);
        return;
    }

    struct selva_string *last_good = get_last_good();
    const char *last_good_str = last_good ? selva_string_to_str(last_good, NULL) : NULL;

    struct dirent **namelist = NULL;
    int n = scandir(".", &namelist, sdb_filter, alphasort);
    if (n == -1) {
        selva_send_errorf(resp, SELVA_EGENERAL, "Failed to scan for dumps");
        goto out;
    }

    const int nr_delete =  n - (int)nr_keep;
    int nr_deleted = 0;
    for (int i = 0; i < n && nr_deleted < nr_delete; i++) {
        struct dirent *dent = namelist[i];

        if (!last_good_str || strcmp(last_good_str, dent->d_name)) {
            SELVA_LOG(SELVA_LOGL_INFO, "Purging dump: \"%s\"", dent->d_name);
            (void)unlink(dent->d_name);
            nr_deleted++;
        }

        free(dent);
    }

    selva_send_ll(resp, nr_deleted);
out:
    selva_string_free(last_good);
    free(namelist);
}

static int sdb_purge_onload(void) {
    selva_mk_command(CMD_ID_PURGE, SELVA_CMD_MODE_PURE, "purge", purge_sdb_cmd);

    return 0;
}
SELVA_ONLOAD(sdb_purge_onload);
