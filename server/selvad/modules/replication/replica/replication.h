/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * Start replicating from an origin server.
 */
int replication_replica_start(struct sockaddr_in *origin_addr);
