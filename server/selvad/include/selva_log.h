/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"

struct selva_string;

#ifndef _EVL_MODULE_H_
const char *evl_modname __attribute__((__common__));
#endif

/**
 * Log levels.
 */
enum selva_log_level {
    /**
     * Critical non-recoverable error.
     * The server should generally call abort() after printing one of these errors.
     */
    SELVA_LOGL_CRIT = '0',
    /**
     * Serious error.
     * The server can still keep running stable but even a memory leak might
     * have occurred.
     */
    SELVA_LOGL_ERR = '1',
    /**
     * Warning.
     * Something is not right but the code is built to handle this error
     * gracefully and the user(s) won't notice any anomaly.
     */
    SELVA_LOGL_WARN = '2',
    /**
     * Information.
     * Version information, configuration log, large task running, etc.
     */
    SELVA_LOGL_INFO = '3',
    /**
     * Debug messages.
     * Debug messages are disabled by default and can be enabled case by case.
     */
    SELVA_LOGL_DBG = '4',
};

/**
 * Dynamic configuration of debug messages.
 */
struct _selva_dyndebug_msg {
    int flags; /*!< Control flags. 1 = enabled. */
    int line; /*!< Source code line of this message in `file`. */
    const char * file; /*!< Source code file of this message. */
};

#if EVL_MAIN
void selva_log(enum selva_log_level level, const char * restrict modname, const char * restrict where, const char * restrict func, const char * restrict fmt, ...) __attribute__((__visibility__("default"), format(printf, 5, 6)));
#else
void (*selva_log)(enum selva_log_level level, const char * restrict modname, const char * restrict where, const char * restrict func, const char * restrict fmt, ...) __attribute__((__common__, format(printf, 5, 6)));
#endif
EVL_EXPORT(enum selva_log_level, selva_log_get_level, void);
EVL_EXPORT(enum selva_log_level, selva_log_set_level, enum selva_log_level new_level);
EVL_EXPORT(void, selva_log_set_dbgpattern, const char *str, size_t len);

#define _SELVA_LOG_WHERESTR (__FILE__ ":" S__LINE__)
#define _SELVA_LOG(level, where, fmt, ...) \
    selva_log(level, evl_modname, where, fmt __VA_OPT__(,) __VA_ARGS__)

/**
 * Print to the server logs.
 * @param level is one of the log level listed in `enum selva_log_level`.
 * @param fmt is a standard printf format string.
 */
#define SELVA_LOG(level, fmt, ...) do { \
    _SELVA_LOG(level, _SELVA_LOG_WHERESTR, __func__, fmt __VA_OPT__(,) __VA_ARGS__); \
} while (0)
