/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#define SELVA_IO_TYPE
#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "util/sigstr.h"
#include "util/finalizer.h"
#include "util/sdb_name.h"
#include "util/selva_string.h"
#include "util/timestamp.h"
#include "sha3iuf/sha3.h"
#include "evl_signal.h"
#include "event_loop.h"
#include "selva_error.h"
#include "selva_io.h"
#include "selva_log.h"
#include "selva_db.h"
#include "selva_onload.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "selva_replication.h"
#include "hierarchy.h"
#include "dump.h"

int selva_db_is_dirty;
enum selva_db_dump_state selva_db_dump_state;
static pid_t save_pid;
static uint64_t save_sdb_eid;

/* TODO It's not necessary to do any of this if replication is not enabled. */
static void handle_last_good_sync(void)
{
    uint8_t hash[SELVA_IO_HASH_SIZE];
    struct selva_string *filename;

    if (!selva_io_last_good_info(hash, &filename)) {
        SELVA_LOG(SELVA_LOGL_INFO, "Found last good: \"%s\"", selva_string_to_str(filename, NULL));
        selva_replication_new_sdb(selva_string_to_str(filename, NULL), hash);
        selva_string_free(filename);
    } else {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to read the last good file");
    }
}

static void handle_last_good_async(void)
{
    uint8_t hash[SELVA_IO_HASH_SIZE];
    struct selva_string *filename;

    if (!selva_io_last_good_info(hash, &filename)) {
        SELVA_LOG(SELVA_LOGL_INFO, "Found last good: \"%s\"", selva_string_to_str(filename, NULL));
        /* TODO Should this function also verify the filename? */
        selva_replication_complete_sdb(save_sdb_eid, hash);
        selva_string_free(filename);
    } else {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to read the last good file");
    }
}

static int handle_child_status(pid_t pid, int status, const char *name)
{
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);

        if (code != 0) {
            SELVA_LOG(SELVA_LOGL_ERR,
                      "%s child %d terminated with exit code: %d",
                      name, (int)pid, code);

            return SELVA_EGENERAL;
        }
    } else if (WIFSIGNALED(status)) {
        int termsig = WTERMSIG(status);

        SELVA_LOG(SELVA_LOGL_ERR,
                  "%s child %d killed by signal SIG%s (%s)%s",
                  name, (int)pid, sigstr_abbrev(termsig), sigstr_descr(termsig),
                  (WCOREDUMP(status)) ? " (core dumped)" : NULL);

        return SELVA_EGENERAL;
    } else {
        SELVA_LOG(SELVA_LOGL_ERR,
                  "%s child %d terminated abnormally",
                  name, pid);

        return SELVA_EGENERAL;
    }

    return 0;
}

static void handle_signal(struct event *ev, void *arg __unused)
{
    struct evl_siginfo esig;
    int signo;
    int status;

    if (evl_read_sigfd(&esig, ev->fd)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to read sigfd");
        return;
    }

    signo = esig.esi_signo;

    switch (signo) {
    case SIGCHLD:
        if (waitpid(esig.esi_pid, &status, 0) != -1) {
            if (esig.esi_pid == save_pid) {
                int err;

                err = handle_child_status(esig.esi_pid, status, "SDB");
                if (!err) {
                    /* FIXME last_good isn't necessarily the right file. */
                    handle_last_good_async();
                    selva_db_is_dirty = 0;
                }

                selva_db_dump_state = SELVA_DB_DUMP_NONE;
                save_pid = 0;
                save_sdb_eid = 0;
            } else {
                (void)handle_child_status(esig.esi_pid, status, "Unknown");
            }
        }
        break;
    default:
        SELVA_LOG(SELVA_LOGL_WARN, "Received unexpected signal (%d): %s", signo, strsignal(esig.esi_signo));
    }
}

static void setup_sigchld(void)
{
    sigset_t mask;
    int sfd;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    sfd = evl_create_sigfd(&mask);
    if (sfd >= 0) {
        evl_wait_fd(sfd, handle_signal, NULL, NULL, NULL);
    }
}

