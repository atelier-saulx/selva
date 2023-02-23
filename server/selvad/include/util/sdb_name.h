/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * Generate a new SDB filename.
 */
int sdb_name(char *buf, size_t buf_size, const char *prefix, uint64_t id);
