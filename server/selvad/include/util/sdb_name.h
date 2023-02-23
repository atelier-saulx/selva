/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * Minimum buffer size needed for a sdb filename.
 * The prefix length needs to be added to this.
 */
#define SDB_NAME_MIN_BUF_SIZE (20 + sizeof(".sdb"))

/**
 * Generate a new SDB filename.
 */
int sdb_name(char *buf, size_t buf_size, const char *prefix, uint64_t id);
