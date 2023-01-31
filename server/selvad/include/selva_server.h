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

/**
 * Command function.
 * @param resp contains information needed to build the response.
 * @param buf is a pointer to the incoming message.
 * @param len is the length of the incoming message in bytes.
 */
typedef void (*selva_cmd_function)(struct selva_server_response_out *resp, const void *buf, size_t len);

SELVA_SERVER_EXPORT(int, selva_mk_command, int nr, const char *name, selva_cmd_function cmd);
#define SELVA_MK_COMMAND(nr, cmd) \
    selva_mk_command(nr, #cmd, cmd)

SELVA_SERVER_EXPORT(size_t, selva_resp_to_str, struct selva_server_response_out *resp, char *buf, size_t bsize);

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
SELVA_SERVER_EXPORT(int, selva_send_ll, struct selva_server_response_out *resp, long long value);
SELVA_SERVER_EXPORT(int, selva_send_str, struct selva_server_response_out *resp, const char *str, size_t len);
SELVA_SERVER_EXPORT(int, selva_send_strf, struct selva_server_response_out *resp, const char *fmt, ...);
SELVA_SERVER_EXPORT(int, selva_send_string, struct selva_server_response_out *resp, const struct selva_string *s);
SELVA_SERVER_EXPORT(int, selva_send_bin, struct selva_server_response_out *resp, const void *b, size_t len);

/**
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
 * Send a replication header.
 */
SELVA_SERVER_EXPORT(int, selva_send_replication, struct selva_server_response_out *resp, int8_t cmd, const void *data, size_t bsize);

/**
 * Run command.
 */
SELVA_SERVER_EXPORT(void, selva_server_run_cmd, int8_t cmd_id, void *msg, size_t msg_size);

#define _import_selva_server(apply) \
    apply(selva_mk_command) \
    apply(selva_resp_to_str) \
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
    apply(selva_send_str) \
    apply(selva_send_strf) \
    apply(selva_send_string) \
    apply(selva_send_bin) \
    apply(selva_send_array) \
    apply(selva_send_array_end) \
    apply(selva_send_replication) \
    apply(selva_server_run_cmd)

#define _import_selva_server1(f) \
    evl_import(f, "mod_server.so");

/**
 * Import all symbols from selva_sever.h.
 */
#define import_selva_server() \
    _import_selva_server(_import_selva_server1)
