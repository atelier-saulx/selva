/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/*
 * Generic tunables.
 */

/**
 * Debug memory usage.
 * 0 or undefined = Nothing
 * 1 = Clear some memory areas before freeing
 */
#define MEM_DEBUG 1

/**
 * Add delay to the replication of the Modify command.
 * Unit is nanoseconds. Normally this should be set to 0.
 */
#define DEBUG_MODIFY_REPLICATION_DELAY_NS 0

/*
 * SVector tunables.
 */

/**
 * Threshold to migrate from an SVECTOR_MODE_ARRAY to SVECTOR_MODE_RBTREE.
 */
#define SVECTOR_THRESHOLD 100

/**
 * How much memory to allocate when more memory is needed in
 * SVECTOR_MODE_RBTREE mode.
 */
#define SVECTOR_SLAB_SIZE 4194304
