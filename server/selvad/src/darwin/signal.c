/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "selva_log.h"
#include "selva_error.h"
#include "evl_signal.h"

#define DARWIN_SIGNALS(apply) \
    apply(SIGHUP) \
    apply(SIGINT) \
    apply(SIGQUIT) \
    apply(SIGILL) \
    apply(SIGTRAP) \
    apply(SIGABRT) \
    apply(SIGEMT) \
    apply(SIGFPE) \
    apply(SIGKILL) \
    apply(SIGBUS) \
    apply(SIGSEGV) \
    apply(SIGSYS) \
    apply(SIGPIPE) \
    apply(SIGALRM) \
    apply(SIGTERM) \
    apply(SIGURG) \
    apply(SIGSTOP) \
    apply(SIGTSTP) \
    apply(SIGCONT) \
    apply(SIGCHLD) \
    apply(SIGTTIN) \
    apply(SIGTTOU) \
    apply(SIGIO) \
    apply(SIGXCPU) \
    apply(SIGXFSZ) \
    apply(SIGVTALRM) \
    apply(SIGPROF) \
    apply(SIGWINCH) \
    apply(SIGINFO) \
    apply(SIGUSR1) \
    apply(SIGUSR2)

static const int darwin_all[] = {
#define F_SIGNUM(sig) \
    sig,
    DARWIN_SIGNALS(F_SIGNUM)
#undef F_SIGNUM
};

static int darwin_sig2fd_wr[] = {
#define F_INIT_SIG2FD(sig) \
    [sig] = -1,
    DARWIN_SIGNALS(F_INIT_SIG2FD)
#undef F_INIT_SIG2FD
};

static void sig_handler(int sig, siginfo_t *info, void *uap __unused)
{
    const int saved_errno = errno;
    const int wfd = darwin_sig2fd_wr[sig];

    if (wfd == -1) {
        SELVA_LOG(SELVA_LOGL_ERR, "Signal hander fd not found for signo: %d", sig);
        return;
    }

    struct evl_siginfo esig = {
        .esi_signo = info->si_signo,
        .esi_code = info->si_code,
        .esi_errno = info->si_errno,
        .esi_pid = info->si_pid,
        .esi_uid = info->si_uid,
        .esi_status = info->si_status,
        .esi_addr = (uintptr_t)info->si_addr,
        .esi_int = info->si_value.sival_int,
        .esi_ptr = info->si_value.sival_ptr,
        .esi_band = info->si_band,
    };

    SELVA_LOG(SELVA_LOGL_INFO, "Handling signal. signo: %d fd: %d", sig, wfd);

    int err = write(wfd, &esig, sizeof(esig));
    if (err == -1) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to handle signal. signo: %d fd: %d err: \"%s\"",
                  sig, wfd, strerror(errno));
    }

    errno = saved_errno;
}

static int set_pipe_flags(int fd)
{
    int flags;

    /*
     * Set FL flags.
     */
    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return SELVA_EGENERAL;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        return SELVA_EGENERAL;
    }

    /*
     * Set FD flags.
     */
    flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        return SELVA_EGENERAL;
    }
    flags |= FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) == -1) {
        return SELVA_EGENERAL;
    }

    fcntl(fd, F_SETNOSIGPIPE, 1);

    return 0;
}

static int is_registered_sigfd(sigset_t *mask)
{
    for (size_t i = 0; i < num_elem(darwin_all); i++) {
        const int sig = darwin_all[i];
        if (sigismember(mask, sig) && darwin_sig2fd_wr[sig] != -1) {
            return 1;
        }
    }

    return 0;
}

int evl_create_sigfd(sigset_t *mask)
{
    int pfd[2];
    struct sigaction act;

    /*
     * Allow creating handler only once per signal.
     */
    if (is_registered_sigfd(mask)) {
        return SELVA_EEXIST;
    }

    if (pipe(pfd) != 0) {
        return SELVA_ENOBUFS;
    }

    set_pipe_flags(pfd[0]);
    set_pipe_flags(pfd[1]);

    act.sa_sigaction = sig_handler;
    sigfillset(&act.sa_mask);
    act.sa_flags = SA_RESTART | SA_SIGINFO;

    for (size_t i = 0; i < num_elem(darwin_all); i++) {
        const int sig = darwin_all[i];
        if (sigismember(mask, sig)) {
            /* TODO Handle error? */
            sigaction(sig, &act, NULL);
            darwin_sig2fd_wr[sig] = pfd[1];
        }
    }

    return pfd[0]; /* sfd */
}

int evl_read_sigfd(struct evl_siginfo *esig, int sfd)
{
    ssize_t s;

	s = read(sfd, esig, sizeof(*esig));
	if (s != sizeof(*esig)) {
        return SELVA_EINVAL;
    }

    return 0;
}
