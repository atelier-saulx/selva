/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <fnmatch.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "util/selva_string.h"
#include "selva_log.h"

static FILE *log_stream;
static enum selva_log_level selva_log_level = SELVA_LOGL_INFO;

static int use_colors;
static char *colors[] = {
    [SELVA_LOGL_CRIT] = "\033[0;31m", /* red */
    [SELVA_LOGL_ERR] = "\033[0;31m", /* red */
    [SELVA_LOGL_WARN] = "\033[0;33m", /* yellow */
    [SELVA_LOGL_INFO] = "\033[0;32m", /* green */
    [SELVA_LOGL_DBG] = "\033[0;34m", /* blue */
};

static struct selva_string *dbg_pattern;
static const char *dbg_pattern_str;

/**
 * Test if `dbg_pattern` matches with `where`
 * - `*` wildcard
 * - `(pattern-list)` pattern|pattern|...
 * - `?(pattern-list)` The pattern matches if zero or one occurrences of any of the patterns match
 * - `*(pattern-list)` The pattern matches if zero or more occurrences of any of the patterns match
 * - `+(pattern-list)` The pattern matches if one or more occurrences of any of the patterns match
 * - `@(pattern-list)` The pattern matches if exactly one occurrence of any of the patterns match
 * - `!(pattern-list)` The pattern matches if the input string cannot be matched with any of the patterns
 */
static int is_dbg_match(const char *where)
{
    if (!dbg_pattern) {
        return 0;
    }

    return !fnmatch(dbg_pattern_str, where, FNM_EXTMATCH);
}

void selva_log(enum selva_log_level level, const char * restrict where, const char * restrict func, const char * restrict fmt, ...) {
    va_list args;

    if (level > selva_log_level && !(level == SELVA_LOGL_DBG && is_dbg_match(where))) {
        return;
    }

    va_start(args, fmt);
    if (use_colors) fprintf(log_stream, "%s", colors[level]);
    fprintf(log_stream, "%s:%s: ", where, func);
    if (use_colors) fprintf(log_stream, "\033[0m"); /* Reset color. */
    vfprintf(log_stream, fmt, args);
    if (fmt[strlen(fmt) - 1] != '\n') {
        fputc('\n', log_stream);
    }
    va_end(args);
}

enum selva_log_level selva_log_get_level(void) {
    return selva_log_level;
}

enum selva_log_level selva_log_set_level(enum selva_log_level new_level) {
    const enum selva_log_level tmp = selva_log_level;

    selva_log_level = new_level;
    return tmp;
}

void selva_log_set_dbgpattern(struct selva_string *s)
{
    if (dbg_pattern) {
        selva_string_free(dbg_pattern);
    }
    dbg_pattern = selva_string_dup(s, 0);
    dbg_pattern_str = selva_string_to_str(dbg_pattern, NULL);
}

__attribute__((constructor(101))) static void init_selva_log(void)
{
    log_stream = stderr;
    use_colors = isatty(fileno(log_stream));
}
