#include "redismodule.h"
#include "selva_set.h"
#include "jemalloc.h"
#include "cdefs.h"
#include "rms.h"

#define SELVA_SHARED_KEY_STR "type"
#define SELVA_SHARED_KEY_LEN 4

static struct SelvaSet shared_strings;

RedisModuleString *Share_RMS(const char *key_str, size_t key_len, RedisModuleString *rms) {
    RedisModuleString *out;

    if (key_len != SELVA_SHARED_KEY_LEN ||
        memcmp(key_str, SELVA_SHARED_KEY_STR, SELVA_SHARED_KEY_LEN)) {
        return NULL;
    }

    out = SelvaSet_FindRms(&shared_strings, rms);
    if (!out) {
        int err;

        out = RedisModule_HoldString(NULL, rms);
        err = SelvaSet_Add(&shared_strings, out);
        if (err) {
            RedisModule_FreeString(NULL, rms);
            return NULL;
        }
    }

    /*
     * Hodl it even if we just added it to the data structure so we can keep
     * it in the set even if/when the caller tries to free the string.
     */
    RedisModule_RetainString(NULL, out);

    return out;
}

__constructor static void init_shared(void) {
    SelvaSet_Init(&shared_strings, SELVA_SET_TYPE_RMSTRING);
}
