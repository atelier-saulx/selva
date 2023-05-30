/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "cdefs.h"
#include "endian.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_proto.h"

/**
 * Supported final types.
 */
enum type_specifier {
    TYPE_void = 0,
    TYPE_char, /* unsigned on some architectures */
    TYPE_short,
    TYPE_int,
    TYPE_long,
    TYPE_longlong,
    TYPE_intmax_t,
    TYPE_ssize_t,
    TYPE_ptrdiff_t,
    TYPE_uchar,
    TYPE_ushort,
    TYPE_uint,
    TYPE_ulong,
    TYPE_ulonglong,
    TYPE_uintmax_t,
    TYPE_size_t,
    TYPE_float,
    TYPE_double,
    TYPE_longdouble,
    TYPE_string,
    TYPE_pointer, /* read as a string */
};

/**
 * Length specifiers as in scanf.
 */
enum length_specifier {
    LENGTH_none = 0,
    LENGTH_hh,
    LENGTH_h,
    LENGTH_l,
    LENGTH_ll,
    LENGTH_j,
    LENGTH_z,
    LENGTH_t,
    LENGTH_L,
};

/**
 * Supported signed integer types from length.
 */
static enum type_specifier type_map_d[LENGTH_L + 1] = {
    [LENGTH_none] = TYPE_int,
    [LENGTH_hh] = TYPE_char,
    [LENGTH_h] = TYPE_short,
    [LENGTH_l] = TYPE_long,
    [LENGTH_ll] = TYPE_longlong,
    [LENGTH_j] = TYPE_intmax_t,
    [LENGTH_z] = TYPE_ssize_t,
    [LENGTH_t] = TYPE_ptrdiff_t,
};

/**
 * Supported unsigned integer types from length.
 */
static enum type_specifier type_map_u[LENGTH_L + 1] = {
    [LENGTH_none] = TYPE_uint,
    [LENGTH_hh] = TYPE_uchar,
    [LENGTH_h] = TYPE_ushort,
    [LENGTH_l] = TYPE_ulong,
    [LENGTH_ll] = TYPE_ulonglong,
    [LENGTH_j] = TYPE_uintmax_t,
    [LENGTH_z] = TYPE_size_t,
};

/**
 * Supported floating-point types from length.
 */
static enum type_specifier type_map_f[LENGTH_L + 1] = {
    [LENGTH_none] = TYPE_float,
    [LENGTH_l] = TYPE_double,
    [LENGTH_L] = TYPE_longdouble,
};

/**
 * All the supported types are sent over just a few selva_proto value types.
 */
static enum selva_proto_data_type type_to_selva_proto_type_map[TYPE_pointer + 1] = {
    [TYPE_char] = SELVA_PROTO_LONGLONG,
    [TYPE_short] = SELVA_PROTO_LONGLONG,
    [TYPE_int] = SELVA_PROTO_LONGLONG,
    [TYPE_long] = SELVA_PROTO_LONGLONG,
    [TYPE_longlong] = SELVA_PROTO_LONGLONG,
    [TYPE_intmax_t] = SELVA_PROTO_LONGLONG,
    [TYPE_ssize_t] = SELVA_PROTO_LONGLONG,
    [TYPE_ptrdiff_t] = SELVA_PROTO_LONGLONG,
    [TYPE_uchar] = SELVA_PROTO_LONGLONG,
    [TYPE_ushort] = SELVA_PROTO_LONGLONG,
    [TYPE_ulong] = SELVA_PROTO_LONGLONG,
    [TYPE_ulonglong] = SELVA_PROTO_LONGLONG,
    [TYPE_uintmax_t] = SELVA_PROTO_LONGLONG,
    [TYPE_size_t] = SELVA_PROTO_LONGLONG,
    [TYPE_float] = SELVA_PROTO_DOUBLE,
    [TYPE_double] = SELVA_PROTO_DOUBLE,
    [TYPE_longdouble] = SELVA_PROTO_DOUBLE,
    [TYPE_string] = SELVA_PROTO_STRING,
    [TYPE_pointer] = SELVA_PROTO_STRING,
};

struct placeholder_state {
    int width;
    int precision;
    enum length_specifier length;
    enum type_specifier type;
};

static void reset_placeholder(struct placeholder_state *ps)
{
    ps->width = -1;
    ps->precision = 0;
    ps->length = LENGTH_none;
    ps->type = TYPE_void;
}

static char *parse_width(const char *fmt, struct placeholder_state *ps)
{
    char *end;

    ps->width = strtol(fmt, &end, 10);
    return end;
}

/*
 * TODO Support reading values into an array
 * E.g. int x[3] <= %3d
 * TODO Support variable length arrays
 */
