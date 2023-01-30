/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

[[nodiscard]]
int replication_replica_connect_to_origin(struct sockaddr_in *origin_addr);
int replication_replica_start(int sock);
