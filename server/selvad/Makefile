SHELL := /bin/bash

include common.mk

# TODO MacOs support
OBJ := \
	   src/ctime.o \
	   src/event_loop.o \
	   src/heap.o \
	   src/main.o \
	   src/module.o \
	   src/promise.o \
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
		  -DEVL_MAIN

#modules
all: main modules

main: $(OBJ) 
	#$(CC) -o $@ $^
	$(CC) $(CFLAGS) -o $@ $^

-include $(DEP)

modules:
	$(MAKE) -C modules

clean:
	$(RM) main
	find . -type f -name "*.d" -exec rm -f {} \;
	find . -type f -name "*.o" -exec rm -f {} \;
	find . -type f -name "*.so" -exec rm -f {} \;

.PHONY: clean modules
