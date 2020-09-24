#include <string.h>
#include <stdlib.h>
#include "redismodule.h"
#include "arg_parser.h"
#include "cdefs.h"
#include "errors.h"
#include "subscriptions.h"

int SelvaArgParser_IntOpt(ssize_t *value, const char *name, RedisModuleString *txt, RedisModuleString *num) {
    TO_STR(txt, num);
    char *end = NULL;

    if (strcmp(name, txt_str)) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    *value = strtoull(num_str, &end, 10);
    if (num_str == end) {
        return SELVA_EINVAL;
    }

    return 0;
}

int SelvaArgParser_StrOpt(const char **value, const char *name, RedisModuleString *arg_key, RedisModuleString *arg_val) {
    TO_STR(arg_key, arg_val);

    if(strcmp(name, arg_key_str)) {
        return SELVA_EINVAL;
    }

    *value = arg_val_str;
    return 0;
}

int SelvaArgParser_SubscriptionId(Selva_SubscriptionId id, RedisModuleString *arg) {
    TO_STR(arg);

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
