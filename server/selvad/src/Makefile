# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

include ../common.mk

OBJ := \
	   event_loop.o \
	   heap.o \
	   main.o \
	   module.o \
	   promise.o \
	   selva_log.o \
	   timers.o

CFLAGS += -fvisibility=hidden \
		  -I../include \
		  -include ../include/cdefs.h \
		  -DEVL_MAIN=1

LDFLAGS += -ljemalloc_selva -lutil
ifeq ($(uname_S),Linux)
OBJ += \
	   linux/poll.o \
	   linux/signal.o
CFLAGS += -DUSE_EPOLL=1
LDFLAGS += -rdynamic -ldl -L ../lib -Wl,-rpath,'$$ORIGIN'/lib
endif
ifeq ($(uname_S),Darwin)
OBJ += \
	   darwin/poll.o \
	   darwin/signal.o
CFLAGS += -DUSE_POLL=1
endif

DEP := $(OBJ:%.o=%.d)

../selvad: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

-include $(DEP)
