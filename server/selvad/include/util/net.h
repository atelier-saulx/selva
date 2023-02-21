/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#define CONN_STR_LEN (INET_ADDRSTRLEN + 5 + 1)

size_t fd_to_str(int fd, char buf[CONN_STR_LEN], size_t bsize);
