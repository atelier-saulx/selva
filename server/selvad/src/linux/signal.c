/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "selva_error.h"
#include "evl_signal.h"

/*
 * > The signalfd mechanism can't be used to receive signals that are
 * > synchronously generated.
 */
int evl_create_sigfd(sigset_t *mask)
{
	int sfd;

	/*
     * Block signals so that they aren't handled
	 * according to their default dispositions.
     */
	if (sigprocmask(SIG_BLOCK, mask, NULL) == -1) {
        /* TODO Log error? */
        return SELVA_EGENERAL;
    }

	sfd = signalfd(-1, mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (sfd == -1) {
        /* TODO Log error? */
        return SELVA_EGENERAL;
    }

    return sfd;
}

int evl_read_sigfd(struct evl_siginfo *esig, int sfd)
{
    struct signalfd_siginfo fdsi;
    ssize_t s;

	s = read(sfd, &fdsi, sizeof(fdsi));
	if (s != sizeof(fdsi)) {
        return SELVA_EINVAL;
    }

    *esig = (struct evl_siginfo){
        .esi_signo = fdsi.ssi_signo,
        .esi_code = fdsi.ssi_code,
        .esi_errno = fdsi.ssi_errno,
        .esi_pid = fdsi.ssi_pid,
        .esi_uid = fdsi.ssi_uid,
        .esi_status = fdsi.ssi_status,
        .esi_utime = fdsi.ssi_utime,
        .esi_stime = fdsi.ssi_stime,
        .esi_addr = fdsi.ssi_addr,
        .esi_int = fdsi.ssi_int,
        .esi_ptr = (void *)fdsi.ssi_ptr,
        .esi_band = fdsi.ssi_band,
    };

    return 0;
}
