/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * Connect to origin server for replication.
 * @returns a socket to the origin.
 */
[[nodiscard]]
int replication_replica_connect_to_origin(struct sockaddr_in *origin_addr);

/**
 * Start replicating from an origin server.
 * @param sock is a socket returned by replication_replica_connect_to_origin().
 */
int replication_replica_start(int sock);
