/*
 * Copyright (c) 2022 SAULX
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
        const char *filename_str,
        size_t filename_len,
        enum selva_io_flags flags,
        int err)
{
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
    const char *filename_str;
    size_t filename_len;
    enum selva_io_flags flags = SELVA_IO_FLAGS_READ;
    struct selva_io *io;
    struct SelvaHierarchy *tmp_hierarchy = main_hierarchy;

    finalizer_init(&fin);

    const int ARGV_FILENAME  = 0;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc < 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }
    filename_str = selva_string_to_str(argv[ARGV_FILENAME], &filename_len);

    err = selva_io_new(filename_str, filename_len, flags, &io);
    if (err) {
        send_open_error(resp, filename_str, filename_len, flags, err);
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
    const char *filename_str;
    size_t filename_len;
    enum selva_io_flags flags = SELVA_IO_FLAGS_WRITE;
    struct selva_io *io;

    finalizer_init(&fin);

    const int ARGV_FILENAME  = 0;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc < 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }
    filename_str = selva_string_to_str(argv[ARGV_FILENAME], &filename_len);

    err = selva_io_new(filename_str, filename_len, flags, &io);
    if (err) {
        send_open_error(resp, filename_str, filename_len, flags, err);
        return;
    }

    Hierarchy_RDBSave(io, main_hierarchy);
    selva_io_end(io);
    selva_send_ll(resp, 1);
}

static int dump_onload(void) {
    selva_mk_command(14, "load", load_db);
    selva_mk_command(15, "save", save_db);

    return 0;
}
SELVA_ONLOAD(dump_onload);
