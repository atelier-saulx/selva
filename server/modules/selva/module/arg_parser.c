#include <string.h>
#include <stdlib.h>
#include "redismodule.h"
#include "arg_parser.h"
#include "cdefs.h"
#include "errors.h"
#include "subscriptions.h"
#include "selva_object.h"

int SelvaArgParser_IntOpt(ssize_t *value, const char *name, const RedisModuleString *txt, const RedisModuleString *num) {
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

int SelvaArgParser_StrOpt(const char **value, const char *name, const RedisModuleString *arg_key, const RedisModuleString *arg_val) {
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
        RedisModuleCtx *ctx,
        RedisModuleStringList *out,
        const char *name,
        const RedisModuleString *arg_key,
        const RedisModuleString *arg_val) {
    const char *cur;
    RedisModuleString **list = NULL;
    size_t n = 1;
    int err;

    err = SelvaArgParser_StrOpt(&cur, name, arg_key, arg_val);
    if (err) {
        return err;
    }

    const size_t list_size = n * sizeof(RedisModuleString *);
    list = RedisModule_Realloc(list, list_size);
    if (!list) {
        return SELVA_ENOMEM;
    }

    list[n - 1] = NULL;
    if (cur[0] != '\0') {
        do {
            RedisModuleString *el;
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
            el = RedisModule_CreateString(ctx, cur, len);
            if (!el) {
#if MEM_DEBUG
                memset(list, 0, list_size);
#endif
                RedisModule_Free(list);
                return SELVA_ENOMEM;
            }

            /*
             * Set to the array.
             */
            list = RedisModule_Realloc(list, ++n * sizeof(RedisModuleString *));
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

/**
 * Parse a set of lists containing strings.
 * Set separator: '\n'
 * List separator: '|'
 * Enf of sets: '\0'
 */
int SelvaArgsParser_StringSetList(
        RedisModuleCtx *ctx,
        struct SelvaObject **out,
        const char *name,
        const RedisModuleString *arg_key,
        const RedisModuleString *arg_val) {
    struct SelvaObject *obj;
    const char *cur;
    size_t n = 0;
    int err;

    err = SelvaArgParser_StrOpt(&cur, name, arg_key, arg_val);
    if (err) {
        return err;
    }

    obj = SelvaObject_New();
    if (!obj) {
        return SELVA_ENOMEM;
    }

    if (cur[0] != '\0') {
        do {
            const RedisModuleString *key;
            const char *next;
#if 0
            size_t len;
#endif

            /*
             * Find the separator between the current and the next field name list.
             */
            next = cur;
            while (*next != '\0' && *next != '\n') {
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
             * Create the strings
             */
            key = RedisModule_CreateStringPrintf(ctx, "%zu", n++);
            if (!key) {
                SelvaObject_Destroy(obj);
                return SELVA_ENOMEM;
            }

            const char *cur_el = cur;
            do {
                RedisModuleString *el;
                const char *next_el;
                size_t el_len;

                /*
                 * Find the separator between the current and the next field name.
                 */
                next_el = cur_el;
                while (*next_el != '\0' && *next_el != '|' && *next_el != '\n') {
                    next_el++;
                }
                el_len = (size_t)((ptrdiff_t)next_el - (ptrdiff_t)cur_el);

                el = RedisModule_CreateString(NULL, cur_el, el_len);
                if (!key || !el) {
                    SelvaObject_Destroy(obj);
                    return SELVA_ENOMEM;
                }

                /*
                 * Add to the list.
                 */
                SelvaObject_AddArray(obj, key, SELVA_OBJECT_STRING, el);

                if (*next_el == '\0' || *next_el == '\n') {
                    break;
                }
                cur_el = next_el + 1;
            } while (*cur_el != '\0');

            if (*next == '\0') {
                break;
            }
            cur = next + 1;
        } while (*cur != '\0');
    }

    *out = obj;
    return 0;
}

int SelvaArgParser_Enum(
        const struct SelvaArgParser_EnumType types[],
        const RedisModuleString *arg) {
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

int SelvaArgParser_NodeId(Selva_NodeId node_id, const RedisModuleString *arg) {
    size_t len;
    const char *str;

    str = RedisModule_StringPtrLen(arg, &len);

    if (len < SELVA_NODE_TYPE_SIZE + 1) {
        return SELVA_EINVAL;
    }

    memset(node_id, 0, SELVA_NODE_ID_SIZE);
    memcpy(node_id, str, min(SELVA_NODE_ID_SIZE, len));

    return 0;
}

int SelvaArgParser_NodeType(Selva_NodeType node_type, const RedisModuleString *arg) {
    size_t len;
    const char *str;

    str = RedisModule_StringPtrLen(arg, &len);

    if (len < SELVA_NODE_TYPE_SIZE) {
        return SELVA_EINVAL;
    }

    memcpy(node_type, str, sizeof(Selva_NodeType));
    return 0;
}

int SelvaArgParser_SubscriptionId(Selva_SubscriptionId id, const RedisModuleString *arg) {
    TO_STR(arg);

    if (arg_len != SELVA_SUBSCRIPTION_ID_STR_LEN) {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    return Selva_SubscriptionStr2id(id, arg_str);
}

int SelvaArgParser_MarkerId(Selva_SubscriptionMarkerId *marker_id, const RedisModuleString *arg) {
    long long ll;

    if (RedisModule_StringToLongLong(arg, &ll) != REDISMODULE_OK) {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    *marker_id = (Selva_SubscriptionMarkerId)ll;
    return 0;
}

int SelvaArgParser_IndexHints(RedisModuleStringList *out, RedisModuleString **argv, int argc) {
    RedisModuleString **list = NULL;
    int n = 0;

    for (int i = 0; i < argc; i += 2) {
        RedisModuleString **new_list;

        if (i + 1 >= argc || strcmp("index", RedisModule_StringPtrLen(argv[i], NULL))) {
            break;
        }

        const size_t list_size = ++n * sizeof(RedisModuleString *);
        new_list = RedisModule_Realloc(list, list_size);
        if (!new_list) {
            RedisModule_Free(list);
            return SELVA_ENOMEM;
        }

        list = new_list;
        list[n - 1] = argv[i + 1];
    }

    *out = list;
    return n;
}
