/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct selva_string;

enum selva_db_dump_state {
    SELVA_DB_DUMP_NONE = 0x00, /*!< No dump operation running. */
    SELVA_DB_DUMP_ACTIVE_CHILD = 0x01, /*!< There is an active child. */
    SELVA_DB_DUMP_IS_CHILD = 0x02, /*!< This is the child process. */
};

extern enum selva_db_dump_state selva_db_dump_state;

int dump_load_default_sdb(void);

/**
 * Enable automatic SDB saving.
 * Load the last good SDB on startup and autosave on interval.
 * This function can be only called once.
 * @param interval_s [sec] must be > 0.
 */
int dump_auto_sdb(int interval_s);
