/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include "jemalloc.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "selva_object.h"
#include "subscriptions.h"
#include "arg_parser.h"

static void buf2strings_cleanup(struct finalizer *fin, selva_stringList list, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        finalizer_del(fin, list[i]);
    }
    selva_free(list);
}

int SelvaArgParser_buf2strings(struct finalizer *fin, const char *buf, size_t bsize, selva_stringList *out) {
    selva_stringList list = NULL;
    size_t list_len = 0;
    size_t i = 0;

    while (i < bsize) {
        enum selva_proto_data_type type;
        size_t data_len;
        int off;

        off = selva_parse_vtype(buf, bsize, i, &type, &data_len);
        if (off <= 0) {
            buf2strings_cleanup(fin, list, list_len);
            return off;
        }

        i += off;

        if (type == SELVA_PROTO_STRING) {
            struct selva_string *s;

            s = selva_string_create(buf + i, data_len, 0);
            selva_string_auto_finalize(fin, s);

            list_len++;
            list = selva_realloc(list, list_len * sizeof(struct selva_string *));
            list[list_len - 1] = s;
            *out = list;

            i += data_len;
        } else if (type == SELVA_PROTO_ARRAY || type == SELVA_PROTO_ARRAY_END) {
            /* NOP */
        } else {
            buf2strings_cleanup(fin, list, list_len);
            return SELVA_EINVAL;
        }
    }

    finalizer_add(fin, out, selva_free);
    *out = list;
    return list_len;
}

int SelvaArgParser_IntOpt(ssize_t *value, const char *name, const struct selva_string *txt, const struct selva_string *num) {
    TO_STR(txt, num);
    char *end = NULL;

    if (strcmp(name, txt_str)) {
        return SELVA_ENOENT;
    }

    *value = strtoull(num_str, &end, 10);
    if (num_str == end) {
        return SELVA_EINVAL;
    }

    return 0;
}

int SelvaArgParser_StrOpt(const char **value, const char *name, const struct selva_string *arg_key, const struct selva_string *arg_val) {
    TO_STR(arg_key, arg_val);

    if (strcmp(name, arg_key_str)) {
        return SELVA_ENOENT;
    }

    if (value) {
        *value = arg_val_str;
    }

    return 0;
}

int SelvaArgsParser_StringList(
        struct finalizer *finalizer,
        selva_stringList *out,
        const char *name,
        const struct selva_string *arg_key,
        const struct selva_string *arg_val) {
    const char *cur;
    struct selva_string **list = NULL;
    size_t n = 1;
    int err;

    err = SelvaArgParser_StrOpt(&cur, name, arg_key, arg_val);
    if (err) {
        return err;
    }

    const size_t list_size = n * sizeof(struct selva_string *);
    list = selva_realloc(list, list_size);

    list[n - 1] = NULL;
    if (cur[0] != '\0') {
        do {
            struct selva_string *el;
            const char *next;
            size_t len;

            /*
             * Find the separator between the current and the next string.
             */
            next = cur;
            while (*next != '\0' && *next != '\n') {
                next++;
            }
            len = (size_t)((ptrdiff_t)next - (ptrdiff_t)cur);

            /*
             * Create a string.
             */
            el = selva_string_create(cur, len, 0);
            selva_string_auto_finalize(finalizer, el);

            /*
             * Set to the array.
             */
            list = selva_realloc(list, ++n * sizeof(struct selva_string *));
            list[n - 2] = el;
            list[n - 1] = NULL;

            if (*next == '\0') {
                break;
            }
            cur = next + 1;
        } while (*cur != '\0');
    }

    *out = list;
    return 0;
}

