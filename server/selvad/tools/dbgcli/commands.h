/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct cmd;

typedef int (*cmd_req_fn)(const struct cmd *cmd, int fd, int seqno, int argc, char *argv[]);
typedef void (*cmd_res_fn)(const struct cmd *cmd, const void *buf, size_t bsize);

struct cmd {
    int cmd_id;
    const char *cmd_name;
    cmd_req_fn cmd_req;
    cmd_res_fn cmd_res;
};

/**
 * Send a message.
 */
int send_message(int fd, void *buf, size_t size, int flags);

/**
 * Receive a message.
 * Not reentrant.
 */
void recv_message(int fd);

void cmd_discover(int fd, int seqno);
void cmd_foreach(void (*cb)(struct cmd *cmd));
