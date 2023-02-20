/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_io.h"
#include "selva_server.h"
#include "selva_onload.h"
#include "arg_parser.h"
#include "hierarchy.h"

static void send_open_error(
        struct selva_server_response_out *resp,
        const struct selva_string *filename,
        enum selva_io_flags flags,
        int err)
{
    TO_STR(filename);
    const char *mode = (flags & SELVA_IO_FLAGS_WRITE) ? "write" : "read";

    SELVA_LOG(SELVA_LOGL_ERR, "Failed to open the db file \"%.*s\" for %s: %s",
            (int)filename_len, filename_str,
            mode,
            selva_strerror(err));
    selva_send_errorf(resp, err, "Failed to open the db file \"%.*s\" for %s",
                      (int)filename_len, filename_str,
                      mode);
}

static void load_db(struct selva_server_response_out *resp, const void *buf, size_t len)
{
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    int err;
    enum selva_io_flags flags = SELVA_IO_FLAGS_READ;
    struct selva_io *io;
    struct SelvaHierarchy *tmp_hierarchy = main_hierarchy;

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

    err = selva_io_new(argv[ARGV_FILENAME], flags, &io);
    if (err) {
        send_open_error(resp, argv[ARGV_FILENAME], flags, err);
        return;
    }

    main_hierarchy = Hierarchy_RDBLoad(io);
    if (main_hierarchy) {
        if (tmp_hierarchy) {
            SelvaModify_DestroyHierarchy(tmp_hierarchy);
        }
    } else {
        selva_send_errorf(resp, SELVA_EGENERAL, "Failed to load main_hierarchy");
        main_hierarchy = tmp_hierarchy;
    }

    selva_io_end(io);
    selva_send_ll(resp, 1);
}

static void save_db(struct selva_server_response_out *resp, const void *buf, size_t len)
{
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    int err;
    enum selva_io_flags flags = SELVA_IO_FLAGS_WRITE;
    struct selva_io *io;

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

    err = selva_io_new(argv[ARGV_FILENAME], flags, &io);
    if (err) {
        send_open_error(resp, argv[ARGV_FILENAME], flags, err);
        return;
    }

    Hierarchy_RDBSave(io, main_hierarchy);
    selva_io_end(io);
    selva_send_ll(resp, 1);
}

static int dump_onload(void) {
    selva_mk_command(CMD_LOAD_ID, SELVA_CMD_MODE_MUTATE, "load", load_db);
    selva_mk_command(CMD_SAVE_ID, SELVA_CMD_MODE_PURE, "save", save_db);

    return 0;
}
SELVA_ONLOAD(dump_onload);
