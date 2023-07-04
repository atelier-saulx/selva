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
    [TYPE_uint] = SELVA_PROTO_LONGLONG,
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

static int get_skip_off(const void *buf, size_t szbuf, size_t buf_i)
{
    enum selva_proto_data_type found_type;
    size_t data_len;

    return selva_proto_parse_vtype(buf, szbuf, buf_i, &found_type, &data_len);
}

static const char *parse_specifier(struct placeholder_state *ps, const char *fmt)
{
    do {
        switch (*fmt) {
        /* precision specifier (pointers) */
        case '.':
            if (*(++fmt) == '*') {
                ps->precision = -1;
            } else {
                return NULL;
            }
            break;
        /* optional width specifiers */
        case '0' ... '9':
            fmt = parse_width(fmt, ps);
            if (isdigit(*fmt)) {
                return NULL;
            }
            fmt--;
            break;
        /* optional length specifiers */
        case 'l':
            if (ps->length == LENGTH_none) {
                ps->length = LENGTH_l;
            } else if (ps->length == LENGTH_l) {
                ps->length = LENGTH_ll;
            } else {
                return NULL;
            }
            break;
        case 'h':
            if (ps->length == LENGTH_none) {
                ps->length = LENGTH_h;
            } else if (ps->length == LENGTH_h) {
                ps->length == LENGTH_hh;
            } else {
                return NULL;
            }
            break;
        case 'j':
            if (ps->length == LENGTH_none) {
                ps->length = LENGTH_j;
            } else {
                return NULL;
            }
            break;
        case 'z':
            if (ps->length == LENGTH_none) {
                ps->length = LENGTH_z;
            } else {
                return NULL;
            }
            break;
        case 't':
            if (ps->length == LENGTH_none) {
                ps->length = LENGTH_t;
            } else {
                return NULL;
            }
            break;
        case 'L':
            if (ps->length == LENGTH_none) {
                ps->length = LENGTH_L;
            } else {
                return NULL;
            }
            break;
        /* type specifiers */
        case 'i':
        case 'd':
            ps->type = type_map_d[ps->length];
            if (ps->type == TYPE_void) {
                return NULL;
            }
            break;
        case 'u':
            ps->type = type_map_u[ps->length];
            if (ps->type == TYPE_void) {
                return NULL;
            }
            break;
        case 'f':
            ps->type = type_map_f[ps->length];
            if (ps->type == TYPE_void) {
                return NULL;
            }
            break;
        case 'c':
            if (ps->length != LENGTH_none) {
                return NULL;
            }
            ps->type = TYPE_char;
            break;
        case 's':
            if (ps->length != LENGTH_none) {
                return NULL;
            }
            ps->type = TYPE_string;
            break;
        case 'p':
            if (ps->length != LENGTH_none) {
                return NULL;
            }
            ps->type = TYPE_pointer;
            break;
        default:
            return NULL;
        }

    } while (ps->type == TYPE_void && (*(++fmt) != '\0'));

    return (*fmt == '\0') ? NULL : fmt;
}

#define int64_to_va_arg(type, v) \
    do { \
        switch ((type)) { \
        case TYPE_char: \
            *va_arg(args, char *) = (v); \
            break; \
        case TYPE_short: \
            *va_arg(args, short *) = (v); \
            break; \
        case TYPE_int: \
            *va_arg(args, int *) = (v); \
            break; \
        case TYPE_long: \
            *va_arg(args, long *) = (v); \
            break; \
        case TYPE_longlong: \
            *va_arg(args, long long *) = (v); \
            break; \
        case TYPE_intmax_t: \
            *va_arg(args, intmax_t *) = (v); \
            break; \
        case TYPE_ssize_t: \
            *va_arg(args, ssize_t *) = (v); \
            break; \
        case TYPE_ptrdiff_t: \
            *va_arg(args, ptrdiff_t *) = (v); \
            break; \
        case TYPE_uchar: \
            *va_arg(args, unsigned char *) = (v); \
            break; \
        case TYPE_ushort: \
            *va_arg(args, unsigned short *) = (v); \
            break; \
        case TYPE_uint: \
            *va_arg(args, unsigned int *) = (v); \
            break; \
        case TYPE_ulong: \
            *va_arg(args, unsigned long *) = (v); \
            break; \
        case TYPE_ulonglong: \
            *va_arg(args, unsigned long long *) = (v); \
            break; \
        case TYPE_uintmax_t: \
            *va_arg(args, uintmax_t *) = (v); \
            break; \
        case TYPE_size_t: \
            *va_arg(args, size_t *) = (v); \
            break; \
        default: \
             return SELVA_PROTO_EINTYPE; \
        } \
    } while (0)

