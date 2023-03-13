/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "selva_log.h"
#include "event_loop.h"
#include "evl_signal.h"
#include "promise.h"
#include "module.h"
#include "trace.h"

static void handle_signal(struct event *ev, void *arg __unused)
{
    struct evl_siginfo esig;
    int signo;

    if (evl_read_sigfd(&esig, ev->fd)) {
        fprintf(stderr, "Failed to read sigfd\n");
        return;
    }

    signo = esig.esi_signo;
    fprintf(stderr, "Received signal (%d): %s\n", signo, strsignal(esig.esi_signo));

    switch (signo) {
    case SIGINT:
    case SIGTERM:
        exit(EXIT_SUCCESS);
    }
}

/**
 * Signals that should terminate the process.
 * These are als often but not always synchronus signals that can't be used with
 * evl_create_sigfd().
 */
#define TERM_SIGNALS(apply) \
    apply(SIGSEGV) \
    apply(SIGBUS) \
    apply(SIGFPE) \
    apply(SIGILL) \
    apply(SIGSYS)

static void setup_async_signals(void)
{
	sigset_t mask;
	int sfd;

    /*
     * We try to catch everything async.
     */
	sigemptyset(&mask);
    sigfillset(&mask);
#define DEL_SIGNAL(sig) \
    sigdelset(&mask, sig);
    TERM_SIGNALS(DEL_SIGNAL);
    sigdelset(&mask, SIGPIPE);
    sigdelset(&mask, SIGCHLD); /* We want to catch this where we use fork(). */
#undef DEL_SIGNAL

    sfd = evl_create_sigfd(&mask);
    if (sfd >= 0) {
        evl_wait_fd(sfd, handle_signal, NULL, NULL, NULL);
    }
}

IMPORT() {
    evl_import_event_loop();
    evl_import_signal();
}

__constructor void init(void)
{

    /*
     * SIGPIPE can be sync or async depending on the system.
     * Either way, we just want to ignore it.
     */
    sigaction(SIGPIPE, &(struct sigaction){ .sa_handler = SIG_IGN }, NULL);

#define SETUP_TERM(sig) \
    setup_term_signal(sig);
    TERM_SIGNALS(SETUP_TERM);
#undef SETUP_TERM

    setup_async_signals();
}
