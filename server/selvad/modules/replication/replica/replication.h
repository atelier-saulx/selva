/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct replication_sock_state;

struct replication_sock_state *replication_replica_init(void);

/**
 * Connect to origin server for replication.
 * @returns a socket to the origin.
 */
[[nodiscard]]
int replication_replica_connect_to_origin(struct replication_sock_state *sv, struct sockaddr_in *origin_addr);

/**
 * Start replicating from an origin server.
 * First call replication_replica_init(),
 * then replication_replica_connect_to_origin(),
 * and finally this function.
 * Freeing sv in case of an error is handled by these functions.
 */
int replication_replica_start(struct replication_sock_state *sv);
