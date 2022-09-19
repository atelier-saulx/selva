#include <stdio.h>
#include <stdarg.h>
#include "selva.h"

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
