/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

void tcp_set_nodelay(int fd);
void tcp_unset_nodelay(int fd);
void tcp_set_keepalive(int fd, int time, int intvl, int probes);
void tcp_cork(int fd);
void tcp_uncork(int fd);

#ifndef FUZZER
#define tcp_read read
#define tcp_write write
#define tcp_send send
#define tcp_sendto sendto
#define tcp_sendmsg sendmsg
#endif
