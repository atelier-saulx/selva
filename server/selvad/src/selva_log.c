/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "selva_log.h"

static FILE *log_stream;
static enum selva_log_level selva_log_level = SELVA_LOGL_INFO;

/* FIXME debug log */
#if 0
#if __APPLE__ && __MACH__
extern struct _selva_dyndebug_msg __start_dbg_msg __asm("section$start$__DATA$dbg_msg");
extern struct _selva_dyndebug_msg __stop_dbg_msg __asm("section$end$__DATA$dbg_msg");
#else
extern struct _selva_dyndebug_msg __start_dbg_msg;
extern struct _selva_dyndebug_msg __stop_dbg_msg;
#endif
#endif

static int use_colors;
static char *colors[] = {
    [SELVA_LOGL_CRIT] = "\033[0;31m", /* red */
    [SELVA_LOGL_ERR] = "\033[0;31m", /* red */
    [SELVA_LOGL_WARN] = "\033[0;33m", /* yellow */
    [SELVA_LOGL_INFO] = "\033[0;32m", /* green */
    [SELVA_LOGL_DBG] = "\033[0;34m", /* blue */
};

void selva_log(enum selva_log_level level, const char * restrict where, const char * restrict func, const char * restrict fmt, ...) {
    va_list args;

    if (level > selva_log_level) {
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

/* FIXME dbgmsg */
/* FIXME log level */
#if 0
static void toggle_dbgmsg(const char * cfg) {
    struct _selva_dyndebug_msg *msg_opt = &__start_dbg_msg;
    struct _selva_dyndebug_msg *stop = &__stop_dbg_msg;
    char strbuf[80];
    char *file;
    char *line;

    if (msg_opt == stop) {
        return;
    }

    snprintf(strbuf, sizeof(strbuf), "%s", cfg);
    file = strbuf;
    line = strchr(strbuf, ':');

    if (line) {
        line[0] = '\0';
        line++;
    }

    while (msg_opt < stop) {
        if (strcmp(file, msg_opt->file) == 0) {
            if (line && *line != '\0') {
                char msgline[12];

                snprintf(msgline, sizeof(msgline), "%d", msg_opt->line);
                if (strcmp(line, msgline) != 0) {
                    goto next;
                }
            }
            msg_opt->flags ^= 1;
        }

next:
        msg_opt++;
    }
}

int SelvaLog_LevelCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc == 1) {
        return RedisModule_ReplyWithStringBuffer(ctx, (char []){ selva_log_level }, 1);
    } else if (argc == 2) {
        size_t level_len;
        const char *level_str = RedisModule_StringPtrLen(argv[1], &level_len);
        const enum selva_log_level level = (enum selva_log_level)(*level_str);
        const enum selva_log_level old_level = selva_log_level;

        if (level_len != 1 || level < SELVA_LOGL_CRIT || level >= SELVA_LOGL_DBG) {
            return replyWithSelvaError(ctx, SELVA_EINVAL);
        }

        selva_log_level = level;
        return RedisModule_ReplyWithStringBuffer(ctx, (char []){ old_level }, 1);
    } else {
        return RedisModule_WrongArity(ctx);
    }
}

int SelvaLog_DbgCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    toggle_dbgmsg(RedisModule_StringPtrLen(argv[1], NULL));

    return RedisModule_ReplyWithLongLong(ctx, 1);
}

int SelvaLog_DbgListCommand(RedisModuleCtx *ctx, RedisModuleString **argv __unused, int argc) {
    RedisModule_AutoMemory(ctx);
    struct _selva_dyndebug_msg * msg_opt = &__start_dbg_msg;
    struct _selva_dyndebug_msg * stop = &__stop_dbg_msg;

    if (argc != 1) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModule_ReplyWithArray(ctx, stop - msg_opt);

    if (msg_opt == stop) {
        return REDISMODULE_OK;
    }

    while (msg_opt < stop) {
        RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "%s:%d: %d", msg_opt->file, msg_opt->line, msg_opt->flags));
        msg_opt++;
    }

    return REDISMODULE_OK;
}

static int SelvaLog_OnLoad(RedisModuleCtx *ctx) {
    /*
     * This message is here just to initialize the section used for debug logs.
     */
    SELVA_LOG_DBG("Loading selva_log");

    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.log.level", SelvaLog_LevelCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.log.dbg", SelvaLog_DbgCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.log.dbglist", SelvaLog_DbgListCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(SelvaLog_OnLoad);
#endif

__attribute__((constructor(101))) static void init_selva_log(void)
{
    log_stream = stderr;
    use_colors = isatty(fileno(log_stream));
}
