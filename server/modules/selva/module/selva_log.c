#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "selva.h"

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

int selva_toggle_dbgmsg(char * cfg) {
    struct _selva_dyndebug_msg * msg_opt = &__start_dbg_msg;
    struct _selva_dyndebug_msg * stop = &__stop_dbg_msg;
    char strbuf[80];
    char * file = strbuf;
    char * line;

    if (msg_opt == stop) {
        return SELVA_EINVAL;
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

    return 0;
}
