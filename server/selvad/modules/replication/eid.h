/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * Generate a new EID for a new SDB dump on origin.
 */
uint64_t replication_new_origin_eid(const char *filename);

/**
 * Extract the EID of an SDB dump.
 * Try to extract the EID the origin would recognize from an SDB dump that
 * was just loaded. This can only work if the EID is present in the filename,
 * as such information is not stored in the file itself.
 */
uint64_t replication_new_replica_eid(const char *filename);