int SelvaArgsParser_StringSetList(
        struct finalizer *finalizer,
        struct SelvaObject **list_out,
        struct selva_string **excluded_out,
        const char *name,
        const struct selva_string *arg_key,
        const struct selva_string *arg_val) {
    const char excl_prefix = '!';
    const char set_separator = '\n';
    const char list_separator = '|';
    const char eos = '\0';
    struct SelvaObject *obj;
    struct selva_string *excl = NULL;
    const char *cur;
    size_t n = 0;
    size_t nr_excl = 0;
    int err;

    err = SelvaArgParser_StrOpt(&cur, name, arg_key, arg_val);
    if (err) {
        return err;
    }

    obj = SelvaObject_New();

    if (excluded_out) {
        excl = selva_string_create("", 0, SELVA_STRING_MUTABLE);
    }

    if (cur[0] != eos) {
        do {
            const size_t key_len = (size_t)(log10(n + 1)) + 1;
            char key_str[key_len + 1];
            const char *next;
#if 0
            size_t len;
#endif

            snprintf(key_str, key_len + 1, "%zu", n);

            /*
             * Find the separator between the current and the next field name list.
             */
            next = cur;
            while (*next != eos && *next != set_separator) {
                next++;
            }
            /*
             * len could be used for splitting in the do..while loop
             * but we are currently looking for the separator chars
             * there.
             */
#if 0
            len = (size_t)((ptrdiff_t)next - (ptrdiff_t)cur);
#endif

            /*
             * Create the set elements.
             */
            size_t nr_el = 0;
            const char *cur_el = cur;
            do {
                const char *next_el;
                size_t el_len;

                /*
                 * Find the separator between the current and the next field name.
                 */
                next_el = cur_el;
                while (*next_el != eos && *next_el != list_separator && *next_el != set_separator) {
                    next_el++;
                }
                el_len = (size_t)((ptrdiff_t)next_el - (ptrdiff_t)cur_el);

                if (el_len > 0) { /* Skip empty elements. */
                    if (excl && cur_el[0] == excl_prefix) {
                        if (el_len > 1) {
                            const char sep[] = { set_separator };
                            const char *name_str = cur_el + 1;
                            const size_t name_len = el_len - 1;

                            /* Ignore id field silently. */
                            if (!(name_len == (sizeof(SELVA_ID_FIELD) - 1) && !memcmp(SELVA_ID_FIELD, name_str, name_len))) {
                                if (selva_string_append(excl, name_str, name_len) ||
                                    selva_string_append(excl, sep, 1)) {
                                    goto fail;
                                }
                                nr_excl++;
                            }
                        }
                        /* Otherwise we ignore the empty element. */
                    } else {
                        struct selva_string *el;

                        el = selva_string_create(cur_el, el_len, 0);

                        /*
                         * Add to the list.
                         */
                        SelvaObject_AddArrayStr(obj, key_str, key_len, SELVA_OBJECT_STRING, el);
                        nr_el++;
                    }
                }

                if (*next_el == eos || *next_el == set_separator) {
                    break;
                }
                cur_el = next_el + 1;
            } while (*cur_el != eos);

            /*
             * Increment the set index only if elements were inserted at the
             * index.
             */
            if (nr_el > 0) {
                n++;
            }

            if (*next == eos) {
                break;
            }
            cur = next + 1;
        } while (*cur != eos);
    }

    *list_out = obj;
    if (excluded_out && nr_excl > 0) {
        *excluded_out = excl;
        selva_string_auto_finalize(finalizer, excl);
    } else if (excluded_out) {
        *excluded_out = NULL;
        selva_string_free(excl);
    }
    return 0;
fail:
    if (obj) {
        SelvaObject_Destroy(obj);
    }
    if (excl) {
        selva_string_free(excl);
    }
    return SELVA_ENOMEM;
}

int SelvaArgParser_Enum(
        const struct SelvaArgParser_EnumType types[],
        const struct selva_string *arg) {
    size_t i = 0;
    TO_STR(arg);

    while (types[i].name) {
        if (!strcmp(types[i].name, arg_str)) {
            return types[i].id;
        }
        i++;
    }

    return SELVA_ENOENT;
}

int SelvaArgParser_NodeType(Selva_NodeType node_type, const struct selva_string *arg) {
    size_t len;
    const char *str = selva_string_to_str(arg, &len);

    if (len < SELVA_NODE_TYPE_SIZE) {
        return SELVA_EINVAL;
    }

    memcpy(node_type, str, sizeof(Selva_NodeType));
    return 0;
}

int SelvaArgParser_SubscriptionId(Selva_SubscriptionId id, const struct selva_string *arg) {
    TO_STR(arg);

    if (arg_len != SELVA_SUBSCRIPTION_ID_STR_LEN) {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    return Selva_SubscriptionStr2id(id, arg_str);
}

int SelvaArgParser_MarkerId(Selva_SubscriptionMarkerId *marker_id, const struct selva_string *arg) {
    long long ll;

    if (!selva_string_to_ll(arg, &ll)) {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    *marker_id = (Selva_SubscriptionMarkerId)ll;
    return 0;
}

int SelvaArgParser_IndexHints(selva_stringList *out, struct selva_string **argv, int argc) {
    struct selva_string **list = NULL;
    int n = 0;

    for (int i = 0; i < argc; i += 2) {
        struct selva_string **new_list;

        if (n > FIND_INDICES_MAX_HINTS_FIND) {
            return SELVA_ENOBUFS;
        }

        if (i + 1 >= argc || strcmp("index", selva_string_to_str(argv[i], NULL))) {
            break;
        }

        const size_t list_size = ++n * sizeof(struct selva_string *);
        new_list = selva_realloc(list, list_size);

        list = new_list;
        list[n - 1] = argv[i + 1];
    }

    *out = list;
    return n;
}
