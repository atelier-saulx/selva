/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <sys/types.h>
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_server.h"
#include "selva_object.h"
#include "selva_db.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "hierarchy.h"

int get_alias_str(struct SelvaHierarchy *hierarchy, const char *ref_str, size_t ref_len, Selva_NodeId node_id)
{
    struct SelvaObject *aliases = GET_STATIC_SELVA_OBJECT(&hierarchy->aliases);
    struct selva_string *value = NULL;
    int err;

    err = SelvaObject_GetStringStr(aliases, ref_str, ref_len, &value);
    if (err) {
        return err;
    } else if (!value) {
        return SELVA_ENOENT;
    }

    return selva_string2node_id(node_id, value);
}

int get_alias(struct SelvaHierarchy *hierarchy, const struct selva_string *ref, Selva_NodeId node_id)
{
    TO_STR(ref);

    return get_alias_str(hierarchy, ref_str, ref_len, node_id);
}

int delete_alias(struct SelvaHierarchy *hierarchy, struct selva_string *ref)
{
    struct SelvaObject *aliases = GET_STATIC_SELVA_OBJECT(&hierarchy->aliases);

    return SelvaObject_DelKey(aliases, ref);
}

int delete_aliases(struct SelvaHierarchy *hierarchy, struct SelvaSet *set)
{
    struct SelvaObject *aliases = GET_STATIC_SELVA_OBJECT(&hierarchy->aliases);
    struct SelvaSetElement *el;

    if (!set || set->type != SELVA_SET_TYPE_STRING) {
        /* Likely there were no aliases. */
        return SELVA_ENOENT;
    }

    SELVA_SET_STRING_FOREACH(el, set) {
        struct selva_string *alias = el->value_string;

        (void)SelvaObject_DelKey(aliases, alias);
    }

    return 0;
}

void update_alias(SelvaHierarchy *hierarchy, const Selva_NodeId node_id, struct selva_string *ref)
{
    struct SelvaObject *aliases = GET_STATIC_SELVA_OBJECT(&hierarchy->aliases);
    struct selva_string *old = NULL;

    /*
     * Remove the alias from the previous node.
     */
    if (!SelvaObject_GetString(aliases, ref, &old)) {
        if (old) {
            TO_STR(old);
            Selva_NodeId old_node_id;
            const struct SelvaHierarchyNode *old_node;

            Selva_NodeIdCpy(old_node_id, old_str);
            old_node = SelvaHierarchy_FindNode(hierarchy, old_node_id);
            if (old_node) {
                struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(old_node);

                SelvaObject_RemStringSetStr(obj, SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD) - 1, ref);
            }
        }
    }

    SelvaObject_SetString(aliases, ref, selva_string_create(node_id, Selva_NodeIdLen(node_id), 0));

    /*
     * This alias might have been a missing accessor.
     */
    TO_STR(ref);
    SelvaSubscriptions_DeferMissingAccessorEvents(hierarchy, ref_str, ref_len);
}

static void lsaliases(struct selva_server_response_out *resp, const void *buf __unused, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    struct SelvaObject *aliases = GET_STATIC_SELVA_OBJECT(&hierarchy->aliases);

    if (len > 0) {
        selva_send_error_arity(resp);
        return;
    }


    (void)SelvaObject_ReplyWithObjectStr(resp, NULL, aliases, NULL, 0, 0);
}

static int Alias_OnLoad(void) {
    SELVA_MK_COMMAND(CMD_ID_LSALIASES, SELVA_CMD_MODE_PURE, lsaliases);

    return 0;
}
SELVA_ONLOAD(Alias_OnLoad);
