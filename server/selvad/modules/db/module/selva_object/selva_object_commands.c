/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "typestr.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "util/svector.h"
#include "selva_error.h"
#include "selva_replication.h"
#include "selva_server.h"
#include "arg_parser.h"
#include "selva_db.h"
#include "hierarchy.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "selva_object.h"

static struct SelvaObject *SelvaObject_Open(struct selva_server_response_out *resp, struct selva_string *key_name) {
    struct SelvaHierarchy *hierarchy = main_hierarchy;
    Selva_NodeId nodeId;
    const struct SelvaHierarchyNode *node;
    int err;

    err = selva_string2node_id(nodeId, key_name);
    if (err) {
        selva_send_error(resp, err, NULL, 0);
        return NULL;
    }

    node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        selva_send_error(resp, SELVA_HIERARCHY_ENOENT, NULL, 0);
        return NULL;
    }

    return SelvaHierarchy_GetNodeObject(node);
}

void SelvaObject_DelCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    struct SelvaObject *obj;
    int err;

    finalizer_init(&fin);

    const int ARGV_KEY = 0;
    const int ARGV_OKEY = 1;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    obj = SelvaObject_Open(resp, argv[ARGV_KEY]);
    if (!obj) {
        return;
    }

    err = SelvaObject_DelKey(obj, argv[ARGV_OKEY]);
    if (err == SELVA_ENOENT) {
        selva_send_ll(resp, 0);
    } else if (err) {
        selva_send_error(resp, err, NULL, 0);
        return;
    } else {
        selva_send_ll(resp, 1);
    }

    /* FIXME Replicate */
#if 0
    RedisModule_ReplicateVerbatim(ctx);
#endif
}

void SelvaObject_ExistsCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    struct SelvaObject *obj;
    int err;

    finalizer_init(&fin);

    const int ARGV_KEY = 0;
    const int ARGV_OKEY = 1;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    obj = SelvaObject_Open(resp, argv[ARGV_KEY]);
    if (!obj) {
        return;
    }

    err = SelvaObject_Exists(obj, argv[ARGV_OKEY]);
    if (err == SELVA_ENOENT) {
        selva_send_ll(resp, 0);
        return;
    } else if (err) {
        selva_send_error(resp, err, NULL, 0);
        return;
    }
    selva_send_ll(resp, 1);
}


void SelvaObject_GetCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    struct SelvaObject *obj;

    finalizer_init(&fin);

    const int ARGV_LANG = 0;
    const int ARGV_KEY = 1;
    const int ARGV_OKEY = 2;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc != 2 && argc != 3) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    struct selva_string *lang = argv[ARGV_LANG];
    obj = SelvaObject_Open(resp, argv[ARGV_KEY]);
    if (!obj) {
        return;
    }

    if (argc == 2) {
        (void)SelvaObject_ReplyWithObjectStr(resp, lang, obj, NULL, 0, 0);
        return;
    }

    for (int i = ARGV_OKEY; i < argc; i++) {
        const struct selva_string *okey = argv[i];
        TO_STR(okey);

        int err = 0;

        if (strstr(okey_str, ".*.")) {
            long resp_count = 0;

            selva_send_array(resp, -1);
            err = SelvaObject_ReplyWithWildcardStr(resp, lang, obj, okey_str, okey_len,
                                                   &resp_count, -1,
                                                   SELVA_OBJECT_REPLY_SPLICE_FLAG);
            if (err == SELVA_ENOENT) {
                /* Keep looking. */
                selva_send_array_end(resp);
                continue;
            } else if (err) {
                selva_send_errorf(resp, err, "Wildcard failed");
                selva_send_array_end(resp);
                return;
            }

            selva_send_array_end(resp);
        } else {
            err = SelvaObject_ReplyWithObjectStr(resp, lang, obj, okey_str, okey_len, 0);
            if (err == SELVA_ENOENT) {
                /* Keep looking. */
                continue;
            } else if (err) {
                selva_send_errorf(resp, err, "get_key");
                return;
            }
        }

        return;
    }

    selva_send_null(resp);
}

