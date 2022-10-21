/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
/*
 * NOTE
 * No guards needed because this file is intended to be included only by
 * selva.h.
 */

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

void selva_log(enum selva_log_level level, const char * restrict where, const char * restrict func, const char * restrict fmt, ...) __attribute__((format(printf, 4, 5)));

#define _SELVA_LOG_WHERESTR (__FILE__ ":" S__LINE__)
#define _SELVA_LOG(level, where, fmt, ...) \
    selva_log(level, where, fmt, ##__VA_ARGS__)

/**
 * Print to the server logs.
 * @param level is one of the log level listed in `enum selva_log_level`.
 * @param fmt is a standard printf format string.
 */
#define SELVA_LOG(level, fmt, ...) do { \
    _SELVA_LOG(level, _SELVA_LOG_WHERESTR, __func__, fmt, ##__VA_ARGS__); \
} while (0)


#if __APPLE__ && __MACH__
#define __dbg_msg_section __section("__DATA,dbg_msg")
#else
#define __dbg_msg_section __section("dbg_msg")
#endif

#define SELVA_LOG_DBG(fmt, ...) do { \
    static struct _selva_dyndebug_msg _dbg_msg __dbg_msg_section __used = { .flags = 0, .file = __FILE__, .line = __LINE__ }; \
    if (_dbg_msg.flags & 1) { \
        _SELVA_LOG(SELVA_LOGL_DBG, _SELVA_LOG_WHERESTR, __func__, fmt, ##__VA_ARGS__); \
    } \
} while (0)