#define double_to_va_arg(type, v) \
    do { \
        switch (type) { \
        case TYPE_float: \
            *va_arg(args, float *) = (v); \
            break; \
        case TYPE_double: \
            *va_arg(args, double *) = (v); \
            break; \
        case TYPE_longdouble: \
            *va_arg(args, long double *) = (v); \
            break; \
        default: \
            return SELVA_PROTO_EINTYPE; \
        } \
    } while (0)


/*
 * TODO Support reading values into an array
 * E.g. int x[3] <= %3d
 * TODO Support variable length arrays
 */
int selva_proto_scanf(struct finalizer * restrict fin, const void * restrict buf, size_t szbuf, const char * restrict fmt, ...)
{
    va_list args;
    size_t buf_i = 0;
    int array_level = 0;
    int postponed_array_end = 0;
    int n = 0;

    if (szbuf == 0) {
        return 0;
    }

    fmt--;
    va_start(args, fmt);
    while (*(++fmt) != '\0') {
        struct placeholder_state ps;

        if (*fmt == '%') {
            reset_placeholder(&ps);
            fmt = parse_specifier(&ps, ++fmt);
            if (!fmt) {
                return SELVA_PROTO_EINVAL;
            }

            if (buf_i >= szbuf && array_level == 0) {
                /* Fewer args given than specified in the fmt string. */
                goto out;
            }

            enum selva_proto_data_type found_type;
            size_t data_len;
            int off;

            off = selva_proto_parse_vtype(buf, szbuf, buf_i, &found_type, &data_len);
            if (off <= 0) {
                return off;
            }

            if (found_type != type_to_selva_proto_type_map[ps.type]) {
                if (found_type == SELVA_PROTO_STRING && ps.type == TYPE_char) {
                    /*
                     * We allow capturing a char from both
                     * SELVA_PROTO_LONGLONG and SELVA_PROTO_STRING.
                     * However a char array can be currently read only from a
                     * SELVA_PROTO_STRING.
                     */
                    if (ps.width < 0) {
                        ps.width = 1;
                    }
                } else {
                    return SELVA_PROTO_EBADMSG;
                }
            }

            if (found_type == SELVA_PROTO_LONGLONG) {
                long long v;

                memcpy(&v, (char *)buf + buf_i + offsetof(struct selva_proto_longlong, v), sizeof(v));
                v = le64toh(v);
                int64_to_va_arg(ps.type, v);
            } else if (found_type == SELVA_PROTO_DOUBLE) {
                char vbuf[8];
                double v;

                memcpy(vbuf, (char *)buf + buf_i + offsetof(struct selva_proto_double, v), sizeof(vbuf));
                v = ledoubletoh(vbuf);
                double_to_va_arg(ps.type, v);
            } else if (found_type == SELVA_PROTO_STRING) {
                const char *str = (char *)buf + buf_i + offsetof(struct selva_proto_string, data);
                typeof_field(struct selva_proto_string, flags) flags;
                typeof_field(struct selva_proto_string, bsize) len;

                memcpy(&flags, (char *)buf + buf_i + offsetof(struct selva_proto_string, flags), sizeof(flags));
                memcpy(&len, (char *)buf + buf_i + offsetof(struct selva_proto_string, bsize), sizeof(len));

                if ((ps.precision == -1 || ps.width >= 0) && (flags & SELVA_PROTO_STRING_FDEFLATE)) {
                    /*
                     * A compressed string must be captured as a selva_string with %p.
                     */
                    return SELVA_PROTO_EINTYPE;
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

                    /* RFE Should this fail if len !=/< ps.width */

                    memcpy(dest, str, copy_size);
                    if (copy_size < (size_t)ps.width) {
                        memset(dest + copy_size, '\0', (size_t)ps.width - copy_size);
                    }
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

                    if (!fin) {
                        return SELVA_PROTO_EINVAL;
                    }

                    s = selva_string_create(str, len, (flags & SELVA_PROTO_STRING_FDEFLATE) ? SELVA_STRING_COMPRESS : 0);
                    selva_string_auto_finalize(fin, s);
                    *va_arg(args, struct selva_string **) = s;
                }
            }

            n++;
        } else {
            char ch = *fmt;

            if (isspace(ch)) {
                /* NOP */
            } else if (ch == ',') { /* jump to next selva_proto value. */
                int off = get_skip_off(buf, szbuf, buf_i);

                if (off <= 0) {
                    return off;
                }
                buf_i += off;
            } else if (ch == '.' && fmt[1] == '.' && fmt[2] == '.' && fmt[3] == '\0') { /* rest */
                struct selva_string ***rest = va_arg(args, struct selva_string ***);
                int err;

                if (!fin) {
                    return SELVA_PROTO_EINVAL;
                }

                err = selva_proto_buf2strings(fin, buf + buf_i, szbuf - buf_i, rest);
                if (err < 0) {
                    return err;
                } else if (err > 0) {
                    n += err;
                }

                fmt += 2;
                buf_i = szbuf;
            } else if (ch == '{') { /* Begin array. */
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
                } else if (flags & (SELVA_PROTO_ARRAY_FLONGLONG | SELVA_PROTO_DOUBLE)) {
                    /*
                     * Embedded array.
                     */
                    uint8_t *data = (uint8_t *)buf + buf_i + sizeof(struct selva_proto_array);

                    for (size_t i = 0; i < data_len; i++) {
                        struct placeholder_state ps;

                        reset_placeholder(&ps);

                        do {
                            fmt++;
                        } while (isspace(*fmt) || *fmt == ',');

                        if (*fmt != '%') {
                            return SELVA_PROTO_EINVAL;
                        }

                        fmt = parse_specifier(&ps, ++fmt);
                        if (!fmt) {
                            return SELVA_PROTO_EINVAL;
                        }

                        if (flags & SELVA_PROTO_ARRAY_FLONGLONG) {
                            int64_t v;

                            memcpy(&v, data + i * sizeof(v), sizeof(v));
                            v = le64toh(v);
                            int64_to_va_arg(ps.type, v);
                        } else { /* SELVA_PROTO_DOUBLE */
                            char vbuf[8];
                            double v;

                            memcpy(&vbuf, data + i * sizeof(v), sizeof(v));
                            v = ledoubletoh(vbuf);
                            double_to_va_arg(ps.type, v);
                        }
                    }

                    /*
                     * Find the end of array mark.
                     */
                    fmt = strchr(fmt, '}');
                    if (!fmt) {
                        return SELVA_PROTO_EINVAL;
                    } else if (fmt[1] == ',') {
                        /* Don't want to do a new skip. */
                        fmt++;
                    }
                } else {
                    array_level++;
                }

                buf_i += off;
            } else if (ch == '}') { /* End array. */
                if (postponed_array_end > 0) {
                    enum selva_proto_data_type found_type;
                    size_t data_len;
                    int off;

                    off = selva_proto_parse_vtype(buf, szbuf, buf_i, &found_type, &data_len);
                    if (off <= 0) {
                        return off;
                    } else if (found_type != SELVA_PROTO_ARRAY_END) {
                        return SELVA_PROTO_EBADMSG;
                    }

                    postponed_array_end--;
                    buf_i += off;
                } else if (fmt[1] == '\0') {
                    /* We can't be sure later whether a skip is needed so we do it here now. */
                    int off = get_skip_off(buf, szbuf, buf_i);

                    if (off > 0) {
                        buf_i += off;
                    }
                }

                array_level--;
            } else {
                /* Invalid format string. */
                return SELVA_PROTO_EINVAL;
            }
        }
    }

    /*
     * Skip the last item if not already skipped so we can check whether more
     * data was passed than parsed. I.e. checking for arity error.
     * `,` and `}` already causes a skip.
     */
    if (!strchr(",}", *(fmt - 1))) {
        int off = get_skip_off(buf, szbuf, buf_i);

        if (off > 0) {
            buf_i += off;
        }
    }

out:
    /*
     * The C standard says va_end() is mandatory. However, it's NOP in
     * GNU C Library and this function expects it to be NOP. If we ever hit a
     * libc where it's not a NOP then we need to change all the return
     * statements in this function.
     */
    va_end(args);

    if (array_level > 0 || buf_i < szbuf) {
        /*
         * This could be either SELVA_PROTO_EBADMSG or SELVA_PROTO_EINVAL.
         * Both seem semantically correct and are used for user supplied
         * values, e.g. arity error is SELVA_PROTO_EINVAL.
         */
        return SELVA_PROTO_EINVAL;
    }

    return n;
}
