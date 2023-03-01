/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct selva_string;

int dump_load_default_sdb(void);

/**
 * Enable automatic SDB saving.
 * Load the last good SDB on startup and autosave on interval.
 * This function can be only called once.
 * @param interval_s [sec] must be > 0.
 */
int dump_auto_sdb(int interval_s);