void SelvaObject_SetCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    struct SelvaObject *obj;
    size_t values_set = 0;
    int err;

    finalizer_init(&fin);

    const int ARGV_KEY = 0;
    const int ARGV_OKEY = 1;
    const int ARGV_TYPE = 2;
    const int ARGV_OVAL = 3;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc <= ARGV_TYPE) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    size_t type_len;
    const char type = selva_string_to_str(argv[ARGV_TYPE], &type_len)[0];

    if (type_len != 1) {
        selva_send_errorf(resp, SELVA_EINVAL, "Invalid or missing type argument");
        return;
    }

    if (!(argc == 4 || (type == 'S' && argc >= 4))) {
        selva_send_error_arity(resp);
        return;
    }

    obj = SelvaObject_Open(resp, argv[ARGV_KEY]);
    if (!obj) {
        return;
    }

    switch (type) {
    case 'f': /* SELVA_OBJECT_DOUBLE */
        err = SelvaObject_SetDouble(
            obj,
            argv[ARGV_OKEY],
            strtod(selva_string_to_str(argv[ARGV_OVAL], NULL), NULL));
        values_set++;
        break;
    case 'i': /* SELVA_OBJECT_LONGLONG */
        err = SelvaObject_SetLongLong(
            obj,
            argv[ARGV_OKEY],
            strtoll(selva_string_to_str(argv[ARGV_OVAL], NULL), NULL, 10));
        values_set++;
        break;
    case 's': /* SELVA_OBJECT_STRING */
        err = SelvaObject_SetString(obj, argv[ARGV_OKEY], argv[ARGV_OVAL]);
        if (err == 0) {
            finalizer_del(&fin, argv[ARGV_OVAL]);
        }
        values_set++;
        break;
    case 'S': /* SELVA_OBJECT_SET */
        for (int i = ARGV_OVAL; i < argc; i++) {
            if (SelvaObject_AddStringSet(obj, argv[ARGV_OKEY], argv[i]) == 0) {
                finalizer_del(&fin, argv[i]);
                values_set++;
            }
        }
        err = 0;
        break;
    default:
        err = SELVA_EINTYPE;
    }
    if (err) {
        selva_send_error(resp, err, NULL, 0);
        return;
    }
    selva_send_ll(resp, values_set);

    /* TODO Function to get THIS command id */
    selva_replication_replicate(CMD_OBJECT_SET_ID, buf, len);
    return;
}

void SelvaObject_TypeCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    struct SelvaObject *obj;

    finalizer_init(&fin);

    const int ARGV_KEY = 0;
    const int ARGV_OKEY = 1;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    obj = SelvaObject_Open(resp, argv[ARGV_KEY]);
    if (!obj) {
        return;
    }

    const struct selva_string *okey = argv[ARGV_OKEY];
    enum SelvaObjectType type;
    const char *type_str;
    size_t type_len;

    type = SelvaObject_GetType(obj, okey);
    if (type == SELVA_OBJECT_NULL) {
        selva_send_errorf(resp, SELVA_ENOENT, "Field not found");
        return;
    }

    type_str = SelvaObject_Type2String(type, &type_len);
    if (!type_str) {
        selva_send_errorf(resp, SELVA_EINTYPE, "invalid key type %d", (int)type);
        return;
    }

    selva_send_array(resp, -1);
    selva_send_str(resp, type_str, type_len);

    if (type == SELVA_OBJECT_ARRAY) {
        enum SelvaObjectType subtype = SELVA_OBJECT_NULL;
        const char *subtype_str;
        size_t subtype_len;

        /*
         * TODO It would be nicer if we wouldn't need to look for the subtype
         * separately.
         */
        (void)SelvaObject_GetArray(obj, okey, &subtype, NULL);
        subtype_str = SelvaObject_Type2String(subtype, &subtype_len);
        if (subtype_str) {
            selva_send_str(resp, subtype_str, subtype_len);
        } else {
            selva_send_errorf(resp, SELVA_EINTYPE, "invalid key subtype %d", (int)subtype);
        }

        selva_send_array_end(resp);
    } else if (type == SELVA_OBJECT_SET) {
        const struct SelvaSet *set;

        set = SelvaObject_GetSet(obj, okey);
        if (set) {
            switch (set->type) {
            case SELVA_SET_TYPE_STRING:
                selva_send_str(resp, "string", 6);
                break;
            case SELVA_SET_TYPE_DOUBLE:
                selva_send_str(resp, "double", 6);
                break;
            case SELVA_SET_TYPE_LONGLONG:
                selva_send_str(resp, "long long", 9);
                break;
            case SELVA_SET_TYPE_NODEID:
                selva_send_str(resp, "nodeId", 6);
                break;
            default:
                selva_send_errorf(resp, SELVA_EINTYPE, "invalid set type %d", (int)set->type);
                break;
            }
        } else {
            /* Technically ENOENT but we already found the key once. */
            selva_send_errorf(resp, SELVA_EINTYPE, "invalid set key");
        }

        selva_send_array_end(resp); /* len: 2 */
    } else {
        selva_send_array_end(resp); /* len: 1 */
    }
}

