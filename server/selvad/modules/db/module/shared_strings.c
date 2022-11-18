/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include "util/selva_string.h"
#include "selva_onload.h"
#include "selva_db.h"

static const char *shared_field_names[] = {
    SELVA_ID_FIELD,
    SELVA_TYPE_FIELD,
    SELVA_ALIASES_FIELD,
    SELVA_PARENTS_FIELD,
    SELVA_CHILDREN_FIELD,
    SELVA_ANCESTORS_FIELD,
    SELVA_DESCENDANTS_FIELD,
    SELVA_CREATED_AT_FIELD,
    SELVA_UPDATED_AT_FIELD,
};

static int shared_init(void) {
    for (size_t i = 0; i < num_elem(shared_field_names); i++) {
        const char *str = shared_field_names[i];

        (void)selva_string_create(str, strlen(str), SELVA_STRING_INTERN);
    }

    return 0;
}
SELVA_ONLOAD(shared_init);