int selva_proto_scanf(struct finalizer * restrict fin, const void * restrict buf, size_t szbuf, const char * restrict fmt, ...)
{
    va_list args;
    size_t buf_i = 0;
    int on_placeholder = 0;
    struct placeholder_state ps;
    int postponed_array_end = 0;
    int n = 0;

    reset_placeholder(&ps);

    if (szbuf == 0) {
        return 0;
    }

    fmt--;
    va_start(args, fmt);
    while (*(++fmt) != '\0') {
        char ch = *fmt;

        if (on_placeholder) {
            switch (ch) {
            /* precision specifier (pointers) */
            case '.':
                if (*(++fmt) == '*') {
                    ps.precision = -1;
                } else {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            /* optional width specifiers */
            case '0' ... '9':
                fmt = parse_width(fmt, &ps);
                if (isdigit(*fmt)) {
                    return SELVA_PROTO_EINVAL;
                }
                fmt--;
                break;
            /* optional length specifiers */
            case 'l':
                if (ps.length == LENGTH_none) {
                    ps.length = LENGTH_l;
                } else if (ps.length == LENGTH_l) {
                    ps.length = LENGTH_ll;
                } else {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            case 'h':
                if (ps.length == LENGTH_none) {
                    ps.length = LENGTH_h;
                } else if (ps.length == LENGTH_h) {
                    ps.length == LENGTH_hh;
                } else {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            case 'j':
                if (ps.length == LENGTH_none) {
                    ps.length = LENGTH_j;
                } else {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            case 'z':
                if (ps.length == LENGTH_none) {
                    ps.length = LENGTH_z;
                } else {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            case 't':
                if (ps.length == LENGTH_none) {
                    ps.length = LENGTH_t;
                } else {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            case 'L':
                if (ps.length == LENGTH_none) {
                    ps.length = LENGTH_L;
                } else {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            /* type specifiers */
            case 'i':
            case 'd':
                ps.type = type_map_d[ps.length];
                if (ps.type == TYPE_void) {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            case 'u':
                ps.type = type_map_u[ps.length];
                if (ps.type == TYPE_void) {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            case 'f':
                ps.type = type_map_f[ps.length];
                if (ps.type == TYPE_void) {
                    return SELVA_PROTO_EINVAL;
                }
                break;
            case 'c':
                if (ps.length != LENGTH_none) {
                    return SELVA_PROTO_EINVAL;
                }
                ps.type = TYPE_char;
                break;
            case 's':
                if (ps.length != LENGTH_none) {
                    return SELVA_PROTO_EINVAL;
                }
                ps.type = TYPE_string;
                break;
            case 'p':
                if (ps.length != LENGTH_none) {
                    return SELVA_PROTO_EINVAL;
                }
                ps.type = TYPE_pointer;
                break;
            default:
                return SELVA_PROTO_EINVAL;
            }

            if (ps.type != TYPE_void) {
                enum selva_proto_data_type found_type;
                size_t data_len;
                int off;

                off = selva_proto_parse_vtype(buf, szbuf, buf_i, &found_type, &data_len);
                if (off <= 0) {
                    return off;
                }

                if (found_type != type_to_selva_proto_type_map[ps.type]) {
                    return SELVA_PROTO_EBADMSG;
                }

                if (found_type == SELVA_PROTO_LONGLONG) {
                    long long v;

                    memcpy(&v, (char *)buf + buf_i + offsetof(struct selva_proto_longlong, v), sizeof(v));
                    v = le64toh(v);
                    switch (ps.type) {
                    case TYPE_char:
                        *va_arg(args, char *) = v;
                        break;
                    case TYPE_short:
                        *va_arg(args, short *) = v;
                        break;
                    case TYPE_int:
                        *va_arg(args, int *) = v;
                        break;
                    case TYPE_long:
                        *va_arg(args, long *) = v;
                        break;
                    case TYPE_longlong:
                        *va_arg(args, long long *) = v;
                         break;
                    case TYPE_intmax_t:
                        *va_arg(args, intmax_t *) = v;
                         break;
                    case TYPE_ssize_t:
                        *va_arg(args, ssize_t *) = v;
                         break;
                    case TYPE_ptrdiff_t:
                        *va_arg(args, ptrdiff_t *) = v;
                         break;
                    case TYPE_uchar:
                        *va_arg(args, unsigned char *) = v;
                         break;
                    case TYPE_ushort:
                        *va_arg(args, unsigned short *) = v;
                         break;
                    case TYPE_uint:
                        *va_arg(args, unsigned int *) = v;
                         break;
                    case TYPE_ulong:
                        *va_arg(args, unsigned long *) = v;
                         break;
                    case TYPE_ulonglong:
                        *va_arg(args, unsigned long long *) = v;
                         break;
                    case TYPE_uintmax_t:
                        *va_arg(args, uintmax_t *) = v;
                         break;
                    case TYPE_size_t:
                        *va_arg(args, size_t *) = v;
                         break;
                    default:
                         __builtin_unreachable();
                         /* FIXME C23 would allow this */
#if 0
                         unreachable();
#endif
                    }
                } else if (found_type == SELVA_PROTO_DOUBLE) {
                    char vbuf[8];
                    double v;

                    memcpy(vbuf, (char *)buf + buf_i + offsetof(struct selva_proto_double, v), sizeof(vbuf));
                    v = ledoubletoh(vbuf);
                    switch (ps.type) {
                    case TYPE_float:
                        *va_arg(args, float *) = v;
                        break;
                    case TYPE_double:
                        *va_arg(args, double *) = v;
                        break;
                    case TYPE_longdouble:
                        *va_arg(args, long double *) = v;
                        break;
                    default:
                        __builtin_unreachable();
                        /* FIXME C23 would allow this */
#if 0
                        unreachable();
#endif
                    }
                } else if (found_type == SELVA_PROTO_STRING) {
                    const char *str = (char *)buf + buf_i + offsetof(struct selva_proto_string, data);
                    typeof_field(struct selva_proto_string, flags) flags;
                    typeof_field(struct selva_proto_string, bsize) len;

                    memcpy(&flags, (char *)buf + buf_i + offsetof(struct selva_proto_string, flags), sizeof(flags));
                    memcpy(&len, (char *)buf + buf_i + offsetof(struct selva_proto_string, bsize), sizeof(len));

                    if ((ps.precision == -1 || ps.width >= 0) && (flags & SElVA_PROTO_STRING_FDEFLATE)) {
                        /*
                         * A compressed string must be captured as a selva_string with %p.
                         */
                        return SELVA_EINTYPE;
                    }

                    if (ps.precision == -1) {
                        /* pass as a pointer */
                        size_t *p_size = va_arg(args, size_t *);
                        const void **p = va_arg(args, const void **);

                        *p_size = ps.width >= 0 && (typeof(len))ps.width < len ? (size_t)ps.width : len;
                        *p = str;
                    } else if (ps.width >= 0) {
                        /* copy to a buffer */
                        char *dest = va_arg(args, char *);
                        size_t copy_size = min((size_t)ps.width, (size_t)len);

                        memcpy(dest, str, copy_size);
                        dest[copy_size] = '\0';
                    } else {
                        /*
                         * no width or precision specifier, assume selva_string.
                         *
                         * This would be a great case for %s -> selva_string but
                         * unfortunately there is no way to make
                         * `__attribute__((format(scanf...` work with that,
                         * hence we actually expect %p to be used.
                         */
                        struct selva_string *s;

                        s = selva_string_create(str, len, (flags & SElVA_PROTO_STRING_FDEFLATE) ? SELVA_STRING_COMPRESS : 0);
                        selva_string_auto_finalize(fin, s);
                        *va_arg(args, struct selva_string **) = s;
                    }
                }

#if 0
                buf_i += off;
#endif
                n++;

                /*
                 * Reset.
                 */
                on_placeholder = 0;
            }
        } else {
            if (isspace(ch)) {
                /* NOP */
            } else if (ch == ',') {
                enum selva_proto_data_type found_type;
                size_t data_len;
                int off;

                off = selva_proto_parse_vtype(buf, szbuf, buf_i, &found_type, &data_len);
                if (off <= 0) {
                    return off;
                }

                buf_i += off;
            } else if (ch == '{') {
                enum selva_proto_data_type found_type;
                size_t data_len;
                int off;
                uint8_t flags;

                off = selva_proto_parse_vtype(buf, szbuf, buf_i, &found_type, &data_len);
                if (off <= 0) {
                    return off;
                }

                if (found_type != SELVA_PROTO_ARRAY) {
                    return SELVA_PROTO_EBADMSG;
                }

                flags = *((uint8_t *)buf + buf_i + offsetof(struct selva_proto_array, flags));
                if (flags & SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH) {
                    postponed_array_end++;
                }

                buf_i += off;
            } else if (ch == '}' && postponed_array_end > 0) {
                enum selva_proto_data_type found_type;
                size_t data_len;
                int off;

                off = selva_proto_parse_vtype(buf, szbuf, buf_i, &found_type, &data_len);
                if (off <= 0) {
                    return off;
                }

                if (found_type != SELVA_PROTO_ARRAY_END) {
                    return SELVA_PROTO_EBADMSG;
                }

                postponed_array_end--;
                buf_i += off;
            } else if (ch == '%') {
                on_placeholder = 1;
                reset_placeholder(&ps);
            }
        }
    }
    va_end(args);

    return n;
}
