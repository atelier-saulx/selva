/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <execinfo.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* gcc trace.c -g3 -rdynamic */

void print_trace(void *pc)
{
    void *array[10];
    void **ap = array;
    int size;

    fprintf(stderr, "Running a backtrace\n");
    size = backtrace(array, 10);
    if (pc) {
        for (int i = 0; i < 10; i++) {
            if (array[i] == pc) {
                fprintf(stderr, "Stack position restored\n");
                ap = array + i;
            }
        }
    }
    size = (array + 10) - ap;

    fprintf(stderr, "Obtained %d stack frames.\nHint: run `addr2line -e a.out -f <addr>` for these lines\n", size);
    backtrace_symbols_fd(ap, size, fileno(stderr));
}

void *get_pc(ucontext_t *ucontext)
{
#if __APPLE__ && defined(__arm64__)
    return (void*)arm_thread_state64_get_pc(uc->uc_mcontext->__ss);
#elif __APPLE__ && defined(_STRUCT_X86_THREAD_STATE64)
    return (void*)ucontext->uc_mcontext->__ss.__rip;
#elif __linux__ && defined(__aarch64__)
    return (void*)uc->uc_mcontext.pc;
#elif __linux__ && (defined(__X86_64__) || defined(__x86_64__))
    return (void*)ucontext->uc_mcontext.gregs[16];
#else
#error "Arch not supported"
#endif
}

void sig_segv_handler(int sig, siginfo_t *info, void *_ucontext)
{
    ucontext_t *ucontext = (ucontext_t*)_ucontext;
    void *pc = get_pc(ucontext);
    struct sigaction act;

    fprintf(stderr, "\n\nFATAL ERROR\n===========\nsignal: %d (%s), si_code: %d\n",
            sig, strsignal(sig), info->si_code);
    if (pc) {
        fprintf(stderr, "Crashed running the instruction at: %p\n", pc);
    } else {
        fprintf(stderr, "No PC\n");
    }
    if (sig == SIGSEGV || sig == SIGBUS) {
        fprintf(stderr, "Accessing address: %p\n", (void*)info->si_addr);
    }
    if (info->si_pid != -1) {
        fprintf(stderr, "Killed by PID: %d, UID: %d\n", info->si_pid, info->si_uid);
    }

    print_trace(pc);

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND;
    act.sa_handler = SIG_DFL;
    sigaction(sig, &act, NULL);
    kill(getpid(), sig);
}

void setup_term_signal(int sig)
{
    struct sigaction act;

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
    act.sa_sigaction = sig_segv_handler;

    sigaction(sig, &act, NULL);
}
