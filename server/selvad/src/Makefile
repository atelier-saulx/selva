# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

include ../common.mk

OBJ := \
	   ctime.o \
	   event_loop.o \
	   heap.o \
	   main.o \
	   module.o \
	   promise.o \
	   selva_error.o \
	   selva_log.o \
	   timers.o
ifeq ($(uname_S),Linux) # Assume Intel x86-64 Linux
OBJ += \
	   linux/poll.o \
	   linux/signal.o
CFLAGS += -rdynamic -ldl -Wl,-rpath,$(MOD_PATH) -DUSE_EPOLL=1
endif
ifeq ($(uname_S),Darwin) # Assume x86-64 macOS
OBJ += \
	   darwin/poll.o \
	   darwin/signal.o
CFLAGS += -DUSE_POLL=1
endif

DEP := $(OBJ:%.o=%.d)
CFLAGS += -fvisibility=hidden \
		  -I../include \
		  -include ../include/cdefs.h \
		  -DEVL_MAIN

../selvad: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

-include $(DEP)
