/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"
#include "../commands.h"

#if SELVA_SERVER_MAIN
#define SELVA_SERVER_EXPORT(_ret_, _fun_name_, ...) _ret_ _fun_name_(__VA_ARGS__) EVL_EXTERN
#else
#define SELVA_SERVER_EXPORT(_ret_, _fun_name_, ...) _ret_ (*_fun_name_)(__VA_ARGS__) EVL_COMMON
#endif

enum selva_proto_data_type;
struct selva_server_response_out;
struct selva_string;

enum selva_cmd_mode {
    /**
     * The command only reads objects.
     * The pure attribute prohibits a command from modifying the state of the
     * database that is observable by means other than inspecting the command's
     * response. However, commands declared with the pure attribute can safely
     * read and modify the database objects in a way that does not affect their
     * observable state.
     */
    SELVA_CMD_MODE_PURE = 0x01,
    /**
     * THe command modifies objects.
     * The command makes changes that may affect the the observable state of the
     * database objects as well as their serialized state.
     */
    SELVA_CMD_MODE_MUTATE = 0x02,
};

/**
 * Command function.
 * @param resp contains information needed to build the response.
 * @param buf is a pointer to the incoming message.
 * @param len is the length of the incoming message in bytes.
 */
typedef void (*selva_cmd_function)(struct selva_server_response_out *resp, const void *buf, size_t len);

/**
 * Set the server to read-only mode.
 * This operation is irreversible at runtime.
 * A read-only server can only invoke commands marked as `SELVA_CMD_MODE_PURE`
 * through the exposed socket.
 */
SELVA_SERVER_EXPORT(void, selva_server_set_readonly, void);

SELVA_SERVER_EXPORT(int, selva_mk_command, int nr, enum selva_cmd_mode mode, const char *name, selva_cmd_function cmd);
#define SELVA_MK_COMMAND(nr, mode, cmd) \
    selva_mk_command(nr, mode, #cmd, cmd)

SELVA_SERVER_EXPORT(size_t, selva_resp_to_str, struct selva_server_response_out *resp, char *buf, size_t bsize);

SELVA_SERVER_EXPORT(int, selva_resp_to_cmd_id, struct selva_server_response_out *resp);

/**
 * Get start ts of the command execution.
 */
SELVA_SERVER_EXPORT(int64_t, selva_resp_to_ts, struct selva_server_response_out *resp);

/**
 * Flush the response buffer.
 */
SELVA_SERVER_EXPORT(int, selva_send_flush, struct selva_server_response_out *restrict resp);

/**
 * Start a long standing stream response.
 */
SELVA_SERVER_EXPORT(int, selva_start_stream, struct selva_server_response_out *resp, struct selva_server_response_out **stream_resp_out);

/**
 * Cancel a stream before it has been used.
 * Stream cannot be cancelled after first use. This function is only provided
 * for cancelling streaming immediately after creation due to an error in
 * initializing the actual streaming.
 */
SELVA_SERVER_EXPORT(void, selva_cancel_stream, struct selva_server_response_out *resp, struct selva_server_response_out *stream_resp_out);

/**
 * End sending a response.
 * Finalizes the response sequence.
 */
SELVA_SERVER_EXPORT(int, selva_send_end, struct selva_server_response_out *restrict resp);

/**
 * Send a null value.
 */
SELVA_SERVER_EXPORT(int, selva_send_null, struct selva_server_response_out *resp);

/**
 * Send an error.
 * @param msg_str can be NULL.
 */
SELVA_SERVER_EXPORT(int, selva_send_error, struct selva_server_response_out *resp, int err, const char *msg_str, size_t msg_len);
SELVA_SERVER_EXPORT(int, selva_send_errorf, struct selva_server_response_out *resp, int err, const char *fmt, ...);
SELVA_SERVER_EXPORT(int, selva_send_error_arity, struct selva_server_response_out *resp);

SELVA_SERVER_EXPORT(int, selva_send_double, struct selva_server_response_out *resp, double value);
/**
 * Send long long.
 */
SELVA_SERVER_EXPORT(int, selva_send_ll, struct selva_server_response_out *resp, long long value);
/**
 * Send long long and suggest hex print formatting.
 */
SELVA_SERVER_EXPORT(int, selva_send_llx, struct selva_server_response_out *resp, long long value);
SELVA_SERVER_EXPORT(int, selva_send_str, struct selva_server_response_out *resp, const char *str, size_t len);
SELVA_SERVER_EXPORT(int, selva_send_strf, struct selva_server_response_out *resp, const char *fmt, ...);
SELVA_SERVER_EXPORT(int, selva_send_string, struct selva_server_response_out *resp, const struct selva_string *s);
SELVA_SERVER_EXPORT(int, selva_send_bin, struct selva_server_response_out *resp, const void *b, size_t len);

/**
 * Send an array.
 * If `len` is set negative then selva_proto_send_array_end() should be used to
 * terminate the array.
 * @param len Number if items in the array.
 */
SELVA_SERVER_EXPORT(int, selva_send_array, struct selva_server_response_out *resp, int len);

/**
 * Terminate a variable length array.
 * Call this only if len was set -1.
 */
SELVA_SERVER_EXPORT(int, selva_send_array_end, struct selva_server_response_out *res);

/**
 * Send a replication command.
 */
SELVA_SERVER_EXPORT(int, selva_send_replication_cmd, struct selva_server_response_out *resp, uint64_t eid, int64_t ts, int8_t cmd, const void *data, size_t bsize);

/**
 * Send a replication SDB dump.
 */
SELVA_SERVER_EXPORT(int, selva_send_replication_sdb, struct selva_server_response_out *resp, uint64_t eid, const char *filename);

SELVA_SERVER_EXPORT(int, selva_send_replication_pseudo_sdb, struct selva_server_response_out *resp, uint64_t eid);

/**
 * Run command.
 */
SELVA_SERVER_EXPORT(void, selva_server_run_cmd, int8_t cmd_id, int64_t ts, void *msg, size_t msg_size);

#define _import_selva_server(apply) \
    apply(selva_server_set_readonly) \
    apply(selva_mk_command) \
    apply(selva_resp_to_str) \
    apply(selva_resp_to_cmd_id) \
    apply(selva_resp_to_ts) \
    apply(selva_send_flush) \
    apply(selva_start_stream) \
    apply(selva_cancel_stream) \
    apply(selva_send_end) \
    apply(selva_send_null) \
    apply(selva_send_error) \
    apply(selva_send_errorf) \
    apply(selva_send_error_arity) \
    apply(selva_send_double) \
    apply(selva_send_ll) \
    apply(selva_send_llx) \
    apply(selva_send_str) \
    apply(selva_send_strf) \
    apply(selva_send_string) \
    apply(selva_send_bin) \
    apply(selva_send_array) \
    apply(selva_send_array_end) \
    apply(selva_send_replication_cmd) \
    apply(selva_send_replication_sdb) \
    apply(selva_send_replication_pseudo_sdb) \
    apply(selva_server_run_cmd)

#define _import_selva_server1(f) \
    evl_import(f, "mod_server.so");

/**
 * Import all symbols from selva_sever.h.
 */
#define import_selva_server() \
    _import_selva_server(_import_selva_server1)
