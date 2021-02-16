#include <string.h>
#include <stdlib.h>
#include "redismodule.h"
#include "arg_parser.h"
#include "cdefs.h"
#include "errors.h"
#include "subscriptions.h"
#include "selva_object.h"

int SelvaArgParser_IntOpt(ssize_t *value, const char *name, RedisModuleString *txt, RedisModuleString *num) {
    TO_STR(txt, num)
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

int SelvaArgParser_StrOpt(const char **value, const char *name, RedisModuleString *arg_key, RedisModuleString *arg_val) {
    TO_STR(arg_key, arg_val)

    if(strcmp(name, arg_key_str)) {
        return SELVA_ENOENT;
    }

    if (value) {
        *value = arg_val_str;
    }

    return 0;
}

int SelvaArgsParser_StringList(RedisModuleCtx *ctx, RedisModuleString ***out, const char *name, RedisModuleString *arg_key, RedisModuleString *arg_val) {
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
int SelvaArgsParser_StringSetList(RedisModuleCtx *ctx, struct SelvaObject **out, const char *name, RedisModuleString *arg_key, RedisModuleString *arg_val) {
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
            RedisModuleString *key;
            const char *next;
            size_t len;

            /*
             * Find the separator between the current and the next field name list.
             */
            next = cur;
            while (*next != '\0' && *next != '\n') {
                next++;
            }
            len = (size_t)((ptrdiff_t)next - (ptrdiff_t)cur);

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

int SelvaArgParser_Enum(const struct SelvaArgParser_EnumType types[], RedisModuleString *arg) {
    size_t i = 0;
    TO_STR(arg)

    while (types[i].name) {
        if (!strcmp(types[i].name, arg_str)) {
            return types[i].id;
        }
        i++;
    }

    return SELVA_ENOENT;
}

void SelvaArgParser_NodeId(Selva_NodeId node_id, RedisModuleString *arg) {
    size_t len;
    const char *str;

    str = RedisModule_StringPtrLen(arg, &len);
    memset(node_id, 0, SELVA_NODE_ID_SIZE);
    memcpy(node_id, str, min(SELVA_NODE_ID_SIZE, len));
}

int SelvaArgParser_SubscriptionId(Selva_SubscriptionId id, RedisModuleString *arg) {
    TO_STR(arg)

    if (arg_len != SELVA_SUBSCRIPTION_ID_STR_LEN) {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    return Selva_SubscriptionStr2id(id, arg_str);
}

int SelvaArgParser_MarkerId(Selva_SubscriptionMarkerId *marker_id, RedisModuleString *arg) {
    long long ll;

    if (RedisModule_StringToLongLong(arg, &ll) != REDISMODULE_OK) {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    *marker_id = (Selva_SubscriptionMarkerId)ll;
    return 0;
}