void SelvaObject_LenCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    struct SelvaObject *obj;

    finalizer_init(&fin);

    const int ARGV_KEY = 0;
    const int ARGV_OKEY = 1;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc != 1 && argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    obj = SelvaObject_Open(resp, argv[ARGV_KEY]);
    if (!obj) {
        return;
    }

    const ssize_t obj_len = SelvaObject_Len(obj, argc == 1 ? NULL : argv[ARGV_OKEY]);
    if (obj_len < 0) {
        int err = (int)obj_len;

        if (err == SELVA_EINTYPE) {
            selva_send_errorf(resp, SELVA_EINTYPE, "key type not supported");
            return;
        } else {
            selva_send_error(resp, err, NULL, 0);
            return;
        }
    }

    selva_send_ll(resp, obj_len);
}

void SelvaObject_GetMetaCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    struct SelvaObject *obj;
    int err;

    finalizer_init(&fin);

    const int ARGV_KEY = 0;
    const int ARGV_OKEY = 1;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    obj = SelvaObject_Open(resp, argv[ARGV_KEY]);
    if (!obj) {
        return;
    }

    SelvaObjectMeta_t user_meta;
    err = SelvaObject_GetUserMeta(obj, argv[ARGV_OKEY], &user_meta);
    if (err) {
        selva_send_errorf(resp, err, "Failed to get key metadata");
        return;
    }

    selva_send_ll(resp, user_meta);
}

void SelvaObject_SetMetaCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    struct SelvaObject *obj;
    SelvaObjectMeta_t user_meta;
    int err;

    finalizer_init(&fin);

    const int ARGV_KEY = 0;
    const int ARGV_OKEY = 1;
    const int ARGV_MVAL = 2;

    argc = SelvaArgParser_buf2strings(&fin, buf, len, &argv);
    if (argc != 3) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    const struct selva_string *mval = argv[ARGV_MVAL];
    TO_STR(mval);

    if (mval_len < sizeof(SelvaObjectMeta_t)) {
        selva_send_errorf(resp, SELVA_EINTYPE,"Expected: %s", typeof_str(user_meta));
        return;
    }

    obj = SelvaObject_Open(resp, argv[ARGV_KEY]);
    if (!obj) {
        return;
    }

    memcpy(&user_meta, mval_str, sizeof(SelvaObjectMeta_t));
    err = SelvaObject_SetUserMeta(obj, argv[ARGV_OKEY], user_meta, NULL);
    if (err) {
        selva_send_errorf(resp, err, "Failed to set key metadata");
        return;
    }

    selva_send_ll(resp, 1);
}

static int SelvaObject_OnLoad(void) {
    selva_mk_command(CMD_OBJECT_DEL_ID, "object.del", SelvaObject_DelCommand);
    selva_mk_command(CMD_OBJECT_EXIST_ID, "object.exists", SelvaObject_ExistsCommand);
    selva_mk_command(CMD_OBJECT_GET_ID, "object.get", SelvaObject_GetCommand);
    selva_mk_command(CMD_OBJECT_LEN_ID, "object.len", SelvaObject_LenCommand);
    selva_mk_command(CMD_OBJECT_SET_ID, "object.set", SelvaObject_SetCommand);
    selva_mk_command(CMD_OBJECT_TYPE_ID, "object.type", SelvaObject_TypeCommand);
    selva_mk_command(CMD_OBJECT_GETMETA_ID, "object.getMeta", SelvaObject_GetMetaCommand);
    selva_mk_command(CMD_OBJECT_SETMETA_ID, "object.setMeta", SelvaObject_SetMetaCommand);

    return 0;
}
SELVA_ONLOAD(SelvaObject_OnLoad);
