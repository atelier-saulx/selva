/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "event_loop.h"
#include "module.h"
#include "selva_log.h"

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

static int server_sockfd;

static int new_server(int port)
{
	int sockfd;
	struct sockaddr_in server;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		handle_error("Could not create socket");
	}

    (void)setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    (void)setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		handle_error("bind failed");
	}

	listen(sockfd, 3);
    SELVA_LOG(SELVA_LOGL_INFO, "Listening on port: %d", port);

    return sockfd;
}

void on_data(struct event *event, void *arg __unused)
{
    const int fd = event->fd;
    char buf[128];
    ssize_t r;

    r = read(fd, buf, sizeof(buf));
    if (r <= 0) {
        evl_end_fd(fd);
        return;
    } else if (r > 0) {
        SELVA_LOG(SELVA_LOGL_INFO, "Received msg: \"%.*s\"", (int)r, buf);
    }

    if (!strncmp(buf, "end", 3)) {
        /* Terminate this connection. */
        evl_end_fd(fd);
    } else if (!strncmp(buf, "quit", 4)) {
        /* Stop the server. */
        evl_end_fd(fd);
        evl_end_fd(server_sockfd);
    }
}

void on_connection(struct event *event, void *arg __unused)
{
    const int fd = event->fd;
    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;
    int new_sockfd;
    char buf[INET_ADDRSTRLEN];

    new_sockfd = accept(fd, (struct sockaddr *)&client, (socklen_t*)&c);
    if (new_sockfd < 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Accept failed");
        return;
    }

    inet_ntop(AF_INET, &client.sin_addr, buf, sizeof(buf));
    SELVA_LOG(SELVA_LOGL_INFO, "Received a connection from %s", buf);

    evl_wait_fd(new_sockfd, on_data, NULL, NULL, NULL);
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_event_loop();
}

__constructor void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init server");

    /* Async server for receiving messages. */
    server_sockfd = new_server(3000);
    evl_wait_fd(server_sockfd, on_connection, NULL, NULL, NULL);
}
