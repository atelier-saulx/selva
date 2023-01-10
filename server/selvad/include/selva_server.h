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

/**
 * Send buffer as a part of the response resp.
 * The data is sent as is framed within selva_proto frames. Typically the buf
 * should point to one of the selva_proto value structs. The buffer might be
 * split into multiple frames and the receiver must reassemble the data. All
 * data within a sequence will be always delivered in the sending order.
 * @returns Return bytes sent; Otherwise an error.
 */
ssize_t server_send_buf(struct selva_server_response_out *restrict resp, const void *restrict buf, size_t len);

/**
 * Flush the response buffer.
 */
int server_send_flush(struct selva_server_response_out *restrict res);

/**
 * End sending a response.
 * Finalizes the response sequence.
 */
int server_send_end(struct selva_server_response_out *restrict res);

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

#define _import_selva_server(apply) \
    apply(selva_mk_command) \
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
    apply(selva_send_array_end)

#define _import_selva_server1(f) \
    evl_import(f, "mod_server.so");

/**
 * Import all symbols from selva_sever.h.
 */
#define import_selva_server() \
    _import_selva_server(_import_selva_server1)
