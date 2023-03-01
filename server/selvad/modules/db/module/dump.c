/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "util/finalizer.h"
#include "util/sdb_name.h"
#include "util/selva_string.h"
#include "util/timestamp.h"
#include "event_loop.h"
#include "selva_error.h"
#include "selva_io.h"
#include "selva_log.h"
#include "selva_db.h"
#include "selva_onload.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "myreadlink.h"
#include "hierarchy.h"
#include "dump.h"

static const char last_good_name[] = "dump.sdb";
int selva_db_is_dirty;

static int dump_load(const char *filename)
{
    const enum selva_io_flags flags = SELVA_IO_FLAGS_READ;
    struct selva_io *io;
    struct SelvaHierarchy *tmp_hierarchy = main_hierarchy;
    int err;

    err = selva_io_new(filename, flags, &io);
    if (err) {
        return err;
    }

    main_hierarchy = Hierarchy_RDBLoad(io);
    if (main_hierarchy) {
        if (tmp_hierarchy) {
            SelvaModify_DestroyHierarchy(tmp_hierarchy);
        }
    } else {
        main_hierarchy = tmp_hierarchy;
        err = SELVA_EGENERAL;
    }

    selva_io_end(io);
    return err;
}

static int dump_save(const char *filename)
{
    const enum selva_io_flags flags = SELVA_IO_FLAGS_WRITE;
    struct selva_io *io;
    int err;

    err = selva_io_new(filename, flags, &io);
    if (err) {
        return err;
    }

    Hierarchy_RDBSave(io, main_hierarchy);
    selva_io_end(io);

    (void)unlink(last_good_name);
    if (symlink(filename, last_good_name) == -1) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to symlink the last good dump \"%s\" as \"%s\"",
                  filename, last_good_name);
    }

    return 0;
}

static void auto_save(struct event *, void *arg)
{
    struct timespec *ts = (struct timespec *)arg;
    int tim, err;

    tim = evl_set_timeout(ts, auto_save, ts);
    if (tim < 0) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to schedule an autosave");
        exit(EXIT_FAILURE);
    }

    if (selva_db_is_dirty) {
        char filename[SDB_NAME_MIN_BUF_SIZE];

        sdb_name(filename, SDB_NAME_MIN_BUF_SIZE, NULL, (uint64_t)ts_now());

        err = dump_save(filename);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to autosave: %s", selva_strerror(err));
        }

        selva_db_is_dirty = 0;
        SELVA_LOG(SELVA_LOGL_INFO, "Autosave complete");
    }
}

int dump_load_default_sdb(void)
{
    struct selva_string *filename;
    int err;

    if (access(last_good_name, F_OK)) {
        return SELVA_ENOENT;
    }

    filename = myreadlink(last_good_name);
    if (!filename) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to resolve the symlink: \"%s\"", last_good_name);
        return SELVA_ENOENT;
    }
    err = dump_load(selva_string_to_str(filename, NULL));
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "SDB load failed: \"%s\" err: %s",
                  last_good_name,
                  selva_strerror(err));
        return err;
    }
    selva_string_free(filename);

    return 0;
}

int dump_auto_sdb(int interval_s)
{
    static struct timespec ts;
    int tim;

    assert(interval_s > 0);
    assert(ts.tv_sec == 0); /* Can be only called once. */

    ts.tv_sec = interval_s;

    /* TODO Add autosave timer */
    tim = evl_set_timeout(&ts, auto_save, &ts);
    if (tim < 0) {
        return tim;
    }

    return 0;
}

static void load_db_cmd(struct selva_server_response_out *resp, const void *buf, size_t len)
{
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc, err;

    finalizer_init(&fin);

    const int ARGV_FILENAME  = 0;

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc != 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    err = dump_load(selva_string_to_str(argv[ARGV_FILENAME], NULL));
    if (err) {
        if (err == SELVA_EGENERAL) {
            selva_send_errorf(resp, SELVA_EGENERAL, "Failed to load main_hierarchy");
        } else {
            selva_send_errorf(resp, err, "Failed to open the db file");
        }
        return;
    }

    selva_send_ll(resp, 1);
}

static void save_db_cmd(struct selva_server_response_out *resp, const void *buf, size_t len)
{
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    int err;

    finalizer_init(&fin);

    const int ARGV_FILENAME  = 0;

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc < 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    err = dump_save(selva_string_to_str(argv[ARGV_FILENAME], NULL));
    if (err) {
        selva_send_errorf(resp, err, "Failed to open a new sdb file for write");
        return;
    }

    selva_send_ll(resp, 1);
}

static int dump_onload(void) {
    selva_mk_command(CMD_LOAD_ID, SELVA_CMD_MODE_MUTATE, "load", load_db_cmd);
    selva_mk_command(CMD_SAVE_ID, SELVA_CMD_MODE_PURE, "save", save_db_cmd);

    return 0;
}
SELVA_ONLOAD(dump_onload);
