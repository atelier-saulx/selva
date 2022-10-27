# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

SHELL := /bin/bash

include common.mk
export uname_S

# Ordered list of libraries
LIBS := \
		lib/jemalloc \
		lib/deflate \
		lib/util

OBJ := \
	   src/ctime.o \
	   src/event_loop.o \
	   src/heap.o \
	   src/main.o \
	   src/module.o \
	   src/promise.o \
	   src/selva_error.o \
	   src/timers.o
ifeq ($(uname_S),Linux) # Assume Intel x86-64 Linux
OBJ += \
	   src/linux/poll.o \
	   src/linux/signal.o
CFLAGS += -rdynamic -ldl -Wl,-rpath,$(PWD)/modules -DUSE_EPOLL=1
endif
ifeq ($(uname_S),Darwin) # Assume x86-64 macOS
OBJ += \
	   src/darwin/poll.o \
	   src/darwin/signal.o
CFLAGS += -DUSE_POLL=1
endif

DEP := $(OBJ:%.o=%.d)
CFLAGS += -fvisibility=hidden \
		  -Iinclude \
		  -include include/cdefs.h \
		  -DEVL_MAIN

#modules
all: selvad modules $(LIBS)

selvad: $(OBJ)
	#$(CC) -o $@ $^
	$(CC) $(CFLAGS) -o $@ $^

-include $(DEP)

# Build all libraries (ordered)
# TODO Doesn't work properly!?
lib: | $(LIBS)

$(LIBS):
	$(MAKE) -C $@

modules:
	$(MAKE) -C modules

clean:
	$(RM) selvad
	find . -type f -name "*.d" -exec rm -f {} \;
	find . -type f -name "*.o" -exec rm -f {} \;
	find . -type f -name "*.so" -exec rm -f {} \;
	find . -type f -name "*.dylib" -exec rm -f {} \;
	find ./lib -type d -maxdepth 1 -exec $(MAKE) -C {} clean \;

.PHONY: clean modules lib $(LIBS)
