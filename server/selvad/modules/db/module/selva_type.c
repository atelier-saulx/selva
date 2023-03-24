/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_db.h"

/*
 * Technically a nodeId is always 10 bytes but sometimes a printable
 * representation without padding zeroes is needed.
 */
size_t Selva_NodeIdLen(const Selva_NodeId nodeId) {
    size_t len = SELVA_NODE_ID_SIZE;

    while (len >= 1 && nodeId[len - 1] == '\0') {
        len--;
    }

    return len;
}

int selva_string2node_id(Selva_NodeId nodeId, const struct selva_string *s)
{
    TO_STR(s);
    int err = 0;

    if (s_len < SELVA_NODE_TYPE_SIZE + 1 || s_len > SELVA_NODE_ID_SIZE) {
        return SELVA_EINVAL;
    }

    /*
     * This may look fancy but if there is no `return` inside the loop
     * then the compiler can unroll the loop.
     */
    for (int i = 0; i <= SELVA_NODE_TYPE_SIZE; i++) {
        if (s_str[i] == '\0') {
            err = SELVA_EINVAL;
        }
    }

    if (!err) {
        Selva_NodeIdCpy(nodeId, s_str);
    }

    return err;
}

/*
 * SHA256 to hex string.
 * The destination buffer must be at least SELVA_SUBSCRIPTION_ID_STR_LEN + 1 bytes.
 */
char *Selva_SubscriptionId2str(char dest[static SELVA_SUBSCRIPTION_ID_STR_LEN + 1], const Selva_SubscriptionId sub_id) {
    for (size_t i = 0; i < sizeof(Selva_SubscriptionId); i++) {
        const size_t k = 2 * i;

        snprintf(dest + k, SELVA_SUBSCRIPTION_ID_STR_LEN + 1 - k, "%02x", sub_id[i]);
    }
    dest[SELVA_SUBSCRIPTION_ID_STR_LEN] = '\0';

    return dest;
}

int Selva_SubscriptionStr2id(Selva_SubscriptionId dest, const char *src) {
    char byte[3] = { '\0', '\0', '\0' };

    for (size_t i = 0; i < sizeof(Selva_SubscriptionId); i++) {
        unsigned long v;

        byte[0] = src[2 * i];
        byte[1] = src[2 * i + 1];
        v = strtoul(byte, NULL, 16);

        if (unlikely(v > 0xff)) {
            return SELVA_SUBSCRIPTIONS_EINVAL;
        }

        dest[i] = v;
    }

    return 0;
}
