#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "redismodule.h"
#include "selva.h"
#include "selva_onload.h"

extern struct _selva_dyndebug_msg __start_dbg_msg;
extern struct _selva_dyndebug_msg __stop_dbg_msg;

void selva_log(enum selva_log_level level, const char * restrict where, const char * restrict func, const char * restrict fmt, ...) {
    FILE *log_stream = stderr;
    va_list args;

    /* TODO Utilize log level */

    va_start(args, fmt);
    fprintf(log_stream, "%s:%s: ", where, func);
    vfprintf(log_stream, fmt, args);
    if (fmt[strlen(fmt) - 1] != '\n') {
        fputc('\n', log_stream);
    }
    va_end(args);
}

static void toggle_dbgmsg(const char * cfg) {
    struct _selva_dyndebug_msg * msg_opt = &__start_dbg_msg;
    struct _selva_dyndebug_msg * stop = &__stop_dbg_msg;
    char strbuf[80];
    char * file = strbuf;
    char * line;

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

    if (msg_opt == stop) {
        return REDISMODULE_OK;
    }

    RedisModule_ReplyWithArray(ctx, stop - msg_opt);
    while (msg_opt < stop) {
        RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "%s:%d: %d", msg_opt->file, msg_opt->line, msg_opt->flags));
        msg_opt++;
    }

    return REDISMODULE_OK;
}

static int SelvaLog_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.log.dbg", SelvaLog_DbgCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.log.dbglist", SelvaLog_DbgListCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(SelvaLog_OnLoad);
