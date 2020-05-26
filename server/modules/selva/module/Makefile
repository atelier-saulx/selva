#set environment variable RM_INCLUDE_DIR to the location of redismodule.h
ifndef RM_INCLUDE_DIR
	RM_INCLUDE_DIR=../
endif

ifndef RMUTIL_LIBDIR
	RMUTIL_LIBDIR=../rmutil
endif

# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?=  -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -shared -Bsymbolic
else
	SHOBJ_CFLAGS ?= -dynamic -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
endif
CFLAGS = -I$(RM_INCLUDE_DIR) -Wall -g -fPIC -lc -lm -std=gnu99  
CC=gcc

all: rmutil module.so

rmutil: FORCE
	$(MAKE) -C $(RMUTIL_LIBDIR)

module.so: module.o id/id.o modify/modify.o modify/queue_r.o
ifeq ($(uname_S),Linux)
	$(LD) -o $@ module.o id/id.o modify/modify.o modify/queue_r.o $(SHOBJ_LDFLAGS) $(LIBS) -L$(RMUTIL_LIBDIR) -lrmutil -lc  -luuid -lhiredis -lcrypto -lssl
else
	$(LD) -o $@ module.o id/id.o modify/modify.o modify/queue_r.o $(SHOBJ_LDFLAGS) $(LIBS) -L$(RMUTIL_LIBDIR) -lrmutil -lc -lhiredis -lcrypto -lssl
endif

clean:
	rm -rf *.xo *.so *.o id/*.o

FORCE:
