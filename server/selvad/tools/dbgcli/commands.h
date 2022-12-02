#pragma  once

struct cmd;

typedef int (*cmd_req_fn)(const struct cmd *cmd, int fd, int seqno);
typedef void (*cmd_res_fn)(const struct cmd *cmd, const void *buf, size_t bsize);

struct cmd {
    int cmd_id;
    const char *cmd_name;
    cmd_req_fn cmd_req;
    cmd_res_fn cmd_res;
};

void cmd_discover(int fd, void (*cb)(struct cmd *cmd));
