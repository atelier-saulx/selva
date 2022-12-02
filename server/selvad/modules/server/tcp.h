/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

void tcp_set_nodelay(int fd);
void tcp_unset_nodelay(int fd);
void tcp_cork(int fd);
void tcp_uncork(int fd);