static int dump_load(struct selva_io *io)
{
    struct SelvaHierarchy *tmp_hierarchy = main_hierarchy;
    int err = 0;

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
    handle_last_good_sync(); /* RFE This is a bit heavy and we could just extract the info from `io`. */
    return err;
}

static int dump_save_async(const char *filename)
{
    pid_t pid;
    int err;

    if (save_pid) {
        /* Already saving */
        return SELVA_EINPROGRESS;
    }

    save_sdb_eid = selva_replication_incomplete_sdb(filename);

    pid = fork();
    if (pid == 0) {
        const enum selva_io_flags flags = SELVA_IO_FLAGS_WRITE;
        struct selva_io io;

        selva_db_dump_state = SELVA_DB_DUMP_IS_CHILD;

        err = selva_io_init(&io, filename, flags);
        if (err) {
            return err;
        }

        Hierarchy_RDBSave(&io, main_hierarchy);
        selva_io_end(&io);

        exit(0);
    } else if (pid < 0) {
        save_sdb_eid = 0;
        return SELVA_EGENERAL;
    }

    selva_db_dump_state = SELVA_DB_DUMP_ACTIVE_CHILD;
    save_pid = pid;

    return 0;
}

static int dump_save_sync(const char *filename)
{
    const enum selva_io_flags flags = SELVA_IO_FLAGS_WRITE;
    struct selva_io io;
    int err;

    if (save_pid) {
        /* Already saving */
        return SELVA_EINPROGRESS;
    }

    err = selva_io_init(&io, filename, flags);
    if (err) {
        return err;
    }

    Hierarchy_RDBSave(&io, main_hierarchy);
    selva_io_end(&io);

    handle_last_good_sync();
    selva_db_is_dirty = 0;

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

        sdb_name(filename, SDB_NAME_MIN_BUF_SIZE, NULL, (uint64_t)ts_monorealtime_now());

        err = dump_save_async(filename);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to autosave: %s", selva_strerror(err));
        }
    }
}

int dump_load_default_sdb(void)
{
    struct selva_io io;
    int err;

    err = selva_io_open_last_good(&io);
    if (err == SELVA_ENOENT) {
        return 0;
    } else if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to open the last good SDB");
        return err;
    }

    err = dump_load(&io);
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to load the last good SDB: %s",
                  selva_strerror(err));
        return err;
    }

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

    const enum selva_io_flags flags = SELVA_IO_FLAGS_READ;
    struct selva_io io;

    err = selva_io_init(&io, selva_string_to_str(argv[ARGV_FILENAME], NULL), flags);
    if (err) {
        selva_send_errorf(resp, SELVA_EGENERAL, "Failed to open the dump file");
        return;
    }

    err = dump_load(&io);
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

    err = dump_save_async(selva_string_to_str(argv[ARGV_FILENAME], NULL));
    if (err) {
        selva_send_errorf(resp, err, "Save failed");
        return;
    }

    /*
     * TODO async response using a stream.
     * TODO queue or dedup saves.
     */
    selva_send_ll(resp, 1);
}

static void dump_on_exit(int code, void *)
{
    char filename[SDB_NAME_MIN_BUF_SIZE];
    int err;

    if (code != 0 || !selva_db_is_dirty) {
        return;
    }

    SELVA_LOG(SELVA_LOGL_INFO, "Dumping before exit...");
    sdb_name(filename, SDB_NAME_MIN_BUF_SIZE, NULL, (uint64_t)ts_monorealtime_now());

    err = dump_save_sync(filename);
    if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Dump on exit failed: %s", selva_strerror(err));
    }
}

static int dump_onload(void) {
    selva_mk_command(CMD_LOAD_ID, SELVA_CMD_MODE_MUTATE, "load", load_db_cmd);
    selva_mk_command(CMD_SAVE_ID, SELVA_CMD_MODE_PURE, "save", save_db_cmd);

    setup_sigchld();

    if (on_exit(dump_on_exit, NULL)) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Can't register an exit function");
        return SELVA_ENOBUFS;
    }

    return 0;
}
SELVA_ONLOAD(dump_onload);
